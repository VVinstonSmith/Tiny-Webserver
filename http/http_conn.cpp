#include "http_conn.h"
#include "util.h"
#include <mysql/mysql.h>
#include <fstream>

// 定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> http_conn::users; // username与passwd的映射关系
map<string, string> http_conn::user_role; // username与role的映射关系
map<unsigned long, string> http_conn::ip_user; // ip地址与user的映射关系(已登录)

/* 将sql中已有的username,passwd,映射全部取出，放入map */
void http_conn::initmysql_result(connection_pool *connPool){
  // 先从sql连接池中取出一个连接
  MYSQL* mysql = NULL;
  connectionRAII mysqlcon(&mysql, connPool);

  // 输入sql语句检索
  if(mysql_query(mysql, "SELECT username,passwd,userrole FROM user")){;
    LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
  }
  // 提取检索结果
  MYSQL_RES* result = mysql_store_result(mysql);
  while(MYSQL_ROW row = mysql_fetch_row(result)){
    string temp1(row[0]);
    string temp2(row[1]);
    string temp3(row[2]);
    users[temp1] = temp2;
    user_role[temp1] = temp3;
  }
}

/* 对文件描述符设置非阻塞 */
int setnonblocking(int fd){
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

/* 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT */
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode){
  epoll_event event;
  event.data.fd = fd;
  if (1 == TRIGMode){
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
  }else{
    event.events = EPOLLIN | EPOLLRDHUP;
  }
  if (one_shot) event.events |= EPOLLONESHOT; 
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  setnonblocking(fd);
}

/* 从内核时间表删除描述符 */
void removefd(int epollfd, int fd){
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
  close(fd);
}

/* 将事件重置为EPOLLONESHOT */
void modfd(int epollfd, int fd, int ev, int TRIGMode){
  epoll_event event;
  event.data.fd = fd;
  if (1 == TRIGMode){
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
  }else{
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
  }
  epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// http_conn的static变量：用户数，epollfd
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

/* 关闭连接，关闭一个连接，客户总量减一 */
void http_conn::close_conn(bool real_close){
  if (real_close && (m_sockfd != -1)){
    printf("close %d\n", m_sockfd);
    removefd(m_epollfd, m_sockfd);
    m_sockfd = -1;
    m_user_count--;
  }
}

/* 初始化连接,外部调用初始化套接字地址 */
void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname)
{
  m_sockfd = sockfd;
  m_address = addr;
  m_ipaddr = addr.sin_addr.s_addr;
  m_username = "";
  m_role = "";
  if(ip_user.find(m_ipaddr)!=ip_user.end()){
    m_username = ip_user[m_ipaddr];
    m_role = user_role[m_username];
  }
  
  addfd(m_epollfd, sockfd, true, m_TRIGMode); // 将sockfd加入epoll事件表
  m_user_count++;

  // 当浏览器出现连接重置时，可能是 网站根目录出错 或 http响应格式出错 或 访问的文件中内容完全为空
  doc_root = root;
  m_TRIGMode = TRIGMode;
  m_close_log = close_log;

  strcpy(sql_user, user.c_str());
  strcpy(sql_passwd, passwd.c_str());
  strcpy(sql_name, sqlname.c_str());

  init(); // 初始化该http_conn的各种状态字和缓冲区
}

/* 初始化新接受的连接 */
// check_state默认为分析请求行状态
void http_conn::init(){
  mysql = NULL;
  bytes_to_send = 0;
  bytes_have_send = 0;
  m_check_state = CHECK_STATE_REQUESTLINE;
  m_linger = false;
  m_method = GET;
  m_url = 0;
  m_version = 0;
  m_string = NULL;
  m_content_length = 0;
  m_host = 0;
  m_start_line = 0;
  m_checked_idx = 0;
  m_read_idx = 0;
  m_write_idx = 0;
  cgi = 0;
  m_state = 0;
  timer_flag = 0;
  improv = 0;

  memset(m_read_buf, '\0', READ_BUFFER_SIZE);
  memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
  memset(m_real_file, '\0', FILENAME_LEN);
}

