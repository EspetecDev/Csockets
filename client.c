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

int makeConnexion(char *port, char *hostname)
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    time_t timestamp;
    char buffer[272];
    int correct = 0;

    // Init socket
    portno = atoi(port);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        fprintf(stderr, "ERROR opening socket\n");
        correct = 1;
        return 1;
    }
    
    // Get server data
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        correct = 1;
        return 1;   
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    
    serv_addr.sin_port = htons(portno);

    // Trying to connect to socket
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    {
        fprintf(stderr, "ERROR connecting!\n");
        correct = 1;
        return 1;
    }

    
    // Listen 
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0)
    {
        fprintf(stderr, "ERROR reading from socket!\n");
        correct = 1;
        return 1;
    }
    else
        printf("%s\n",buffer);

    // Response
    bzero(buffer,276); //256 for the message + 7 for get/set id + 9 for timestamp + id if client/server

    while(fgets(buffer, 276,stdin) != NULL && !correct)
    {
        // Prepare the info for the server
        timestamp = time(NULL);
        char *message, ans[256];
        asprintf(&message, "%d %ld ", 1, timestamp);
        strcat(message, buffer);

        // Send it
        n = write(sockfd, message, strlen(message));
        if (n < 0)
        {
            fprintf(stderr, "ERROR writing to socket!\n");
            correct = 1;
            return 1;
        }
        bzero(buffer,256);
        // Receive the server's response
        n = read(sockfd, ans, 256);
        if (n < 0)
        {
             fprintf(stderr, "ERROR reading from socket!\n");
             correct = 1;
             return 1;
        }
        printf("%s\n", ans);
    }
    
    close(sockfd);

    return 0;
}

int main(int argc, char *argv[])
{

    
    if (argc < 5) {
       fprintf(stderr,"usage %s hostname port backup_hostname backup_ port\n", argv[0]);
       exit(0);
    }

    if(makeConnexion(argv[2], argv[1]) == 1)
        makeConnexion(argv[4], argv[3]);    
    

    return 0;
}
