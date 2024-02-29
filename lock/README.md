线程同步模块:

class sem; // 信号量类，用于记录空闲的线程和mysql连接

class locker; // 互斥锁类，用于操作队列(线程池，mysql连接池，定时器列表)时加锁。

class cond; // 条件变量

对信号量和锁进行封装