#include "network_utils.h"


/* Global conditional variable for multithreading. Must be destroyed later */
pthread_cond_t nwThreadCond = PTHREAD_COND_INITIALIZER;

/* Global thread lock for multithreading. Must be destroyed later */
pthread_mutex_t nwThreadLock = PTHREAD_MUTEX_INITIALIZER;


char nw_setupExtConnection(struct socket_con* connection, int port, struct in_addr* IP){
	
	char s_ip[128];
	struct timeval tv;
	memset(connection->addr.sin_zero, 0, sizeof(connection->addr.sin_zero));
	
	
	/* Sets up addr structure */
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 0, "%s", "\n");
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 1, "%s%d%s%s%s", "Setting up External Connection in port ", port, ", with IP ", (IP ? inet_ntop(AF_INET, IP, s_ip, sizeof(s_ip)) : "127.0.0.1"), NW_LOG_DOTS);
	connection->addr.sin_family = AF_INET;	/* Sets address family to IPv4	*/
	connection->addr.sin_port = htons(port);/* Sets destination port number	*/
	if(IP) connection->addr.sin_addr = *IP;	/* Sets destination IP number	*/
	else connection->addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "%s\n", "Done");
	
	
	/* Opens socket for connection */
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 1, "%s%s", "Reserving socket for External Connetion", NW_LOG_DOTS);
	if((connection->socketFD = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		return 1;
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "%s%d\n", "Done. Socket opened: ", connection->socketFD);
	
	
	/* Sets recv() function to wait at max NW_RECV_TIMEOUT seconds */
	tv.tv_usec = 0;
	tv.tv_sec = NW_RECV_TIMEOUT;
	if(setsockopt(connection->socketFD, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)))
		return 1;
	
	
	/* Connects to host */
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 1, "%s%s%s%d%s", "Connecting to ", s_ip, " on socket ", connection->socketFD, NW_LOG_DOTS);
	if(connect(connection->socketFD, (struct sockaddr *)&connection->addr, sizeof(struct sockaddr_in)))
		return 1;
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "%s\n", "Done");
	
	
	/* Success */
	return 0;
	
}


char nw_setupListener(struct socket_con* connection, int port, struct in_addr* IP){
	
	char s_ip[128];
	memset(connection, 0, sizeof(*connection));
	
	
	/* Sets up addr structure */
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 1, "%s%d%s%s%s", "Setting up listener in port ", port, ", with IP ", (IP ? inet_ntop(AF_INET, &IP, s_ip, sizeof(s_ip)) : "0.0.0.0"), NW_LOG_DOTS);
	connection->addr.sin_family = AF_INET;				/* Sets address family to IPv4		*/
	connection->addr.sin_port = htons(port);			/* Sets the server port number		*/
    if(IP) connection->addr.sin_addr = *IP;		
	else connection->addr.sin_addr.s_addr = INADDR_ANY;	/* Sets address to any interface	*/
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "%s\n", "Done");
	
	
	/* Opens socket for listener, returns error if any occur */
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 1, "%s%s", "Reserving socket for listener", NW_LOG_DOTS);
	if((connection->socketFD = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return 1;
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "%s%d\n", "Done. Socket opened: ", connection->socketFD);
	
	
	/* */
	if(setsockopt(connection->socketFD, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
		return 1;
	
	
	/* */
	if(setsockopt(connection->socketFD, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) < 0)
		return 1;
	
	
	/* Binds opened socket, returns error if any occur */
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 1, "%s%d%s", "Binding socket ", connection->socketFD, NW_LOG_DOTS);
	if(bind(connection->socketFD, (struct sockaddr*)&connection->addr, sizeof(connection->addr)))
		return 1;
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "%s\n", "Done");
	
	
	/* Listens on opened socket, returns error if any occur */
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 1, "%s%d%s", "Listening on socket ", connection->socketFD, NW_LOG_DOTS);
	if(listen(connection->socketFD, NW_MAX_QUEUE))
		return 1;
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "%s\n", "Done");
	
	
	/* Success */
	return 0;
	
}


