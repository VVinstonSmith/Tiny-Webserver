#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <set>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn{
public:
  static const int FILENAME_LEN = 200;
  static const int READ_BUFFER_SIZE = 2048;
  static const int WRITE_BUFFER_SIZE = 1024;
  /* http报文请求方法 */
  enum METHOD{GET = 0, POST, HEAD, PUT, DELETE};
  /* 主状态机状态 */
  enum CHECK_STATE{
    CHECK_STATE_REQUESTLINE = 0,
    CHECK_STATE_HEADER,
    CHECK_STATE_CONTENT
  };
  /* 报文解析结果 */
  enum HTTP_CODE{
    NO_REQUEST, // 请求不完整
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR,
    CLOSED_CONNECTION
  };
  /* 从状态机状态 */
  enum LINE_STATUS{
    LINE_OK = 0,
    LINE_BAD,
    LINE_OPEN
  };

public:
  http_conn() {}
  ~http_conn() {}

public:
  /* 初始化socket地址，函数内部会调用私有方法init() */
  void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
  void close_conn(bool real_close = true);
  /* process_read() and process_write() */
  void process();
  /* 读取浏览器发来的全部数据 */
  bool read_once();
  /* 响应报文写入函数 */
  bool write();
  sockaddr_in *get_address(){
    return &m_address;
  }
  /* 将sql中已有的username与passwd映射全部取出，放入map */
  void initmysql_result(connection_pool *connPool);
  int timer_flag;
  int improv;

private:
  void init();
  /* 从m_read_buf中读取并处理请求报文 */
  HTTP_CODE process_read();
  /* 向m_write_buf中写入响应报文 */
  bool process_write(HTTP_CODE ret);
  /* 主状态机解析报文中的 请求行、头部、消息内容 */
  HTTP_CODE parse_request_line(char *text);
  HTTP_CODE parse_headers(char *text);
  HTTP_CODE parse_content(char *text);
  /* 生成响应报文 */
  HTTP_CODE do_request();
  /* 指向还未处理的字符 */
  char *get_line() { return m_read_buf + m_start_line; };
  LINE_STATUS parse_line(); // 从状态机
  void unmap(); // 解除映射

  /* 根据响应报文格式，生成8个对应部分，均由do_request()调用 */
  bool add_response(const char *format, ...);
  bool add_content(const char *content);
  bool add_status_line(int status, const char *title);
  bool add_headers(int content_length);
  bool add_content_type();
  bool add_content_length(int content_length);
  bool add_linger();
  bool add_blank_line();

public:
  static int m_epollfd;
  static int m_user_count;
  static map<string, string> users;
  static map<string, string> user_role;
  static map<unsigned long, string> ip_user;

  MYSQL *mysql; // 本http连接对应的mysql连接
  int m_state; // 读为0, 写为1

private:
  int m_sockfd;
  sockaddr_in m_address;
  unsigned long m_ipaddr;
  string m_username;
  string m_role;

  char m_read_buf[READ_BUFFER_SIZE];
  int m_read_idx; // 缓冲区最后一个byte的下一个
  int m_checked_idx; // 解析到的位置
  int m_start_line; // 已经解析的字符个数

  char m_write_buf[WRITE_BUFFER_SIZE];
  int m_write_idx;

  CHECK_STATE m_check_state; // 主状态机状态
  METHOD m_method; // http请求方式

  /* 解析请求报文中对应的6个变量 */
  char m_real_file[FILENAME_LEN];
  char *m_url, *m_version, *m_host;
  int m_content_length;
  bool m_linger;

  char *m_file_address; // 读取服务器上的文件地址
  struct stat m_file_stat; // 文件属性(类型，大小)
  struct iovec m_iv[2]; // 向量元素(地址，长度)
  int m_iv_count;

  int cgi; // 是否启用的POST
  char *m_string; // 存储POST数据
  int bytes_to_send; // 剩余发送
  int bytes_have_send; // 已发送
  char *doc_root;

  int m_TRIGMode;
  int m_close_log;

  char sql_user[100];
  char sql_passwd[100];
  char sql_name[100];
};

#endif
