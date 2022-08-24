#include "server_utils.h"


/* Simple linked list. Must be destroyed later */
struct singly_node node = SINGLY_NODE_INITIALIZER;

/* */
struct {
	
	unsigned long long files_bytes;
	unsigned long files_downloaded;
	unsigned long files_uploaded;
	unsigned long files_deleted;
	
	unsigned client_n;
	struct singly_node* db;
	
} sv_database = {0, 0, 0, 0, 0, &node};


static void* sv_handleRequest_mt(void* argument);
static void* sv_initClient_mt(void* argument);
static char  sv_downloadFile(struct client_info* sourceClient, struct client_info* targetClient);
static char  sv_uploadFile(struct client_info* sourceClient, struct client_info* targetClient);
static char  sv_deleteFile(struct client_info* sourceClient, struct client_info* targetClient);
static void* sv_status_mt(void* argument);
static void* sv_sendList_mt(void* argument);
static void* sv_removeClient_mt(void* argument);


char sv_queryOperation(char op_code, struct client_info* client, pthread_t* thread){
	
	char ret_code = 0;
	static void* (*sv_query)(void*);


	switch(op_code){
		
		case INI:
			sv_query = sv_initClient_mt;
			break;
			
		case DLD:
		case UPD:
		case DEL:
			sv_query = sv_handleRequest_mt;
			break;
			
		case STS:
			sv_query = sv_status_mt;
			break;
			
		case LST:
			sv_query = sv_sendList_mt;
			break;
			
		case EXIT:
			sv_query = sv_removeClient_mt;
			break;
			
		default:
			close(client->connection.socketFD);
			return errno = ret_code;
		
	}
	
	/* Since port member is not used inside any of
	   the above functions, we use it as a workaround
	   for download, upload and delete queries, so we
	   don't need to create another structure just for
	   multithreading argument */
	client->connection.port = op_code;
	
	if(pthread_create(thread, NULL, sv_query, client))
		return errno = ERR_UNKNOWN;
	
	return 0;
	
}


