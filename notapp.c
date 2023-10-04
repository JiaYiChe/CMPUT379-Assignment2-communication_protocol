#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <stdint.h>
#include <setjmp.h>
#include <limits.h>
#include <getopt.h>
#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

#define SIZE 1024
#define EVENT_STRUCT_SIZE sizeof(struct inotify_event)
#define BUFFER_SIZE (EVENT_STRUCT_SIZE+NAME_MAX+1)
//mode 1 = observer, mode 2 = user, mode 3 = server
int mode =0;
float interval=0.0;
unsigned short sport =0;
char logfile[SIZE]="NULL";
char saddr[SIZE]="NULL";
char fileordir[SIZE]="NULL";

pthread_mutex_t x_mute = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread[SIZE];
int new_socket[SIZE]={0};
int socket_identifier[SIZE]={0};
char file_change[SIZE] = "NULL";

int socket_num=0;
//This is daemon function
static void skeleton_daemon(){
    pid_t pid;
    pid=fork();
    if(pid<0){
        exit(EXIT_FAILURE);
    }
    if(pid>0){
        exit(EXIT_SUCCESS);
    }
    if(setsid()<0){
        exit(EXIT_FAILURE);
    }
    signal(SIGCHLD,SIG_IGN);
    signal(SIGHUP,SIG_IGN);

    if(pid<0){
        exit(EXIT_FAILURE);
    }
    if(pid>0){
        exit(EXIT_SUCCESS);
    }
    umask(0);
    chdir("/");
    int x;
    for(x-sysconf(_SC_OPEN_MAX);x>=0;x--){
        close(x);
    }

    openlog("firstdaemon",LOG_PID,LOG_DAEMON);


}
//This is a server function use as thread to send information to client intervaly
void *message_sender(){
    while(1){
        pthread_mutex_lock(&x_mute);
        if(strcmp(file_change,"NULL")!=0){
            for(int i =0;i<SIZE;i++){
                if(socket_identifier[i]==2){
                    send(new_socket[i],file_change,strlen(file_change),0);
                }
            }
        }
        strcpy(file_change,"NULL");
        pthread_mutex_unlock(&x_mute);
        sleep(interval);
    }
}

//This is the singal handler
void signalError(int signo){
    if(mode==3){
        for(int i =0; i<SIZE; i++){
            if(socket_identifier[i]==2){
                send(new_socket[i],"server closed",strlen("server closed"),0);
            }
        }
        exit(0);
    }
    if((mode=1 )| (mode==2)){
        send(socket_num,"close",strlen("close"),0);
        exit(0);
    }
}
//This is thread function that handles the message from observer and save it
void *server_observer(void *arg){
    printf("server_observer");
    int i =1;
    int index = atoi(arg);
    char buffer[SIZE] = {0};
    FILE *fp;
    while(i==1){
        read(new_socket[index], buffer, SIZE);
        pthread_mutex_lock(&x_mute);
        if(strcmp(buffer,"close")!=0){
            strcpy(file_change, buffer);
        }else{//if buffer is close meaning observer closed
            i=0;
            break;
        }//check if there is a file to record actions
        if(strcmp(logfile,"NULL")!=0){
            fp=fopen(logfile,"w+");
            fputs(buffer,fp);
            fputs("\n",fp);
            fclose(fp);

        }
        pthread_mutex_unlock(&x_mute);

    }
    socket_identifier[index]=0;
    new_socket[index]=0;
    pthread_exit(0);
    
}

//This is a server thread function working for receive exit information from user client

