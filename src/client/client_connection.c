#include "client_utils.h"


/* */
int CL_DEFAULT_ID;

/* */
unsigned CL_DEFAULT_CLIENT_PORT;


static char cl_handleRequest(char op_code, struct in_addr* IP, int port,
							 struct client_info* (*cl_menu_query)(struct client_info*, int, int*),
							 void* (*cl_query_mt)(void*));

static void* cl_downloadFile_mt(void* argument);
static void* cl_uploadFile_mt(void* argument);
static void* cl_deleteFile_mt(void* argument);
static char* cl_getStatus(struct in_addr* IP, int port);


char cl_queryOperation(char op_code, struct in_addr* IP, int port){
	
	char ret_code;
	
	switch(op_code){
		case INI:
			if((ret_code = cl_initClient(IP, port)))
				printf("%s", cl_error_assess(ret_code));
			return ret_code;

		case DLD:
			return cl_handleRequest(op_code, IP, port, cl_menu_download, cl_downloadFile_mt);

		case UPD:
			return cl_handleRequest(op_code, IP, port, cl_menu_upload, cl_uploadFile_mt);

		case DEL:
			return cl_handleRequest(op_code, IP, port, cl_menu_delete, cl_deleteFile_mt);

		case STS:
			return cl_showStatus(IP, port);

		case LST:
			if((ret_code = cl_showList(IP, port)))
				printf("%s", cl_error_assess(ret_code));
			return ret_code;

		case EXIT:
			ret_code = cl_exitPool(IP, port);
			return EXIT;

		default:
			printf("%s", cl_error_assess(ERR_UNKNOWN));
			return ERR_UNKNOWN;

	}
	
}


