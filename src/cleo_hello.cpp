#include<stdio.h> 
#include<unistd.h> 
#include<stdlib.h> 
#include<string.h>    
#include<sys/socket.h>    
#include<arpa/inet.h> 
#include <netdb.h>
#include <fcntl.h>

 
int main(int argc , char *argv[])
{
    int sock;
    int bytes_read;
    struct sockaddr_in server;      // arg to connect
    struct hostent *server_info;    // for address resolution
    char message[10] , server_reply[20000];

    char * server_host_name;
    int portno;

    // cmd line args
    if (argc == 3) {
      server_host_name = argv[1];
      portno = atoi(argv[2]);
      }
    else {
      fprintf(stderr,"usage: cleo_hello hostname port\n");
      exit(1);
      }
  
    int i;

    // resolve server host name
    server_info = gethostbyname(server_host_name);
    if (server_info == NULL) { fprintf(stderr,"ERROR, no such host\n"); exit(1); }
     
    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) { fprintf(stderr,"Could not create socket\n"); exit(1); }

    // populate server struct
    memset((char *) &server, 0, sizeof(server));  
    server.sin_family = AF_INET;
    memcpy((char *)&server.sin_addr.s_addr,         
           (char *)server_info->h_addr,          
           server_info->h_length);
    server.sin_port = htons(portno);            // network byte order
 
    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0) { fprintf(stderr,"Connect to cleo failed\n"); close(sock); exit(1); }
     
    strcpy(message,"HELLO\n");

    // fprintf(stderr,"sending message: --%s--\n",message);

    //Send some data
    if( send(sock , message , strlen(message) , 0) < 0) { fprintf(stderr,"HELLO send failed\n"); close(sock); exit(1); }

    int saved_flags = fcntl(sock, F_GETFL);
    fcntl(sock, F_SETFL, saved_flags & ~O_NONBLOCK);

    // //Wait five seconds for GOODDAY
    // for (i = 1; i <= 5; i++) {

    // let's try 10 seconds - 5 seconds seems to fail often for no obvious reason
    for (i = 1; i <= 10; i++) {

        // fprintf(stderr,"try: %d\n",i);
        //Receive a reply from the server
        if( (bytes_read = recv(sock , server_reply , 20000 , 0)) < 0) { fprintf(stderr,"receive failed\n"); close(sock); exit(1); }

        server_reply[bytes_read] = '\0';
        // fprintf(stderr,"reply:\n%s\n=====\n",server_reply);
        if (strstr(server_reply,"GOODDAY")!=NULL) { fprintf(stderr,"GOODDAY\n"); close(sock); exit(0); }

        sleep(1);
    }
 
    fprintf(stderr,"no goodday!\n"); close(sock); exit(1);
     
}
