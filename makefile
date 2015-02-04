all: 
	gcc -c proxy.c && gcc -c csapp.c
	gcc -pthread csapp.o proxy.o -o proxy