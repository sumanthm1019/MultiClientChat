#include "utilities.h"
#include "server.h"

#define MAX_CLIENTS	(10)

pthread_mutex_t buffmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t packet_available = PTHREAD_COND_INITIALIZER;
pthread_cond_t packet_slot_available = PTHREAD_COND_INITIALIZER;

static map *root_map;
int server_socket;
static int recv_packet(int client_socket);

static void insert_client(map** root, int socket, char* name);

/*static int lookup_client(map* root, char* name);*/


void sig_handler(int signo)
{
	close(server_socket);
	exit(1);
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

int send_msg(int server_socket, char *msg) {

	return send(server_socket, msg, strlen(msg), 0);

}

//Insert a new client into the LinkedList data structure
static void insert_client(map** root_map, int socket, char* name) {

	map* new_node = (map *) malloc(sizeof(map));
	strcpy(new_node->name, name);
	new_node->socket_id = socket;

	if (*root_map == NULL) {
		//First CLient
		*root_map = new_node;
		new_node->next = NULL;
	} else {
		//Else add new client and make it as root
		new_node->next = *root_map;
		*root_map = new_node;
	}

}

//Given client name, lookup client_socket in the map  linklist and return
/*static int lookup_client(map* root, char* name) {

	while (root != NULL) {
		if (strcmp(root->name, name) == 0) {
			return root->socket_id;
		} else
			root = root->next;
	}

	printf("Client with Name:%s not connected to server", name);
	return -1;

}*/

static int recv_packet(int client_socket) {

	pkt_t packet;
	int recv_status = 0;
	if (recv(client_socket, &packet, sizeof(pkt_t), 0) == -1) {
		ERROR("Receiving first packet!\n");
		return 1;
	}

	if (packet.pkt_type == MESSAGE) {
		recv_status = recv_msg(client_socket, packet.len, &packet);
		if (recv_status < 0)
			return 1;

		printf("%s: %s\n", packet.sender_name, packet.data);

	} else if (packet.pkt_type == FILE) {
		printf("%s: %s\n", packet.sender_name, packet.file_name);
		recv_status = recv_file(client_socket, packet.file_name, packet.len);
		if (recv_status < 0)
			return 1;
	}

	pkt_t first_packet;
	first_packet.cast_type = packet.cast_type;
	first_packet.len = packet.len;
	first_packet.pkt_type = packet.pkt_type;
	strcpy(first_packet.sender_name, packet.sender_name);
	strcpy(first_packet.file_name, packet.file_name);
	int send_status = send(client_socket, &first_packet, sizeof(pkt_t), 0);
	if (send_status == -1) {
		ERROR("Sending first packet!");
		return 1;
	}

	if (packet.pkt_type == MESSAGE) {
		send_status = send_msg(client_socket, (packet.data));
		if (send_status == -1) {
			ERROR("Sending main packet!");
			return 1;
		}
	} else if (packet.pkt_type == FILE) {
		send_status = send_file(client_socket, (packet.file_name));
		if (send_status == -1) {
			ERROR("Sending main packet!");
			return 1;
		}
	}
	return 0;
}

void *rx_interface(void *args) {
	int client_socket = *(int *) args;

	while (1) {

		int recv_status = recv_packet(client_socket);
		if (recv_status != 0) {
			ERROR("Receiving message from server!");
		}

	}
	return NULL;
}

int main(int argc, char *argv[]) {


	pthread_t tid[MAX_CLIENTS + 1]; //Initializing server threads

	static int tnum = 0; //Thread ids assigned for newly created thread after incrementing
	char CLIENT_NAME[MAX_NAME_LEN];		  //Hold Client Name for mapping
	memset(&CLIENT_NAME, 0, sizeof(CLIENT_NAME));//Initializing 0 to avoid junk data

	signal(SIGINT, sig_handler);

	server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (server_socket == -1) {
		ERROR("Error while creating server_socket\n");
		return 1;
	}

	// init structure for server socket
	struct sockaddr_in server_address;

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(SERVER_PORT);
	server_address.sin_addr.s_addr = INADDR_ANY;

	// bind the socket with the struct

	if (bind(server_socket, (struct sockaddr*) &server_address,
			sizeof(server_address)) == -1) {
		ERROR("Error while binding server_socket\n");
		return 1;
	}

	// listening for connections
	if (listen(server_socket, MAX_CLIENTS) == -1) {
		ERROR("Error while listening to server_socket");
		return 1;
	}

	root_map = NULL;

	int client_socket;
	//Parent Thread
	while (1) {
		tnum = (tnum + 1) % MAX_CLIENTS; //Thread id
		client_socket = accept(server_socket, NULL, NULL);
		recv(client_socket, CLIENT_NAME, sizeof(CLIENT_NAME), 0);
		insert_client(&root_map, client_socket, CLIENT_NAME);
		//Create a new thread for the client
		pthread_create(&tid[tnum], NULL, rx_interface, &client_socket);

	}

	return 0;
}
