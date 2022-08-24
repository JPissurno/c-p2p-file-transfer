#ifndef __NETWORK_UTILS__
#define __NETWORK_UTILS__



#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>



/* TODO LIST:
 *
 *	- Error checking revamp;
 *	- code spacing ('\n') revamp;
 *  - consistency of types used in IDs and counts (unsigned, long or int? why?);
 *  - rework of globals (there's too many, maybe we don't need some?);
 *	- upload, download & delete: prompt user for input/output directory (existing or not);
 *	- what about same filenames but different directories?;
 *	- what if some external factor opens (check)/removes (check)/creates files on working directory & sub-directories?;
 *	- swap calloc for malloc when there's no initialization required;
 *	- when assigning ports to clients, check if they could open them (maybe chosen port is in use...);
 *	- function guards;
 *	- comments;
 *	- so we can work with binary files:
 *		- change cl_files_readFile template to: (long) -> (char* pathorigin, char* destinationdata);
 *		- change nw_recv template to: (long) -> (struct in_addr* connection, void* msgbuffer)
 *
 */


/* INTERNAL PROTOCOL:
 *
 * 		- Clients' ID range -> 1 to INT_MAX;
 *
 *		- Server must send its list in crescent client ID order;
 *
 *		- When increasing client_info file counter (files_n member),
 *		  there must exist some filename corresponding to that counter-1,
 *		  but its filename's data is optional, although it has to have
 *		  heap data allocated to it;
 *
 *		- Multithreading is per different client ID, not per file!
 *		  Example: user requests files 1 and 3 from client with ID 1.
 *		  In this case, only one thread will be created.
 *
 *		- If any file contains the NW_SEND_FINISH string, the behavior
 *		  when transfering it is undefined.
 *
 */


/* */
enum{EXIT = -1, INI, DLD, UPD, DEL, STS, LST};


/* */
enum{ERR_CONNECT = 1, ERR_SENDRECV, ERR_UNKNOWN};


/* Max number of requests to wait on queue to connect to a listener */
#define NW_MAX_QUEUE 10


/* Max number of threads for static pools of them */
#define NW_MAX_THREADS 500


/* Max time, in seconds, an opened socket will wait for data to be sent/received */
#define NW_RECV_TIMEOUT 10


/* */
#define NW_PORT_MAX_RANGE 65534


/* Default server port to connect to. Server listens on this port */
#define NW_DEFAULT_SERVER_PORT 6969


/* Clients listen on ports starting from here */
#define NW_DEFAULT_STARTING_CLIENT_PORT 7070


/* Default finish signal to send between client and server */
#define NW_SEND_FINISH "nw_send_finish"


/* Default message when pipe breaks midway any operation */
#define NW_PIPE_ERRORMSG "Broken pipe for some reason, couldn't write/read current request to/from socket..."



/**	@Description
 *		Macro to enable/disable log messages
 * 		and where to display them.
 *
 * 	@Options
 *		0: no log;
 * 		1: log in stdout;
 * 		2: log in file
 *
 */
#ifndef NW_LOG
#	define NW_LOG 0
#endif


/* Log file name */
#ifndef NW_LOG_FILE
#	define NW_LOG_FILE "connection_log.txt"
#endif


/* */
#ifndef NW_LOG_SUPRESS_CONNECTION_LOG
#	define NW_LOG_SUPRESS_CONNECTION_LOG 1
#endif


/* Supresses log of bytes received/sent if true */
#ifndef NW_LOG_SUPRESS_BYTES
#	define NW_LOG_SUPRESS_BYTES 1
#endif


/* Used on log macro function */
#define NW_LOG_DOTS ".........."



/** @Functionality
 *		Macro function to log stuff, the location
 *		depending on NW_LOG_FILE macro. It does not
 *		log if there's any error opening the file
 *		or if NW_LOG is 0.
 *
 *		It prints, optionally, the current time
 *		surrounded by square brackets and current
 *		thread ID, by using pthread_self() function.
 *
 *	@Arguments
 *		printTime:	option to print time surrounded by square brackets
 *					before the message;
 *
 *					0	= does not print time;
 *					1	= prints time;
 *
 *		printTID:	option to print current thread ID;
 *
 *					0	= does not print TID;
 *					1	= prints TID;
 *
 *		fmt:		format string;
 *
 *		...:		variables according to fmt.
 *
 *	@Return
 *		None
 *
 */
