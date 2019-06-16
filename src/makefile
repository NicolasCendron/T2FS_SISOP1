#
# Makefile para programas exemplo
# 

CC=gcc
LIB_DIR=../lib

all: main 
	$(CC) -o main main.o $(LIB_DIR)/apidisk.o

main: main.c 
	$(CC) -c main.c -o main.o 

	
clean:
	rm -rf main *.o *~

