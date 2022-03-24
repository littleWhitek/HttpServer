#ifndef __HTTPSERVER_HTTP
#define __HTTPSERVER_HTTP

#include<iostream>
#include<pthread.h>
#include<signal.h>
#include"ProtocolUtil.hpp"
#include"ThreadPool.hpp"

class HttpServer{
	private:
		int listen_sock;
		int port;
	public:
		HttpServer(int port_)
			: port(port_)
			, listen_sock(-1)
		{}
		void InitServer()
		{
			listen_sock = SocketApi::Socket();
			SocketApi::Bind(listen_sock,port);
			SocketApi::Listen(listen_sock);
		}
		void Start()
		{
            signal(SIGPIPE,SIG_IGN);
			while(1)
			{
				std::string peer_ip;
				int peer_port;
				int sock = SocketApi::Accept(listen_sock,peer_ip,peer_port);	
				if(sock>= 0)
				{
					std::cout << peer_ip << " : " << peer_port << std::endl; 	
                    Task t(sock,Entry::HandlerRequest);
                    singleton::GetInstance()->PushTask(t);  
					//pthread_t tid;
					//pthread_create(&tid,NULL,Entry::HandlerRequest,(void*)sockp);
				}
			}
		}
		~HttpServer()
		{
			if(listen_sock >= 0)
				close(listen_sock);
		}
};




#endif
