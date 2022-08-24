#ifndef __CLIENT_UTILS__
#define __CLIENT_UTILS__


#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../network_utils.h"


/* */
extern pthread_cond_t nwThreadCond;

/* */
extern pthread_mutex_t nwThreadLock;

/* */
enum{ERR_CANCEL = ERR_UNKNOWN+1, ERR_FEXIST, ERR_FNFOUND, ERR_WRITE, ERR_FREMOVE, ERR_FOPEN};

/* */
#define CL_INPUT_MAX 1024

/* */
#define CL_FILENAME_MAX 1024

/* */
#define CL_CLIENT_DELIM '|'

/* */
#define CL_RECURSIVE_FILE_SEARCH 0

/* https://en.wikipedia.org/wiki/ANSI_escape_code */
#define CL_COLOR_BLUE   "\x1B[34m"
#define CL_COLOR_GREEN  "\x1B[32m"
#define CL_COLOR_RED	"\x1B[31m"
#define CL_COLOR_NORMAL "\x1B[0m"


/* ===Connection Module=== */
/* */
char cl_queryOperation(char op_code, struct in_addr* IP, int port);

/* */
char cl_initClient(struct in_addr* IP, int port);

/* */
char cl_showStatus(struct in_addr* IP, int port);

/* */
char cl_showList(struct in_addr* IP, int port);

/* */
char cl_exitPool(struct in_addr* IP, int port);

/* */
struct client_info* cl_getList(struct in_addr* IP, int port, int* size);



/* ===Listen Module=== */
/* */
void* cl_listen(void* dummy);



/* ===Files Module=== */
/* */
char* cl_files_getFileFullPath(char* startingpath, char* filename);

/* */
int cl_files_getLocalFiles(char* path, char*** files, int* size);

/* */
char* cl_files_readFile(char* filename, long* size);

/* */
char cl_files_saveFile(char* filename, char* data);



/* ===Menus Module=== */
/* */
char cl_menu_main(void);

/* */
struct client_info* cl_menu_download(struct client_info* list, int list_size, int* chosenfiles_info_size);

/* */
struct client_info* cl_menu_upload(struct client_info* list, int list_size, int* chosenfiles_info_size);

/* */
struct client_info* cl_menu_delete(struct client_info* list, int list_size, int* chosenfiles_info_size);

/* */
char cl_menu_status(char* status);

/* */
char cl_menu_list(struct client_info* list, int list_size);



/* ===Error Module=== */
/* */
const char* cl_error_assess(char ret_code);


#endif