all: server client

server: SERVER/server23.c SERVER/serverfunc.c INCLUDE/datastruct.c INCLUDE/utils.c
	gcc -o server SERVER/server23.c SERVER/serverfunc.c INCLUDE/datastruct.c INCLUDE/utils.c -lpthread
	mv server SERVER
client: CLIENT/client23.c CLIENT/clientfunc.c INCLUDE/datastruct.c INCLUDE/utils.c
	gcc -o client CLIENT/client23.c CLIENT/clientfunc.c INCLUDE/datastruct.c INCLUDE/utils.c -lpthread -g
	mv client CLIENT
