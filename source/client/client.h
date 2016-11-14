#ifndef __CLIENT__
#define __CLIENT__
#include "utilities.h"

int send_msg		(int socket, char *msg);
int send_file		(int socket, char *file_name);

int recv_file		(int socket, char *file_name);
int recv_msg		(int server_socket, int len, pkt_t *packet);

#endif
