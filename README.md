# web-proxy-hw1
hw1 for cps512 distributed information systems spring 2015 duke maggs

To compile:
gcc -c proxy.c && gcc -c csapp.c
gcc -pthread csapp.o proxy.o -o proxy

Or, type "make" to use the makefile