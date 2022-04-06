#include "webserver.h"

//主要完成服务器初始化：http连接、设置根目录、开启定时器对象
WebServer::WebServer(){
  //http_conn最大连接数
  users=new http_conn[MAX_FD];
  
  //root文件夹路径
  char server_path[200];
  getcwd(server_path,200);
  char root[9]="/wwwroot";
  m_root=(char *)malloc(strlen(server_path)+strlen(root)+1);
  strcpy(m_root,server_path);
  strcat(m_root,root);
  
  //定时器
  user_timer=new client_data[MAX_FD];
}

//服务器资源释放
WebServer::~WebServer(){
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

//初始化用户名、数据库等信息
void WebServer::init(int port, int opt_linger, int trigmode, int thread_num, int actor_model){
    m_port = port;
    m_thread_num = thread_num;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_actormodel = actor_model;
}
//设置epoll的触发模式：ET、LT
void WebServer::trig_mode(){
    //LT + LT
    if (0 == m_TRIGMode){
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
    }
    //LT + ET
    else if (1 == m_TRIGMode){
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    //ET + LT
    else if (2 == m_TRIGMode){
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
    }
    //ET + ET
    else if (3 == m_TRIGMode){
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}
//初始化数据库连接池
void WebServer::sql_pool(){
    //单例模式获取唯一实例
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num, m_close_log);
    //初始化数据库读取表
    users->initmysql_result(m_connPool);
}
//创建线程池
void WebServer::thread_pool(){
    //线程池
    m_pool = new threadpool<http_conn>(m_actormodel, m_connPool, m_thread_num);
}

//创建网络编程
void WebServer::eventListen()
{
    //SOCK_STREAM 表示使用面向字节流的TCP协议
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    //优雅关闭连接
    //就是在closesocket的时候立刻返回，底层会将未发送完的数据发送完成后再释放资源
    if (0 == m_OPT_LINGER)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    //在调用closesocket的时候不会立刻返回，内核会延迟一段时间，这个时间就由l_linger得值来决定。
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    //SO_REUSERADDR表示可以重用本地地址
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    //表示已连接和未连接的最大队列数总和为5
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    //设置服务器的最小时间间隙
    utils.init(TIMESLOT);

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    //添加socket到内核监听表中
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    //socketpair()函数用于创建一对无名的、相互连接的套接子。
    //详情：https://blog.csdn.net/weixin_40039738/article/details/81095013
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    //当套接字发送缓冲区变满时，send通常会堵塞，
    //除非套接字设置为非堵塞模式，当缓冲区变满时，返回EAGAIN或者EWOULDBLOCK错误，此时可以调用select函数来监视何时可以发送数据。
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    utils.addsig(SIGPIPE, SIG_IGN);
    //SIGALRM:由alarm系统调用产生timer时钟信号
    utils.addsig(SIGALRM, utils.sig_handler, false);
    //SIGTERM:终端发送的终止信号
    utils.addsig(SIGTERM, utils.sig_handler, false);
    //来设置信号SIGALRM在经过参数seconds秒数后发送给目前的进程。如果未设置信号SIGALRM的处理函数，那么alarm()默认处理终止进程.
    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}


