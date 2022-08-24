#include "client_utils.h"


/* */
int clThreadCounterCond = 0;

/* */
extern unsigned CL_DEFAULT_CLIENT_PORT;


static void* cl_listen_handleRequest(void* argument);
static char  cl_listen_getQuery(struct socket_con* peer);
static char  cl_listen_downloadFile(struct socket_con* peer);
static char  cl_listen_uploadFile(struct socket_con* peer);
static char  cl_listen_deleteFile(struct socket_con* peer);
static void  cl_listen_mt_cleaner(void* listener);


void* cl_listen(void* dummy){
	
	int oldstate;
	char ip_string[INET_ADDRSTRLEN];
	pthread_t thread[NW_MAX_THREADS];
	struct socket_con client, peer[NW_MAX_THREADS];


	log_print(0, 1, "Setting up client listener%s", NW_LOG_DOTS);
	if(nw_setupListener(&client, CL_DEFAULT_CLIENT_PORT, NULL))
		return NULL;
	log_print(0, 0, "Done. Using port %d\n", CL_DEFAULT_CLIENT_PORT);
	
	pthread_cleanup_push(cl_listen_mt_cleaner, &client);
	
	for(int i = 0; ; i = (i+1)%NW_MAX_THREADS){

		log_print(1, 1, "Client listener is waiting for requests on port %d\n", CL_DEFAULT_CLIENT_PORT);
		if(nw_acceptConnection(&client, &peer[i])){
			printf("%s", cl_error_assess(ERR_CONNECT));
			i--;
			continue;
		}
		log_print(1, 1, "Client listener accepted a connection (%s:%d)\n", inet_ntop(AF_INET, &peer[i].addr.sin_addr, ip_string, sizeof(ip_string)), ntohs(peer[i].addr.sin_port));

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);

		log_print(0, 1, "\t- Dispatching thread to handle that request\n");
		if(pthread_create(&thread[i], NULL, cl_listen_handleRequest, &peer[i]))
			printf("%s", cl_error_assess(ERR_UNKNOWN));
		sleep(1); //Just to print everything nicely

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);

	}
	
	pthread_cleanup_pop(1);
	return NULL;
	
}


static void* cl_listen_handleRequest(void* argument){
	
	char op_code, ret_code = 0;
	static char (*cl_listen_query)(struct socket_con*);
	struct socket_con peer = *(struct socket_con*)argument;
	
	
	/* Thread is working, so we prevent system from shutting down */
	pthread_mutex_lock(&nwThreadLock);
	clThreadCounterCond++;
	pthread_mutex_unlock(&nwThreadLock);
	
	switch((op_code = cl_listen_getQuery(&peer))){
		
		case DLD:
			cl_listen_query = cl_listen_uploadFile;
			break;
			
		case UPD:
			cl_listen_query = cl_listen_downloadFile;
			break;
			
		case DEL:
			cl_listen_query = cl_listen_deleteFile;
			break;
			
		default:
			ret_code = ERR_UNKNOWN;
			break;
		
	}
	
	if(ret_code) printf("%s", cl_error_assess(ret_code));
	else if((ret_code = (*cl_listen_query)(&peer))){
		log_print(0, 0, "Error (%d): %s\n\n", ret_code, cl_error_assess(ret_code));
		printf("%s", cl_error_assess(ret_code));
	} else log_print(0, 1, ".\t\tDone %s operation.\n\n", (op_code == DLD ? "uploading" : op_code == UPD ? "downloading" : "deleting"));
	
	close(peer.socketFD);
	pthread_mutex_lock(&nwThreadLock);
	
	clThreadCounterCond--;
	pthread_cond_signal(&nwThreadCond);
	
	pthread_mutex_unlock(&nwThreadLock);
	
	return NULL;
	
}


static char cl_listen_getQuery(struct socket_con* peer){
	
	char query, *op_code;
	
	
	/* Receives query type: download, upload or delete */
	log_print(0, 1, ".\t\t- Receiving operation code%s", NW_LOG_DOTS);
	if(!(op_code = nw_recv(peer)))
		return errno = ERR_SENDRECV;
	log_print(0, 0, "Done. Code = %d\n", *op_code);
	query = *op_code;
	free(op_code);


	/* 1 == download; 2 == upload; 3 == delete */
	return query;
	
}


