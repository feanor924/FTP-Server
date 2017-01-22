#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/sendfile.h>
#include <pthread.h>

#ifndef WORD_MAX
#define WORD_MAX 4096
#endif

#define MAXCLIENT 5

static volatile sig_atomic_t doneflag = 0;

static void setdoneflag(int signo) {
    doneflag = 1;
}

int new_sock[10];
int new1_sock[10];
int new1sockets=0;
int sockets=0;
double connectionTimes[10];
int numberClient=0;
int client_arr[10];
pthread_mutex_t mutexes[20];
pid_t threadID[20];
int thid=0;


struct file_t{
    char filename[56];
    char operation[26];
    char id[10];
    int size;
};

struct client_t{
    double connectionTime;
    int clientnumber;
    int client_arr[10]; 
};

int findDirectorys(DIR *dirp,char *path,char files[][255]);
int establish (unsigned short portnum);
void *connection_handler(void *);
 
int main(int argc , char *argv[]){
    int socket_desc, client_sock, c;
    struct sigaction act1;
    struct client_t client;
    pthread_t *sniffer_thread;

    act1.sa_handler = setdoneflag;
    act1.sa_flags = 0;
    if ((sigemptyset(&act1.sa_mask) == -1) || (sigaction(SIGINT, &act1, NULL) == -1)){
        perror("Failed to set SIGTERM handler\n");
        return 1;
    }
    
    socket_desc=establish(atoi(argv[1]));
    if (socket_desc == -1){
        fprintf(stderr, "Bind Failed try again with new port\n" );
        exit(0);
    }
    
    fprintf(stderr, "Server Port Number: %d\n",atoi(argv[1]) );
    fprintf(stdout,"Server is waiting to be connect\n");

    numberClient=0;
    sniffer_thread=(pthread_t*)malloc(sizeof(pthread_t)*5);
         
    while(1){
        
        new1_sock[new1sockets++] = accept(socket_desc, NULL, NULL);
        
        pthread_mutex_init(&mutexes[new1_sock[new1sockets-1]],NULL);
        
        if ( new1_sock[sockets-1]  < 0){
            fprintf(stderr,"accept failed\n");
            return 1;
        }
        new_sock[sockets++] = new1_sock[new1sockets-1];
        ++numberClient;
        client_arr[numberClient-1]=new_sock[sockets-1];
        
        //reading connection time
        read(new_sock[sockets-1],&client,sizeof(struct client_t));
        
        connectionTimes[numberClient-1]=client.connectionTime;
        
        while(doneflag){
            fprintf(stdout, "\nCTRL-C pressed ! Exitting.......\n");
            exit(0);
        }
        
        fprintf(stdout,"\n------Connection accepted\n");
        fprintf(stderr, "Client ID: %d ",new_sock  [sockets-1] );
        fprintf(stdout,"------Connection time is: %d\n",(int)client.connectionTime);
        
        if( pthread_create( &sniffer_thread[numberClient-1] , NULL , 
            (void*) connection_handler , &new_sock[sockets-1]) < 0){
            fprintf(stderr,"could not create thread");
            return 1;
        }
        
    
    }
     
    return 0;
}
 

