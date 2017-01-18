#include <stdlib.h>         
#include <string.h>         
#include <stdio.h>      
#include <netinet/in.h>      
#include <sys/types.h>      
#include <sys/socket.h>   
#include <sys/stat.h>          
#include <unistd.h>     /*Unix 标准函数定义*/
#include <fcntl.h>      /*文件控制定义*/
#include <termios.h>    /*PPSIX 终端控制定义*/
#include <errno.h>      /*错误号定义*/
#include <pthread.h>    /*线程定义*/
#include <arpa/inet.h>

#include "serial.h"
#include "flower.h"
#include "camera.h"

#define SERVER_PORT 8000
#define LENGTH_OF_LISTEN_QUEUE 20
#define BUFFER_SIZE 1024

struct flower
{
	char ip[20];
	int port;
};
     
struct camera
{
	char ip[20];
	int port;
	int flag;
};
     	            	
int main(int argc, char **argv)
{
	//花卉识别服务器ip和port
	struct flower addr_flower;
	strcpy(addr_flower.ip, "192.168.1.104");
	addr_flower.port = 20101;
	
	//摄像头服务器ip和port
	struct camera addr_camera;
	strcpy(addr_camera.ip, "192.168.1.104");
	addr_camera.port = 8885;
	addr_camera.flag = 1;
	
	struct sockaddr_in server_addr;  
    int server_socket;  
    int opt = 1;  
     
    bzero(&server_addr, sizeof(server_addr));   
      
    server_addr.sin_family = AF_INET;  
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);  
    server_addr.sin_port = htons(SERVER_PORT);  
  
    /* create a socket */  
    server_socket = socket(PF_INET, SOCK_STREAM, 0);  
    if(server_socket < 0)  
    {  
        printf("Socket Server: Create Socket Failed!");  
        exit(1);  
    }  
   
    /* bind socket to a specified address*/  
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  
    if(bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)))  
    {  
        printf("Socket Server: Server Bind Port: %d Failed!", SERVER_PORT);   
        exit(1);  
    }  
  
    /* listen a socket */  
    if(listen(server_socket, LENGTH_OF_LISTEN_QUEUE))  
    {  
        printf("Socket Server: Server Listen Failed!");   
        exit(1);  
    }  
      
    /* run server */  
    while(1)   
    {  
        struct sockaddr_in client_addr;  
        int client_socket;        
        socklen_t length;  
        unsigned char buffer[BUFFER_SIZE];  
  
        /* accept socket from client */  
        length = sizeof(client_addr);  
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &length);  
        if(client_socket < 0)  
        {  
            printf("Socket Server: Server Accept Failed!\n");  
            break;  
        }  
        else
        {
        	struct sockaddr_in peeraddr;
			socklen_t len = sizeof(peeraddr);
			if(!getpeername(client_socket, (struct sockaddr *)&peeraddr, &len))
			{
				printf("Socket Server: IP： %s Connected!\n", inet_ntoa(peeraddr.sin_addr));
				//printf("Socket Server: PORT：%d\n", ntohs(peeraddr.sin_port));
			}
        }
        /* receive data from client */  
        pthread_t id1 = 0;
        pthread_t id2 = 0;
        while(1)  
        {  
            bzero(buffer, BUFFER_SIZE);  
            length = recv(client_socket, buffer, BUFFER_SIZE, 0);  
            if(length < 0)  
            {  
                printf("Socket Server: Server Recieve Data Failed!\n");  
                break;  
            }  
                  
            if(0xc0 == buffer[0] || 0xc8 == buffer[0] || 0xc9 == buffer[0] || 
               0xca == buffer[0] || 0xcb == buffer[0] || 0xcc == buffer[0] || 0x80 == buffer[0])
            {
            	printf("Socket Server: Command: %x\n", buffer[0]);
            	char serialPort[] = "/dev/tty0";
            	int ret = serial_send(serialPort, &buffer[0]);
            	if(ret != 0)
            	{
            		printf("Command Send Failed\n");
            		continue;
            	}
            }
            else if(0xd0 == buffer[0]) //开启花卉识别功能
            {
            	printf("Socket Server: Flower Classification!\n");

				int ret = pthread_create(&id1, NULL, thread_flower_classification, &addr_flower);
				if(ret != 0)
				{
					printf("Flower Classification Pthread Create Failed\n");
					continue;
				}
            }
            else if(0xe0 == buffer[0] || 0xee == buffer[0]) //打开或关闭摄像头
            {
				if(0xe0 == buffer[0])
				{
					printf("Socket Server: Open Camera!\n"); 
					addr_camera.flag = 1;
				}
				else if(0xee == buffer[0])
				{
					printf("Socket Server: Close Camera!\n");
					addr_camera.flag = 0;
					//pthread_cancel(id2);
				}           	
				
            	int ret = pthread_create(&id2, NULL, thread_open_camera, &addr_camera);
				if(ret != 0)
				{
					printf("Camera Pthread Create Failed\n");
					continue;
				}
            }
            else if(0xff == buffer[0])  //关闭套接字
            {  
                printf("Socket Server: Quit From Client!\n");  
                close(client_socket);
                pthread_cancel(id1);
            	pthread_cancel(id2);
                break;  
            }
            else
            {
            	printf("Socket Server: Command Error!\n");
            	close(client_socket);
            	pthread_cancel(id1);
            	pthread_cancel(id2);
                break; 
            }
        } 
		close(client_socket); 
        pthread_join(id1, NULL);
        pthread_join(id2, NULL); 
    }  
      
    close(server_socket); 
	return 0;
}
