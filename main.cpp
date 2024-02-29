#include "config.h"

int main(int argc, char *argv[])
{
    std::cout<<"start"<<std::endl;
    // 需要修改的数据库信息,登录名,密码,库名
    string user = "TWS";
    string passwd = "1";
    string databasename = "online_shop";

    // 命令行解析
    Config config;
    config.parse_arg(argc, argv);

    // 初始化
    WebServer server;
    server.init(config.PORT, user, passwd, databasename, config.LOGWrite, 
                config.OPT_LINGER, config.TRIGMode,  config.sql_num,  config.thread_num, 
                config.close_log, config.actor_model);
    std::cout<<"init"<<std::endl;

    // 日志
    server.log_write();

    // 数据库
    server.sql_pool();
    std::cout<<"create SQL pool"<<std::endl;
    
    // 线程池
    server.thread_pool();
    std::cout<<"create thread pool"<<std::endl;

    // 触发模式
    server.trig_mode();

    // 监听
    server.eventListen();

    std::cout<<"run:"<<std::endl;
    // 运行
    server.eventLoop();

    return 0;
}
