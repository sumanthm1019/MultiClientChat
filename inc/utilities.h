#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#define FRED        "\033[31m"
#define FGREEN      "\033[32m"
#define NONE        "\033[0m"

#define DEBUG() printf(FGREEN"DEBUG: Function: %s \tLine Number: %d", __FUNCTION__, __LINE__); printf(NONE"\n");

#define ERROR(x) printf(FRED"ERROR: Function: %s \tLine Number: %d \tError: %s", __FUNCTION__, __LINE__, x); printf(NONE"\n");

#define UNICAST   0
#define BROADCAST 1
#define BLOCKCAST 2

#define MESSAGE   0
#define FILE	  1

#define MAX_NAME_LEN 	(15)
#define MAX_RECV_BUF	(256)
#define MAX_SEND_BUF	(256)

#define EOF_SEQ		"@@@EOF@@@"
typedef struct {
	int pkt_type;
	int cast_type;
	int len;
	char *data;
	char file_name[MAX_NAME_LEN];
	char peer_name[MAX_NAME_LEN];
	char sender_name[MAX_NAME_LEN];
} pkt_t;
