
INC = -I./inc/

all:

		gcc -Wall $(INC) -g -o ./bin/server ./source/server/server.c -pthread
		gcc -Wall $(INC) -g -o ./bin/client ./source/client/client.c -pthread
				
clean:
		rm -rf *o *so *out
		rm -rf /run/shm/*txt