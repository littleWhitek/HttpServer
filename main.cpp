#include "config.h"

//服务器主程序，调用WebServer类实现Web服务器
int main(int argc, char *argv[])
{

    //命令行解析
    Config config;
    config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(config.PORT, config.OPT_LINGER, config.TRIGMode, config.thread_num, 
                config.actor_model);
    
    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}
