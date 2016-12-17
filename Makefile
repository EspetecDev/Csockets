all: client server
	#gcc -o client client.c
	#gcc -o server server.c

client: client.c 
	gcc -o client client.c -lpthread -l sqlite3

server: server.c
	gcc -o server server.c -lpthread -l sqlite3

clean:
	rm server client

clean_db:
	rm server_data.db
