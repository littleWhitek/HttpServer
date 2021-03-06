# HTTP
一款基于Linux的轻量级HTTP服务器，目前更新至第二版
# 第一版
## 功能
- 收到TCP/IP协议栈发送过来的数据，并对这些数据进行解析，得到有用的信息，然后对请求做出相应的相应。
- 模拟实现了HTTP协议的一些功能，比如GET、POST方法。

## 模块化介绍
共有Socket模块、处理请求模块、返回响应模块、CGI程序模块、线程池模块
### Socket模块
该模块为网络之间的通信做准备
1. 服务器首先调用socket()，创建一个socket，得到一个socket的文件描述符sockfd。
2. 再调用bind()，将sockfd和addr(ip+port)绑定到一起，使sockfd这个用于网络通讯的文件描述符监听addr所描述的地址和端口。
3. 调用listen()，声明sockfd处于监听状态，并设置同时连接最大数值。
4. 调用accept()，等待客户端连接，第三次握手后，accept()得到返回值，返回用于传输的socket的文件描述符（新的socket）。
### 处理请求模块
这个模块在建立连接后，且客户端请求服务器时，负责解析这个请求，若请求方法合法并且访问的资源存在且权限合理，则响应请求，若请求方法不合法或者资源不存在，则发送错误信息的报文。
1. 首先拿到请求起始行，解析得到**请求方法**、**URL**、**HTTP版本信息**，对于HTTP版本信息，这里不进行处理
2. 若服务器不支持请求的方法，就返回对应的错误码400，告知客户端发送了一个错误的请求，并且应该将缓冲区中未读完的数据读完，但是不处理这些请求，防止下次读取时出现错误。
3. 对于请求的URL，需要判断服务器中有没有这个资源，或者请求的URL是一个CGI的话是否具有执行权限。这些判断使用一个Linux中的**stat函数**。
4. 若是访问的资源没有的话，依旧是先把缓冲区中的所有数据全部读取完，再构建一张404的页面，告知用户，服务器上没有找到所请求的资源。
5. 若以上全部成功，则读取请求的首部，用vector<string>类型进行存储，解析成KV类型时用unordered_map<string,string>类型进行存储。
6. 对于请求报文的请求体，需要判断方法是GET还是POST，若是POST，则根据首部中的Content-Length字段读取请求体。
7. 解析完请求后，需要判断请求是否是CGI程序，若不是，则直接进入响应模块，若是则进入CGI模块，之后在交给响应模块。
### 返回响应模块
当解析完请求后，不论请求的结果如何，都应该给客户端发送一个响应。
1. 首先构建响应起始行：【版本、状态码、原因短语】。版本号我们是固定的1.0 版本。根据不同的情况我们填上不同的状态码，根据不同的状态码填写对应的原因短语。
2. 构建响应首部，填写不同的字段和字段对应的值。这块需要注意的是，不同文件的文件扩展名对应不同的Content-Type 值，因此我们需要根据URL 中的文件后缀来对应我们的Content-Type 值。
3. 对于正文的话，通常客户端请求我的资源大多是一张网页，而我的网页在我的服务器里面就是一个文件。因此发送给客户端一个文件的时候，我们不用将文件打开，在将文件内容读取出来，最后发送给对方。我们直接调用 linux 中一个函数 sendfile()，就可以实现直接将文件发送出去。
4. 当构建好模块后，就可以发送响应报文了。
### CGI程序模块
这里实现了原生的CGI，没有实现快速CGI，原生CGI的缺点在于每条CGI请求都需要创建一个新进程，开销是很大的，而快速CGI作为持久守护进程运行，消除了为每个请求建立或拆除新进程所带来的性能损耗。
- 对于我们的Web服务器需要新启一个进程去替换CGI程序。
- 进程的替换使用**exec函数**，使用**双管道**进行子进程和父进程的通信
- 由于进程之间数据独有，所以管道还在，但管道的文件描述符却不见了，针对这种情况，将将管道的文件描述符重定向到标准输入和标准输出。让子进程从标准输入中去读，去标准输出中去写。
### 线程池模块
- 预先创建一堆线程，并且设置线程数量的上限，当一大堆请求来的时候，我会将这些请求放到线程池的任务队列中，让线程池中的活跃线程去处理任务就好。虽然谈不上性能有多高，但是最起码是稳定的。
- 将线程池设置为单例模式了。单例对象的类必须保证只有一个实例存在。许多时候整个系统只需要拥有一个的全局对象，这样有利于我们协调系统整体的行为。

# 第二版
## 功能
- 使用 **线程池** + **非堵塞**socket + epoll + **Reactor**的并发处理模型
- 使用状态机解析HTTP请求报文，支持解析GET和POST请求
- 为每个连接创建一个定时器，若长时间不交换数据，会断开此连接，释放连接资源
## epoll实现I/O多路复用，实现Reactor并发处理模型
创建一个指示epoll内核事件的文件描述符，当使用listen()将socket声明为监听状态后，将监听的socket注册到内核事件监控表中，调用epoll_wait，等待所监控文件描述符上有事件发生。有事件发生后，再调用accept进行连接，实现了非堵塞的socket。  
    
该代码实现了Reactor并发处理模型，主线程只负责监听文件描述符上是否有事件发生，若有，则立即通知工作线程，将socket可读可写事件放入请求队列中，读写数据、接受新连接及处理客户请求均在工作线程中完成。
## 定时器功能
服务器需要一种手段监测无意义的连接，并对这些连接进行处理，为实现这些功能，服务器就需要为各事件分配一个定时器，每个定时事件都处于一个升序双向链表上，**通过alarm()函数周期性触发SIGALARM信号**，而后信号函数利用管道通知主循环，主循环接受到信号之后对升序链表上的定时器进行处理：若一定时间内无数据交换则关闭连接。
### 信号通知流程
Linux下的信号采用的是异步处理机制，信号处理函数和当前进程是两条不同的执行路线。具体的，当进程收到信号时，操作系统会中断进程当前的正常流程，转而进入信号处理函数执行操作，完成后再返回中断的地方继续执行。
### 信号通知逻辑
- 创建管道，其中管道写端写入信号值，管道读端通过I/O复用系统监测事件
- 设置信号处理函数SIGALRM（时间到了就触发）和SIGTERM（kill会触发，Ctrl+C）
  - 通过struct sigaction结构体和sigaction函数注册信号捕捉函数
  - 在结构体的handler参数设置信号处理函数，具体的，从管道写端写入信号的名字
- 利用I/O复用系统监听管道读端文件描述符的可读事件
- 信息值传递给主循环，主循环再根据接收到的信号值执行目标信号对应的逻辑代码
![img_timer](https://github.com/littleWhitek/HttpServer/blob/master/img/image.png)
  
### 线程池功能
预先创建一定数量的线程，当线程创建时会执行所绑定的函数，函数内部是一个死循环，开头会预先执行sem_wait()操作，使得每个线程都堵塞，进行睡眠，直到请求队列中有连接进入，每当有连接进入后会执行sem_post()操作，使得信号量加1，会唤醒其中一个被堵塞的线程，该线程执行相应的I/O操作，执行完后不会释放资源，而是再一次进入循环，循环的开头还是会执行sem_wait()操作，进行堵塞，直到有新的连接请求进入队列。
  
