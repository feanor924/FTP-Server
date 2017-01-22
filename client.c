#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>   
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/sendfile.h>
#include <pthread.h>

int sock;
pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t doneflag = 0;

static void myHand(int signo){

    if ( signo == SIGINT){
        fprintf(stdout,"\n---------Ctrl C Signal terminated\n");
    }
    exit(0);
}

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


#ifndef WORD_MAX
#define WORD_MAX 4096
#endif

int findDirectorys(DIR *dirp,char *path);
int callSocket(char *hostname, unsigned short portnum,char ip[128]);
void *receiving(void *socket_desc);

int main(int argc,char **argv){
    struct sockaddr_in server;
    char myname[HOST_NAME_MAX+1]; 
    char portNumber[10];
    int i=0,tmpi=0,k=0,l=0,count=0;
    struct sigaction act;
    int flag=0,filehandle,size;
    struct stat obj;
    struct timeval start, end;
    char choice[56],anotherchoice[56],cwd[1024];
    char f;
    struct file_t file;
    struct client_t client;
    double _second;
    long seconds, useconds;
    double mtime;
    DIR *dirp;
    pthread_t _thread;
    int new_sock,new1;
    int number=0,read_size;
    char ip[128];
    int ip_=0;
    
    gettimeofday(&start, NULL);
    
    act.sa_handler = myHand; /* set up signal handler */
    act.sa_flags = 0;
    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1)) {
        perror("Failed to set SIGINT handler");
        return 1;
    }
    while ( argv[1][i] != '\0'){
        ip[ip_++]=argv[1][i];       
        if ( argv[1][i] == ':'){
            ip[ip_-1]='\0';
            ++i;
            while ( argv[1][i] != '\0'){
                portNumber[tmpi]=argv[1][i];
                ++tmpi;
                ++i;
            }
            break;
        }
        ++i;
    }
    i=0;
    portNumber[tmpi]='\0';

    gethostname(myname, HOST_NAME_MAX);
    sock=callSocket(myname,atoi(portNumber),ip);
    
    if ( sock == -1){
        fprintf(stderr, "There is no server! \n" );
        exit(0);
    }
    // server ile bağlantı kurulmuş file descriptor(sock)

    gettimeofday(&end, NULL);
    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;
    mtime = ((seconds)*1000 + useconds/1000.0) + 0.5;
    mtime*=1000;
    client.connectionTime=mtime;
    // writing connection time
    write(sock, &client,sizeof(struct client_t));

    if( pthread_create( &_thread, NULL, (void*) receiving, &sock) < 0){
        fprintf(stderr,"could not create thread");
        return 1;
    }

    sigset_t intmask;

    if ((sigemptyset(&intmask) == -1) || (sigaddset(&intmask, SIGINT) == -1)){
        perror("Failed to initialize the signal mask");
        exit(0); 
    }

    while(1){

        if (sigprocmask(SIG_BLOCK,&intmask, NULL) == -1){
            fprintf(stderr, "Sigprocmask failed\n" );
            exit(0);
        }

        fprintf(stdout,"Enter choice you want: ");
        fgets (choice, 56, stdin);

        write(sock, &choice,56);
        
        choice[strlen(choice)-1]='\0';
        l=0;
        k=0;
        while ( choice[k] != '\0' ){
            if ( choice[k] == ' ' )
                break;
            anotherchoice[l++]=choice[k++];
        }
        anotherchoice[l]='\0';
        
        
        if ( strcmp(anotherchoice,"listServer") == 0){
            
            strcpy(file.operation,anotherchoice);
            write(sock, &file,sizeof(struct file_t));
        
        }
        
        else if ( strcmp(anotherchoice,"listLocal") == 0){
            
            strcpy(file.operation," ");
            write(sock, &file,sizeof(struct file_t));
            if (getcwd(cwd, sizeof(cwd)) != NULL){
                dirp=opendir(cwd);
                fprintf(stderr, "-----Local Directory\n" );
                findDirectorys(dirp,cwd);
            }
        
        }
        
        else if ( strcmp(anotherchoice,"help") == 0 ){
            
            strcpy(file.operation," ");
            write(sock, &file,sizeof(struct file_t));
            fprintf(stdout,"-----listLocal\n");
            fprintf(stdout,"-----listServer\n");
            fprintf(stdout,"-----lsClients\n");
            fprintf(stdout,"sendFile <filename> <clientid>\n");
            fprintf(stdout,"-----help\n");
            fprintf(stderr, "USAGE: ./exec <portNumber>\n" );
        }
        
        else if ( strcmp(anotherchoice,"lsClients") == 0 ){
            
            strcpy(file.operation,anotherchoice);
            write(sock, &file,sizeof(struct file_t));
        
        }
        
        else if ( strcmp(anotherchoice,"sendFile") == 0 ){
            
            strcpy(file.operation,anotherchoice);
            
            l=0;
            ++k;
            flag=0;
            count=0;
            
            while ( choice[k] != '\0' ){
                if ( choice[k] != ' ' && choice[k] != '\0' ){
                    anotherchoice[l++]=choice[k++];
                }
                if ( choice[k] == ' ' || choice[k] == '\0'){
                    anotherchoice[l]='\0';
                    ++flag;
                    if ( flag == 1){
                        strcpy(file.filename,anotherchoice);
                    }
                    else if (flag == 2){
                        strcpy(file.id,anotherchoice);
                    }
                    else
                        fprintf(stderr, "You entered more arguman for sendFile\n" );
                    if ( choice[k] == '\0')
                        break;
                    l=0;
                    ++k;
                }
            }
            if (flag == 1)
                strcpy(file.id,"-2");

            stat(file.filename, &obj);
            size = obj.st_size;
            file.size=size;
            
            // file bilgilerini gönderir
            write(sock, &file,sizeof(struct file_t));
            
            filehandle = open(file.filename,O_RDONLY);
            if(filehandle == -1){
                fprintf(stderr,"No such file on the local directory\n");
            }
            fprintf(stderr, "File < %s > sending to server\n",file.filename );
            
            for ( count=0;count<file.size;++count){
                read(filehandle,&f, sizeof(char));
                write(sock, &f, sizeof(char));
            }
            close(filehandle);
            
            fprintf(stderr, "File < %s > sent to server succesfully\n",file.filename );
            
        }
        else{
            fprintf(stderr, "\nWrong Operation !!\n" );
        }
        if (sigprocmask(SIG_UNBLOCK,&intmask, NULL) == -1){
            fprintf(stderr, "Sigprocmask UNfailed\n" );
            exit(0);
        }
    }
     
    close(sock);
    return 0;

}


