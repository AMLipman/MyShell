# choose your compiler
CC=gcc
#CC=gcc -Wall
LDFLAGS = -lkstat -DHAVE_KSTAT

all: mysh showload

mysh: sh.o get_path.o main.c 
	$(CC) -g -pthread main.c sh.o get_path.o -o mysh 
#	$(CC) -g main.c sh.o get_path.o bash_getcwd.o -o mysh 

sh.o: sh.c sh.h
	$(CC) -g -c sh.c

get_path.o: get_path.c get_path.h
	$(CC) -g -c get_path.c

showload: showload.c
	$(CC) -g showload.c -o showload $(LDFLAGS)

sleep: sleep.o
	$(CC) -g sleep.c -o sleep

test: test-1+2.o
	$(CC) -g test-1+2.c -o test-1+2

clean:
	rm -rf sh.o get_path.o mysh









