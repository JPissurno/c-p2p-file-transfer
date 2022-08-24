#include <signal.h>
#include "client_utils.h"


extern int clThreadCounterCond;
volatile sig_atomic_t sigint_flag	= 0;
volatile sig_atomic_t sigpipe_flag	= 0;

void  sigint_handler(int signal);
void  sigpipe_handler(int signal);
int   sigset(int sig, void (*handler)(int));


int main(void){
	
	char ret;
	pthread_t listen_thread;
	sigset(SIGINT,  sigint_handler);
	sigset(SIGPIPE, sigpipe_handler);


	if(cl_queryOperation(INI, NULL, NW_DEFAULT_SERVER_PORT))
		return 2;

	if(pthread_create(&listen_thread, NULL, cl_listen, NULL))
		return 1;

	/* Just to print everything nicely */
	sleep(1);
	
	while(1){

		ret = cl_queryOperation(cl_menu_main(), NULL, NW_DEFAULT_SERVER_PORT);

		if(sigint_flag){
			cl_queryOperation(EXIT, NULL, NW_DEFAULT_SERVER_PORT);
			break;
		}

		if(sigpipe_flag){
			printf("%s\n", NW_PIPE_ERRORMSG);
			sigpipe_flag = 0;
		}

		if(ret == EXIT) break;

	}

	pthread_cancel(listen_thread);
	pthread_join(listen_thread, NULL);

	while(clThreadCounterCond)
		pthread_cond_wait(&nwThreadCond, &nwThreadLock);

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
	
	sigint_flag = 2;
	
}


void sigpipe_handler(int signal){
	
	sigpipe_flag = 1;
	
}