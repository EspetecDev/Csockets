#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
void error(const char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    time_t timestamp;
    char buffer[272];
    
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }

    // Init socket
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        fprintf(stderr, "ERROR opening socket\n");
    
    // Get server data
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);   
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    
    serv_addr.sin_port = htons(portno);

    // Trying to connect to socket
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        fprintf(stderr, "ERROR connecting!\n");

    
    // Listen 
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0) 
         fprintf(stderr, "ERROR reading from socket!\n");
    printf("%s\n",buffer);

    // Response
    bzero(buffer,272); //256 for the message + 7 for get/set id

    while(fgets(buffer, 272,stdin) != NULL)
    {
        // Prepare the info for the server
        timestamp = time(NULL);
        char *message, ans[256];
        asprintf(&message, "%d %ld ", 1, timestamp);
        strcat(message, buffer);

        // Send it
        n = write(sockfd, message, strlen(message));
        if (n < 0) 
            fprintf(stderr, "ERROR writing to socket!\n");   
        bzero(buffer,256);
        // Receive the server's response
        n = read(sockfd, ans, 256);
        if (n < 0) 
             fprintf(stderr, "ERROR reading from socket!\n");
        printf("%s\n", ans);
    }
    
    close(sockfd);

    return 0;
}
