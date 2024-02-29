#include "webserver.h"

WebServer::WebServer(){
  // 创建http_conn类对象
  users = new http_conn[MAX_FD];

  // root文件夹路径: server_path/root 
  char server_path[200];
  getcwd(server_path, 200); // 将当前工作目录的绝对路径复制进server_path
  char root[6] = "/root";
  m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
  strcpy(m_root, server_path);
  strcat(m_root, root);

  // 创建与定时器关联的socketfd和网络地址
  users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer(){
  close(m_epollfd);
  close(m_listenfd);
  close(m_pipefd[1]);
  close(m_pipefd[0]);
  delete[] users;
  delete[] users_timer;
  delete m_pool;
}

void WebServer::init(int port, string user, string passWord, string databaseName, int log_write, 
  int opt_linger, int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
  m_port = port;
  m_user = user;
  m_passWord = passWord;
  m_databaseName = databaseName;
  m_sql_num = sql_num;
  m_thread_num = thread_num;
  m_log_write = log_write;
  m_OPT_LINGER = opt_linger;
  m_TRIGMode = trigmode;
  m_close_log = close_log;
  m_actormodel = actor_model;
}

// 根据m_TRIGMode，设置 监听socket 和 连接socket 的触发模式
void WebServer::trig_mode(){
  if (0 == m_TRIGMode){ // LT + LT
    m_LISTENTrigmode = 0;
    m_CONNTrigmode = 0;
  }else if (1 == m_TRIGMode){ // LT + ET
    m_LISTENTrigmode = 0;
    m_CONNTrigmode = 1;
  }else if (2 == m_TRIGMode){ // ET + LT
    m_LISTENTrigmode = 1;
    m_CONNTrigmode = 0;
  }else if (3 == m_TRIGMode){ // ET + ET
    m_LISTENTrigmode = 1;
    m_CONNTrigmode = 1;
  }
}

void WebServer::log_write()
{
    if (0 == m_close_log)
    {
        //初始化日志
        if (1 == m_log_write)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

void WebServer::sql_pool(){
  // 初始化数据库连接池
  m_connPool = connection_pool::GetInstance();
  // m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);
  m_connPool->init("127.0.0.1", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);

  // 初始化数据库读取表，取出mysql中的(username, passwd)映射装入内存中的map users
  users->initmysql_result(m_connPool);
}

void WebServer::thread_pool(){ // 初始化线程池
  m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

void WebServer::eventListen(){
  // 创建监听socket
  m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
  assert(m_listenfd >= 0);

  /* 关闭连接的方式 */
  if(0 == m_OPT_LINGER){ // 调用close之后，如果可能将会传输任何未发送的数据
    struct linger tmp = {0, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  }else if(1 == m_OPT_LINGER){ // TCP将丢弃保留在套接口发送缓冲区中的任何数据并发送一个RST给对方
    struct linger tmp = {1, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
  }

  /* 初始化服务器网络地址 */
  int ret = 0;
  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(m_port);

  /* 绑定socketfd和ip地址，并打开监听 */
  int flag = 1;
  setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
  ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
  assert(ret >= 0);
  ret = listen(m_listenfd, 5);
  assert(ret >= 0);

  utils.init(TIMESLOT);

  /* 创建内核事件表 */
  epoll_event events[MAX_EVENT_NUMBER];
  m_epollfd = epoll_create(5);
  assert(m_epollfd != -1);

  // 将监听socket注册进内核事件表
  utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
  http_conn::m_epollfd = m_epollfd;

  /* 创建管道，设置非阻塞，再注册进内核事件表 */
  ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd); 
  assert(ret != -1);
  utils.setnonblocking(m_pipefd[1]);
  utils.addfd(m_epollfd, m_pipefd[0], false, 0);

  /* 添加信号处理函数需要处理的信号 */
  utils.addsig(SIGPIPE, SIG_IGN);
  utils.addsig(SIGALRM, utils.sig_handler, false);
  utils.addsig(SIGTERM, utils.sig_handler, false);

  alarm(TIMESLOT); // TIMESLOT后产生SIGALRM信号

  // 工具类,信号和描述符基础操作
  Utils::u_pipefd = m_pipefd;
  Utils::u_epollfd = m_epollfd;
}

void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
  // 初始化http_conn对象
  users[connfd].init(connfd, client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_passWord, m_databaseName);
  // 初始化client_data数据
  users_timer[connfd].address = client_address;
  users_timer[connfd].sockfd = connfd;
  util_timer *timer = new util_timer;
  timer->user_data = &users_timer[connfd];
  timer->cb_func = cb_func;
  time_t cur = time(NULL);
  timer->expire = cur + 3 * TIMESLOT;
  users_timer[connfd].timer = timer;
  utils.m_timer_lst.add_timer(timer);
}

/* 重置定时器，延迟3个单位，并调整在链表上的位置 */
void WebServer::adjust_timer(util_timer *timer){
  time_t cur = time(NULL);
  timer->expire = cur + 3 * TIMESLOT;
  utils.m_timer_lst.adjust_timer(timer);
  LOG_INFO("%s", "adjust timer once");
}

/* 手动调用timer的回调函数，然后从timer_list中移除timer */
void WebServer::deal_timer(util_timer *timer, int sockfd){
  timer->cb_func(&users_timer[sockfd]);
  if(timer) utils.m_timer_lst.del_timer(timer);
  LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

/* 处理socket事件 */
bool WebServer::dealclinetdata(){
  struct sockaddr_in client_address;
  socklen_t client_addrlength = sizeof(client_address);
  if(0 == m_LISTENTrigmode){ 
    /* 若监听socket是LT模式，一次接收一个 */
    // 建立连接socket
    int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
    if(connfd < 0){
      LOG_ERROR("%s:errno is:%d", "accept error", errno);
      return false;
    }
    if (http_conn::m_user_count >= MAX_FD){
      utils.show_error(connfd, "Internal server busy");
      LOG_ERROR("%s", "Internal server busy");
      return false;
    }
    // 初始化connfd对应的http_conn对象与定时器对象
    timer(connfd, client_address);
  }
  else{
    /* 若监听socket是ET模式，需要一次接收完毕 */
    while(1){
      int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
      if (connfd < 0){
          LOG_ERROR("%s:errno is:%d", "accept error", errno);
          break;
      }
      if (http_conn::m_user_count >= MAX_FD){
          utils.show_error(connfd, "Internal server busy");
          LOG_ERROR("%s", "Internal server busy");
          break;
      }
      timer(connfd, client_address);
    }
    return false;
  }
  return true;
}

/* 主线程从管道接收信号(来自信号处理函数) */
bool WebServer::dealwithsignal(bool &timeout, bool &stop_server){
  int ret = 0;
  int sig;
  char signals[1024];
  ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
  if (ret==-1 || ret==0){
    return false;
  }else{
    for (int i = 0; i < ret; ++i){
      switch (signals[i]){
        case SIGALRM:{
          timeout = true;
          break;
        }case SIGTERM:{
          stop_server = true;
          break;
        }
      }
    }
  }
  return true;
}

/* 若是EPOLLIN时间，读连接socket */
void WebServer::dealwithread(int sockfd){
  util_timer *timer = users_timer[sockfd].timer;
  /* reactor模型 */
  // 由工作线程将报文从内核读到内存buffer
  if(1 == m_actormodel){
    if (timer){ // 重置定时器
      adjust_timer(timer);
    }
    // 若监测到读事件，将该事件放入请求队列，并注明m_state=0，读状态
    m_pool->append(users + sockfd, 0);
    while (true){
      if (1 == users[sockfd].improv){ // 工作线程read_once()之后就将improv置1
        if (1 == users[sockfd].timer_flag){ // 如果read_once出错，则将timer_flag置1
          deal_timer(timer, sockfd); // 关闭该连接socket，并移除其定时器 
          users[sockfd].timer_flag = 0;
        }
        users[sockfd].improv = 0;
        break;
      }
    }
  }
  /* proactor模型 */
  // 主线程将报文从内核读到内存buffer
  else{
    if(users[sockfd].read_once()){ // 内核->内存buffer
      LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
      // 若监测到读事件，将该事件放入请求队列
      m_pool->append_p(users + sockfd);
      if(timer){
        adjust_timer(timer); // 重置事件定时器
      }
    }else{ // 读报文失败，关闭连接socket，删除对应的定时器
      deal_timer(timer, sockfd); 
    }
  }
}

/* 若是EPOLLOUT事件，写连接socket */
void WebServer::dealwithwrite(int sockfd){
  util_timer *timer = users_timer[sockfd].timer;
  /* reactor模型 */
  // 由工作线程来将http请求报文从内存buffer写到内核
  if(1 == m_actormodel){
    if(timer){ // 重置定时器
      adjust_timer(timer);
    }
    // 放入请求队列
    m_pool->append(users + sockfd, 1);
    while(true){
      if(1 == users[sockfd].improv){ // 等工作线程执行了write()，则improv置1
        if(1 == users[sockfd].timer_flag){ // 如果write()出错，则timer_flag置1
          deal_timer(timer, sockfd); // 关闭连接socket，并移除其定时器
          users[sockfd].timer_flag = 0;
        }
        users[sockfd].improv = 0;
        break;
      }
    }
  }
  /* proactor模型 */
  // 由主线程将http响应报文从内存buffer写到内核
  else{
    if (users[sockfd].write()){
      LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
      if (timer){
        adjust_timer(timer);
      }
    }else{
      deal_timer(timer, sockfd);
    }
  }
}

/* 主循环，等待内核事件表中发生新事件 */
void WebServer::eventLoop(){
  bool timeout = false;
  bool stop_server = false;

  while (!stop_server){
    int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
    if(number < 0 && errno != EINTR){
      LOG_ERROR("%s", "epoll failure");
      break;
    }
    for(int i = 0; i < number; i++){
      int sockfd = events[i].data.fd;
      /* 是监听socket，处理新到的TCP客户连接 */
      if(sockfd == m_listenfd){
        bool flag = dealclinetdata();
        if(false == flag) continue;
      }
      /* 处理异常事件，关闭连接，并移除其定时器 */
      else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
        util_timer *timer = users_timer[sockfd].timer;
        deal_timer(timer, sockfd);
      }
      /* 处理依靠管道传送的信号(SIGALRM, SIGTERM) */
      else if((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)){
        bool flag = dealwithsignal(timeout, stop_server);
        if(false == flag) LOG_ERROR("%s", "dealclientdata failure");
      }
      /* 是连接socket，处理http请求报文 */
      else if (events[i].events & EPOLLIN){
        // 处理http请求
        dealwithread(sockfd);
      }
      else if (events[i].events & EPOLLOUT){
        // 发送http响应
        dealwithwrite(sockfd);
      }
    }
    if(timeout){ // 定时处理任务
      // 调用tick()扫描timer_list，并重新定时
      utils.timer_handler();
      LOG_INFO("%s", "timer tick");
      timeout = false;
    }
  }
}