all: nclient nserver

nserver:
	g++ Server/server.cpp -o nserver -lsqlite3 -pthread -lcurl
nclient:
	g++ Client/client.cpp -o nclient 