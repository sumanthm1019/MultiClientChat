#include "utilities.h"

#define NUM_CLIENT_THREADS 3

/** Local variables
 */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t send_packet_signal = PTHREAD_COND_INITIALIZER;
pthread_cond_t build_packet_signal = PTHREAD_COND_INITIALIZER;
int server_socket;
static pkt_t client_packet;
static pkt_t server_packet;
static char my_name[MAX_NAME_LEN];
/** Static Local functions
 *
 */
static int build_packet(char *cast_type, char *pkt_type, char *data, char *peer);
static int send_packet(int server_socket);
static int recv_packet(int server_socket);

static int last_word_of(char *data, char *last)
{
	int i;
	int lastspace = 0;
	for(i = 0; i <strlen(data); i++)
	{
		if(data[i] == ' ')
		{
			lastspace = i;
		}

	}
	memcpy(last, data + lastspace + 1, (i - lastspace));

	return 0;

}

void sig_handler(int signo)
{
	close(server_socket);
	exit(1);
}

int send_msg(int server_socket, char *msg) {

	return send(server_socket, msg, strlen(msg), 0);

}

int send_file(int server_socket, char *file_name) {

	ssize_t read_bytes, sent_bytes, sent_file_size = 0;
	int sent_count = 0;

	int fd = open(file_name, O_RDWR);

	char buffer[MAX_SEND_BUF];

	while ((read_bytes = read(fd, buffer, MAX_RECV_BUF)) > 0) {
		if ((sent_bytes = send(server_socket, buffer, read_bytes, 0))
				< read_bytes) {
			ERROR("Sending file segment");
			return -1;
		}
		sent_count++;
		sent_file_size += sent_bytes;
	}
	close(fd);
	return 0;
}

int recv_msg(int server_socket, int len, pkt_t *packet) {

	packet->data = (char *) malloc(len);

	return recv(server_socket, packet->data, len, 0);

}

int recv_file(int server_socket, char *file_name, int file_size) {

	ssize_t rcvd_bytes, rcvd_file_size = 0;
	char recv_str[MAX_RECV_BUF]; /* buffer to hold received data */
	int fd = open(file_name, O_RDWR | O_CREAT, 0777);

	while ((rcvd_bytes = recv(server_socket, recv_str, MAX_RECV_BUF, 0)) > 0) {

		rcvd_file_size += rcvd_bytes;

		if ((write(fd, recv_str, rcvd_bytes)) < 0) {
			ERROR("error writing to file");
			return -1;

		}
		if(rcvd_file_size >= file_size)
			break;
	}

	close(fd);
	return 0;
}


/** @brief Accepts user input and builds the packet
 *
 */
void *user_interface(void *args) {
	char pkt_type[10];
	char cast_type[10];
	char text_link[100];
	char peer_name[20];
	char client_name[MAX_NAME_LEN];
	strcpy(client_name, (char *)args);

	while (1) {
		printf("%s:", client_name);
		scanf("%s %s %[^\n]s %s", cast_type, pkt_type, text_link, peer_name);
		if (cast_type == NULL || pkt_type == NULL || text_link == NULL) {
			ERROR("Entered parameters incorrect\n");
			printf(
					"Usage: unicast/broadcast/blockcast message/file message-content/file-path");
			continue;
		}
		pthread_mutex_lock(&mutex);
		int build_packet_status = build_packet(cast_type, pkt_type, text_link, peer_name);
		if (build_packet_status != 0) {
			ERROR("building packet!\n");
		}
		pthread_mutex_unlock(&mutex);
		pthread_cond_signal(&send_packet_signal);
	}

	return NULL;
}

/** @brief Sends the packet over the socket
 *
 */
void *tx_interface(void *args) {
	int server_socket = *(int *) args;
	while (1) {
		pthread_mutex_lock(&mutex);
		pthread_cond_wait(&send_packet_signal, &mutex);
		int send_status = send_packet(server_socket);
		if (send_status != 0) {
			ERROR("Sending message to server!");
		}
		pthread_mutex_unlock(&mutex);
	}
	return NULL;
}

/** @brief Receives Packets from the server
 *
 */
void *rx_interface(void *args) {
	int server_socket = *(int *) args;
	while (1) {
		int recv_status = recv_packet(server_socket);
		if (recv_status != 0) {
			ERROR("Receiving message from server!");
		}
	}

	return NULL;
}