void *server_user(void *arg){
    printf("server_user");
    int i =1;
    int index = atoi(arg);
    char buffer[SIZE] = {0};
    while(i==1){
        read(new_socket[index], buffer, SIZE);
        if(strcmp(buffer,"close")==0){
            i=0;
        }
    }
    socket_identifier[index]=0;
    new_socket[index]=0;
    pthread_exit(0);
}
//This is the function of observer working on inotify ojects
void observer(int sock, char *ipAddress){
    int length = strlen(fileordir);
    int start = length-6;
    char filename[1024]={0};
    strncpy(filename,fileordir+start,5);

    int i=1;
    int inotify_fd = inotify_init();
    if(inotify_fd<0){
        printf("can't create the inotify fd");
        exit(0);
    }
    int watch_des = inotify_add_watch(inotify_fd, fileordir,IN_ALL_EVENTS); 
    if(watch_des==-1){
        printf("can't create watch descriptor");
        exit(0);
    }
    char buffer[BUFFER_SIZE]={0};
    //checking what kind event is happening
    while(i==1){
        int bytesRead = read(inotify_fd,buffer,BUFFER_SIZE);
        int bytesProcessed = 0;

        if (bytesRead<0){
            printf("read error");
            exit(0);
        }
            time_t t;
            float timer;
            timer = time(&t);
            char time_now[SIZE];
            sprintf(time_now,"%f",timer);
            char message_send[SIZE]={0};
            strcat(message_send,time_now);
            strcat(message_send," ");
            strcat(message_send,ipAddress);
            strcat(message_send,"   ");
            strcat(message_send,filename);
            strcat(message_send,"   ");
            char activity[200];
            char eventName[20];
        while(bytesProcessed<bytesRead){
            struct inotify_event *event = (struct inotify_event *)(buffer+bytesProcessed);
            
            
            strcpy(eventName,event->name);
            if(event->mask & IN_ACCESS){
                
                
                strcat(activity," IN_ACCESS");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }
    
            }else if(event->mask & IN_ATTRIB){
                
               
                strcat(activity," IN_ATTRIB");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }

            }else if(event->mask & IN_CLOSE_WRITE){
                
             
                strcat(activity," IN_CLOSE_WRITE");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }
                

            }else if(event->mask & IN_CLOSE_NOWRITE){
                
            
                strcat(activity," IN_CLOSE_NOWRITE");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }
            
            }else if(event->mask & IN_CREATE){
                
          
                strcat(activity," IN_CREATE");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }

            }else if(event->mask & IN_DELETE){
                
             
                strcat(activity," IN_DELETE");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }

            }else if(event->mask & IN_DELETE_SELF){
                
         
                strcat(activity," IN_DELETE_SELF");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }
                strcat(message_send,eventName);
                strcat(message_send,activity);
                send(sock,message_send,strlen(message_send),0);
                send(sock,"close",strlen("close"),0);
                printf("The monitoring object has been deleted, observer end");
               
                exit(0);
            }else if(event->mask & IN_MODIFY){
                
             
                strcat(activity," IN_MODIFY");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }
                

            }else if(event->mask & IN_MOVE_SELF){
               
          
                strcat(activity," IN_MOVE_SELF");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }
                strcat(message_send,eventName);
                strcat(message_send,activity);
                send(sock,message_send,strlen(message_send),0);
                send(sock,"close",strlen("close"),0);
                printf("The monitoring object has been moved to other file or renamed");
                exit(0);
            }else if(event->mask & IN_MOVED_FROM){
                
             
                strcat(activity," IN_MOVED_FROM");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }

            }else if(event->mask & IN_MOVED_TO){
     
                strcat(activity," IN_MOVED_TO");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }

            }else if(event->mask & IN_OPEN){
               
       
                strcat(activity," IN_OPEN");
                if (event->mask & IN_ISDIR){
                    strcat(activity," IN_ISDIR");
                }

            }
            
            bytesProcessed += EVENT_STRUCT_SIZE + event ->len;
        }
        strcat(message_send,eventName);      
        strcat(message_send,activity);
        strcat(message_send,"\n");
        send(sock,message_send,strlen(message_send),0);
   

    }

    
}
//This is user client function waiting for exit information to come
void user(int sock){

    char buffer[SIZE] = {0};
    while(1){
        read(sock,buffer,SIZE);
        printf("%s\n",buffer);
    }
}

