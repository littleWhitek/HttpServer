#ifndef __THREADPOLL_HPP__
#define __THREADPOLL_HPP__ 

#include<iostream>
#include<queue>
#include<pthread.h>

typedef void (*handler_t)(int);

class Task{
    private:
        int sock;
        handler_t handler;
    public:
        Task(int sock_,handler_t handler_) 
            : sock(sock_)
            , handler(handler_)
        {}
        void Run()
        {
            handler(sock);
        }
        ~Task()
        {}

};

class ThreadPool{
    private:
        int num;
        int idle_num;
        std::queue<Task> task_queue;
        pthread_mutex_t lock;       //互斥锁
        pthread_cond_t cond;        //条件变量
    public:
        ThreadPool(int num_)
            : num(num_)
            , idle_num(0)
        {
            pthread_mutex_init(&lock,NULL);     //互斥锁的初始化
            pthread_cond_init(&cond,NULL);      //条件变量的初始化
        }
        void InitThreadPool()
        {
            pthread_t tid;
            for(auto i = 0;i < num; ++i)
            {
                //pthread_create创建线程，开始运行相关线程函数，运行结束则线程退出
                pthread_create(&tid,NULL,ThreadRoutine,(void*)this);
            }
        }
        bool IsTaskQueueEmpty()
        {
            return task_queue.size() == 0 ? true : false;
        }
        void LockQueue()
        {
            pthread_mutex_lock(&lock);  //锁定互斥锁，如果尝试锁定已经被上锁的互斥锁则阻塞至可用为止
        }
        void UnlockQueue()
        {
            pthread_mutex_unlock(&lock);  //释放互斥锁
        }
        void Idle()
        {
            idle_num++;
            //线程堵塞在条件变量
            pthread_cond_wait(&cond,&lock); // 让一个线程去等
            idle_num--;
        }
        Task PopTask()
        {
            Task t = task_queue.front();
            task_queue.pop();
            return t;
        }
        void Wakeup()
        {
            pthread_cond_signal(&cond);
        }
        void PushTask(Task &t)
        {
            LockQueue();
            task_queue.push(t);
            UnlockQueue();
            Wakeup();
        }
        static void *ThreadRoutine(void* arg)  // 生产者消费者模型
        {
            //pthread_self()得到实际上是线程描述符pthread指针地址
            //pthread_detach()若线程调用了该函数，主进程不会被阻塞等待进程结束后才返回，而是启动了线程后去继续自己的工作
            //每个线程创建以后都应该调用pthread_join或pthread_detach函数，只有这样在线程结束时，资源才能被释放。
            pthread_detach(pthread_self());
            ThreadPool* tp = (ThreadPool*)arg;

            for(;;)
            {
                tp->LockQueue();
                while(tp->IsTaskQueueEmpty()) // 当这个线程醒来的时候在检测一次，保证百分之百的一定有任务来了
                {
                    tp->Idle(); // 假设出现了假唤醒,也就是说没有任务的时候把这个线程唤醒了
                }
                Task t = tp->PopTask();
                tp->UnlockQueue();
                std::cout << "task is handler by: " << pthread_self() << std::endl;
                t.Run();
            }
        }
        ~ThreadPool()
        {
            pthread_mutex_destroy(&lock);
            pthread_cond_destroy(&cond);
        }

};

class singleton{
    private:
        static ThreadPool* p;
        static pthread_mutex_t lock;
        singleton();
        singleton(const singleton&);

    public:
        static ThreadPool* GetInstance()
        {
            if(NULL == p) // 加锁也是比较耗费时间的,因此我们二次判断
            {  
                pthread_mutex_lock(&lock);
                if(NULL == p)
                {
                    p = new ThreadPool(5);
                    p->InitThreadPool();
                }
                pthread_mutex_unlock(&lock);
            }
            return p;
        }
};

ThreadPool *singleton::p = NULL;
pthread_mutex_t singleton::lock = PTHREAD_MUTEX_INITIALIZER;

#endif 