int main(int argc, char *argv[]) {


	signal(SIGINT, sig_handler);
	if (argc != 2) {
		printf("Usage: %s <name_of_client>\n", argv[0]);
		return 0;
	}
	pthread_t tid[NUM_CLIENT_THREADS];

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket == -1) {
		ERROR("Socket creation!\n");
		return 1;
	}
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(SERVER_PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	while (1) {
		int connection_status = connect(server_socket,
				(struct sockaddr *) &server_address, sizeof(server_address));
		if (connection_status == -1) {
			continue;
		} else {
			break;
		}
	}
	strcpy(my_name, argv[1]);
	int send_status = send(server_socket, argv[1], strlen(argv[1]), 0);
	if (send_status == -1) {
		ERROR("Sending first packet!\n");
		return 1;
	}
	pthread_create(&tid[0], NULL, user_interface, argv[1]);
	pthread_create(&tid[1], NULL, tx_interface, &server_socket);
	pthread_create(&tid[2], NULL, rx_interface, &server_socket);

	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);
	pthread_join(tid[2], NULL);
	close(server_socket);
	return 0;
}

// Static Function Definitions
static int send_packet(int server_socket) {

	pkt_t *first_packet = (pkt_t *) malloc(sizeof(pkt_t));

	first_packet->cast_type = client_packet.cast_type;
	first_packet->len = client_packet.len;
	first_packet->pkt_type = client_packet.pkt_type;
	first_packet->data = NULL;
	strcpy(first_packet->sender_name, client_packet.sender_name);
	if(client_packet.pkt_type == FILE)
		strcpy(first_packet->file_name, client_packet.file_name);

	int send_status = send(server_socket, first_packet, sizeof(pkt_t), 0);
	if (send_status == -1) {
		ERROR("Sending first packet!");
		return 1;
	}
	if (client_packet.pkt_type == MESSAGE) {
		send_status = send_msg(server_socket, (client_packet.data));
		if (send_status == -1) {
			ERROR("Sending main packet!");
			return 1;
		}
	} else if (client_packet.pkt_type == FILE) {
		send_status = send_file(server_socket, (client_packet.file_name));
		if (send_status == -1) {
			ERROR("Sending main packet!");
			return 1;
		}
	}

	return 0;
}

static int build_packet(char *cast_type, char *pkt_type, char *data, char *peer) {

	struct stat stat_buf;
	int fd;
	if (cast_type == NULL || pkt_type == NULL || data == NULL) {
		printf(
				"Enter correct arguments!\n Usage <cast type> <packet type> <packet data> \n");
		return 1;
	}

	if (!strcmp(cast_type, "unicast")) {
		client_packet.cast_type = UNICAST;
		last_word_of(data, peer);
		strcpy(client_packet.peer_name, peer);
	} else if (!strcmp(cast_type, "broadcast")) {
		client_packet.cast_type = BROADCAST;
	} else {
		last_word_of(data, peer);
		strcpy(client_packet.peer_name, peer);
		client_packet.cast_type = BLOCKCAST;
	}
	if (!strcmp(pkt_type, "message")) {
		client_packet.pkt_type = MESSAGE;
		client_packet.len = strlen(data);
		client_packet.data = data;
		strcpy(client_packet.sender_name, my_name);
	} else {
		client_packet.pkt_type = FILE;
		fd = open(data, O_RDWR);
		fstat(fd, &stat_buf);
		client_packet.len = stat_buf.st_size;
		strcpy(client_packet.sender_name, my_name);
		strcpy(client_packet.file_name, data);
	}
	return 0;
}

static int recv_packet(int server_socket) {

	int recv_status = recv(server_socket, &server_packet, sizeof(pkt_t), 0);
	if (recv_status == -1) {
		ERROR("Receiving first packet!");
		return 1;
	}

	if (server_packet.pkt_type == MESSAGE) {
		recv_status = recv_msg(server_socket, server_packet.len,
				&server_packet);
		if (recv_status <= 0) {
			ERROR("Receiving the message!");
			return 1;
		}
		printf("\n%s: %s\n", server_packet.sender_name, server_packet.data);
	} else {
		printf("\n%s: %s\n", server_packet.sender_name, server_packet.file_name);
		recv_status = recv_file(server_socket, server_packet.file_name, server_packet.len);
		if (recv_status != 0) {
			ERROR("Receiving the file!");
			return 1;
		}
	}
	printf("%s: ", my_name);
	fflush(stdout);

	return 0;
}