//This function is to set up the socket and determine which client had connected in.
void setServer(){
    struct sigaction handle;
    handle.sa_flags = SA_NODEFER; // no flags
    sigemptyset(&handle.sa_mask); // clear the mask so no signals are blocked
    handle.sa_handler = signalError; // set function pointer for handler
    sigaction(SIGTERM, &handle, NULL);// set the handler

    int server_fd;
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	char buffer[SIZE]={0};

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(sport);

	if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

    if (listen(server_fd, SIZE) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

    printf("Server opend");
    pthread_t timer_check;
    pthread_create(&timer_check,NULL,(void*)message_sender,NULL);

    
    while(1){
        skeleton_daemon();
        for(int i =0; i<SIZE; i++){
            
            if(socket_identifier[i]==0){
                new_socket[i] = accept(server_fd, (struct sockaddr *)&address,(socklen_t*)&addrlen);
                
                if (new_socket[i]<0) {
                    perror("accept");
                    exit(EXIT_FAILURE);
                }

                read(new_socket[i],buffer,SIZE);

                char message[SIZE];
                sprintf(message,"%d",i);
                if(strcmp(buffer,"observer")==0){
                    
                    socket_identifier[i]=1;
                    pthread_create(&thread[i],NULL,(void*)server_observer,(void *)message);
                    
                }
                if(strcmp(buffer,"user")){
                    socket_identifier[i]=2;
                    pthread_create(&thread[i],NULL,(void*)server_user,(void *)message);
                    
                }
                break;
            }
        }
    }
    syslog(LOG_NOTICE,"First daemon terminated.");
    closelog();


}
//This a function set up the client and determine which client class is creating
void setClient(){
    struct sigaction handle;
    handle.sa_flags = SA_NODEFER; // no flags
    sigemptyset(&handle.sa_mask); // clear the mask so no signals are blocked
    handle.sa_handler = signalError; // set function pointer for handler
    sigaction(SIGTERM, &handle, NULL);// set the handler

    int sock = 0;
	struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
	int cli_length = sizeof(cli_addr);
    char ipAddress[INET_ADDRSTRLEN];

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(sport);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, saddr, &serv_addr.sin_addr)<=0) {
		printf("\nInvalid address/ Address not supported \n");
		exit(EXIT_FAILURE);
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect");
		exit(EXIT_FAILURE);
	}
    printf("Client connected to the server\n");
    getsockname(sock,(struct sockaddr*)&cli_addr,&cli_length);
    inet_ntop(AF_INET,&cli_addr.sin_addr,ipAddress,sizeof(ipAddress));

    if(mode==1){
        send(sock,"observer",strlen("observer"),0);
        socket_num=sock;
        observer(sock,ipAddress);
    }
    if(mode==2){
        send(sock,"user",strlen("user"),0);
        socket_num=sock;
        printf("TIME              HOST        MONITORED     EVENT\n");
        user(sock);
    }

}

int main(int argc,char* argv[]) 
{ 
    char state;
    char *temp;
    if ((state = getopt(argc, argv, "so:u:")) != -1) {
        switch (state) {
            case 's':
                mode = 3;
                while((state=getopt(argc,argv,"t:p:l:"))!=-1){
                    switch(state){
                        case 't':
                            interval=atof(optarg);
                            break;
                        case 'p':
                            sport = atof(optarg);
                            break;
                        case 'l':
                            strcpy(logfile,optarg);
                            break;
                    }
                }
                break;
            case 'o':
                mode=1;
                strcpy(saddr,optarg);
                sport = atof(argv[optind]);
                strcpy(fileordir,argv[optind+1]);
                break;
            case 'u':
                mode=2;
                temp = optarg;
                strcpy(saddr,temp);
                sport = atof(argv[optind]);
                break;
            case '?':
                // option not in optstring or error
                break;
        }
    }

    if(sport==0){
        srand((unsigned) time(NULL));
        sport = rand()%65534+1;
        printf("Random Port Number: %d\n",sport);
    }

    if(mode == 3){
        //printf("0");
        setServer();
    }else{
        setClient();
    }
    
    
    return 0; 
} 
