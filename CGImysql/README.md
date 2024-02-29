数据库连接池模块：

class connection_pool{ // 单例模式

 	list<MYSQL *> connList; // 连接池

 	MYSQL *GetConnection(); // 获取一个数据库连接

 	bool ReleaseConnection(MYSQL *conn); // 释放一个连接

 	void DestroyPool(); // 销毁所有连接

};

初始化时创建若干个与本地mysql的连接，组成一个列表。

每当http报文解析模块需要访问数据库时，从列表中摘取一个连接，用完后再返还给列表。