CC = clang
CFLAGS = -Wall -Werror -Wextra -pedantic 

all: httpserver

httpserver: httpserver.c
	$(CC) -o httpserver httpserver.c asgn2_helper_funcs.a
	
httpserver.o: *.c
	&(CC) $(CFLAGS) -c *.c
	
clean: 
	rm -f httpserver *.o

format:
	clang-format -i -style=file *.c
