#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class WebServer
{
public:
    WebServer();
    ~WebServer();

    void init(int port ,int opt_linger, int trigmode, int thread_num, int actor_model);
    //线程池
    void thread_pool();
    //数据库池
    void trig_mode();
    void eventListen();
    void eventLoop();
    //创建定时器
    void timer(int connfd, struct sockaddr_in client_address);
    //调整定时器链表
    void adjust_timer(util_timer *timer);
    //处理定时器
    void deal_timer(util_timer *timer, int sockfd);
    //处理客户数据
    bool dealclinetdata();
    //处理信号
    bool dealwithsignal(bool& timeout, bool& stop_server);
    //处理读请求
    void dealwithread(int sockfd);
    //处理写请求
    void dealwithwrite(int sockfd);

public:
    //基础信息
    int m_port;//端口
    char *m_root;//根目录
    int m_actormodel;//Reactor/Proactor
    //网络信息
    int m_pipefd[2];//相互连接的套接字
    int m_epollfd;//epoll对象
    http_conn *users;//单个http连接


    //线程池相关
    threadpool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;//监听套接字
    int m_OPT_LINGER;//是否优雅下线
    int m_TRIGMode;//ET/LT
    int m_LISTENTrigmode;//ET/LT

    //定时器相关
    client_data *users_timer;
    //工具类
    Utils utils;
};
#endif
