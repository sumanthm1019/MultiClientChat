#include "utilities.h"
#include "server.h"

#define MAX_CLIENTS	(10)

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t packet_available = PTHREAD_COND_INITIALIZER;

//Root node which stores the head address of the linkedlist containing clientID-CLientName mapping
static map *root_map;

static pkt_t packet_buffer; //Packet which stores packet data

int static global_client_id;

static int recv_packet(int client_socket);

static void insert_client(map** root, int socket, char* name);

static int lookup_client_id( char* name);

static char* lookup_client_name( int clientId);

//Insert a new client into the LinkedList data structure
static void insert_client(map** head, int socket, char* name) {

	map* new_node = (map *) malloc(sizeof(map));
	strcpy(new_node->name, name);
	new_node->socket_id = socket;
	new_node->next = NULL;

	if (*head == NULL) {
		//First CLient
		*head = new_node;
	} else {
		//Else add new client and make it as root
		new_node->next = *head;
		*head = new_node;
	}

}

//Given client name, lookup client_socket in the map  linklist and return
static int lookup_client_id( char* name) {

	map *root=root_map;
	while (root != NULL) {
		printf("Client with Name:%s registered\n", root->name);
		if (strcmp(root->name, name) == 0) {
			return root->socket_id;
		} else
			root = root->next;
	}

	printf("Client with Name:%s not connected to server\n", name);
	return -1;

}

//Given client name, lookup client_socket in the map  linklist and return
static char* lookup_client_name( int clientId) {
	map *root=root_map;
	while (root != NULL) {
		if (root->socket_id == clientId) {
			return root->name;
		} else
			root = root->next;
	}

	return NULL;

}

int send_msg(int client_socket, char *msg) {

	return send(client_socket, msg, strlen(msg), 0);

}

int send_file(int client_socket, char *file_name) {

	int fd = open(file_name, O_RDWR);
	int send_status;
	char buffer[255];
	char eof[10] = EOF_SEQ;

	while (read(fd, buffer, 255)) {
		send_status = send(client_socket, buffer, 255, 0);
		if (send_status == -1)
			ERROR("Sending file segment");
	}

	send_status = send(client_socket, eof, 10, 0);
	if (send_status == -1)
		ERROR("Sending EOF");
	close(fd);
	return 0;
}

int recv_msg(int client_socket, int len, pkt_t *packet) {

	packet->data = (char *) malloc(len);
	return recv(client_socket, packet->data, len, 0);
}

