#ifndef __SERVER_UTILS__
#define __SERVER_UTILS__


#include <time.h>
#include "singly.h"
#include "../network_utils.h"


/* */
#define SV_BUFFER_MAXSIZE 2048


/* */
enum{ERR_SRVUPDATE = ERR_UNKNOWN+1, ERR_CNFOUND, ERR_FNFOUND};

/* */
int svThreadCounterCond;

/* */
extern pthread_cond_t nwThreadCond;

/* */
extern pthread_mutex_t nwThreadLock;



/* ===Connection Module=== */
/* */
char sv_queryOperation(char op_code, struct client_info* client, pthread_t* thread);

/* */
char sv_getQuery(struct socket_con* server, struct client_info* client);



/* ===Error Module=== */
/* */
const char* sv_error_assess(char ret_code);


#endif