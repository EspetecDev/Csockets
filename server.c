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

#define STRING_SIZE 255
#define MAX_TRIES 5

struct Data{
	int id;
	char text[STRING_SIZE];
	int timestamp;
};

sqlite3 *db;

void dbinit()
{
	int rc;
	char *sql, *testinit;
	char *zErrMsg = 0;
	sqlite3_stmt *stmt;

	// Init Database
	rc = sqlite3_open("server_data.db", &db);
	if(rc)	
		fprintf(stderr, "[DB INFO] Can't open the DB: %s\n", sqlite3_errmsg(db));
	else
		fprintf(stderr, "[DB INFO] DB successfully opened\n");
	
	// Let's check if the table exists
	asprintf(&testinit,"select count(type) from sqlite_master where type='table' and name='DATA'");

	rc = sqlite3_prepare_v2(db, testinit, -1, &stmt, 0);
	rc = sqlite3_step(stmt);

	// There's no table, so let's create it
	if(atoi(sqlite3_column_text(stmt, 0)) < 1)
	{
		sqlite3_finalize(stmt);
		fprintf(stderr, "[DB INFO] Crating table...\n");
		
		asprintf(&sql, "CREATE TABLE DATA(ID INT PRIMARY KEY NOT NULL,VALUE TEXT NOT NULL,TIMESTAMP INT NOT NULL);");


		rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
		if(rc != SQLITE_OK)
			fprintf(stderr, "[GET DATA] SQL failed in sqlite3_prepare_v2\n");
		else
			rc = sqlite3_step(stmt);
	}
	sqlite3_finalize(stmt);
}

struct Data getData(int id)
{
	int rc;
	sqlite3_stmt *stmt;
	char *n, *sql;
	struct Data data;
	const unsigned char *m;

	// Set data for empty struct
	data.id = id;
	strcat(data.text, "\0");
	data.timestamp = 0;

	// Prepare sql statement
	asprintf(&sql, "SELECT * FROM DATA WHERE ID=%d", id);

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);	
	if(rc != SQLITE_OK)
		fprintf(stderr, "[GET DATA] SQL failed in sqlite3_prepare_v2\n");
	else
	{
		rc = sqlite3_step(stmt);
		data.id = id;
		m = sqlite3_column_text(stmt, 1);

		if(m != NULL)
		{
			strcpy(data.text, m);
			data.timestamp = atoi(sqlite3_column_text(stmt, 2));
		}
		else
			fprintf(stderr, "[GET DATA] Data not found\n");
		
	}
	sqlite3_finalize(stmt);

	return data;
}

int writeData(int id, char buffer[STRING_SIZE], int timestamp)
{
	int rc;
	char *sql;
	char *zErrMsg = 0;
	struct Data d = getData(id);
	sqlite3_stmt *message;

	// 1.- Check if the id is already created, if so, then modify	
	if(d.timestamp != 0) //Then does exist on db, check the timestamp to see if the data is newer
	{
		// 1.1- Modify the value on db
		if( timestamp > d.timestamp )
		{
	
			asprintf(&sql, "UPDATE DATA SET VALUE='%s', TIMESTAMP=%d WHERE ID=%d;", buffer, timestamp, id);

			rc = sqlite3_prepare_v2(db, sql, -1, &message, 0);
			if( rc != SQLITE_OK)
			{
				fprintf(stderr, "[WRITEDATA] SQL Error: %s\n", zErrMsg);
				sqlite3_free(zErrMsg);
				return 1;
			}
			else
			{
				rc = sqlite3_step(message);
				fprintf(stderr, "[WRITEDATA] Data written successfully!\n");
			}
		}
		else
			fprintf(stderr, "[WRITEDATA] Discarding obsolete value...\n");
	}
	else
	{
		// 3.- Write data to db
		asprintf(&sql, "INSERT INTO DATA (ID,VALUE,TIMESTAMP) VALUES (%d, '%s', %d)", id, buffer, timestamp);

		rc = sqlite3_prepare_v2(db, sql, -1, &message, 0);
		if( rc != SQLITE_OK)
		{
			fprintf(stderr, "[WRITEDATA] SQL Error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			return 1;
		}
		else
		{
			rc = sqlite3_step(message);
			fprintf(stderr, "[WRITEDATA] Data written successfully!\n");
		}
	}
	sqlite3_finalize(message);
	return 0;
}

void reachServer(struct Data data, int bbPort, char *arg, char *arg2)
{
	int sockfd, n, bPort;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	char buffer[256], ans[256];

    printf("[RS] Trying to reach server...\n");

    // Init socket
    bPort = atoi(arg2);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    	fprintf(stderr, "[RS] ERROR opening socket\n");

	//Get server data
	server = gethostbyname(arg);
	if(server == NULL)
		fprintf(stderr, "[RS] Error, no such host!\n");

	bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    
    serv_addr.sin_port = htons(bPort);

    // Trying to connect to socket
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        fprintf(stderr, "[RS] ERROR connecting to socket.\n");
 	
	// Listen welcome message
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0) 
        n = fprintf(stderr, "[RS] ERROR reading from socket\n");

	char *message;
	asprintf(&message, "%d %d set %d %s", 0, data.timestamp, data.id, data.text);
	write(sockfd, message, strlen(message));

	n = read(sockfd, ans, 256);
    if (n < 0) 
    	fprintf(stderr, "[RS] ERROR reading from socket\n");

	printf("[RS] FINISH\n");
}

