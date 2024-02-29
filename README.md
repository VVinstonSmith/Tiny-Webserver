# 一个web服务器端程序



## 使用方法

1.安装mysql5.7

2.make server对项目完成编译

3.运行create_table.sql初始化数据库

4.打开数据库服务：service mysqld start

5.运行webserver程序：./server



## 命令行选项

-p，自定义端口号(默认9006)

-l，选择日志写入方式(默认同步写入)

 	0，同步写入
 	
 	1，异步写入

-m，listenfd和connfd的模式组合(默认使用LT + LT)

 	0，表示使用LT + LT
 	
 	1，表示使用LT + ET
 	
 	2，表示使用ET + LT
 	
 	3，表示使用ET + ET

-s，数据库连接数量(默认为8)

-t，线程数量(默认为8)

-c，关闭日志(默认打开)

 	0，打开日志
 	
 	1，关闭日志

-a，选择反应堆模型(默认Proactor)

 	0，Proactor模型
 	
 	1，Reactor模型

**==============================================================================**

## 程序概述

 	基于Linux的网络库接收浏览器端发来的 网络连接 和 报文请求，
 	
 	对每个http报文进行处理，
 	
 	根据处理结果 写应答报文 和 定位请求页面(文件)，再发送给浏览器端。



## 程序流程

程序采用多线程的实现方式，

包括一个主线程和若干个工作线程，

主线程负责监听网络请求，包括：

 	监听socket：用于与客户机建立TCP连接
 	
 	连接socket：用于接收客户机发来的http请求报文，将其插入请求队列

主线程也同时负责接收信号和维护定时器队列。

工作线程负责从请求队列中获取http报文请求，并进行处理，包括：

 	解析http报文内容
 	
 	根据url定位请求的页面与文件
 	
 	写http响应报文

在reactor模式下，http请求报文由工作线程读取，

在proactor模式下，http报文由主线程读取。



## 主线程

(用于创建网络连接和监听网络连接)

1.创建 信号处理函数sig_handler() 和 管道pipefd，信号处理函数通过管道将注册的信号(定时器信号，终止信号)发给主进程。

2.创建监听socket，用于监听由客户机发起的网络连接。

3.创建内核事件表epollfd，将 监听socket 和 管道pipefd 加入到内核事件表。

4.创建任务请求队列和线程池，每个http报文处理请求会被插入到请求队列中，由线程池中空闲的工作线程进行处理。

5.主循环中通过epoll_wait等待内核事件表中的新事件发生，判断事件类型：

 	(a)监听socket：
 	
 		接受客户连接，初始化该客户的连接socket，并注册进内核事件表。
 	
 		给该连接socket创建定时器，加入到定时器列表。
 	
 	(b)管道信号:
 	
 		if(定时器信号)：移除定时器列表中到期的定时器，并释放其对应的socket连接
 	
 		else if(终止信号)：程序终止，销毁所有开辟的资源，退出。
 	
 	(c)连接socket：
 	
 		if(接收报文信号)：
 	
 			读取http报文信息到内存，
 	
 			加入请求队列，等待工作线程来解析http报文，
 	
 			重置该连接socket的定时器
 	
 		else if(发送报文信号):
 	
 			将要发送的报文由内存缓冲区发送到内核缓冲区，
 	
 			重置该连接socket的定时器



## 工作线程

(用于对http请求报文进行解析，并对请求内容作出响应)

1.从请求队列中摘取待处理的http报文请求

2.处理http报文请求：

	(1)通过从状态机分离并读取http报文党的每一行
	
	(2)通过主状态机分别对报文请求行、报文头部、报文内容进行处理
	
	(3)根据url定位浏览器请求的html页面或文件
	
	(4)根据请求报文的解析状态向写缓冲区中写入http响应报文
	
	(5)将 页面/文件 和 响应报文 写入内核缓冲区，向内核事件表中注册发送事件，发送二者到浏览器端

PS:

​	第(2)步需要从数据库连接池中获取空闲的mysql连接，

​	用于与数据库进行数据交互，例如:

​	登录操作：在数据库中查询(username, passwd)