/* 循环读取客户数据到read_buffer，直到无数据可读或对方关闭连接 */
// 非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once(){
  if(m_read_idx >= READ_BUFFER_SIZE){
    return false;
  }
  int bytes_read = 0;

  /* LT读取数据 */
  if (0 == m_TRIGMode){
    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
    m_read_idx += bytes_read;
    if(bytes_read <= 0) return false; // 发生读取错误
    return true;
  }
  /* ET读数据 */
  else{
    while (true){ /* 循环读取，保证一次性读完 */
      bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
      if(bytes_read == -1){
        if (errno == EAGAIN || errno == EWOULDBLOCK) // 暂时没有数据可读
          break;
        return false;
      }
      else if(bytes_read == 0){
        return false;
      }
      m_read_idx += bytes_read;
    }
    return true;
  }
}

/* 从状态机，用于分析出一行内容 */
// 返回值为行的读取状态，有 LINE_OK, LINE_BAD, LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line(){
  char temp;
  for(; m_checked_idx < m_read_idx; ++m_checked_idx){
    temp = m_read_buf[m_checked_idx];
    if(temp == '\r'){
      if((m_checked_idx + 1) == m_read_idx){
        return LINE_OPEN; // 视下一个是不是'\n'而定
      }else if(m_read_buf[m_checked_idx + 1] == '\n'){
        m_read_buf[m_checked_idx++] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }else if(temp == '\n'){
      if(m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r'){
        // 上一次只读取到了'\r'
        m_read_buf[m_checked_idx - 1] = '\0';
        m_read_buf[m_checked_idx++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_OPEN;
}

/* 解析http请求行，获得请求方法，目标url及http版本号 */
http_conn::HTTP_CODE http_conn::parse_request_line(char *text){
  m_url = strpbrk(text, " \t"); // 第一个空格或'\t'的位置
  if(!m_url) return BAD_REQUEST;
  *m_url++ = '\0';
  
  char *method = text;
  if(strcasecmp(method, "GET") == 0){
    m_method = GET;
  }else if(strcasecmp(method, "POST") == 0){
    m_method = POST;
    cgi = 1;
  }else return BAD_REQUEST;
      
  m_url += strspn(m_url, " \t");
  m_version = strpbrk(m_url, " \t");
  if(!m_version) return BAD_REQUEST;
  *m_version++ = '\0';
  m_version += strspn(m_version, " \t");

  if(strcasecmp(m_version, "HTTP/1.1") != 0){
    return BAD_REQUEST;
  }

  /* 去掉url中的http或https前缀 */
  if(strncasecmp(m_url, "http://", 7) == 0){
    m_url += 7;
    m_url = strchr(m_url, '/');
  }
  if(strncasecmp(m_url, "https://", 8) == 0){
    m_url += 8;
    m_url = strchr(m_url, '/');
  }
  if(!m_url || m_url[0] != '/') return BAD_REQUEST; // url必须'/'开头

  // 当url为'/'时，显示判断界面
  if(strlen(m_url) == 1)  strcat(m_url, "judge.html");

  m_check_state = CHECK_STATE_HEADER;
  return NO_REQUEST;
}

/* 解析http请求的一个头部信息 */
http_conn::HTTP_CODE http_conn::parse_headers(char *text){
  /* 若是空行 */
  if(text[0] == '\0'){
    if(m_content_length != 0){ // 对应POST指令，还有content没有解析
      m_check_state = CHECK_STATE_CONTENT;
      return NO_REQUEST;
    }
    return GET_REQUEST; // 对应GET指令，http报文解析完毕
  }
  else if(strncasecmp(text, "Connection:", 11) == 0){
    text += 11;
    text += strspn(text, " \t");
    if(strcasecmp(text, "keep-alive") == 0){
      m_linger = true; // 是长连接
    }
  }
  else if(strncasecmp(text, "Content-length:", 15) == 0){
    text += 15;
    text += strspn(text, " \t");
    m_content_length = atol(text);
  }
  else if(strncasecmp(text, "Host:", 5) == 0){
    text += 5;
    text += strspn(text, " \t");
    m_host = text;
  }
  else{
    LOG_INFO("oop!unknow header: %s", text);
  }
  return NO_REQUEST;
}

/* 判断http请求是否被完整读入 */
http_conn::HTTP_CODE http_conn::parse_content(char *text){
  if(m_read_idx >= (m_content_length + m_checked_idx)){
    text[m_content_length] = '\0';
    // POST请求中最后为输入的用户名和密码
    m_string = text;
    return GET_REQUEST;
  }
  return NO_REQUEST;
}

/* 主状态机 */
http_conn::HTTP_CODE http_conn::process_read(){
  LINE_STATUS line_status = LINE_OK;
  HTTP_CODE ret = NO_REQUEST;
  char *text = 0;

  while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || \
        ((line_status = parse_line()) == LINE_OK))
  { // 当m_check_state变为CHECK_STATE_CONTENT之后，将不再调用parse_line()
    text = get_line(); // 指针移动到尚未解析的字符处
    m_start_line = m_checked_idx;
    LOG_INFO("%s", text);
    switch (m_check_state){
      case CHECK_STATE_REQUESTLINE:{ // 请求行读取完毕，开始解析
        ret = parse_request_line(text);
        if(ret == BAD_REQUEST) return BAD_REQUEST;
        break;
      }case CHECK_STATE_HEADER:{ // 报文头读取完毕，开始解析
        ret = parse_headers(text);
        if(ret == BAD_REQUEST) return BAD_REQUEST;
        else if(ret == GET_REQUEST){
          // 响应GET命令，或无content的POST命令
          return do_request();
        }
        break;
      }case CHECK_STATE_CONTENT:{ // 读到了完整的报文内容
        // std::cout<<"enter CHECK_STATE_CONTENT"<<std::endl;
        ret = parse_content(text);
        if(ret == GET_REQUEST){
          // 响应POST命令
          return do_request();
        }
        // content没有读完整，更改条件，下次进入循环时需要parse_line()
        line_status = LINE_OPEN;
        break;
      }
      default:
        return INTERNAL_ERROR;
    }
  }
  return NO_REQUEST;
}

/*
m_url
无需登录状态：
/ : GET请求，跳转到欢迎页面 judge.html
/0 : POST请求，跳转到注册页面 register.html
/1 : POST请求，跳转到登录页面 log.html
/2CGISQL.cgi : 
  POST请求，进行登录校验
  验证成功跳转到 welcome.html
  验证失败跳转到 logError.html
/3CGISQL.cgi : 
  POST请求，进行注册校验
  注册成功跳转到 log.html
  注册失败跳转到 registerError.html
登录状态：
/function-buyer:买家功能页面
/function-seller:卖家功能页面
/search_item.html:商品查询页面
/create_item.html:商品创建页面
*/
/* 返回对请求的文件分析后的结果,将目标文件copy到内存中 */
http_conn::HTTP_CODE http_conn::do_request(){
  strcpy(m_real_file, doc_root); // 将根目录复制到文件名字符串
  int len = strlen(doc_root);
  //printf("m_url:%s\n", m_url);
  const char *p = strrchr(m_url, '/'); // 最后一次出现'/'的位置

  /* cgi登录或注册 */
  if(cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3')){ // 根据标志判断是登录检测还是注册检测
    /* 将用户名,密码,身份提取出来 */
    // 形如：user=123&password=123&role=buyer
    char name[100], password[100], role[10];
    char* substr[] = {name, password, role};
    split_str(m_string, substr, 3);

    if (*(p + 1) == '3'){
      // 如果是注册，先检测数据库中是否有重名的
      // 没有重名的，进行增加数据
      if(users.find(name) == users.end()){ // 若map中没有
        // 生成mysql查询词条
        char* sql_insert = new char[200];
        gen_sql_insert_user(sql_insert, name, password, role);

        m_lock.lock();
        int res = mysql_query(mysql, sql_insert); // 插入mysql
        users.insert(pair<string,string>(name, password)); // 插入[username, passwd]
        user_role.insert(pair<string,string>(name, role)); // 插入[username, role]
        m_lock.unlock();
        
        delete [] sql_insert;
        if(!res) strcpy(m_url, "/log.html"); // 若成功插入mysql
        else strcpy(m_url, "/registerError.html");
      }else{
        strcpy(m_url, "/registerError.html");
      }
    }else if (*(p + 1) == '2'){
      // 如果是登录，直接判断
      if (users.find(name) != users.end() && users[name] == password && user_role[name]==role){
        if(strcmp(role, "buyer")==0){
          strcpy(m_url, "/login-function-buyer.html");
        }else{
          strcpy(m_url, "/login-function-seller.html");
        }
        ip_user[m_ipaddr] = m_username = name;
        m_role = user_role[m_username];
      }else{
        strcpy(m_url, "/logError.html");
      }
    }
  }
  std::cout<<m_username<<"  "<<m_url<<std::endl;
  /* 无需登录的url或页面 */
  if(strncasecmp(m_url+1, "login", 5)!=0){ 
    // 0转到注册页面
    if(strcmp(m_url, "/0")==0)
      strcpy(m_real_file+len, "/register.html");
    // 1转到登录页面
    else if(strcmp(m_url, "/1")==0)
      strcpy(m_real_file+len, "/log.html");
    // 未登录情况下，碰到其他url直接回首页
    else
      strncpy(m_real_file+len, m_url, FILENAME_LEN-len-1);
  }
  /* 需要登录的url或页面，并且ip_addr已经处于登录状态 */
  else if(!m_username.empty()){ 
    /*--------------------处理buyer发送的url--------------------------*/
    // 希望转到商品查找页面
    if(strcmp(m_url+7, "search_item")==0){ 
      if(m_role=="buyer")
        strcpy(m_real_file+len, "/login-search_item.html");
      else
        strcpy(m_real_file+len, "/log.html");
    }
    // 执行商品查询
    else if(strcmp(m_url+7, "search")==0){
      if(m_role=="buyer"){
        select_from_itemList(m_string, "buyer", mysql);
        strcpy(m_real_file+len, "/login-itemList-buyer.html");
      }else
        strcpy(m_real_file+len, "/log.html");
    }
    // 商品下单操作
    else if(strncmp(m_url+7, "order", 5)==0){ // 
      if(m_role=="buyer"){
        int res = insert_into_dealList(m_url+8, m_string, m_username.c_str(), mysql);
        if(!res)
          strcpy(m_real_file+len, "/order_success.html");
        else
          strcpy(m_real_file+len, "/mysql_insert_error.html");
      }else
        strcpy(m_real_file+len, "/log.html");
    }
    /*--------------------处理seller发送的url--------------------------*/
    // 希望转到商品创建页面
    else if(strcmp(m_url+7, "create_item")==0){
      if(m_role=="seller")
        strcpy(m_real_file+len, "/login-create_item.html");
      else
        strcpy(m_real_file+len, "/log.html");
    }
    // 执行商品创建
    else if(strcmp(m_url+7, "create")==0){
      if(m_role=="seller"){
        int res = insert_into_itemList(m_string, m_username.c_str(), mysql);
        if(!res) 
          strcpy(m_real_file+len, "/login-create_item.html");
        else
          strcpy(m_real_file+len, "/mysql_insert_error.html");
      }else
        strcpy(m_real_file+len, "/log.html");
    }
    // 本卖家待售商品查询
    else if(strcmp(m_url+7, "check_itemList")==0){
      if(m_role=="seller"){
        select_from_itemList(m_username.c_str(), "seller", mysql);
        strcpy(m_real_file+len, "/login-itemList-seller.html");
      }else
        strcpy(m_real_file+len, "/log.html");
    }
    // 商品删除操作
    else if(strcmp(m_url+7, "unshelve")==0){
      if(m_role=="seller"){
        if(m_string){ // operate sql
          int res1 = delete_from_table(m_string, "deal", "itemID", mysql);
          int res2 = delete_from_table(m_string, "item", "id", mysql);
          if(!res1 && !res2)
            strcpy(m_real_file+len, "/delete_item_success.html");
          else                         
            strcpy(m_real_file+len, "/mysql_delete_error.html");
        }else
          strcpy(m_real_file+len, "/login-itemList-seller.html");
      }else // is not seller
        strcmp(m_real_file+len, "/log.html");
    }
    /*--------------------buyer与seller通用的url--------------------------*/
    // 订单查询操作
    else if(strcmp(m_url+7, "check_dealList")==0){  
      select_from_dealList(m_username.c_str(), m_role.c_str(), mysql);
      if(m_role=="buyer")
        strcpy(m_real_file+len, "/login-dealList-buyer.html");
      else
        strcpy(m_real_file+len, "/login-dealList-seller.html");
    }
    // 订单删除操作
    else if(strcmp(m_url+7, "cancelDeal")==0){ 
      if(m_string){ 
        int res = delete_from_table(m_string, "deal", "id", mysql);
        if(!res)
          strcpy(m_real_file+len, "/delete_deal_success.html");
        else
          strcpy(m_real_file+len, "/mysql_delete_error.html");
      }else{
        if(m_role=="buyer")
          strcpy(m_real_file+len, "/login-dealList-buyer.html");
        else
          strcpy(m_real_file+len, "/login-dealList-seller.html");
      }
    }
    // 解除登录
    else if(strcmp(m_url+7, "unlog")==0){
      ip_user.erase(m_ipaddr);
      m_username = "";
      m_role = "";
      strcpy(m_real_file+len, "/log.html");
    }
    // 其他情况下(已登录)，将url直接加到根目录下
    else{
      strncpy(m_real_file+len, m_url, FILENAME_LEN-len-1);
    }
  }
  /* 未登录情况下访问需要登录才能访问的页面 */
  else{
    strcpy(m_real_file+len, "/log.html");
  }

  m_string = NULL;

  /* 通过stat获取请求资源的文件信息，成功则将信息更新到m_file_stat结构体 */
  if (stat(m_real_file, &m_file_stat) < 0){
    return NO_RESOURCE;
  }
  /* 判断文件权限，是否可读 */
  if (!(m_file_stat.st_mode & S_IROTH)){
    return FORBIDDEN_REQUEST;
  }
  /* 判断文件类型，确保不是目录 */
  if (S_ISDIR(m_file_stat.st_mode)){
    return BAD_REQUEST;
  }

  /* 以只读方式获取文件描述符，通过mmap将该文件映射回到内存中 */
  int fd = open(m_real_file, O_RDONLY);
  m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  return FILE_REQUEST;
}

/* 取消文件映射 */
void http_conn::unmap(){
  if (m_file_address){
    munmap(m_file_address, m_file_stat.st_size);
    m_file_address = 0;
  }
}

/* 添加状态行的函数，通过可变参数列表方式输出到m_write_buff */
bool http_conn::add_response(const char *format, ...){
  if(m_write_idx >= WRITE_BUFFER_SIZE) return false;
  /* 定义可变参数列表 */
  va_list arg_list;
  va_start(arg_list, format);

  /* 将数据format从可变从参数列表写入写缓冲区，返回写入数据的长度 */
  int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
  if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)){
    va_end(arg_list);
    return false;
  }
  m_write_idx += len;
  va_end(arg_list);
  LOG_INFO("request:%s", m_write_buf);
  return true;
}

bool http_conn::add_status_line(int status, const char *title){
  return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len){
  return add_content_length(content_len) && add_linger() && add_blank_line();
}
bool http_conn::add_content_length(int content_len){
  return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type(){
  return add_response("Content-Type:%s\r\n", "text/html");
}
bool http_conn::add_linger(){
  return add_response("Connection:%s\r\n", (m_linger==true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line(){
  return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content){
  return add_response("%s", content);
}

/* 填写缓冲区和向量元素 */
bool http_conn::process_write(HTTP_CODE ret){
  switch(ret){
    case INTERNAL_ERROR:{
      add_status_line(500, error_500_title);
      add_headers(strlen(error_500_form));
      if(!add_content(error_500_form)) return false;
      break;
    }case BAD_REQUEST:{
      add_status_line(404, error_404_title);
      add_headers(strlen(error_404_form));
      if(!add_content(error_404_form)) return false;
      break;
    }case FORBIDDEN_REQUEST:{
      add_status_line(403, error_403_title);
      add_headers(strlen(error_403_form));
      if(!add_content(error_403_form)) return false;
      break;
    }case FILE_REQUEST:{
      add_status_line(200, ok_200_title);
      if (m_file_stat.st_size != 0){ // 文件存在
        add_headers(m_file_stat.st_size);
        // 响应报文
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        // 请求文件
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_stat.st_size;
        m_iv_count = 2;
        bytes_to_send = m_write_idx + m_file_stat.st_size;
        return true;
      }else{ // 文件不存在
        const char *ok_string = "<html><body></body></html>"; // content为空白页面
        add_headers(strlen(ok_string));
        if(!add_content(ok_string)) return false;
      }
    }default: return false;
  }
  m_iv[0].iov_base = m_write_buf;
  m_iv[0].iov_len = m_write_idx;
  m_iv_count = 1;
  bytes_to_send = m_write_idx;
  return true;
}

/* 将响应报文发送到浏览器端 */
bool http_conn::write(){
  int temp = 0;
  if(bytes_to_send == 0){
    modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
    init();
    return true;
  }
  while(1){
    // 将分散内存块中的数据一并写入文件描述符
    temp = writev(m_sockfd, m_iv, m_iv_count);
    if(temp < 0){
      if (errno == EAGAIN){
        // 若TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件
        modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
        return true;
      }
      unmap();
      return false;
    }
    /* 更新待发送的数据 */
    bytes_have_send += temp;
    bytes_to_send -= temp;
    if (bytes_have_send >= m_iv[0].iov_len){ // 若m_iv[0]已经方完毕
      m_iv[0].iov_len = 0;
      m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
      m_iv[1].iov_len = bytes_to_send;
    }else{ // m_iv[0]都没发送完
      m_iv[0].iov_base = m_write_buf + bytes_have_send;
      m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
    }
    /* 发送完毕，解除内存映射，重新注册epoll事件表，初始化http_conn对象 */
    if(bytes_to_send <= 0){ // m_iv[0],m_iv[1]都已发送完毕
      unmap();
      modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
      if (m_linger){ // 长连接，将http_conn对象初始化
        init();
        return true;
      }else return false;
    }
  }
}

/* 由工作线程调用，处理http报文请求：worker->run->process() */
void http_conn::process(){
  HTTP_CODE read_ret = process_read();
  if(read_ret == NO_REQUEST){ // 没读完整,重新注册EPOLLIN
    modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
    return;
  }
  bool write_ret = process_write(read_ret);
  if(!write_ret){ // 若是短连接，则直接关闭该连接socket
    close_conn();
  }
  modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode); // 重新注册epoll事件表
}
