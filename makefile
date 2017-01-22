all:
	gcc fileShareServer.c -o exec -pthread
	gcc -c client.c
	gcc -o exec1 client.o -pthread