void *receiving(void *socket_desc){
    int count=0;
    char f;
    int filehandle;
    int sock=*(int*)socket_desc;
    struct file_t file;
    struct client_t client;
    int i=0,an=0,number;
    char files[10][255];
    int read_size=0;
    sigset_t intmask;

    if ((sigemptyset(&intmask) == -1) || (sigaddset(&intmask, SIGINT) == -1)){
        perror("Failed to initialize the signal mask");
        exit(0); 
    }

    while (1){

        if (sigprocmask(SIG_BLOCK,&intmask, NULL) == -1){
            fprintf(stderr, "Sigprocmask failed\n" );
            exit(0);
        }
        
        pthread_mutex_lock(&_mutex);  
        
        read_size=read(sock,&file,sizeof(struct file_t));
        
        if(read_size == 0 || read_size == -1){
            fprintf(stderr,"\n\nServer did CTRL-C!!\n");
            close(sock);
            exit(0);
        }
        
        if ( strcmp(file.operation,"sendFile") == 0){   
            
            filehandle = open(file.filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
            if(filehandle == -1){
                fprintf(stderr,"No such file on the local directory\n");
            }  
            fprintf(stderr, "I'm receiving file...Please wait\n" );
            for ( count=0;count<file.size;++count){
                read(sock, &f, sizeof(char));
                if(read_size == 0 || read_size == -1){
                    fprintf(stderr,"\n\nServer did CTRL-C!!\n");
                    exit(0);
                }
                write(filehandle,&f, sizeof(char));
            }
            
            fprintf(stderr, "I received file succesfully\n" );
            close(filehandle);
        
        }
        
        else if ( strcmp(file.operation,"lsClients") == 0){

            read(sock,&client,sizeof(struct client_t));
            if(read_size == 0 || read_size == -1){
                fprintf(stderr,"\n\nServer did CTRL-C!!\n");
                exit(0);
            }

            fprintf(stderr, "\nClients: \n" );
            
            for ( i=0;i<client.clientnumber;++i)
                fprintf(stderr, "Client ID: %d \n",client.client_arr[i] );
        
        }
        
        else if ( strcmp(file.operation,"listServer") == 0 ){

            read(sock,&number,sizeof(int));
            if(read_size == 0 || read_size == -1){
                fprintf(stderr,"\n\nServer did CTRL-C!!\n");
                exit(0);
            }
            read(sock,files,sizeof(files));
            if(read_size == 0 || read_size == -1){
                fprintf(stderr,"\n\nServer did CTRL-C!!\n");
                exit(0);
            }
            for (an=0;an<number;++an ){
                fprintf(stderr, "%s\n",files[an] );
            }
        
        }
        pthread_mutex_unlock(&_mutex);
        
        if (sigprocmask(SIG_UNBLOCK,&intmask, NULL) == -1){
            fprintf(stderr, "Sigprocmask UNfailed\n" );
            exit(0);
        }
    }
}


int findDirectorys(DIR *dirp,char *path){
    int counter=0;
    char current[WORD_MAX];
    struct dirent *direntp;
    DIR *dir;
    do{
        direntp=readdir(dirp);

        if( direntp == NULL){
            break;
        }
        
        if(strcmp(direntp->d_name,"..") != 0 && strcmp(direntp->d_name,".") != 0 
            && direntp->d_name[strlen(direntp->d_name)-1] != '~'){
            strcpy(current,path);
            strcat(current,"/");
            strcat(current,direntp->d_name);
            fprintf(stderr, "%s\n",direntp->d_name );
            if( !( ( dir=opendir(current) ) == NULL ) ){
                ++counter;
               counter+=findDirectorys(dir,current); 
            }
        }

    }while( direntp != NULL );

    return counter;
}

int callSocket(char *hostname, unsigned short portnum,char ip[128]) {
    struct sockaddr_in sa;
    struct hostent *hp;
    int a, s;

    if ((hp= gethostbyname(hostname)) == NULL) { 
        return(-1);
    } 
    
    memset(&sa,0,sizeof(sa));
    sa.sin_family= hp->h_addrtype; 
    sa.sin_port= htons((u_short)portnum);
    inet_pton(AF_INET, ip, &(sa.sin_addr.s_addr));
    
    if ((s= socket(hp->h_addrtype,SOCK_STREAM,0)) < 0) 
        return(-1);

    if (connect(s,(struct sockaddr *)&sa,sizeof sa) < 0) { 
        close(s);
        return(-1);
    }
    return(s); 
}