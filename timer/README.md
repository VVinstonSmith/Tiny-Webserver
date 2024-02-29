定时器模块：

class util_timer{ // 定时器类

 	time_t expire; // 截止时间

 	void (* cb_func)(client_data *); // 定时器到期后要执行的函数

 	client_data *user_data; // 定时器所指向的连接socket

}；

class sort_time_list{ // 定时器列表 

​	 add_timer( ); // 添加

​	 adjust_timer( ); // 重新插入

 	del_timer( ); // 删除

​	 tick(); // 处理列表中到期的定时

};

对每个连接socket进行定时，若一段时间没有数据发送，则将其断开。

每个连接socket对应于一个定时器对象，顺序添加进定时器列表里。

利用alarm函数周期性地触发SIGALRM信号,该信号的信号处理函数利用管道通知主循环执行定时器链表上的定时任务tick()

执行tick()将摘除每个到期的表项(非活动连接)，并执行cb_func(断开socket，解放线程)