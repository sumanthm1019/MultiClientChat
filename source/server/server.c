#include "utilities.h"
#include "server.h"



#define MAX_CLIENTS	(10)



pthread_mutex_t mutex 		           = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  packet_available       = PTHREAD_COND_INITIALIZER;

//Root node which stores the head address of the linkedlist containing clientID-CLientName mapping
static map *root_map;


static pkt_t packet_buffer;//Packet which stores packet data

int static global_client_id;
static int recv_packet(int client_socket);

static void insert_client(map** root, int socket, char* name);

static int lookup_client_id(map* root, char* name);

static char* lookup_client_name(map* root, int clientId);

//Insert a new client into the LinkedList data structure
static void insert_client(map** root_map, int socket, char* name){

	map* new_node = (map *) malloc(sizeof(map));
	strcpy (new_node->name, name);
	new_node->socket_id=socket;

	if(*root_map==NULL){
		//First CLient
		*root_map= new_node;
		new_node->next=NULL;
	}else{
		//Else add new client and make it as root
		new_node->next=*root_map;
		*root_map= new_node;
	}


}


//Given client name, lookup client_socket in the map  linklist and return
static int lookup_client_id(map* root, char* name){

	while(root!=NULL){
		if (strcmp(root->name,name)==0){
			return root->socket_id;
		}else
			root=root->next;
	}

	printf("Client with Name:%s not connected to server", name);
	return -1;

}

//Given client name, lookup client_socket in the map  linklist and return
static char* lookup_client_name(map* root, int clientId){

	while(root!=NULL){
		if (root->socket_id==clientId){
			return root->name;
		}else
			root=root->next;
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
	if(fd < 0)
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

	pkt_t *first_packet = (pkt_t *) malloc(sizeof(pkt_t));

	first_packet->cast_type = packet_buffer.cast_type;
	first_packet->len = packet_buffer.len;
	first_packet->pkt_type = packet_buffer.pkt_type;
	first_packet->data = NULL;
	if (packet_buffer.pkt_type == FILE)
		strcpy(first_packet->file_name, packet_buffer.file_name);

	int send_status = send(client_socket, first_packet, sizeof(pkt_t), 0);
	if (send_status == -1) {
		ERROR("Sending first packet!");
		return 1;
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

static int recv_packet(int client_socket) {

	int recv_status = recv(client_socket, &packet_buffer, sizeof(pkt_t), 0);
	if (recv_status == -1) {
		ERROR("Receiving first packet!");
		return 1;
	}
	char * clientName= lookup_client_name(root_map,client_socket);
	char *cast;
	if (packet_buffer.cast_type== 0){
		 cast= "UNICAST";
	}else if (packet_buffer.cast_type== 1){
		 cast= "BROADCAST";
	}else{
		 cast= "BLOCKCAST";
	}

	printf("Request: %s %s %s %s \n", clientName, cast , packet_buffer.data, packet_buffer.peer_name);
	//storing the client id of the current(Used in identifying current clientID in broadcast)
	global_client_id=client_socket;

	if (packet_buffer.pkt_type == MESSAGE) {
		recv_status = recv_msg(client_socket, packet_buffer.len,
				&packet_buffer);
		if (recv_status == -1) {
			ERROR("Receiving the message!");
			return 1;
		}
		printf("%s: %s\n", packet_buffer.peer_name, packet_buffer.data);
	} else {
		recv_status = recv_file(client_socket, packet_buffer.file_name);
		if (recv_status == -1) {
			ERROR("Receiving the file!");
			return 1;
		}
		printf("%s: %s\n", packet_buffer.peer_name, packet_buffer.file_name);
	}

	return 0;
}
static int server_send_packet(map* root) {

	pkt_t packet = packet_buffer;
	if (root == NULL) {
		ERROR("No client registered");
		return 0;
	}
	int clientid;

	if (packet.cast_type == BROADCAST) {
		while (root != NULL) {

			clientid = (*root).socket_id;
			//Skip the client which sends broadcast message
			if (clientid == global_client_id) {
				continue;
			}
			send_packet(clientid);
			root = (*root).next;
		}

	} else if (packet.cast_type == BLOCKCAST) {
		while (root != NULL) {

			map client = *root;
			if (strcmp(client.name, packet.peer_name) == 0) {
				continue;
			}
			clientid = (*root).socket_id;
			send_packet(clientid);
			root = (*root).next;
		}

	} else if (packet.cast_type == UNICAST) {

		clientid = lookup_client_id(root, packet.peer_name);
		send_packet(clientid);

	}

	return 0;
}

void *rx_interface(void *args) {
 	int client_socket = *(int *) args;
 	while (1) {
 			pthread_mutex_lock(&mutex);

			int recv_status = recv_packet(client_socket);
			if (recv_status != 0) {
				ERROR("Receiving message from server!");
			}
			pthread_mutex_unlock(&mutex);
			pthread_cond_signal(&packet_available);
 	}
 	return NULL;
}


void *tx_interface(void *args) {
	map *root = (map *) args;
  	while (1) {
  		pthread_mutex_lock(&mutex);
  		pthread_cond_wait(&packet_available, &mutex);
  		int send_status = server_send_packet(root);
  		if (send_status != 0) {
  			ERROR("Sending message to server!");
  		}
  		pthread_mutex_unlock(&mutex);
  	}

  	return NULL;
  }

int main(int argc, char *argv[]) {



	pthread_t tid[MAX_CLIENTS+1];//Initializing server threads


	static int tnum=0;		  //Thread ids assigned for newly created thread after incrementing
	char CLIENT_NAME[20];//Hold Client Name for mapping
	memset(&CLIENT_NAME,0 ,sizeof(CLIENT_NAME));//Initializing 0 to avoid junk data

	//Place holders for client and server socket descriptor
	int server_socket;
	int client_socket;

	server_socket = socket(PF_INET, SOCK_STREAM, 0);
	if (server_socket==-1){
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
			sizeof(server_address))==-1){
			ERROR("Error while binding server_socket\n");
			return 1;
	}

	// listening for connections
	if (listen(server_socket, MAX_CLIENTS)==-1){
		ERROR("Error while listening to server_socket");
		return 1;
	}
	printf("Server Started and listening  @ Port No:%d\n",SERVER_PORT);
	//Initialize head node to be null
	root_map=NULL;

	//Thread to send the message/file across client/s as requested.
	pthread_create(&tid[10], NULL, tx_interface, &root_map);

	//Parent Thread
	while(1){
		//Initialization step
		//Accept new client
		//Receive Client name and map it to the client descriptor
		tnum=(tnum+1)%MAX_CLIENTS; //Thread id
		client_socket = accept(server_socket, NULL, NULL);//(**will this create new client_socket fd everytime?**)
		recv(client_socket, CLIENT_NAME, sizeof(CLIENT_NAME), 0);
		printf("Connection established with client: %s  \n", CLIENT_NAME);
		insert_client(&root_map,client_socket,CLIENT_NAME);
		//Create a new thread for the client
		pthread_create(&tid[tnum], NULL, rx_interface, &client_socket);

	}


	pthread_join(tid[10], NULL);
	int i;
	for (i=0;i<tnum;i++){
		pthread_join(tid[tnum], NULL);
	}
	close(server_socket);
	close(client_socket);
	return 0;

	return 0;
}
