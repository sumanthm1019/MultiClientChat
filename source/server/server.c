#include "utilities.h"
#include "server.h"

#define MAX_CLIENTS	(10)

static map *root_map;
int server_socket;
static int recv_packet(int client_socket);
static pkt_t packet_buffer; //Packet which stores packet data

static int global_client_id;
static void insert_client(map** root, int socket, char* name);
static int server_send_packet();
static int lookup_client_id(char* name);

void sig_handler(int signo) {
	close(server_socket);
	while(root_map != NULL)
	{
		close(root_map->socket_id);
		root_map = root_map->next;
	}
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
		if (rcvd_file_size >= file_size)
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
//Given client name, lookup client_socket in the map  linklist and return
static int lookup_client_id(char* name) {

	map *root = root_map;
	while (root != NULL) {
		if (strcmp(root->name, name) == 0) {
			return root->socket_id;
		} else
			root = root->next;
	}

	return -1;

}

static char* lookup_client_name(int id) {

	map *root = root_map;
	while (root != NULL) {
		if (root->socket_id==id) {
			return root->name;
		} else
			root = root->next;
	}

	return NULL;

}

static int send_packet(int client_socket) {

	pkt_t *first_packet = (pkt_t *) malloc(sizeof(pkt_t));

	first_packet->cast_type = packet_buffer.cast_type;

	first_packet->len = packet_buffer.len;

	first_packet->pkt_type = packet_buffer.pkt_type;

	first_packet->data = NULL;

	strcpy(first_packet->sender_name, packet_buffer.sender_name);
	if (packet_buffer.pkt_type == FILE)
		strcpy(first_packet->file_name, packet_buffer.file_name);

	int send_status = send(client_socket, first_packet, sizeof(pkt_t), 0);
	if (send_status == -1) {
		ERROR("Error: Sending first packet!");
		exit(1);
	}

	if (packet_buffer.pkt_type == MESSAGE) {
		send_status = send_msg(client_socket, (packet_buffer.data));
		if (send_status == -1) {
			ERROR("Sending main packet!");
			return 1;
		}
	} else if (packet_buffer.pkt_type == FILE) {
		send_status = send_file(client_socket, (packet_buffer.file_name));
		if (send_status == -1) {
			ERROR("Sending main packet!");
			return 1;
		}
	}

	return 0;
}
static int server_send_packet() {

	map* root = root_map;
	pkt_t packet = packet_buffer;
	if (root == NULL) {
		ERROR("No client registered");
		return 1;
	}
	int clientid;

	if (packet.cast_type == BROADCAST) {
		while (root != NULL) {
			clientid = root->socket_id;
			if (clientid == global_client_id) {
				root = root->next;
				continue;
			} else {
				send_packet(clientid);
				root = root->next;
			}
		}

	} else if (packet.cast_type == BLOCKCAST) {
		while (root != NULL) {
			map* client = root;
			if ((strcmp(client->name, packet.peer_name) == 0)
					|| (client->socket_id == global_client_id)) {
				root = root->next;
				continue;
			} else {
				clientid = root->socket_id;
				send_packet(clientid);
				root = root->next;
			}
		}

	} else if (packet.cast_type == UNICAST) {
		clientid = lookup_client_id(packet.peer_name);
		if(clientid == -1)
		{
			return 1;
		}
		send_packet(clientid);

	}

	return 0;
}

static int recv_packet(int client_socket) {

	int recv_status = recv(client_socket, &packet_buffer, sizeof(pkt_t), 0);
	if (recv_status == -1) {
		ERROR("Receiving first packet!");
		return 1;
	}else if(recv_status==0){
		return 1;
	}

	printf("%s ", packet_buffer.sender_name);
	if(packet_buffer.cast_type == UNICAST)
		printf("unicast ");
	else if(packet_buffer.cast_type == BROADCAST)
		printf("broadcast ");
	else
		printf("blockcast ");
	//storing the client id of the current(Used in identifying current clientID in broadcast)
	global_client_id = client_socket;
	if (packet_buffer.pkt_type == MESSAGE) {
		recv_status = recv_msg(client_socket, packet_buffer.len,
				&packet_buffer);
		if (recv_status == -1) {
			ERROR("Receiving the message!");
			return 1;
		}
		printf("message - %s\n", packet_buffer.peer_name);
	} else {
		recv_status = recv_file(client_socket, packet_buffer.file_name, packet_buffer.len);
		if (recv_status == -1) {
			ERROR("Receiving the file!");
			return 1;
		}
		printf("file - %s\n", packet_buffer.peer_name);
	}

	int server_send_status = server_send_packet();
	if(server_send_status != 0)
		return 1;
	return 0;
}

void *rx_interface(void *args) {
	int client_socket = *(int *) args;
	char *name = lookup_client_name(client_socket);

	while (1) {

		int recv_status = recv_packet(client_socket);
		if (recv_status != 0) {
			printf("client : %s  closed its connection \n",name);
			return NULL;
		}

	}
	return NULL;
}

int main(int argc, char *argv[]) {

	if(argc != 2){
		printf("Usage: %s <port Number>\n", argv[0]);
		return 0;
	}
	int port_num = atoi(argv[1]);
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
	int enable = 1;
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
	    ERROR("setsockopt(SO_REUSEADDR) failed");

	// init structure for server socket
	struct sockaddr_in server_address;

	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port_num);
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
		memset(&CLIENT_NAME, 0, sizeof(CLIENT_NAME));
		//Create a new thread for the client
		pthread_create(&tid[tnum], NULL, rx_interface, &client_socket);

	}

	return 0;
}
