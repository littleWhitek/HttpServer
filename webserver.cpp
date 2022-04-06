#include "webserver.h"

//主要完成服务器初始化：http连接、设置根目录、开启定时器对象
WebServer::WebServer()
{
    //http_conn最大连接数
    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    //获取当前工作目录
    getcwd(server_path, 200);
    char root[9] = "/wwwroot";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //定时器
    users_timer = new client_data[MAX_FD];
}

//服务器资源释放
WebServer::~WebServer()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

//初始化用户名、数据库等信息
void WebServer::init(int port, string user, string passWord, string databaseName, 
                     int sql_num, int thread_num)
{
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
}



//初始化数据库连接池
void WebServer::sql_pool()
{
    //单例模式获取唯一实例
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost", m_user, m_passWord, m_databaseName, 3306, m_sql_num);

    //初始化数据库读取表
    users->initmysql_result(m_connPool);
}

//创建线程池
void WebServer::thread_pool()
{
    //线程池
    m_pool = new threadpool<http_conn>(m_connPool, m_thread_num);
}

//创建网络编程：Socket网络编程基础步骤
void WebServer::eventListen()
{
    //SOCK_STREAM 表示使用面向字节流的TCP协议
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    //优雅关闭连接
    //就是在closesocket的时候立刻返回，底层会将未发送完的数据发送完成后再释放资源
    struct linger tmp = {0, 1};
    setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));


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
    utils.addfd(m_epollfd, m_listenfd, false);
    http_conn::m_epollfd = m_epollfd;

    //socketpair()函数用于创建一对无名的、相互连接的套接子。
    //详情：https://blog.csdn.net/weixin_40039738/article/details/81095013
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    //当套接字发送缓冲区变满时，send通常会堵塞，
    //除非套接字设置为非堵塞模式，当缓冲区变满时，返回EAGAIN或者EWOULDBLOCK错误，此时可以调用select函数来监视何时可以发送数据。
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], fals);

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

//创建一个定时器节点，将连接信息挂载
void WebServer::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_close_log, m_user, m_passWord, m_databaseName);

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    util_timer *timer = new util_timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    //TIMESLOT:最小时间间隔单位为5s
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    utils.m_timer_lst.add_timer(timer);
}

//若数据活跃，则将定时器节点往后延迟3个时间单位
//并对新的定时器在链表上的位置进行调整
void WebServer::adjust_timer(util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

//删除定时器节点，关闭连接
void WebServer::deal_timer(util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        utils.m_timer_lst.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

//http 处理用户数据
bool WebServer::dealclinetdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
    if (connfd < 0)
    {
        LOG_ERROR("%s:errno is:%d", "accept error", errno);
        return false;
    }
    if (http_conn::m_user_count >= MAX_FD)
    {
        utils.show_error(connfd, "Internal server busy");
        LOG_ERROR("%s", "Internal server busy");
        return false;
    }
    timer(connfd, client_address);
    return true;
}

//处理定时器信号,set the timeout ture
bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    //从管道读端读出信号值，成功返回字节数，失败返回-1
    //正常情况下，这里的ret返回值总是1，只有14和15两个ASCII码对应的字符
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        // handle the error
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        //处理信号值对应的逻辑
        for (int i = 0; i < ret; ++i)
        {
            
            //这里面明明是字符
            switch (signals[i])
            {
                //这里是整型
                case SIGALRM:
                {
                    timeout = true;
                    break;
                }
                //关闭服务器
                case SIGTERM:
                {
                    stop_server = true;
                    break;
                }
            }
        }
    }
    return true;
}

//处理客户连接上接收到的数据
void WebServer::dealwithread(int sockfd)
{
    //创建定时器临时变量，将该连接对应的定时器取出来
    util_timer *timer = users_timer[sockfd].timer;

    //reactor
    if (timer)
    {
        //将定时器往后延迟3个单位
        adjust_timer(timer);
    }

    //若监测到读事件，将该事件放入请求队列
    m_pool->append(users + sockfd, 0);
    while (true)
    {
        //是否正在处理中
        if (1 == users[sockfd].improv)
        {
            //事件类型关闭连接
            if (1 == users[sockfd].timer_flag)
            {
                deal_timer(timer, sockfd);
                users[sockfd].timer_flag = 0;
            }
            users[sockfd].improv = 0;
            break;
        }
    }
}

//写操作
void WebServer::dealwithwrite(int sockfd)
{
    util_timer *timer = users_timer[sockfd].timer;
    //reactor
    if (timer)
    {
        adjust_timer(timer);
    }

    m_pool->append(users + sockfd, 1);

    while (true)
    {
        if (1 == users[sockfd].improv)
        {
            if (1 == users[sockfd].timer_flag)
            {
                deal_timer(timer, sockfd);
                users[sockfd].timer_flag = 0;
            }
            users[sockfd].improv = 0;
            break;
        }
    }
}

//事件回环（即服务器主线程）
void WebServer::eventLoop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        //等待所监控文件描述符上有事件的产生，返回就绪的文件描述符个数，events用来存内核得到事件的集合
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        //EINTR错误的产生：当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时，该系统调用可能返回一个EINTR错误。
        //例如：在socket服务器端，设置了信号捕获机制，有子进程，
        //当在父进程阻塞于慢系统调用时由父进程捕获到了一个有效信号时，内核会致使accept返回一个EINTR错误(被中断的系统调用)。
        //在epoll_wait时，因为设置了alarm定时触发警告，导致每次返回-1，errno为EINTR，对于这种错误返回
        //忽略这种错误，让epoll报错误号为4时，再次做一次epoll_wait
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }
        //对所有就绪事件进行处理
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            //处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = dealclinetdata();
                if (false == flag)
                    continue;
            }
            //处理异常事件，EPOLLRDHUP表示读关闭，EPOLLHUP表示被挂断，EPOLLERR表示对应的文件描述符发生错误
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //处理定时器信号，表示对应的文件描述符可以读
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                //接收到SIGALRM信号，timeout设置为True
                bool flag = dealwithsignal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            //处理客户连接上send的数据
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }

        //处理定时器为非必须事件，收到信号并不是立马处理
        //完成读写事件后，再进行处理
        if (timeout)
        {
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    }
}