#include "utilities.h"
#include "server.h"

#define MAX_CLIENTS	(10)

pthread_mutex_t buffmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t packet_available = PTHREAD_COND_INITIALIZER;
pthread_cond_t packet_slot_available = PTHREAD_COND_INITIALIZER;

static map *root_map;
int clientid;

static int recv_packet(int client_socket);

static void insert_client(map** root, int socket, char* name);

static int lookup_client(map* root, char* name);


int recv_msg(int server_socket, int len, pkt_t *packet) {

	packet->data = (char *) malloc(len);

	return recv(server_socket, packet->data, len, 0);

}

int recv_file(int server_socket, char *file_name) {
	int fd = open(file_name, O_RDWR);
	if(fd < 0)
		ERROR("opening file");
	int recv_status;
	char buffer[255];
	char eof[10] = EOF_SEQ;

	while (1) {
		recv_status = recv(server_socket, buffer, 255, 0);
		if (recv_status == -1) {
			return -1;
		}
		if (strncmp(eof, buffer, 10) == 0)
			break;
		write(fd, buffer, 255);

	}

	close(fd);
	return 0;
}
int send_file(int server_socket, char *file_name) {

	int fd = open(file_name, O_RDWR);
	int send_status;
	char buffer[255];
	char eof[10] = EOF_SEQ;

	while (read(fd, buffer, 255)) {
		send_status = send(server_socket, buffer, 255, 0);
		if (send_status == -1)
			ERROR("Sending file segment");
	}

	send_status = send(server_socket, eof, 10, 0);
	if (send_status == -1)
		ERROR("Sending EOF");
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
static int lookup_client(map* root, char* name) {

	while (root != NULL) {
		if (strcmp(root->name, name) == 0) {
			return root->socket_id;
		} else
			root = root->next;
	}

	printf("Client with Name:%s not connected to server", name);
	return -1;

}

static int recv_packet(int client_socket) {

	pthread_mutex_lock(&buffmutex);
	pkt_t packet;
	if (recv(client_socket, &packet, sizeof(pkt_t), 0) == -1) {
		ERROR("Receiving first packet!\n");
		return 1;
	}


	if (packet.pkt_type == MESSAGE) {
		recv_msg(client_socket, packet.len, &packet);
		printf("Received: %s\n", packet.data);

	} else if (packet.pkt_type == FILE) {
		recv_file(client_socket, packet.file_name);
		printf("Received: %s\n", packet.file_name);
	}

	pthread_cond_signal(&packet_available);
	pthread_mutex_unlock(&buffmutex);

	return 0;
}
static int send_packet(map* root) {
	pthread_mutex_lock(&buffmutex);
	pkt_t packet;
	if (root == NULL) {
		ERROR("No client registered");
		return 0;
	}

	if (packet.pkt_type == MESSAGE) {

		if (packet.cast_type == BROADCAST) {
			while (root != NULL) {

				clientid = (*root).socket_id;
				//send Message
				root = (*root).next;
			}

		} else if (packet.cast_type == BLOCKCAST) {
			while (root != NULL) {

				map client = *root;
				if (strcmp(client.name, packet.peer_name) == 0) {
					continue;
				}
				clientid = (*root).socket_id;
				//send Message
				root = (*root).next;
			}

		} else if (packet.cast_type == UNICAST) {

			clientid = lookup_client(root, packet.peer_name);
			//send message

		}
	} else if (packet.pkt_type == FILE) {

		if (packet.cast_type == BROADCAST) {
			while (root != NULL) {
				//send Message
				clientid = (*root).socket_id;

				root = (*root).next;
			}

		} else if (packet.cast_type == BLOCKCAST) {
			while (root != NULL) {
				//send Message
				map client = *root;
				if (strcmp(client.name, packet.peer_name) == 0) {
					continue;
				}
				clientid = (*root).socket_id;
				root = (*root).next;
			}

		} else if (packet.cast_type == UNICAST) {

			clientid = lookup_client(root, packet.peer_name);
			//send message

		}
	}

	pthread_cond_signal(&packet_available);
	pthread_mutex_unlock(&buffmutex);

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

void *tx_interface(void *args) {
	map *root = (map *) args;
	while (1) {

		int send_status = send_packet(root);
		if (send_status != 0) {
			ERROR("Sending message to server!");
		}

	}

	return NULL;
}

int main(int argc, char *argv[]) {

	printf("Server\n");

	pthread_t tid[MAX_CLIENTS + 1]; //Initializing server threads

	static int tnum = 0; //Thread ids assigned for newly created thread after incrementing
	char CLIENT_NAME[MAX_NAME_LEN];		  //Hold Client Name for mapping
	memset(&CLIENT_NAME, 0, sizeof(CLIENT_NAME));//Initializing 0 to avoid junk data

	int server_socket;

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

	//Thread to send the message/file across client/s as requested.
	//pthread_create(&tid[10], NULL, tx_interface, &root_map);
	int client_socket;
	//Parent Thread
	while (1) {
		//Initialization step
		//Accept new client
		//Receive Client name and map it to the client-fd
		tnum = (tnum + 1) % MAX_CLIENTS; //Thread id
		client_socket = accept(server_socket, NULL, NULL);
		recv(client_socket, CLIENT_NAME, sizeof(CLIENT_NAME), 0);
		insert_client(&root_map, client_socket, CLIENT_NAME);
		//Create a new thread for the client
		pthread_create(&tid[tnum], NULL, rx_interface, &client_socket);

	}

	return 0;
}
