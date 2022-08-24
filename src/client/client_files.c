#include "client_utils.h"


char* cl_files_getFileFullPath(char* startingpath, char* filename){
	
	struct dirent *dir;
	char *fullpath = NULL;
	DIR *d = opendir(startingpath);
	
	
	if(!d){
		errno = ERR_UNKNOWN;
		return NULL;
	}
	
	while((dir = readdir(d)) && !fullpath){
		
		if(dir->d_type != DT_DIR){
			
			if(!strncmp(filename, dir->d_name, strlen(filename))){
				
				char fullpathx[CL_FILENAME_MAX+1];
				
				closedir(d);
				snprintf(fullpathx, CL_FILENAME_MAX, "%s/%s", startingpath, filename);
				return strcpy(calloc(strlen(fullpathx)+1, sizeof(char)), fullpathx);
				
			}
			
		} else if(CL_RECURSIVE_FILE_SEARCH && dir->d_type == DT_DIR) {
			
			if(strncmp(dir->d_name, ".", 1) && strncmp(dir->d_name, "..", 2)){

				char next_path[CL_FILENAME_MAX+1];
				
				snprintf(next_path, CL_FILENAME_MAX, "%s/%s", startingpath, dir->d_name);
				fullpath = cl_files_getFileFullPath(next_path, filename);
				
			}
			
		}
		
	}
	
	closedir(d);
	return fullpath;
	
}


int cl_files_getLocalFiles(char* path, char*** files, int* size){
	
	if(!path || !files || !size) return -1;
	
	struct dirent *dir;
	DIR *d = opendir(path);
	if(!d) return *size;
	
	/* !! readdir does not guarantee an order !! */
	/* maybe the first cl_getLocalFiles gets an other
	   order compared to the second. That would screw
	   things up bad... */
	/* IF DIFFERENT FILES ARE BEING TRANSFERRED,
	   LOOK THIS ONE UP FIRST! */
	while((dir = readdir(d))){
		
		if(dir->d_type != DT_DIR){
			
			*files = (*size) ? realloc(*files, (*size+1)*sizeof(char*)) : calloc(1, sizeof(char*));
			(*files)[*size] = strcpy(calloc(strlen(dir->d_name)+1, sizeof(char)), dir->d_name);
			(*size)++;
			
		} else if(CL_RECURSIVE_FILE_SEARCH && dir->d_type == DT_DIR)
			if(strncmp(dir->d_name, ".", 1) && strncmp(dir->d_name, "..", 2)){

				char next_path[CL_FILENAME_MAX+1];
				
				snprintf(next_path, CL_FILENAME_MAX, "%s/%s", path, dir->d_name);
				cl_files_getLocalFiles(next_path, files, size);
				
			}
		
	}
	
	closedir(d);
	
	return *size;
	
}


char* cl_files_readFile(char* filename, long* size){
	
	if(!filename || !size) _goto(ERROR, ERR_UNKNOWN);
	if(!strlen(filename)) _goto(ERROR, ERR_FOPEN);
	
	char *data = NULL;
	FILE *file = fopen(filename, "rb");
	
	if(!file) _goto(ERROR, ERR_FOPEN);
	
	fseek(file, 0L, SEEK_END);
	*size = ftell(file);
	fseek(file, 0L, SEEK_SET);
	
	data = calloc((*size)+1, sizeof(char));
	fread(data, *size, sizeof(char), file);
	data[*size] = '\0';
	fclose(file);
	
	return data;
	
	ERROR:
		return NULL;
	
}


char cl_files_saveFile(char* filename, char* data){
	
	int fd = open(filename, O_CREAT | O_WRONLY | O_EXCL, S_IRUSR | S_IWUSR);
	
	if(fd < 0) return errno = ERR_FEXIST;
	
	FILE *file = fdopen(fd, "wb");
	
	if(!file) return errno = ERR_FOPEN;
	
	fprintf(file, "%s", data);
	
	fclose(file);
	close(fd);
	
	return 0;
	
}