static char cl_handleRequest(char op_code, struct in_addr* IP, int port,
							 struct client_info* (*cl_menu_query)(struct client_info*, int, int*),
							 void* (*cl_query_mt)(void*)){
	
	errno = 0;
	pthread_t *thread = NULL;
	struct socket_con *server = NULL;
	struct client_info *list, *chosen_files;
	int counter = 0, list_size = 0, cf_size = 0, *ret;

	
	log_print(0, 0, "%s operation. Getting client list from server, then prompting user%s\n\n", (op_code == DLD ? "Download": op_code == UPD ? "Upload" : "Delete"), NW_LOG_DOTS);
	list = cl_getList(IP, port, &list_size);
	chosen_files = (*cl_menu_query)(list, list_size, &cf_size);
	log_print(0, 0, "\nDone getting list and prompting user. User chose files from %d %s\n", cf_size, cf_size <= 1 ? "peer" : "different peers");

	if(!chosen_files) _goto(FINISH, errno);
	
	thread = calloc(cf_size, sizeof(pthread_t));
	server = calloc(cf_size, sizeof(struct socket_con));

	for(int i = 0; i < cf_size; i++, counter++){
		
		/* Sets up connection to server */
		log_print(0, 0, ".\t- [%d/%d] Setting up connection to server%s", i+1, cf_size, NW_LOG_DOTS);
		if(nw_setupExtConnection(&server[i], port, IP))
			_goto(FINISH, ERR_CONNECT);
		log_print(0, 0, "Done\n");
		
		chosen_files[i].connection = server[i];
		
		/* Sends client required operation (Download, Upload or Delete) */
		log_print(0, 0, ".\t- [%d/%d] Sending operation code to server%s", i+1, cf_size, NW_LOG_DOTS);
		if(nw_send(&server[i], &op_code, sizeof(op_code)))
			_goto(FINISH, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		/* Sends this client's ID */
		log_print(0, 0, ".\t- [%d/%d] Sending our ID (%d) to server%s", i+1, cf_size, CL_DEFAULT_ID, NW_LOG_DOTS);
		if(nw_send(&server[i], &CL_DEFAULT_ID, sizeof(CL_DEFAULT_ID)))
			_goto(FINISH, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		/* Sends to which client (ID) current one wants the operation to */
		log_print(0, 0, ".\t- [%d/%d] Sending peer ID (%d) to server%s", i+1, cf_size, chosen_files[i].ID, NW_LOG_DOTS);
		if(nw_send(&server[i], &chosen_files[i].ID, sizeof(chosen_files[i].ID)))
			_goto(FINISH, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		/* Sends how many files the client requested */
		log_print(0, 0, ".\t- [%d/%d] Sending how many files (%d) we want to/from peer (%d) to server%s", i+1, cf_size, chosen_files[i].files_n, chosen_files[i].ID, NW_LOG_DOTS);
		if(nw_send(&server[i], &chosen_files[i].files_n, sizeof(chosen_files[i].files_n)))
			_goto(FINISH, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		/* Dispatch thread to handle request */
		log_print(0, 0, ".\t- [%d/%d] Dispatching thread to handle request%s\n", i+1, cf_size, NW_LOG_DOTS);
		if(pthread_create(&thread[i], NULL, (*cl_query_mt), &chosen_files[i]))
			_goto(FINISH, ERR_UNKNOWN);
		
	}
	FINISH:;
	
	/* Logs and frees stuff */
	if(errno){
		printf("%s", cl_error_assess(errno));
		log_print(0, 0, "Error (%d): %s\n\n", errno, cl_error_assess(errno));
	}
	
	for(int i = 0; i < counter; i++){
		pthread_join(thread[i], (void**)&ret);
		if(ret)
			if(*ret){
				printf("%s", cl_error_assess(*ret));
				log_print(0, 0, "Error (%d): %s\n\n", *ret, cl_error_assess(*ret));
			} else log_print(0, 0, "Done %s operation.\n\n", (op_code == DLD ? "downloading": op_code == UPD ? "uploading" : "deleting"));
		else log_print(0, 0, "Done %s operation.\n\n", (op_code == DLD ? "downloading": op_code == UPD ? "uploading" : "deleting"));
	}
	
	for(int i = 0; i < counter; i++)
		close(server[i].socketFD);
	for(int i = 0; i < list_size; i++)
		nw_freeInfo(&list[i]);
	for(int i = 0; i < cf_size; i++)
		nw_freeInfo(&chosen_files[i]);
	
	free(chosen_files);
	free(server);
	free(thread);
	free(list);

	/* 0 if success, error code otherwise */
	return errno;
	
}


char cl_initClient(struct in_addr* IP, int port){
	
	errno = 0;
	struct socket_con server;
	char op_code = INI, **files;
	int files_n = 0, *ID = NULL, *listen_port = NULL;
	
	
	log_print(0, 0, "Initializing client\n.\t- Getting local files%s", NW_LOG_DOTS);
	if((files_n = cl_files_getLocalFiles(".", &files, &files_n)) < 0)
		return errno = ERR_UNKNOWN;
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Setting up connection to server%s", NW_LOG_DOTS);
	if(nw_setupExtConnection(&server, port, IP))
		_goto(FINISH, ERR_CONNECT);
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Sending operation code%s", NW_LOG_DOTS);
	if(nw_send(&server, &op_code, sizeof(op_code)))
		_goto(FINISH, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Sending how many files we have (%d)%s", files_n, NW_LOG_DOTS);
	if(nw_send(&server, &files_n, sizeof(files_n)))
		_goto(FINISH, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Sending files to server\n");
	for(int i = 0; i < files_n; i++){
		log_print(0, 0, ".\t\t- File [%d/%d]: ", i+1, files_n);
		if(nw_send(&server, files[i], strlen(files[i])))
			_goto(FINISH, ERR_SENDRECV);
		log_print(0, 0, "%s\n", files[i]);
	}
	log_print(0, 0, ".\t\tDone\n");
	
	log_print(0, 0, ".\t- Receiving ID server allocated for us%s", NW_LOG_DOTS);
	if(!(ID = nw_recv(&server)))
		_goto(FINISH, ERR_SENDRECV);
	log_print(0, 0, "Done. ID = %d\n", *ID);
	
	log_print(0, 0, ".\t- Receiving on which port we'll list on%s", NW_LOG_DOTS);
	if(!(listen_port = nw_recv(&server)))
		_goto(FINISH, ERR_SENDRECV);
	log_print(0, 0, "Done. Port = %d\n", *listen_port);
	
	CL_DEFAULT_ID = *ID;
	CL_DEFAULT_CLIENT_PORT = *listen_port;
	
	FINISH:;
	
	free(ID);
	if(errno) log_print(0, 0, "Error (%d): %s\n\n", errno, cl_error_assess(errno));
	else log_print(0, 0, "Done initializing operation.\n\n");
	for(int i = 0; i < files_n; i++)
		free(files[i]);
	free(files);
	free(listen_port);
	if(server.socketFD)
		close(server.socketFD);
	
	/* 0 if success, error code otherwise */
	return errno;
	
}


static void* cl_downloadFile_mt(void* argument){
	
	unsigned *peer_port;
	unsigned long datasize;
	struct in_addr *peer_IP;
	struct client_info peer, info = *(struct client_info*)argument;
	char op_code = DLD, ret_code, *ack = NULL, *data = NULL, ip_string[INET_ADDRSTRLEN];
	memset(&peer, 0, sizeof(struct client_info));


	/* Receives peer's IP and port from server */
	log_print(0, 1, "\t\tDownload operation: %d files from peer %d\n", info.files_n, info.ID);
	log_print(0, 0, ".\t\t\t- Receiving peer's (%d) IP and port from server%s", info.ID, NW_LOG_DOTS);
	if(!(peer_IP = nw_recv(&info.connection)))
		_goto(ERROR, ERR_SENDRECV);
	if(!(peer_port = nw_recv(&info.connection))){
		free(peer_IP);
		_goto(ERROR, ERR_SENDRECV);
	}
	log_print(0, 0, "Done: %s:%d\n", inet_ntop(AF_INET, peer_IP, ip_string, sizeof(ip_string)), *peer_port);
	
	/* Sets up peer structure */
	peer.connection.addr.sin_addr = *peer_IP;
	peer.connection.port = *peer_port;
	peer.ID = info.ID;
	free(peer_port);
	free(peer_IP);
	
	/* Sets up connection to peer so we download the files */
	log_print(0, 1, ".\t\t\t- Setting up connection to peer (%d)%s", peer.ID, NW_LOG_DOTS);
	if(nw_setupExtConnection(&peer.connection, peer.connection.port, &peer.connection.addr.sin_addr))
		_goto(ERROR, ERR_CONNECT);
	log_print(0, 0, "Done\n");
	
	log_print(0, 1, ".\t\t\t- Sending operation code (download) to peer (%d)%s", peer.ID, NW_LOG_DOTS);
	if(nw_send(&peer.connection, &op_code, sizeof(op_code)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	log_print(0, 1, ".\t\t\t- Sending peer (%d) how many files we want to download (%d)%s", peer.ID, info.files_n, NW_LOG_DOTS);
	if(nw_send(&peer.connection, &info.files_n, sizeof(info.files_n)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n\n");
	
	
	for(int i = 0; i < info.files_n; i++){
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Sending filename (%s) to server%s", i+1, info.files_n, info.filename[i], NW_LOG_DOTS);
		if(nw_send(&info.connection, info.filename[i], strlen(info.filename[i])))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Sending filename (%s) to peer (%d)%s", i+1, info.files_n, info.filename[i], peer.ID, NW_LOG_DOTS);
		if(nw_send(&peer.connection, info.filename[i], strlen(info.filename[i])))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Receiving peer's (%d) found file ack%s", i+1, info.files_n, peer.ID, NW_LOG_DOTS);
		if(!(ack = nw_recv(&peer.connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. Peer (%d) %s\n", peer.ID, *ack ? "has that file" : "doesn't have that file anymore");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Notifying server that peer (%d) does%s have that file%s", i+1, info.files_n, peer.ID, *ack ? "" : "n't", NW_LOG_DOTS);
		if(nw_send(&info.connection, ack, sizeof(*ack)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done.\n");

		if(!(*ack)){
			free(ack);
			ack = NULL;
			continue;
		}
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Receiving filename's data (%s) from peer (%d)%s", i+1, info.files_n, info.filename[i], peer.ID, NW_LOG_DOTS);
		if(!(data = nw_recv(&peer.connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Saving filename (%s) locally%s", i+1, info.files_n, info.filename[i], NW_LOG_DOTS);
		if((ret_code = cl_files_saveFile(info.filename[i], data)))
			log_print(0, 0, "Failed (%d): %s", ret_code, cl_error_assess(ret_code));
		else log_print(0, 0, "Done\n");
		*ack = !ret_code;
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Notifying server that the operation %s%s", i+1, info.files_n, *ack ? "succeeded" : "failed", NW_LOG_DOTS);
		if(nw_send(&info.connection, ack, sizeof(*ack)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		if(*ack){
			
			datasize = strlen(data);
			log_print(0, 1, ".\t\t\t\t- [%d/%d] Informing server about filename's (%s) data size (%ld)%s", i+1, info.files_n, info.filename[i], datasize, NW_LOG_DOTS);
			if(nw_send(&info.connection, &datasize, sizeof(datasize)))
				_goto(ERROR, ERR_SENDRECV);
			log_print(0, 0, "Done\n");
			
		}
		
		if(i+1 < info.files_n) log_print(0, 1, ".\n");
		if(ret_code && ret_code != ERR_FEXIST && ret_code != ERR_FOPEN)
			_goto(ERROR, ret_code);
		
		
		free(ack);
		free(data);
		ack = NULL;
		data = NULL;
		
	}
	
	close(peer.connection.socketFD);
	return NULL;
	
	ERROR:
		free(ack);
		free(data);
		if(peer.connection.socketFD)
			close(peer.connection.socketFD);
		pthread_exit(&errno);
	
}


static void* cl_uploadFile_mt(void* argument){

	unsigned *peer_port;
	unsigned long datasize;
	struct in_addr *peer_IP;
	struct client_info peer, info = *(struct client_info*)argument;
	char op_code = UPD, *ack = NULL, ip_string[INET_ADDRSTRLEN];
	memset(&peer, 0, sizeof(struct client_info));


	/* Receives peer's IP and port from server */
	log_print(0, 1, "\t\tUpload operation: %d file%s to peer %d\n", info.files_n, info.files_n > 1 ? "s" : "", info.ID);
	log_print(0, 0, ".\t\t\t- Receiving peer's (%d) IP and port from server%s", info.ID, NW_LOG_DOTS);
	if(!(peer_IP = nw_recv(&info.connection)))
		_goto(ERROR, ERR_SENDRECV);
	if(!(peer_port = nw_recv(&info.connection))){
		free(peer_IP);
		_goto(ERROR, ERR_SENDRECV);
	}
	log_print(0, 0, "Done: %s:%d\n", inet_ntop(AF_INET, peer_IP, ip_string, sizeof(ip_string)), *peer_port);
	
	/* Sets up peer structure */
	peer.connection.addr.sin_addr = *peer_IP;
	peer.connection.port = *peer_port;
	peer.ID = info.ID;
	free(peer_port);
	free(peer_IP);
	
	/* Sets up connection to peer so we upload the files */
	log_print(0, 1, ".\t\t\t- Setting up connection to peer (%d)%s", peer.ID, NW_LOG_DOTS);
	if(nw_setupExtConnection(&peer.connection, peer.connection.port, &peer.connection.addr.sin_addr))
		_goto(ERROR, ERR_CONNECT);
	log_print(0, 0, "Done\n");
	
	log_print(0, 1, ".\t\t\t- Sending operation code (upload) to peer (%d)%s", peer.ID, NW_LOG_DOTS);
	if(nw_send(&peer.connection, &op_code, sizeof(op_code)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	log_print(0, 1, ".\t\t\t- Sending peer (%d) how many files we want to upload (%d)%s", peer.ID, info.files_n, NW_LOG_DOTS);
	if(nw_send(&peer.connection, &info.files_n, sizeof(info.files_n)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n\n");
	
	for(int i = 0; i < info.files_n; i++){
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Sending filename (%s) to server%s", i+1, info.files_n, info.filename[i], NW_LOG_DOTS);
		if(nw_send(&info.connection, info.filename[i], strlen(info.filename[i])))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 1, "Done\n");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Sending filename (%s) to peer (%d)%s", i+1, info.files_n, info.filename[i], peer.ID, NW_LOG_DOTS);
		if(nw_send(&peer.connection, info.filename[i], strlen(info.filename[i])))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Sending filename's data (%s) to peer (%d)%s", i+1, info.files_n, info.filename[i], peer.ID, NW_LOG_DOTS);
		if(nw_send(&peer.connection, info.filedata[i], strlen(info.filedata[i])))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		/* Receives peer's ack in saving the file */
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Receiving peer's (%d) ack%s", i+1, info.files_n, peer.ID, NW_LOG_DOTS);
		if(!(ack = nw_recv(&peer.connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. Peer (%d) %s in saving the file\n", peer.ID, *ack ? "succeeded" : "failed");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Notifying server that the operation %s%s", i+1, info.files_n, *ack ? "succeeded" : "failed", NW_LOG_DOTS);
		if(nw_send(&info.connection, ack, sizeof(*ack)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		if(*ack){
			
			datasize = strlen(info.filedata[i]);
			log_print(0, 1, ".\t\t\t\t- [%d/%d] Informing server about filename's (%s) data size (%ld)%s", i+1, info.files_n, info.filename[i], datasize, NW_LOG_DOTS);
			if(nw_send(&info.connection, &datasize, sizeof(datasize)))
				_goto(ERROR, ERR_SENDRECV);
			log_print(0, 0, "Done\n");
			
		}
		if(i+1 < info.files_n) log_print(0, 1, ".\n");

		free(ack);
		ack = NULL;

	}
	
	close(peer.connection.socketFD);
	return NULL;
	
	ERROR:
		free(ack);
		if(peer.connection.socketFD)
			close(peer.connection.socketFD);
		pthread_exit(&errno);
	
}


static void* cl_deleteFile_mt(void* argument){
	
	unsigned *peer_port;
	struct in_addr *peer_IP;
	struct client_info peer, info = *(struct client_info*)argument;
	char op_code = DEL, *ack = NULL, ip_string[INET_ADDRSTRLEN];
	memset(&peer, 0, sizeof(struct client_info));


	/* Receives peer's IP and port from server */
	log_print(0, 1, "\t\tDelete operation: %d files from peer %d\n", info.files_n, info.ID);
	log_print(0, 0, ".\t\t\t- Receiving peer's (%d) IP and port from server%s", info.ID, NW_LOG_DOTS);
	if(!(peer_IP = nw_recv(&info.connection)))
		_goto(ERROR, ERR_SENDRECV);
	if(!(peer_port = nw_recv(&info.connection))){
		free(peer_IP);
		_goto(ERROR, ERR_SENDRECV);
	}
	log_print(0, 0, "Done: %s:%d\n", inet_ntop(AF_INET, peer_IP, ip_string, sizeof(ip_string)), *peer_port);
	
	/* Sets up peer structure */
	peer.connection.addr.sin_addr = *peer_IP;
	peer.connection.port = *peer_port;
	peer.ID = info.ID;
	free(peer_port);
	free(peer_IP);
	
	/* Sets up connection to peer so we upload the files */
	log_print(0, 1, ".\t\t\t- Setting up connection to peer (%d)%s", peer.ID, NW_LOG_DOTS);
	if(nw_setupExtConnection(&peer.connection, peer.connection.port, &peer.connection.addr.sin_addr))
		_goto(ERROR, ERR_CONNECT);
	log_print(0, 0, "Done\n");
	
	log_print(0, 1, ".\t\t\t- Sending operation code (upload) to peer (%d)%s", peer.ID, NW_LOG_DOTS);
	if(nw_send(&peer.connection, &op_code, sizeof(op_code)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	log_print(0, 1, ".\t\t\t- Sending peer (%d) how many files we want to upload (%d)%s", peer.ID, info.files_n, NW_LOG_DOTS);
	if(nw_send(&peer.connection, &info.files_n, sizeof(info.files_n)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n\n");
	
	for(int i = 0; i < info.files_n; i++){
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Sending filename (%s) to server%s", i+1, info.files_n, info.filename[i], NW_LOG_DOTS);
		if(nw_send(&info.connection, info.filename[i], strlen(info.filename[i])))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 1, "Done\n");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Sending filename (%s) to peer (%d)%s", i+1, info.files_n, info.filename[i], peer.ID, NW_LOG_DOTS);
		if(nw_send(&peer.connection, info.filename[i], strlen(info.filename[i])))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Receiving peer's (%d) found file ack%s", i+1, info.files_n, peer.ID, NW_LOG_DOTS);
		if(!(ack = nw_recv(&peer.connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. Peer (%d) %s\n", peer.ID, *ack ? "has that file" : "doesn't have that file anymore");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Notifying server that peer (%d) does%s have that file%s", i+1, info.files_n, peer.ID, *ack ? "" : "n't", NW_LOG_DOTS);
		if(nw_send(&info.connection, ack, sizeof(*ack)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done.\n");
		
		if(!(*ack)){
			free(ack);
			ack = NULL;
			continue;
		}
		free(ack);
		ack = NULL;
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Receiving peer's (%d) delete file ack%s", i+1, info.files_n, peer.ID, NW_LOG_DOTS);
		if(!(ack = nw_recv(&peer.connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. Peer (%d) %s in deleting the file\n", peer.ID, *ack ? "succeeded" : "failed");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Notifying server that the operation %s%s", i+1, info.files_n, *ack ? "succeeded" : "failed", NW_LOG_DOTS);
		if(nw_send(&info.connection, ack, sizeof(*ack)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		if(i+1 < info.files_n) log_print(0, 1, ".\n");
		
		free(ack);
		ack = NULL;
		
	}

	close(peer.connection.socketFD);
	return NULL;
	
	ERROR:
		free(ack);
		if(peer.connection.socketFD)
			close(peer.connection.socketFD);
		pthread_exit(&errno);
	
}


char cl_showStatus(struct in_addr* IP, int port){
	
	char *status = cl_getStatus(IP, port);
	
	if(!status) return errno;
	
	cl_menu_status(status);
	
	return 0;
	
}


char cl_showList(struct in_addr* IP, int port){
	
	int list_size;
	struct client_info *list = cl_getList(IP, port, &list_size);
	
	
	if(!list) return errno;
	
	if(cl_menu_list(list, list_size))
		return errno = ERR_UNKNOWN;
	
	for(int i = 0; i < list_size; i++)
		nw_freeInfo(&list[i]);
	
	return 0;
	
}


char cl_exitPool(struct in_addr* IP, int port){
	
	char op_code = EXIT;
	struct socket_con server;
	
	log_print(0, 0, "Exit operation\n.\t- Setting up connection to server%s", NW_LOG_DOTS);
	if(nw_setupExtConnection(&server, port, IP))
		return errno = ERR_CONNECT;
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Sending operation code to server%s", NW_LOG_DOTS);
	if(nw_send(&server, &op_code, sizeof(op_code)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Sending our ID (%d) to server%s", CL_DEFAULT_ID, NW_LOG_DOTS);
	if(nw_send(&server, &CL_DEFAULT_ID, sizeof(CL_DEFAULT_ID)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\nDone exiting operation.\n\n");
	
	return 0;
	
	ERROR:
		close(server.socketFD);
		log_print(0, 0, "Error (%d): %s\n\n", errno, cl_error_assess(errno));
		return errno;
	
}


static char* cl_getStatus(struct in_addr* IP, int port){
	
	int *string_n = NULL;
	struct socket_con server;
	char op_code = STS, *infostring = NULL, *status = calloc(1, sizeof(char));
	
	
	log_print(0, 0, "List operation\n.\t- Setting up connection to server%s", NW_LOG_DOTS);
	if(nw_setupExtConnection(&server, port, IP))
		_goto(ERROR, ERR_CONNECT);
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Sending operation code%s", NW_LOG_DOTS);
	if(nw_send(&server, &op_code, sizeof(op_code)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Sending our ID (%d) to server%s", CL_DEFAULT_ID, NW_LOG_DOTS);
	if(nw_send(&server, &CL_DEFAULT_ID, sizeof(CL_DEFAULT_ID)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Receiving how many info strings we'll get%s", NW_LOG_DOTS);
	if(!(string_n = nw_recv(&server)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done. %d info strings\n", *string_n);
	
	for(int i = 1; i <= *string_n; i++){
		
		log_print(0, 0, ".\t\t- [%d/%d] Receiving info string%s", i, *string_n, NW_LOG_DOTS);
		if(!(infostring = nw_recv(&server)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		status = realloc(status, (strlen(status)+strlen(infostring)+2)*sizeof(char));
		strcat(status, infostring);
		strcat(status, "\n");
		
		free(infostring);
		infostring = NULL;
		
	}
	log_print(0, 0, "Done listing operation.\n\n");
	
	free(string_n);
	close(server.socketFD);
	return status;
	
	ERROR:
		free(status);
		free(string_n);
		free(infostring);
		if(server.socketFD)
			close(server.socketFD);
		return NULL;
	
}


struct client_info* cl_getList(struct in_addr* IP, int port, int* size){
	
	char op_code = LST;
	struct socket_con server;
	struct client_info *list = NULL;
	int *IDs_c = NULL, *files_n = NULL, *aux;
	
	
	log_print(0, 0, "List operation\n.\t- Setting up connection to server%s", NW_LOG_DOTS);
	if(nw_setupExtConnection(&server, port, IP)){
		errno = ERR_CONNECT;
		return NULL;
	}
	log_print(0, 0, "Done\n");
	
	log_print(0, 0, ".\t- Sending operation code%s", NW_LOG_DOTS);
	if(nw_send(&server, &op_code, sizeof(op_code)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");

	/* Receives how many clients (IDs) there are */
	log_print(0, 0, ".\t- Receiving how many clients there are%s", NW_LOG_DOTS);
	if(!(IDs_c = nw_recv(&server)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done. %d clients\n", *IDs_c);
	
	list = calloc(*IDs_c, sizeof(struct client_info));
	for(int i = 0; i < *IDs_c; i++){
		
		/* Receives ID number */
		log_print(0, 0, ".\t- Client %d:\n.\t\t- Receiving their ID%s", i+1, NW_LOG_DOTS);
		if(!(aux = nw_recv(&server)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. ID = %d\n", *aux);
		
		list[i].ID = *aux;
		free(aux);
		
		/* Receives how many files there are */
		log_print(0, 0, ".\t\t- Receiving how many files \"%d\" has%s", list[i].ID, NW_LOG_DOTS);
		if(!(files_n = nw_recv(&server)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. \"%d\" has %d files:\n", list[i].ID, *files_n);
		
		list[i].filename = calloc(*files_n, sizeof(char*));
		/* though we don't use it now, it is needed so it can be properly freed afterwards */
		list[i].filedata = calloc(*files_n, sizeof(char*));
		
		/* Receives file names */
		for(int j = 0; j < *files_n; j++){
			log_print(0, 0, ".\t\t\tFile %d: ", j+1);
			if(!(list[i].filename[j] = nw_recv(&server)))
				_goto(ERROR, ERR_SENDRECV);
			log_print(0, 0, "%s\n", list[i].filename[j]);
			list[i].files_n++;
		}

		free(files_n);
		
		files_n = NULL;

	}
	log_print(0, 0, "Done listing operation.\n\n");

	close(server.socketFD);
	*size = *IDs_c;
	free(IDs_c);
	return list;
	
	ERROR:
		log_print(0, 0, "Error (%d): %s\n\n", errno, cl_error_assess(errno));
		for(int i = (IDs_c ? *IDs_c-1 : -1); i >= 0 ; i--)
			nw_freeInfo(&list[i]);
		close(server.socketFD);
		free(files_n);
		free(IDs_c);
		free(list);
		return NULL;
	
}