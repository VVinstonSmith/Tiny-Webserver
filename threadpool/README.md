线程池模块：

class threadpool{

​	 list<T*> workqueue; // 请求队列，解除了主线程和工作线程的耦合关系

 	bool append(T* request, int state); // 将任务插入队列

​	 static void* worker(void* arg); // 工作线程函数

};

初始化时通过pthread开辟n个线程，每个线程都监控请求队列。

主线程将收到客户请求插入请求队列，由空闲的线程取来执行。

执行的过程包括解析http请求报文，输出http响应报文，传输相关文件。