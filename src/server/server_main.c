#include <signal.h>
#include "server_utils.h"


volatile sig_atomic_t sigint_flag	= 0;
volatile sig_atomic_t sigpipe_flag	= 0;

void  sigint_handler(int signal);
void  sigpipe_handler(int signal);
int   sigset(int sig, void (*handler)(int));


int main(void){

	char op_code, ret_code;
	struct socket_con server;
	pthread_t thread[NW_MAX_THREADS];
	struct client_info client[NW_MAX_THREADS];

	sigset(SIGINT,  sigint_handler);
	sigset(SIGPIPE, sigpipe_handler);

	if(nw_setupListener(&server, NW_DEFAULT_SERVER_PORT, NULL))
		return 1;

	for(int i = 0; ; i = (i+1)%NW_MAX_THREADS){

		sleep(1); //Just to print everything nicely
		if((op_code = sv_getQuery(&server, &client[i])) < EXIT){
			printf("%s", sv_error_assess(errno));
			continue;
		}

		if(sigint_flag){

			while(svThreadCounterCond)
				pthread_cond_wait(&nwThreadCond, &nwThreadLock);

			break;

		}

		if((ret_code = sv_queryOperation(op_code, &client[i], &thread[i])))
			printf("%s", sv_error_assess(ret_code));

		if(sigpipe_flag){
			
			printf("%s\n", NW_PIPE_ERRORMSG);
			sigpipe_flag = 0;
			
		}

	}
	
	pthread_cond_destroy(&nwThreadCond);
	pthread_mutex_destroy(&nwThreadLock);
	
	return 0;
	
}


int sigset(int sig, void (*handler)(int)){
	
	struct sigaction sa;
	
	
	/* Sets up structure */
	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	
	
	/* Sets signal 'sig' to be handled by 'handler' function */
	return sigaction(sig, &sa, NULL);
	
}


void sigint_handler(int signal){
	
	sigint_flag = 1;
	
}


void sigpipe_handler(int signal){
	
	sigpipe_flag = 1;
	
}