#ifndef LST_TIMER
#define LST_TIMER

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

#include <time.h>
#include "../log/log.h"

class util_timer;

/* 每个定时器所关联的sockaddr和sockfd */
struct client_data{
  sockaddr_in address;
  int sockfd;
  util_timer *timer;
};

/* 定时器类 */
class util_timer
{
public:
  util_timer() : prev(NULL), next(NULL) {}

public:
  time_t expire; // 截止时间
  
  void (* cb_func)(client_data *); // 定时器到期后要执行的函数
  client_data *user_data;
  util_timer *prev;
  util_timer *next;
};

/* 定时器列表 */
class sort_timer_lst{
public:
  sort_timer_lst();
  ~sort_timer_lst();

  void add_timer(util_timer *timer); // 添加
  void adjust_timer(util_timer *timer); // 重新插入
  void del_timer(util_timer *timer); // 删除
  void tick(); // 处理列表中到期的定时器

private:
  void add_timer(util_timer *timer, util_timer *lst_head);

  util_timer *head;
  util_timer *tail;
};

/* 辅助函数类 */
class Utils{
public:
  Utils() {}
  ~Utils() {}

  void init(int timeslot);

  // 对文件描述符设置非阻塞
  int setnonblocking(int fd);

  // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
  void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

  // 信号处理函数
  static void sig_handler(int sig);

  // 设置信号函数
  void addsig(int sig, void(handler)(int), bool restart = true);

  // 定时处理任务，重新定时以不断触发SIGALRM信号
  void timer_handler();

  void show_error(int connfd, const char *info);

public:
  static int *u_pipefd; // 管道，用于从 信号处理函数 向 主线程 写信号
  sort_timer_lst m_timer_lst; // 定时器链表
  static int u_epollfd; // epollfd
  int m_TIMESLOT; // 最小超时单位
};

void cb_func(client_data *user_data); // 一旦超时执行该函数

#endif
