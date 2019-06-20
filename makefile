#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
# 

CC=gcc
CFLAGS=-Wall -g
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src

OBJS=lib/apidisk.o  lib/t2fs.o 

all: lib/t2fs.o 
			ar crs lib/libt2fs.a $(OBJS)


lib/t2fs.o: src/t2fs.c #dependencies
			 gcc -c src/t2fs.c -o lib/t2fs.o -Wall


tar:
	@cd .. && tar -zcvf 274693.tar.gz t2fs

clean:
	rm -rf $(LIB_DIR)/*.a lib/t2fs.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~