​	注册操作：向数据库中插入(username, passwd)



**=======================================================================================**

## 程序分为如下几部分

线程池模块，http报文处理模块，数据库连接池模块，定时器模块，线程同步模块



**线程池模块：**

class threadpool{

 	list<T*> workqueue; // 请求队列
 	
 	bool append(T* request, int state); // 将任务插入队列
 	
 	static void* worker(void* arg); // 工作线程函数

};

初始化时通过pthread开辟n个线程，每个线程都监控请求队列。

主线程将收到客户请求插入请求队列，由空闲的线程取来执行。

执行的过程包括解析http请求报文，输出http响应报文，传输相关文件。



**http报文处理模块：**

class http_conn{

 	bool read_once(); // 将内核缓冲区接收的http报文读入内存缓冲区read_buffer
 	
 	parse_line(); // 从状态机，解析出http报文中的每一行数据
 	
 	process_read(); // 主状态机，在从状态机的基础上，对http报文每一部分(请求行,报文头,报文内容)进行解析
 	
 	do_request(); // 根据http报文的解析结果进行响应，定位相应的html文件
 	
 	bool write()；// 将响应报文和浏览器请求的文件发回浏览器端

};

将内核缓冲区内的http报文读到内存缓冲区，

通过有限状态机对报文进行解析，从状态机将每一行分离开，主状态机依次解析每行的内容。

根据http报文的解析结果：1.从数据库中查找和对比数据。2，定位相应的页面和文件路径，

发送http响应报文，附加上html页面和文件到浏览器端。



**数据库连接池模块：**

class connection_pool{

 	list<MYSQL *> connList; // 连接池
 	
 	MYSQL *GetConnection(); // 获取一个数据库连接
 	
 	bool ReleaseConnection(MYSQL *conn); // 释放一个连接
 	
 	void DestroyPool(); // 销毁所有连接

};

初始化时创建若干个与本地mysql的连接，组成一个列表。

每当http报文解析模块需要访问数据库时，从列表中摘取一个连接，用完后再返还给列表。



**定时器模块：**

class util_timer{ // 定时器类

 	time_t expire; // 截止时间
 	
 	void (* cb_func)(client_data *); // 定时器到期后要执行的函数
 	
 	client_data *user_data; // 定时器所指向的连接socket

}；

class sort_time_list{ // 定时器列表 

 	add_timer( ); // 添加
 	
 	adjust_timer( ); // 重新插入
 	
 	del_timer( ); // 删除
 	
 	tick(); // 处理列表中到期的定时

};

对每个连接socket进行定时，若一段时间没有数据发送，则将其断开。

每个连接socket对应于一个定时器对象，顺序添加进定时器列表里

每隔一段时间执行一次tick(),摘除每个到期的表项，并执行cb_func(断开socket，解放线程)



**线程同步模块:**

class sem; // 信号量类，用于记录空闲的线程和mysql连接

class locker; // 互斥锁类，用于操作队列(线程池，mysql连接池，定时器列表)时加锁。

对信号量和锁进行封装.

**=======================================================================================**
## 页面动作：
**买家buyer**

商品查询页面: /login-search_item.html
动作：
  查询：/login-search    POST: name=x&lowest_price=x&highest_price=x
  返回:  /login-function-buyer.html

商品下单页面：/login-itemList-buyer.html
动作：
  下单：/login-order&itemID=x   POST: num=x
  返回：/login-search_item.html

订单查询页面：/login-dealList-buyer.html
动作：
  #删除订单：/login-cancelDeal  POST:dealID=1&dealID=2
  返回：/login-function-buyer.html


============================================
**卖家seller**

商品创建页面: /login-create_item.html
动作：
  创建：/login-create    POST: name=x&price=x
  返回：/login-function-seller.html

待售商品页面：/login-itemList-seller.html
动作：
  #下架商品：/login-unshelve    POST:itemID=1&itemID=2
  返回：/login-function-seller.html

订单查询页面：/login-dealList-seller.html
动作：
  #删除订单: /login-cancelDeal    POST:dealID=1&dealID=2
  返回：/login-function-seller.html