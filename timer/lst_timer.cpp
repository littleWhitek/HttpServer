#include "lst_timer.h"
#include "../http/http_conn.h"

//定时器容器类的构造函数
void sort_timer_lst::sort_timer_lst(){
  head = NULL;
  tail = NULL;
}
//定时器容器类的析构函数
void sort_timer_lst::~sort_timer_lst(){
  util_timer *temp=head;
  while(temp){
    head=temp->next;
    delete temp;
    temp=head;
  }
}

//添加定时器
void sort_timer_lst::add_timer(util_timer *timer){
  if(!timer){
    return;
  }
  if(!head){
    head=tail=timer;
  }
  //定时器链表中是以expire（超过时间）从小到大排序的
  //若当前定时器的expire小于头节点的expire，则直接插入到表头之前
  if(timer->expire<head->expire){
    timer->next=head;
    head->prev=timer;
    head=timer;
    return;
  }
  //若当前定时器的expire不小于表头的expire，则将当前定时器插入到表中
  add_timer(timer,head);
}

//调整定时器，任务发生变化，调整定时器在链表中的位置
void sort_timer_lst::adjust_timer(util_timer *timer){
  if(!timer){
    return;
  }
  util_timer *temp_next=timer->next;
  //若当前定时器是表尾或者当前定时器的expire仍然小于下个节点的expire，则不进行调整
  if(!temp_next||timer->expire<temp_next->expire){
    return;
  }
  //被调整定时器是链表头结点，将定时器取出，重新插入
  if(timer==head){
    head=head->next;
    head->prev=NULL;
    timer->next=NULL;
    add_timer(timer,head);
  }else{ //被调整定时器在内部，将定时器取出，重新插入
    timer->prev->next=timer->next;
    timer->next->prev=timer->prev;
    timer->prev=NULL;
    timer->next=NULL;
    add_timer(timer,head);
  }
}

void sort_timer_lst::del_timer(util_timer *timer){
  if(!timer){
    return;
  }
  //链表中只有一个定时器，需要删除该定时器
  if((timer==head)&&(timer==tail)){
    delete timer;
    head=NULL;
    tail=NULL;
    return;
  }
  //被删除的定时器为头结点
  if(timer==head){
    head=head->next;
    head->prev=NULL;
    delete timer;
    return;
  }
  //被删除的定时器为尾结点
  if(timer==tail){
    tail=tail->prev;
    tail->next=NULL;
    delete timer;
    return;
  }
  //被删除的定时器在链表内部，常规链表结点删除
  timer->prev->next=timer->next;
  timer->next->prev=timer->prev;
  delete timer;
}

//定时任务处理函数
void sort_timer_lst::tick(){
  if(!head){
    return;
  }
  //获取当前时间
  time_t cur=time(NULL);
  util_timer *temp=head;
  //遍历链表
  while(temp){
    //if当前时间小于定时器的超时时间，后面的定时器也没有到期
    if(temp->expire>cur){
      break;
    }
    //当前定时器到期，则调用回调函数，执行定时事件
    temp->cb_func(temp->user_data);
    //将处理后的定时器从链表容器中删除，并重置头结点
    head=temp->next;
    if(head){
      head->prev=NULL;
    }
    delete temp;
    temp=head;
  }
}

//加入新的定时器(私有函数，在类函数内部中调用)
void sort_timer_lst::add_timer(util_timer *timer,util_timer *lst_head){
  util_timer *prev=lst_head;
  util_timer *cur=prev->next;
  /*
    遍历链表，找到对应位置插入，时间复杂度为O(n)，可以考虑用堆实现
  */
  while(cur){
    if(cur->expire>timer->expire){
      pre->next=timer;
      timer->prev=pre;
      timer->next=cur;
      cur->prev=timer;
      break;
    }
    pre=cur;
    cur=cur->next;
  }
  //遍历完发现，目标定时器需要放到尾结点处
  if(!cur){
    pre->next=timer;
    timer->prev=pre;
    timer->next=NULL;
    tail=timer;
  }
}

void Utils::init(int timeslot)
{
  //设置最小时间间隙
  m_TIMESLOT = timeslot;
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd){
  //fcntl可以获取/设置文件描述符性质
  int old_option=fcntl(fd, F_GETFL);
  int new_option=old_option|O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

//将内核事件表注册读事件，ET模式，是否选择开启EPOLLONESHOT
void Utils::addfd(int epollfd,int fd,bool one_shot,int TRIGMode){
  epoll_event event;
  event.data.fd=fd;
  //EPOLLIN表示对应的文件描述符可读，EPOLLRDHUP表示读关闭，对端关闭，EPOLLET将EPOLL设为边缘触发
  if(TRIGMode==1)
    event.events=EPOLLIN | EPOLLET | EPOLLRDHUP;
  else
    event.events = EPOLLIN | EPOLLRDHUP;
  //如果对描述符socket注册了EPOLLONESHOT事件，
  //那么操作系统最多触发其上注册的一个可读、可写或者异常事件，且只触发一次。
  if (one_shot)
        event.events |= EPOLLONESHOT;
  //操作内核事件表监控的文件描述符上的事件：注册、修改、删除，EPOLL_CTL_ADD表示注册新的fd到epfd，event告诉内核监听的事件    
  epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
  setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig){
  //为保证函数的可重入性，保留原来的errno
  int save_errno=errno;
  int msg=sig;
  send(u_pipefd[1],(char *)&msg,1,0);
  errno=save_errno;
}

//设置信号函数
void Utils::addsig(int sig,void(handler)(int),bool restart){
  struct sigaction sa;
  memset(&sa,'\0',sizeof(sa));
  //sa_handler是一个函数指针，指向信号处理函数
  sa.sa_handler = handler;
  if (restart)
    //sa_flags指定信号处理的行为，SA_RESTART，使被信号打断的系统调用自动重新发起
    sa.sa_flags |= SA_RESTART;
  //sa_mask用来指定在信号处理函数执行期间需要被屏蔽的信号
  //sigfillset用于将参数信号初始化，然后把所有信号加入到此信号集中
  sigfillset(&sa.sa_mask);
  //sigaction函数中，sig表示操作的信号，sa表示对信号设置新的处理方式
  assert(sigaction(sig, &sa, NULL) != -1);
}

//定时处理任务，重新定时以不断触发SIGALRM信号
void Utils::timer_handler()
{
    m_timer_lst.tick();
    //最小的时间单位为5s
    alarm(m_TIMESLOT);
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data *user_data){
  //删除非活动连接在socket上的注册事件
  epoll_ctl(Utils::u_epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
  assert(user_data);
  //删除非活动连接在socket上的注册事件
  close(user_data->sockfd);
  //减少连接数
  http_conn::m_user_count--;
}