void *connection_handler(void *socket_desc){
    int sock;
    int number=0;
    sock = *(int*)socket_desc;
    int read_size,i=0,filehandle,count=0;
    struct file_t file;
    struct client_t client;
    char messeage[56];
    char files[10][255],cwd[255];
    DIR *dirp;
    struct timeval start, end;
    double mtime;
    long seconds, useconds;

    while (1){
        
        while(doneflag){
            fprintf(stdout, "\nCTRL-C pressed ! Exitting.......\n");
            exit(0);
        }
        // reading command
        read_size=read(sock,messeage,56);
        
        if(read_size == 0){
            fprintf(stderr,"Client disconnected\n");
            break;
        }
        
        else if(read_size == -1){
            fprintf(stderr,"Client disconnected\n");
            break;
        }
        //file bilgilerini alır
        read_size=read(sock,&file,sizeof(struct file_t));
        
        fprintf(stdout,"\n------Client Request is: %s\n", messeage);
        
        count=0;

        if ( strcmp(file.operation,"listServer") == 0  ){
            
            if (getcwd(cwd, sizeof(cwd)) != NULL){
                dirp=opendir(cwd);
                number=findDirectorys(dirp,cwd,files);
            }
            
            strcpy(file.operation,"listServer");
            write(sock, &file,sizeof(struct file_t));
            write(sock,&number,sizeof(int));
            write(sock,files,sizeof(files));
        }
        
        else if ( strcmp(file.operation,"lsClients") == 0 ){
            
            client.clientnumber=numberClient;
            
            for ( i=0;i<client.clientnumber;++i){
                client.client_arr[i]=client_arr[i];
            }
            
            strcpy(file.operation,"lsClients");
            write(sock,&file,sizeof(struct file_t));
            write(sock,&client,sizeof(struct client_t));
        }
        
        else if ( strcmp(file.operation,"sendFile") == 0 ){
            char f;
            
            pthread_mutex_lock(&mutexes[atoi(file.id)]);
            
            if ( atoi(file.id) != -2){
                // 2.threadin iş gördüğü kısım

                write(atoi(file.id), &file,sizeof(struct file_t));
                
                fprintf(stderr,"I'm < %s > file and now I'm sending file to Client %d...Please wait\n",file.filename,atoi(file.id) );
                
                for ( count=0;count<file.size;++count){
                    read(sock,&f, sizeof(char));
                    write(atoi(file.id), &f, sizeof(char));
                }
                close(filehandle);
                
                strcpy(file.operation,"---");
                fprintf(stderr,"< %s > transferred successfully to Client %d...\n",file.filename,atoi(file.id) );
                
            }
            else{
                
                filehandle = open(file.filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
                
                fprintf(stderr,"I'm receiving < %s > file to our server...Please wait\n",file.filename );
                gettimeofday(&start, NULL);
                
                for ( count=0;count<file.size;++count){
                    read(sock, &f, sizeof(char));
                    write(filehandle,&f, sizeof(char));
                }
                
                close(filehandle);
                
                gettimeofday(&end, NULL);
                seconds  = end.tv_sec  - start.tv_sec;
                useconds = end.tv_usec - start.tv_usec;
                mtime = ((seconds)*1000 + useconds/1000.0) + 0.5;
                
                strcpy(file.operation,"---");
                fprintf(stderr,"< %s > transferred successfully to our server!\n",file.filename);
                fprintf(stderr, "File operation took: %f ms\n",mtime );

            }
            
            pthread_mutex_unlock(&mutexes[atoi(file.id)]);
        }
    }
    return 0;
}

int findDirectorys(DIR *dirp,char *path,char files[][255]){
    int counter=0;
    char current[WORD_MAX];
    struct dirent *direntp;
    DIR *dir;
    
    do{
        direntp=readdir(dirp);
        
        if( direntp == NULL){
            break;
        }
        
        if(strcmp(direntp->d_name,"..") != 0 && strcmp(direntp->d_name,".") != 0 && direntp->d_name[strlen(direntp->d_name)-1] != '~'){
            strcpy(current,path);
            strcat(current,"/");
            strcat(current,direntp->d_name);
            strcpy(files[counter],direntp->d_name);
            ++counter;
        }

    }while( direntp != NULL );
    
    closedir(dir);
    closedir(dirp);
    
    return counter;
}

int establish (unsigned short portnum) {
    char myname[HOST_NAME_MAX+1]; 
    int s;
    struct sockaddr_in sa; 
    struct hostent *hp;
    memset(&sa, 0, sizeof(struct sockaddr_in)); 
    
    gethostname(myname, HOST_NAME_MAX);
    hp= gethostbyname(myname);

    if (hp == NULL) /* we don't exist !? */ 
        return(-1);
    
    sa.sin_family= hp->h_addrtype; 
    sa.sin_port= htons(portnum);

    if ( ( s = socket(AF_INET, SOCK_STREAM, 0) ) < 0) 
        return(-1);

    if ( bind( s, (struct sockaddr *)&sa,sizeof(struct sockaddr_in) ) < 0 ) {
        close(s);
        return -1;
    }

    listen(s, MAXCLIENT);
    
    return(s);

}
