all: nclient nserver

nserver:
	g++ Server/server.cpp -o server -lsqlite3 -pthread
nclient:
	g++ Client/client.cpp -o client 