char nw_acceptConnection(struct socket_con* server, struct socket_con* client){
	
	socklen_t len = sizeof(client->addr);
	
	
	/* Waits for client's connection request on opened socket */
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 1, "%s%d%s", "\"Server\" waiting connection request on socket ", server->socketFD, NW_LOG_DOTS);
	if((client->socketFD = accept(server->socketFD, (struct sockaddr *)&client->addr, &len)) < 0)
		return 1;
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 0, "%s\n", "\"Client\" connected");
	
	
	/* Success */
	return 0;
	
}


char nw_send(struct socket_con* con, void* msg, long len){
	
	char *temp = msg; //We work with char* because it's always 1 byte per address
	long bSent = 0, offset = 0, err_flag = 0;
	
	
	nw_logTransfer(con, len, 's');
	for(int i = 0; i < 2; i++){
		
		/* Repeats until all len bytes are sent */
		do {
			
			/* If true, an error ocurred or the connection was closed, so it stops there */
			if((bSent += send(con->socketFD, temp+offset, len, 0)) <= offset){
				
				/* Logs stuff, sets error flag and finishes */
				if(bSent == offset)
					if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "\t\t%s%s\n", "Connection closed", NW_LOG_DOTS);
				if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "\t\t%s%ld%s%ld%s%ld%s%s\n", "Failed to send from byte ", (bSent < 0 ? 0 : bSent), " to ", len, " (", len-(bSent < 0 ? 0 : bSent), " total)", NW_LOG_DOTS);
				err_flag = 1;
				break;
				
			}
			if(!NW_LOG_SUPRESS_CONNECTION_LOG && !NW_LOG_SUPRESS_BYTES)
				log_print(0, 1, "\t\t%s%ld%c%ld%s%s\n", "Sent (", bSent, '/', len, ") bytes", i ? " [finish signal]" : "");
			
			/* Updates bytes count */
			offset = bSent;
			
		} while(bSent != len);
		
		
		/* 	If an error occurred, it doesn't send the finish signal */
		if(err_flag) break;
		temp = NW_SEND_FINISH;
		bSent = 0; offset = 0; len = strlen(temp);
		
	}
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "\t%s\n", "====DATA TRANSFER FINISH====");
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 0, "%s", "");
	
	
	/* Returns 0 if success, 1 otherwise */
	return err_flag;
	
}


