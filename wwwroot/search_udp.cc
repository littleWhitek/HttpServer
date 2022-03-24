#include<iostream>
#include<string>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<string.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<stdlib.h>


#define IP "172.20.10.12"
#define PORT 9090

int main()
{
    int sock = socket(AF_INET,SOCK_DGRAM,0);
    if(sock < 0)
    {
        perror("sock create fail");
        return 1;
    }
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(IP);
    server.sin_port = htons(PORT);


    char size[64];
    char param[1024];
    if(getenv("CONTENT-LENGTH"))
    {
        strcpy(size,getenv("CONTENT-LENGTH"));
        int cl = atoi(size);
        int i = 0;
        for(;i < cl;++i)
        {
            read(0,param+i,1);
        }
        param[i] = 0;
        printf("echo: %s\n",param);
        sendto(sock,param,sizeof(param),0,(struct sockaddr*)&server,sizeof(server));

        char buf[10240] = {0};
        struct sockaddr_in peer;
        socklen_t len = sizeof(peer);

        ssize_t s = 0;
        while((s = recvfrom(sock,buf,sizeof(buf)-1,0,(struct sockaddr*)&peer,&len)) >= 10239)
        {
            buf[10239] = 0;
            printf("%s\n",buf);
        }
        buf[s] = 0;
        printf("%s\n",buf);
    }
    close(sock);
    return 0;
}