static char cl_listen_downloadFile(struct socket_con* peer){
	
	int *files_n;
	char ack, ret_code, *filename = NULL, *filedata = NULL;
	
	
	log_print(0, 1, ".\t\t- Download operation\n");
	log_print(0, 1, ".\t\t\t- Receiving how many files we'll download%s", NW_LOG_DOTS);
	if(!(files_n = nw_recv(peer)))
		return errno = ERR_SENDRECV;
	log_print(0, 0, "Done. %d file%c\n\n", *files_n, *files_n > 1 ? 's' : ' ');
	
	for(int i = 0; i < *files_n; i++){
		
		log_print(0, 1, ".\t\t\t- [%d/%d] Receiving filename%s", i+1, *files_n, NW_LOG_DOTS);
		if(!(filename = nw_recv(peer)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. File \"%s\"\n", filename);
		
		log_print(0, 1, ".\t\t\t- [%d/%d] Receiving filename's data (%s)%s", i+1, *files_n, filename, NW_LOG_DOTS);
		if(!(filedata = nw_recv(peer)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		
		log_print(0, 1, ".\t\t\t- [%d/%d] Saving file (%s) locally%s", i+1, *files_n, filename, NW_LOG_DOTS);
		if((ret_code = cl_files_saveFile(filename, filedata)))
			log_print(0, 0, "Failed (%d): %s\n", ret_code, cl_error_assess(ret_code));
		else log_print(0, 0, "Done\n");
		ack = !ret_code;
		
		log_print(0, 1, ".\t\t\t- [%d/%d] Sending peer that the request %s%s", i+1, *files_n, ack ? "succeeded" : "failed", NW_LOG_DOTS);
		if(nw_send(peer, &ack, sizeof(ack)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n\n");
		
		if(ret_code && ret_code != ERR_FEXIST && ret_code != ERR_FOPEN)
			_goto(ERROR, ret_code);
		
		free(filename);
		free(filedata);
		
		filename = NULL;
		filedata = NULL;
		
	}
	free(files_n);
	
	return 0;
	
	ERROR:
		free(files_n);
		free(filename);
		free(filedata);
		return errno;
	
}


static char cl_listen_uploadFile(struct socket_con* peer){
	
	int *files_n;
	long filedata_size;
	char flag_found = 1, *filename = NULL, *filepath = NULL, *filedata = NULL;
	
	
	log_print(0, 1, ".\t\t- Upload operation\n");
	log_print(0, 1, ".\t\t\t- Receiving how many files we'll upload%s", NW_LOG_DOTS);
	if(!(files_n = nw_recv(peer)))
		return errno = ERR_SENDRECV;
	log_print(0, 0, "Done. %d file%c\n", *files_n, *files_n > 1 ? 's' : ' ');
	
	for(int i = 0; i < *files_n; i++, flag_found = 1){
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Receiving filename peer wants%s", i+1, *files_n, NW_LOG_DOTS);
		if(!(filename = nw_recv(peer)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. File \"%s\"\n", filename);
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Getting \"%s\"'s path%s", i+1, *files_n, filename, NW_LOG_DOTS);
		if(!(filepath = cl_files_getFileFullPath(".", filename))){
			printf("Error (%d): %s\n", ERR_FNFOUND, cl_error_assess(ERR_FNFOUND));
			flag_found = 0;
		} else log_print(0, 0, "Done. The path of \"%s\" is \"%s\"\n", filename, filepath);
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Notifying peer that we %s have that file%s", i+1, *files_n, flag_found ? "do" : "actually don't", NW_LOG_DOTS);
		if(nw_send(peer, &flag_found, sizeof(flag_found)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		if(!flag_found) goto FREES;
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Getting \"%s\"'s data%s", i+1, *files_n, filename, NW_LOG_DOTS);
		if(!(filedata = cl_files_readFile(filepath, &filedata_size)))
			_goto(ERROR, errno);
		log_print(0, 0, "Done\n");
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Sending \"%s\"'s data to peer%s", i+1, *files_n, filename, NW_LOG_DOTS);
		if(nw_send(peer, filedata, filedata_size))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n\n");
		
		FREES:;
		
		free(filename);
		free(filepath);
		free(filedata);
		
		filename = NULL;
		filepath = NULL;
		filedata = NULL;
		
	}
	free(files_n);
	
	return 0;
	
	ERROR:
		free(files_n);
		free(filename);
		free(filepath);
		free(filedata);
		return errno;
	
}


static char cl_listen_deleteFile(struct socket_con* peer){
	
	int *files_n;
	char ack, ret_code, flag_found = 1, *filename = NULL, *filepath = NULL;
	
	
	log_print(0, 1, ".\t\t- Delete operation\n");
	log_print(0, 1, ".\t\t\t- Receiving how many files we'll delete%s", NW_LOG_DOTS);
	if(!(files_n = nw_recv(peer)))
		return errno = ERR_SENDRECV;
	log_print(0, 0, "Done. %d file%c\n", *files_n, *files_n > 1 ? 's' : ' ');
	
	for(int i = 0; i < *files_n; i++, flag_found = 1){
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Receiving filename to delete locally%s", i+1, *files_n, NW_LOG_DOTS);
		if(!(filename = nw_recv(peer)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done. File: \"%s\"\n", filename);
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Getting \"%s\"'s path%s", i+1, *files_n, filename, NW_LOG_DOTS);
		if(!(filepath = cl_files_getFileFullPath(".", filename))){
			printf("Error (%d): %s\n", ERR_FNFOUND, cl_error_assess(ERR_FNFOUND));
			flag_found = 0;
		} else log_print(0, 0, "Done. The path of \"%s\" is \"%s\"\n", filename, filepath);
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Notifying peer that we %s have that file%s", i+1, *files_n, flag_found ? "do" : "actually don't", NW_LOG_DOTS);
		if(nw_send(peer, &flag_found, sizeof(flag_found)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n");
		if(!flag_found) goto FREES;
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Removing \"%s\"%s", i+1, *files_n, filename, NW_LOG_DOTS);
		if((ret_code = remove(filepath)))
			log_print(0, 0, "Failed (%d): %s\n", ret_code, cl_error_assess(ret_code));
		else log_print(0, 0, "Done\n");
		ack = !ret_code;
		
		log_print(0, 1, ".\t\t\t\t- [%d/%d] Notifying peer that the deletion %s%s", i+1, *files_n, ack ? "succeeded" : "failed", NW_LOG_DOTS);
		if(nw_send(peer, &ack, sizeof(ack)))
			_goto(ERROR, ERR_SENDRECV);
		log_print(0, 0, "Done\n\n");
		
		FREES:;
		
		free(filename);
		free(filepath);
		
		filename = NULL;
		filepath = NULL;
		
	}
	free(files_n);
	
	return 0;
	
	ERROR:
		free(files_n);
		free(filename);
		free(filepath);
		return errno;
	
}


static void cl_listen_mt_cleaner(void* listener){
	
	close(((struct socket_con*)listener)->socketFD);
	
}