void* nw_recv(struct socket_con* con){
	
	char *zeroparse_aux;
	char *msg = calloc(1, 1); //We work with char* because it's always 1 byte per address
	long parsec = 0, len = 1, bReceived = 0, offset = 0, err_flag = 0;
	long *zeroparse = calloc(1, sizeof(long)), fmsg_size = strlen(NW_SEND_FINISH);
	nw_logTransfer(con, -1, 'r');
	
	
	/* Repeats until finish signal is received */
	do {
		
		/* If true, an error occurred or the connection was closed, so it stops there */
		if((bReceived += recv(con->socketFD, msg+offset, 1, 0)) <= offset){
			
			/* Logs stuff, sets error flag, if necessary, and finishes */
			if(bReceived <= offset){
				if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "\t\t%s%ld%s%ld%s%ld%s%s\n", "Failed to receive from byte ", (bReceived < 0 ? 0 : bReceived), " to ", len, " (", len-(bReceived < 0 ? 0 : bReceived), " total)", NW_LOG_DOTS);
				err_flag = 1;
			}
			break;
			
		}
		if(!NW_LOG_SUPRESS_CONNECTION_LOG && !NW_LOG_SUPRESS_BYTES)
			log_print(0, 1, "\t\t%s%ld%c%ld%s\n", "Received (", bReceived, '/', len, ") bytes");
		
		/* Updates bytes count and reallocates buffer, if necessary */
		offset = bReceived;
		if(bReceived >= len) msg = realloc(msg, (len *= 2)+1);
		*(msg+bReceived) = '\0';
		
		/* If there's a 0 inside msg, we need to parse it */
		if((zeroparse_aux = strchr(msg, '\0')) != msg+bReceived){
			*zeroparse_aux = 1;
			zeroparse[parsec] = zeroparse_aux-msg;
			zeroparse = realloc(zeroparse, (++parsec+1)*sizeof(long));
		}
		
		/* Detects end of message sent, according to NW_SEND_FINISH macro */
		if(bReceived >= fmsg_size)
			if(!strncmp(msg+bReceived-fmsg_size, NW_SEND_FINISH, fmsg_size))
				break;

	} while(1);
	
	
	/* If no errors occurred, removes finish signal from the message received
	   and restores original zeroes */
	if(!err_flag){
		
		if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "\t\t%s\n", "Finish signal received");
		for(long i = 1, l = fmsg_size+1; i < l; i++)
			*(msg+bReceived-i) = '\0';
		
		for(long i = 0; i < parsec; i++)
			msg[zeroparse[i]] = '\0';
		
	}
	
	
	/* Logs stuff, frees message, if an error occurred, and frees zeroparser */
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(0, 1, "\t%s\n", "====DATA TRANSFER FINISH====");
	if(!NW_LOG_SUPRESS_CONNECTION_LOG) log_print(1, 0, "%s", "");
	if(err_flag) free(msg);
	free(zeroparse);
	
	/* Returns received message if no errors occurred, NULL otherwise */
	return err_flag ? NULL : (void*)msg;
	
}


void nw_freeInfo(struct client_info* info){
	
	for(int i = 0; i < info->files_n; i++){
		free(info->filename[i]);
		free(info->filedata[i]);
	}
	
	if(info->connection.socketFD)
		close(info->connection.socketFD);
	
	free(info->filename);
	free(info->filedata);
	
}


void nw_logTransfer(struct socket_con* con, long len, char type){
	
	/* If NW_LOG set to 0, i.e. disabled, does not log stuff */
	if(!NW_LOG || NW_LOG_SUPRESS_CONNECTION_LOG) return;
	
	
	/* Logs stuff */
	log_print(0, 0, "%s", "\n");
	log_print(0, 1, "\t%s\n", "====DATA TRANSFER START====");
	log_print(0, 1, "\t\t%s%s\n", "Type: ", type == 's' ? "send" : "receive");
	log_print(0, 1, "\t\t%s", "Length (bytes): ");
	if(len < 0) log_print(0, 0, "%c\n", '?');
	else log_print(0, 0, "%ld\n", len);
	nw_logConnection(con, type);
	
}


void nw_logConnection(struct socket_con* con, char type){
	
	/* If NW_LOG set to 0, i.e. disabled, does not log stuff */
	if(!NW_LOG || NW_LOG_SUPRESS_CONNECTION_LOG) return;
	
	char ip[128];
	
	
	/* Logs stuff */
	log_print(0, 1, "\t\t%s%d\n", "Port: ", htons(con->addr.sin_port));
	log_print(0, 1, "\t\t%s%d\n", "Socket: ", con->socketFD);
	log_print(0, 1, "\t\t%s%d\n", "Protocol Version: ", con->addr.sin_family == AF_INET ? 4 : 6);
	log_print(0, 1, "\t\t%s%s%s\n\n", type == 's' ? "To" : "From", " Address: ", inet_ntop(con->addr.sin_family, &con->addr.sin_addr, ip, sizeof(ip)));
	
}


const char* nw_assessError(char ret_code){
	
	switch(ret_code){
		case ERR_CONNECT:
			return "Falha ao conectar-se com o servidor\n";
		case ERR_SENDRECV:
			return "Falha ao enviar/receber dados ao/do servidor\n";
		case ERR_UNKNOWN:
			return "Erro desconhecido\n";
		default:
			return "";
	}
	
}