char sv_getQuery(struct socket_con* server, struct client_info* client){
	
	int *ID = NULL;
	char query, *op_code = NULL, ip_string[INET_ADDRSTRLEN];
	
	
	/* Accepts client's connection */
	log_print(0, 0, "Server is set up and waiting for requests%s", NW_LOG_DOTS);
	if(nw_acceptConnection(server, &client->connection)){
		if(errno == EINTR) return 0;
		else _goto(ERROR, ERR_CONNECT);
	}
	log_print(0, 0, "Received a request (%s:%d)\n\n", inet_ntop(AF_INET, &client->connection.addr.sin_addr, ip_string, sizeof(ip_string)), ntohs(client->connection.addr.sin_port));
	
	/* Receives query type (refer to network_utils.h's enum) */
	log_print(0, 0, "Receiving operation code%s", NW_LOG_DOTS);
	if(!(op_code = nw_recv(&client->connection)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done. Code = %d\n", *op_code);
	query = *op_code;
	free(op_code);
	
	/* Only INI and LST queries don't need client's ID */
	if(query != INI && query != LST){
		log_print(0, 0, "Receiving client's ID%s", NW_LOG_DOTS);
		if(!(ID = nw_recv(&client->connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. ID = %d\n", *ID);
		client->ID = *ID;
		free(ID);
	}

	return query;
	
	ERROR:
		log_print(0, 0, "Error (%d): %s\n\n", errno, sv_error_assess(errno));
		return -2;
	
}


static void* sv_handleRequest_mt(void* argument){
	
	struct singly_node *node;
	int *ID = NULL, *files_n = NULL;
	static char ret_code, (*sv_query)(struct client_info*, struct client_info*);
	struct client_info *targetClient, sourceClient = *(struct client_info*)argument;
	
	
	/* Thread is working, so we prevent system from shutting down */
	pthread_mutex_lock(&nwThreadLock);
	svThreadCounterCond++;
	pthread_mutex_unlock(&nwThreadLock);
	
	/* Receives target's ID from which source wants to work on */
	log_print(0, 1, "%s operation\n", (sourceClient.connection.port == DLD ? "Download" : sourceClient.connection.port == UPD ? "Upload" : "Delete"));
	log_print(0, 1, ".\t- Receiving target client ID source ID (%d) wants to work on%s", sourceClient.ID, NW_LOG_DOTS);
	if(!(ID = nw_recv(&sourceClient.connection)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done. Target ID = %d\n", *ID);
	
	/* Searches database for that ID */
	pthread_mutex_lock(&nwThreadLock);
	log_print(0, 1, ".\t- Searching internal database for target client ID (%d)%s", *ID, NW_LOG_DOTS);
	if(!(node = singly_pick(sv_database.db, singly_search(sv_database.db, *ID)))){
		pthread_mutex_unlock(&nwThreadLock);
		_goto(ERROR, ERR_CNFOUND);
	}
	log_print(0, 0, "Found\n");
	pthread_mutex_unlock(&nwThreadLock);
	free(ID);
	ID = NULL;
	
	/* Found */
	targetClient = (struct client_info*)node->data;
	
	/* Receives how many files source wants to/from target */
	log_print(0, 1, ".\t- Receiving how many files source client (%d) wants to/from target client (%d)%s", sourceClient.ID, targetClient->ID, NW_LOG_DOTS);
	if(!(files_n = nw_recv(&sourceClient.connection)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done. %d file%c\n", *files_n, *files_n > 1 ? 's' : ' ');
	sourceClient.files_n = *files_n;
	free(files_n);
	
	/* Workaround. Source client's port now holds op_code */
	switch(sourceClient.connection.port){
		
		case DLD:
			sv_query = sv_downloadFile;
			break;
			
		case UPD:
			sv_query = sv_uploadFile;
			break;
			
		case DEL:
			sv_query = sv_deleteFile;
			break;
			
		default:
			_goto(ERROR, ERR_UNKNOWN);
			break;
		
	}
	
	if((ret_code = (*sv_query)(&sourceClient, targetClient))){
		printf("%s", sv_error_assess(ret_code));
		log_print(0, 1, "Error (%d): %s\n\n", ret_code, sv_error_assess(ret_code));
	} else log_print(0, 1, "Done %s operation.\n\n", (sourceClient.connection.port == DLD ? "downloading" : sourceClient.connection.port == UPD ? "uploading" : "deleting"));
	close(sourceClient.connection.socketFD);
	
	pthread_mutex_lock(&nwThreadLock);
	
	svThreadCounterCond--;
	pthread_cond_signal(&nwThreadCond);
	
	pthread_mutex_unlock(&nwThreadLock);
	
	return NULL;
	
	ERROR:
		free(ID);
		close(sourceClient.connection.socketFD);
		pthread_mutex_lock(&nwThreadLock);
		
		svThreadCounterCond--;
		pthread_cond_signal(&nwThreadCond);
		printf("Error (%d): %s\n\n", errno, sv_error_assess(errno));
		
		pthread_mutex_unlock(&nwThreadLock);
		return NULL;
	
}


static void* sv_initClient_mt(void* argument){
	
	int i = 0, *files_n;
	struct client_info *cinfo = NULL, client = *(struct client_info*)argument;


	pthread_mutex_lock(&nwThreadLock);
	svThreadCounterCond++;
	pthread_mutex_unlock(&nwThreadLock);

	log_print(0, 1, "Initializing operation\n");
	log_print(0, 1, ".\t- Receiving how many files this client has%s", NW_LOG_DOTS);
	if(!(files_n = nw_recv(&client.connection)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done. %d file%c\n", *files_n, *files_n > 1 ? 's' : ' ');
	
	cinfo = calloc(1, sizeof(struct client_info));
	cinfo->filename = calloc(*files_n, sizeof(char*));
	cinfo->filedata = calloc(*files_n, sizeof(char*));
	
	pthread_mutex_lock(&nwThreadLock);
	for(i = 1; singly_search(sv_database.db, i) != -1; i++);
	cinfo->ID = i;
	sv_database.client_n++;
	pthread_mutex_unlock(&nwThreadLock);
	
	log_print(0, 1, ".\t- Getting %s%s\n", *files_n > 1 ? "those files" : "that file", NW_LOG_DOTS);
	for(i = 0; i < *files_n; i++){
		
		log_print(0, 1, ".\t\t- File [%d/%d]: ", i+1, *files_n);
		if(!(cinfo->filename[i] = nw_recv(&client.connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "%s\n", cinfo->filename[i]);
		
		cinfo->filedata[i] = calloc(1, sizeof(char));
		pthread_mutex_lock(&nwThreadLock);
		sv_database.files_bytes += strlen(cinfo->filename[i]);
		pthread_mutex_unlock(&nwThreadLock);
		
	}
	cinfo->connection.port = (cinfo->ID+NW_DEFAULT_STARTING_CLIENT_PORT-1)%NW_PORT_MAX_RANGE;
	cinfo->connection.addr = client.connection.addr;
	cinfo->files_n = *files_n;
	free(files_n);
	
	log_print(0, 1, ".\t- Sending client their ID (%d)%s", cinfo->ID, NW_LOG_DOTS);
	if(nw_send(&client.connection, &cinfo->ID, sizeof(cinfo->ID)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	log_print(0, 1, ".\t- Sending client (%d) its port to list on (%d)%s", cinfo->ID, cinfo->connection.port, NW_LOG_DOTS);
	if(nw_send(&client.connection, &cinfo->connection.port, sizeof(cinfo->connection.port)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	log_print(0, 1, "Done initializing operation.\n\n");
	
	close(client.connection.socketFD);
	pthread_mutex_lock(&nwThreadLock);
	
	singly_orderedInsert(sv_database.db, cinfo, cinfo->ID);
	svThreadCounterCond--;
	pthread_cond_signal(&nwThreadCond);
	
	pthread_mutex_unlock(&nwThreadLock);
	
	return NULL;
	
	ERROR:
		log_print(0, 0, "Error (%d): %s\n\n", errno, sv_error_assess(errno));
		for(int j = 0; j < i; j++){
			free(cinfo->filename[j]);
			free(cinfo->filedata[j]);
		}
		free(cinfo->filename);
		free(cinfo);
		free(files_n);
		close(client.connection.socketFD);
		
		pthread_mutex_lock(&nwThreadLock);
		
		svThreadCounterCond--;
		pthread_cond_signal(&nwThreadCond);
		
		pthread_mutex_unlock(&nwThreadLock);
		return NULL;
	
}


static char sv_downloadFile(struct client_info* sourceClient, struct client_info* targetClient){
	
	struct client_info *scinfo;
	unsigned long *datasize = NULL;
	char ip_string[INET_ADDRSTRLEN];
	char *ack = NULL, *filename = NULL, *filedata = NULL;
	
	
	/* Sends target client's IP and port to source client */
	log_print(0, 1, ".\t- Sending target client's (%d) IP and port (%s:%d) to source client (%d)%s", targetClient->ID, inet_ntop(AF_INET, &targetClient->connection.addr.sin_addr, ip_string, sizeof(ip_string)), targetClient->connection.port, sourceClient->ID, NW_LOG_DOTS);
	if(nw_send(&sourceClient->connection, &targetClient->connection.addr.sin_addr, sizeof(targetClient->connection.addr.sin_addr)))
		_goto(ERROR, ERR_SENDRECV);
	if(nw_send(&sourceClient->connection, &targetClient->connection.port, sizeof(targetClient->connection.port)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done.\n");
	
	for(int i = 0, j; i < sourceClient->files_n; i++){
		
		/* Receives the filename the client wants to download */
		log_print(0, 1, ".\t\t- [%d/%d] Receiving filename source client (%d) wants to download%s", i+1, sourceClient->files_n, sourceClient->ID, NW_LOG_DOTS);
		if(!(filename = nw_recv(&sourceClient->connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. File to download: %s\n", filename);

		pthread_mutex_lock(&nwThreadLock);
		sv_database.files_bytes += strlen(filename);
		pthread_mutex_unlock(&nwThreadLock);

		/* Receives source client's ack that target client does indeed have that file */
		log_print(0, 1, ".\t\t- [%d/%d] Receiving target client's (%d) found file ack, from source client (%d)%s", i+1, sourceClient->files_n, targetClient->ID, sourceClient->ID, NW_LOG_DOTS);
		if(!(ack = nw_recv(&sourceClient->connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. Target client (%d) does%s\n", targetClient->ID, *ack ? " have that file" : "n't that file anymore, so we update our database");

		/* Updates database, if target client doesn't have the file anymore */
		if(!(*ack)){
			
			for(j = 0; j < targetClient->files_n; j++)
				if(!strncmp(targetClient->filename[j], filename, strlen(filename)))
					break;

			free(targetClient->filename[j]);
			free(targetClient->filedata[j]);
			
			for(int k = j, l = k+1; l < targetClient->files_n; k++, l++){
				targetClient->filename[k] = targetClient->filename[l];
				targetClient->filedata[k] = targetClient->filedata[l];
			}
			
			targetClient->files_n--;
			targetClient->filename = realloc(targetClient->filename, (targetClient->files_n)*sizeof(char*));
			targetClient->filedata = realloc(targetClient->filedata, (targetClient->files_n)*sizeof(char*));
			
			free(filename);
			free(ack);
			
			filename = NULL;
			ack = NULL;
			continue;
			
		}
		
		/* Receives source client's ack in saving the file */
		log_print(0, 1, ".\t\t- [%d/%d] Receiving source client's (%d) ack%s", i+1, sourceClient->files_n, sourceClient->ID, NW_LOG_DOTS);
		if(!(ack = nw_recv(&sourceClient->connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. Source client (%d) %s in saving the file, so we %s our database\n", sourceClient->ID, *ack ? "succeeded" : "failed", *ack ? "update" : "don't update");
		
		/* Updates source's file count and filenames held */
		if(*ack){
			
			pthread_mutex_lock(&nwThreadLock);
			scinfo = (struct client_info*)(singly_pick(sv_database.db, singly_search(sv_database.db, sourceClient->ID))->data);
			pthread_mutex_unlock(&nwThreadLock);
			
			scinfo->filename = realloc(scinfo->filename, (scinfo->files_n+1)*sizeof(char*));
			scinfo->filedata = realloc(scinfo->filedata, (scinfo->files_n+1)*sizeof(char*));
			scinfo->filename[scinfo->files_n] = strcpy(calloc(strlen(filename)+1, sizeof(char)), filename);
			scinfo->filedata[scinfo->files_n] = calloc(1, sizeof(char));
			scinfo->files_n++;
			
			/* Receive how many bytes filedata is, from source client */
			log_print(0, 1, ".\t\t- [%d/%d] Receiving file data size from source client (%d)%s", i+1, sourceClient->files_n, sourceClient->ID, NW_LOG_DOTS);
			if(!(datasize = nw_recv(&sourceClient->connection)))
				_goto(ERROR, ERR_SENDRECV);
			log_print(0, 0, "Done. Data size = %ld\n", *datasize);
			
			pthread_mutex_lock(&nwThreadLock);
			sv_database.files_downloaded++;
			sv_database.files_bytes += *datasize;
			pthread_mutex_unlock(&nwThreadLock);
			
		}
		
		free(ack);
		free(datasize);
		free(filename);
		free(filedata);
		
		ack = NULL;
		datasize = NULL;
		filename = NULL;
		filedata = NULL;
		
	}
	
	return 0;
	
	ERROR:
		free(ack);
		free(datasize);
		free(filename);
		free(filedata);
		return errno;
	
}


static char sv_uploadFile(struct client_info* sourceClient, struct client_info* targetClient){
	
	unsigned long *datasize = NULL;
	char ip_string[INET_ADDRSTRLEN];
	char *ack = NULL, *filename = NULL;
	
	
	/* Sends target client's IP and port to source client */
	log_print(0, 1, ".\t- Sending target client's (%d) IP and port (%s:%d) to source client (%d)%s", targetClient->ID, inet_ntop(AF_INET, &targetClient->connection.addr.sin_addr, ip_string, sizeof(ip_string)), targetClient->connection.port, sourceClient->ID, NW_LOG_DOTS);
	if(nw_send(&sourceClient->connection, &targetClient->connection.addr.sin_addr, sizeof(targetClient->connection.addr.sin_addr)))
		_goto(ERROR, ERR_SENDRECV);
	if(nw_send(&sourceClient->connection, &targetClient->connection.port, sizeof(targetClient->connection.port)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done.\n");
	
	for(int i = 0; i < sourceClient->files_n; i++){
		
		log_print(0, 1, ".\t\t- [%d/%d] Receiving filename source client (%d) wants to upload%s", i+1, sourceClient->files_n, sourceClient->ID, NW_LOG_DOTS);
		if(!(filename = nw_recv(&sourceClient->connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. File to upload: %s\n", filename);
		
		pthread_mutex_lock(&nwThreadLock);
		sv_database.files_bytes += strlen(filename);
		pthread_mutex_unlock(&nwThreadLock);
		
		log_print(0, 1, ".\t\t- [%d/%d] Receiving target client's (%d) save file ack, from source client (%d)%s", i+1, sourceClient->files_n, targetClient->ID, sourceClient->ID, NW_LOG_DOTS);
		if(!(ack = nw_recv(&sourceClient->connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. Target client (%d) %s in saving the file, so we %s our database\n", targetClient->ID, *ack ? "succeeded" : "failed", *ack ? "update" : "don't update");
		
		/* Updates target's file count e filenames held */
		if(*ack){
			
			targetClient->filename = realloc(targetClient->filename, (targetClient->files_n+1)*sizeof(char*));
			targetClient->filedata = realloc(targetClient->filedata, (targetClient->files_n+1)*sizeof(char*));
			targetClient->filename[targetClient->files_n] = strcpy(calloc(strlen(filename)+1, sizeof(char)), filename);
			targetClient->filedata[targetClient->files_n] = calloc(1, sizeof(char));
			targetClient->files_n++;
			
			log_print(0, 1, ".\t\t- [%d/%d] Receiving file data size from source client (%d)%s", i+1, sourceClient->files_n, sourceClient->ID, NW_LOG_DOTS);
			if(!(datasize = nw_recv(&sourceClient->connection)))
				_goto(ERROR, ERR_SENDRECV);
			log_print(0, 0, "Done. Data size = %ld\n", *datasize);
			
			pthread_mutex_lock(&nwThreadLock);
			sv_database.files_uploaded++;
			sv_database.files_bytes += *datasize;
			pthread_mutex_unlock(&nwThreadLock);
			
		}
		
		free(ack);
		free(filename);
		free(datasize);
		
		ack = NULL;
		filename = NULL;
		datasize = NULL;
		
	}
	
	return 0;
	
	ERROR:
		free(ack);
		free(filename);
		free(datasize);
		return errno;
	
}


static char sv_deleteFile(struct client_info* sourceClient, struct client_info* targetClient){
	
	char ip_string[INET_ADDRSTRLEN];
	char *ack = NULL, *flag_ffound = NULL, *filename = NULL;
	
	
	/* Sends target client's IP and port to source client */
	log_print(0, 1, ".\t- Sending target client's (%d) IP and port (%s:%d) to source client (%d)%s", targetClient->ID, inet_ntop(AF_INET, &targetClient->connection.addr.sin_addr, ip_string, sizeof(ip_string)), targetClient->connection.port, sourceClient->ID, NW_LOG_DOTS);
	if(nw_send(&sourceClient->connection, &targetClient->connection.addr.sin_addr, sizeof(targetClient->connection.addr.sin_addr)))
		_goto(ERROR, ERR_SENDRECV);
	if(nw_send(&sourceClient->connection, &targetClient->connection.port, sizeof(targetClient->connection.port)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done.\n");
	
	for(int i = 0; i < sourceClient->files_n; i++){
		
		log_print(0, 1, ".\t\t- [%d/%d] Receiving filename source client (%d) wants to delete on target client (%d)%s", i+1, sourceClient->files_n, sourceClient->ID, targetClient->ID, NW_LOG_DOTS);
		if(!(filename = nw_recv(&sourceClient->connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. File to delete: %s\n", filename);
		
		pthread_mutex_lock(&nwThreadLock);
		sv_database.files_bytes += strlen(filename);
		pthread_mutex_unlock(&nwThreadLock);

		/* Receives source client's ack that target client does indeed have that file */
		log_print(0, 1, ".\t\t- [%d/%d] Receiving target client's (%d) found file ack, from source client (%d)%s", i+1, sourceClient->files_n, targetClient->ID, sourceClient->ID, NW_LOG_DOTS);
		if(!(flag_ffound = nw_recv(&sourceClient->connection)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. Target client (%d) does%s\n", targetClient->ID, *flag_ffound ? " have that file" : "n't that file anymore, so we update our database");

		if(*flag_ffound){
			
			log_print(0, 1, ".\t\t\t- [%d/%d] Receiving target client's (%d) delete ack, from source client (%d)%s", i+1, sourceClient->files_n, targetClient->ID, sourceClient->ID, NW_LOG_DOTS);
			if(!(ack = nw_recv(&sourceClient->connection)))
				_goto(ERROR, ERR_SENDRECV);
			log_print(0, 0, "Done. Target client (%d) %s in deleting the file, so we %s our database\n", targetClient->ID, *ack ? "succeeded" : "failed", *ack ? "update" : "don't update");
			free(flag_ffound);
			flag_ffound = ack;
			
		} else ack = flag_ffound;

		/* Updates target's file count e filenames held */
		if(*ack || !(*flag_ffound)){
			
			for(int k = 0; k < targetClient->files_n; k++)
				if(!strncmp(filename, targetClient->filename[k], strlen(filename))){

					/* Frees target's deleted filename */
					free(targetClient->filename[k]);
					free(targetClient->filedata[k]);
					
					/* Rearranges array, so there's no gap and it's still in crescent order */
					for(int l = k+1; l < targetClient->files_n; k++, l++){
						targetClient->filename[k] = targetClient->filename[l];
						targetClient->filedata[k] = targetClient->filedata[l];
					}
					
					break;
					
				}
			
			/* Shrinks both arrays and decrement target's file count */
			targetClient->files_n--;
			targetClient->filename = realloc(targetClient->filename, (targetClient->files_n)*sizeof(char*));
			targetClient->filedata = realloc(targetClient->filedata, (targetClient->files_n)*sizeof(char*));
			
		}
		
		free(ack);
		free(filename);
		
		ack = NULL;
		filename = NULL;
		
		pthread_mutex_lock(&nwThreadLock);
		sv_database.files_deleted++;
		pthread_mutex_unlock(&nwThreadLock);
		
	}
	
	return 0;
	
	ERROR:
		free(ack);
		free(filename);
		return errno;
	
}


static void* sv_status_mt(void* argument){
	
	int infostring_n = 5;
	char infobuffer[SV_BUFFER_MAXSIZE];
	struct client_info client = *(struct client_info*)argument;
	
	const char* message[] = {"Bytes transferidos", "Arquivos baixados",
							 "Arquivos enviados",  "Arquivos deletados",
							 "Clientes online"};
	
	pthread_mutex_lock(&nwThreadLock);
	svThreadCounterCond++;
	unsigned long long messagevalue[] = {sv_database.files_bytes, sv_database.files_downloaded,
										 sv_database.files_uploaded, sv_database.files_deleted,
										 sv_database.client_n};
	pthread_mutex_unlock(&nwThreadLock);
	
	
	log_print(0, 1, "Status operation\n");
	log_print(0, 1, ".\t- Sending how many info strings (%d) we have to client (%d)%s", infostring_n, client.ID, NW_LOG_DOTS);
	if(nw_send(&client.connection, &infostring_n, sizeof(infostring_n)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done\n");
	
	for(int i = 0; i < infostring_n; i++){
		
		snprintf(infobuffer, SV_BUFFER_MAXSIZE, "%s: %llu", message[i], messagevalue[i]);
		log_print(0, 1, ".\t\t- [%d/%d] Sending info string to client (%d)%s", i+1, infostring_n, client.ID, NW_LOG_DOTS);
		if(nw_send(&client.connection, infobuffer, strlen(infobuffer)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
	}
	
	close(client.connection.socketFD);
	pthread_mutex_lock(&nwThreadLock);
	
	svThreadCounterCond--;
	pthread_cond_signal(&nwThreadCond);
	
	pthread_mutex_unlock(&nwThreadLock);
	
	return NULL;
	
	ERROR:
		printf("Error (%d): %s\n\n", errno, sv_error_assess(errno));
	
		pthread_mutex_lock(&nwThreadLock);
		svThreadCounterCond--;
		pthread_mutex_unlock(&nwThreadLock);
		
		return NULL;
	
}


static void* sv_sendList_mt(void* argument){
	
	struct client_info cinfo, client = *(struct client_info*)argument;

	pthread_mutex_lock(&nwThreadLock);
	svThreadCounterCond++;

	log_print(0, 1, "List operation\n");
	log_print(0, 1, ".\t- Sending how many clients the server has at the moment%s", NW_LOG_DOTS);
	if(nw_send(&client.connection, &sv_database.client_n, sizeof(sv_database.client_n)))
		_goto(ERROR, ERR_SENDRECV);
	log_print(0, 0, "Done. %d client%c\n", sv_database.client_n, sv_database.client_n > 1 ? 's' : ' ');
	
	for(unsigned i = 1; i <= sv_database.client_n; i++){
		
		cinfo = *(struct client_info*)(singly_pick(sv_database.db, i)->data);
		
		log_print(0, 1, ".\t- Sending ID (%d) [%d/%d]%s", cinfo.ID, i, sv_database.client_n, NW_LOG_DOTS);
		if(nw_send(&client.connection, &cinfo.ID, sizeof(cinfo.ID)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		log_print(0, 1, ".\t- Sending how many files ID (%d) has (%d) [%d/%d]%s", cinfo.ID, cinfo.files_n, i, sv_database.client_n, NW_LOG_DOTS);
		if(nw_send(&client.connection, &cinfo.files_n, sizeof(cinfo.files_n)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		for(int j = 0; j < cinfo.files_n; j++){
			
			log_print(0, 1, ".\t\t- Sending \"%d\"'s filename [%d/%d] (%s)%s", cinfo.ID, j+1, cinfo.files_n, cinfo.filename[j], NW_LOG_DOTS);
			if(nw_send(&client.connection, cinfo.filename[j], strlen(cinfo.filename[j])))
				_goto(ERROR, ERR_SENDRECV);
			log_print(0, 0, "Done\n");
			
		}
		
		if(i+1 <= sv_database.client_n) log_print(0, 1, ".\t\t- Done\n\n");
		else log_print(0, 1, ".\t\t- Done\n");
		
	}
	log_print(0, 1, "Done listing operation.\n\n");
	
	svThreadCounterCond--;
	close(client.connection.socketFD);
	pthread_cond_signal(&nwThreadCond);
	
	pthread_mutex_unlock(&nwThreadLock);
	
	return NULL;
	
	ERROR:
		log_print(0, 1, "Error (%d): %s\n\n", errno, sv_error_assess(errno));
		svThreadCounterCond--;
		close(client.connection.socketFD);
		pthread_cond_signal(&nwThreadCond);
		
		pthread_mutex_unlock(&nwThreadLock);
		
		return NULL;
	
}


static void* sv_removeClient_mt(void* argument){
	
	errno = 0;
	struct singly_node *node;
	struct client_info *scinfo, client = *(struct client_info*)argument;
	
	pthread_mutex_lock(&nwThreadLock);
	svThreadCounterCond++;
	
	log_print(0, 1, "Remove operation\n");
	log_print(0, 1, ".\t- Searching database for client ID (%d)%s", client.ID, NW_LOG_DOTS);
	if(!(node = singly_pick(sv_database.db, singly_search(sv_database.db, client.ID))))
		_goto(FINISH, ERR_CNFOUND);
	scinfo = (struct client_info*)(node->data);
	log_print(0, 0, "Found\n");
	
	log_print(0, 1, ".\t- Cleaning up client %d (closing connection, freeing stuff, removing from database)%s\n", client.ID, NW_LOG_DOTS);
	nw_freeInfo(scinfo);
	free(scinfo);
	
	singly_delete(sv_database.db, singly_search(sv_database.db, client.ID));
	log_print(0, 1, "Done removing client.\n\n");
	
	FINISH:;
	
	if(!errno) sv_database.client_n--;
	else printf("Error (%d): %s\n\n", errno, sv_error_assess(errno));
	svThreadCounterCond--;
	close(client.connection.socketFD);
	pthread_cond_signal(&nwThreadCond);
	
	pthread_mutex_unlock(&nwThreadLock);
	
	return NULL;
	
}