int recv_file(int client_socket, char *file_name) {
	int fd = open(file_name, O_RDWR);
	if (fd < 0)
		ERROR("opening file");
	int recv_status;
	char buffer[255];
	char eof[10] = EOF_SEQ;

	while (1) {
		recv_status = recv(client_socket, buffer, 255, 0);
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

static int send_packet(int client_socket) {

	printf("Before send_packet \n");
	printf("packet.cast_type %d \n",packet_buffer.cast_type);
	printf("packet.data %s \n",packet_buffer.data);
	printf("packet.peer_name %s \n",packet_buffer.peer_name);
	printf("packet.pkt_type %d \n",packet_buffer.pkt_type);

	pkt_t *first_packet = (pkt_t *) malloc(sizeof(pkt_t));

	first_packet->cast_type = packet_buffer.cast_type;

	first_packet->len = packet_buffer.len;

	first_packet->pkt_type = packet_buffer.pkt_type;

	first_packet->data = NULL;

	if (packet_buffer.pkt_type == FILE)
		strcpy(first_packet->file_name, packet_buffer.file_name);

	int send_status = send(client_socket, first_packet, sizeof(pkt_t), 0);
	if (send_status == -1) {
		ERROR("Error: Sending first packet!");
		return 1;
	}
	printf("Sending main packet! %s to client %d \n", packet_buffer.data,client_socket);

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

	map* root =root_map;
	pkt_t packet = packet_buffer;
	if (root == NULL) {
		ERROR("No client registered");
		return 0;
	}
	int clientid;
	printf("Before server_send_packet \n");
	printf("packet.cast_type %d \n",packet.cast_type);
	printf("packet.data %s \n",packet.data);
	printf("packet.peer_name %s \n",packet.peer_name);
	printf("packet.pkt_type %d \n",packet.pkt_type);

	if (packet.cast_type == BROADCAST) {
		printf("Entered BROADCAST\n");
		while (root != NULL) {
			clientid = root->socket_id;
			printf("clientid, global_client_id: %d,%d \n", clientid, global_client_id);
			if (clientid == global_client_id) {
				root = root->next;
				continue;
			}else{
				send_packet(clientid);
				root = root->next;
			}
		}

	} else if (packet.cast_type == BLOCKCAST) {
		printf("Entered BLOCKCAST\n");
		while (root != NULL) {
			map* client = root;
			printf("client.name, packet.peer_name: %s,%s \n", client->name, packet.peer_name);
			printf("clientid, global_client_id: %d,%d \n", client->socket_id, global_client_id);
			if ((strcmp(client->name, packet.peer_name) == 0)||(client->socket_id == global_client_id)) {
				root = root->next;
				continue;
			}else{
				clientid = root->socket_id;
				send_packet(clientid);
				root = root->next;
			}
		}

	} else if (packet.cast_type == UNICAST) {
		printf("Entered UNICAST\n");
		printf("unicast lookup %s \n", packet.peer_name);
		clientid = lookup_client_id(packet.peer_name);
		send_packet(clientid);

	}

	return 0;
}
static int recv_packet(int client_socket) {
	printf("clientID: %d waiting\n", client_socket);
	int recv_status = recv(client_socket, &packet_buffer, sizeof(pkt_t), 0);
	if (recv_status == -1) {
		ERROR("Receiving first packet!");
		return 1;
	}
	printf("clientID: %d recieved first packet %d %d %d \n", client_socket,
			packet_buffer.cast_type, packet_buffer.len, packet_buffer.pkt_type);
	char * clientName = lookup_client_name( client_socket);
	char *cast;
	if (packet_buffer.cast_type == 0) {
		cast = "UNICAST";
	} else if (packet_buffer.cast_type == 1) {
		cast = "BROADCAST";
	} else {
		cast = "BLOCKCAST";
	}

	//storing the client id of the current(Used in identifying current clientID in broadcast)
	global_client_id = client_socket;
	printf("global client id %d \n", global_client_id);
	if (packet_buffer.pkt_type == MESSAGE) {
		recv_status = recv_msg(client_socket, packet_buffer.len,
				&packet_buffer);
		if (recv_status == -1) {
			ERROR("Receiving the message!");
			return 1;
		}
		if (packet_buffer.peer_name != NULL) {
			printf("Request: %s %s %s %s \n", clientName, cast,
					packet_buffer.data, packet_buffer.peer_name);
		} else
			printf("Request: %s %s %s  \n", clientName, cast,
					packet_buffer.data);

	} else {
		recv_status = recv_file(client_socket, packet_buffer.file_name);
		if (recv_status == -1) {
			ERROR("Receiving the file!");
			return 1;
		}
		printf("%s: %s\n", packet_buffer.peer_name, packet_buffer.file_name);
	}

	server_send_packet();
	return 0;
}


void *rx_interface(void *args) {
	int client_socket = *(int *) args;
	printf("Client socketid %d \n", client_socket);
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

	static int tnum = 0;//Thread ids assigned for newly created thread after incrementing
	char CLIENT_NAME[20];		  //Hold Client Name for mapping
	memset(&CLIENT_NAME, 0, sizeof(CLIENT_NAME));//Initializing 0 to avoid junk data

	//Place holders for client and server socket descriptor
	int server_socket;
	int client_socket;

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
	printf("Server Started and listening  @ Port No:%d\n", SERVER_PORT);
	//Initialize head node to be null
	root_map = NULL;

	//Thread to send the message/file across client/s as requested.
	//pthread_create(&tid[10], NULL, tx_interface, &root_map);

	//Parent Thread
	while (1) {
		//Initialization step
		//Accept new client
		//Receive Client name and map it to the client descriptor
		tnum = (tnum + 1) % MAX_CLIENTS; //Thread id
		client_socket = accept(server_socket, NULL, NULL); //(**will this create new client_socket fd everytime?**)
		recv(client_socket, CLIENT_NAME, sizeof(CLIENT_NAME), 0);
		printf("Connection established with client: %s  \n", CLIENT_NAME);
		insert_client(&root_map, client_socket, CLIENT_NAME);
		//Create a new thread for the client
		pthread_create(&tid[tnum], NULL, rx_interface, &client_socket);

	}

	//pthread_join(tid[10], NULL);
	int i;
	for (i = 0; i < tnum; i++) {
		pthread_join(tid[tnum], NULL);
	}
	close(server_socket);
	close(client_socket);
	return 0;

	return 0;
}