void manageConnection(int sock, int bPort, char *arg, char *arg2)
{		
	int n, i_arg0, i_arg2, msg_len, sender;
	char *message, client_input[270];
	char *getdata = malloc(sizeof(STRING_SIZE));
	char output[256], i_arg1[10], i_arg3[256];
	struct Data d;

	fprintf(stderr, "[SERVER] Client connected!\n");

	message = "[SERVER] Connection accepted, type your command: \n";
	write(sock, message, strlen(message));

	// Read client's input
	while((msg_len = recv(sock, client_input, 276, 0)) > 0)
	{
		// Receiving client's data
		printf("[SERVER] Input:\n\t%s\n", client_input);
		
		// Splitting data 
		n = sscanf(client_input, "%d %d %s %d %255[^\n]c", &sender, &i_arg0, i_arg1, &i_arg2, i_arg3);
		
		if(i_arg2 >= 1 && i_arg2 <= 31)
		{
			if(strcmp(i_arg1, "get") == 0)
			{
				strcpy(output, getData(i_arg2).text);
				if(strcmp(output, "\0") == 0)
					write(sock, "[SERVER] Data not found!.", 26);
				else
					write(sock, output, 256);
			}
			else if(strcmp(i_arg1, "set") == 0)
			{
				if(!writeData(i_arg2, i_arg3, i_arg0))
				{
					write(sock, "[SERVER] Data written successfully!", 36);
					d = getData(i_arg2);
					if(sender == 1) //If sender is a client
						reachServer(d, bPort, arg, arg2);
				}
				else
					write(sock, "[SERVER] Error writing data!", 29);
			}
			else
				fprintf(stderr, "[SERVER] Command not valid!\n");
		}
		else
			fprintf(stderr, "[SERVER] ID not valid!\n");
	}


	if(msg_len == 0)
    {
        fprintf(stderr, "[SERVER] Client disconnected\n");
    }
    else if(msg_len == -1)
        fprintf(stderr, "[SERVER] recv failed\n");
}

int main(int argc, char *argv[])
{
	int sock, client_sock, *new_sock;
	int port, bPort, pid;
	struct sockaddr_in server_addr, b_server_addr, client_addr;
	socklen_t client_len;
	struct hostent *bserver;

	// Open db
	dbinit();

	// Check if correct number of args are passed
	if( argc != 4)
	{
		fprintf(stderr, "Invalid arguments!\n");
		fprintf(stderr, "Usage: ./server port backup_port, backup_ip\n");
		exit(EXIT_FAILURE);
	}

	// Simple TCP connection
	bPort = (unsigned short) atoi(argv[2]);
	// bserver = gethostbyname(argv[1]);

	// Inits own socket on specified port
	sock = 	socket(AF_INET, SOCK_STREAM, 0);

	if( sock < 0 )
		fprintf(stderr, "Error opening socket!\n");

	printf("[SERVER] Socket created\n");

	// Init of server addr by reserving space in mem
	bzero((char *)&server_addr, sizeof(server_addr));
	port = atoi(argv[1]);

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;	//server IP, got by INADDR_ANY
	server_addr.sin_port = htons(port); 		//need to use htons to convert port no network bytes

	if( bind(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
		fprintf(stderr, "Error in local binding!\n");

	printf("[SERVER] Bind done\n");
	
	//Waiting connections, max 5
	listen(sock, 5); 	
	client_len = sizeof(client_addr);

	while(1)
	{
		client_sock = accept(sock, (struct sockaddr *) &client_addr, &client_len);

		if(client_sock < 0)
		{
			fprintf(stderr, "Error accepting connection!\n");
			exit(1);
		}

		// Create child process
		pid = fork();
		if(pid < 0)
		{
			fprintf(stderr, "Error forking the process!\n");
			exit(1);
		}

		if(pid == 0)
		{
			close(sock);
			manageConnection(client_sock, bPort, argv[3], argv[2]);
			exit(0);
		}
		// else
		// 	close(client_sock);
	}

	sqlite3_close(db);						
	
	return 0;
}