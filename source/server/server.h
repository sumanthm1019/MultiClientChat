#ifndef __SERVER__
#define __SERVER__

typedef struct clientSocketMap_t map;
struct clientSocketMap_t
{
	char name[20];
	int  socket_id;
	map* next;
};


#endif