#define log_print(printTime, printTID, ...)												\
		do {																			\
			if(!NW_LOG) break;															\
			FILE *file = NW_LOG == 1 ? stdout : fopen(NW_LOG_FILE, "a");				\
			if(!file) break;															\
			if(printTID)																\
				fprintf(file, "%s%lu%s", "(T", (pthread_self()%NW_MAX_THREADS), ") ");	\
			if(printTime){																\
				char buffer[26];														\
				time_t t = time(NULL);													\
				struct tm *ti = localtime(&t);											\
				strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", ti);							\
				fprintf(file, "[%s] ", buffer);											\
			}																			\
			fprintf(file, __VA_ARGS__);													\
			fflush(file);																\
			if(NW_LOG != 1) fclose(file);												\
		} while(0)

/* */
#define _goto(label, error) do{ errno = error; goto label; }while(0)



/**	@Description
 *		Structure that holds all information
 *		to connect to some host.
 * 
 *	@Members
 *		int socketFD:				holds a file descriptor
 *									returned by socket() function;
 *
 *		struct sockaddr_in addr:	structure that holds
 *									connection information
 *
 */
struct socket_con {
	
	int socketFD;
	unsigned port;
	struct sockaddr_in addr;
	
};


/* */
struct client_info {
	
	int ID;
	int files_n;
	char** filename;
	char** filedata;
	struct socket_con connection;
	
};





/**	@Functionality
 *		Sets up socket_con structure with said port and IPv4,
 *		reserves a socket and connects to it.
 *
 *	@Arguments
 *		struct socket_con* client:	pointer to the socket_con structure to be configured;
 *
 *		int port:					port to connect to;
 *
 *		struct in_addr* IP:			pointer to an in_addr structure,
 *									with a valid IPv4 into. If IP is
 *									NULL, loopback is used.
 *
 *	@Returns
 *		On success: 0;
 *
 *		On failure: 1 (if there's any error opening a socket, binding it or listening on it)
 *
 */
char nw_setupExtConnection(struct socket_con* client, int port, struct in_addr* IP);



/* */
char nw_setupListener(struct socket_con* connection, int port, struct in_addr* IP);



/* */
char nw_acceptConnection(struct socket_con* server, struct socket_con* client);



/** @Functionality
 *		Sends bytes to host, along with NW_SEND_FINISH.
 *
 *		Requires an opened socket and host's information,
 *		on a socket_con structure, all given by nw_setupExtConnection() function.
 *
 *	@Arguments
 *		struct socket_con* con:	pointer to a socket_con structure
 *								with an opened socket and host information;
 *		
 *		void* msg:				pointer to the bytes to send;
 *		
 *		long len: 				how many bytes of msg to send.
 *
 *	@Returns
 *		On success: 0;
 *
 *		On failure: 1 (if any error occurs while sending the bytes)
 *
 */		 
char nw_send(struct socket_con* con, void* msg, long len);



/**	@Functionality
 *		Receives bytes from host, heap allocated, and guarantees
 *		a null character at the very end of the bytes received.
 *		The memory allocated for the message received may be
 *		longer than the message itself.
 *
 *		Requires an opened socket and host's information,
 *		on a socket_con structure, all given by nw_setupExtConnection() function.
 *
 *		Since msg is heap allocated, it must be freed by the caller afterwards.
 *
 *	@Arguments
 *		struct socket_con* con:	pointer to a socket_con structure
 *								with an opened socket and host information;
 *
 *	@Returns
 *		On success: a pointer to the received message,
 *					heap allocated;
 *
 *		On failure: NULL (if any error occurs while receiving the bytes)
 *
 */
void* nw_recv(struct socket_con* con);



/* */
void nw_freeInfo(struct client_info* info);



/**	@Functionality
 *		Logs connection between current client and some external connection,
 *		according to NW_LOG macro configuration, displaying host information.
 *
 *		Requires an opened socket and host's information, on a socket_con
 *		structure, all given by nw_setupExtConnection() function.
 *
 *	@Arguments
 *		struct socket_con* con:	pointer to a socket_con structure
 *								with an opened socket and host information;
 *
 *		char type:				'r' for a read connection;
 *								's' for a send connection.
 *
 *	@Return
 *		None
 *
 */
void nw_logConnection(struct socket_con* con, char type);



/**	@Functionality
 *		Logs message transfer between current client and some external connection,
 *		according to NW_LOG macro configurantion, displaying bytes and protocol information.
 *
 *		Requires an opened socket and host's information,
 *		on a socket_con structure, all given by nw_setupExtConnection() function.
 *
 *	@Arguments
 *		struct socket_con* con:	pointer to a socket_con structure
 *								with an opened socket and host information;
 *
 *		long len:				how many bytes are being transmited,
 *								-1 if the quantity is unknown;
 *
 *		char type:				'r' for a read connection;
 *								's' for a send connection.
 *
 *	@Return
 *		None
 *
 */
void nw_logTransfer(struct socket_con* con, long len, char type);



/* */
const char* nw_assessError(char ret_code);



#endif