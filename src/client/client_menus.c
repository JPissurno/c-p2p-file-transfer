#include "client_utils.h"


/* */
extern int CL_DEFAULT_ID;


static int* cl_menu_parseInput(char* input, int files_n, int* chosenfiles_size);
static int* cl_menu_parseInputUpload(char* input, int files_n, struct client_info* list, int list_size, int** chosenIDs, int* chosen_size);
static struct client_info* cl_menu_fillInfo(struct client_info* list, int list_size, int* chosenfiles, int* chf_size, char ignoreSameID);
static struct client_info* cl_menu_fillInfoUpload(char** list, int* chosenIDs, int* chosenfiles, int chosen_size, int* chosenfiles_info_size);
static void cl_menu_clrscr(void);
static int cl_menu_qsortComp(const void* x, const void* y);


char cl_menu_main(void){
	
	char OP, input[CL_INPUT_MAX];
	
	while(1){
		
		printf("======Gerenciador de Arquivos======\n");
		printf("||[1] Baixar arquivo             ||\n");
		printf("||[2] Enviar arquivo             ||\n");
		printf("||[3] Deletar arquivo            ||\n");
		printf("||[4] Informações do sistema     ||\n");
		printf("||[5] Listar clientes e arquivos ||\n");
		printf("||[0] Sair...                    ||\n");
		printf("===================================\n");
		printf("||Input: ");
		if(!fgets(input, sizeof(input), stdin)) _goto(ERROR, errno);
		cl_menu_clrscr();
		
		if((OP = input[0]) >= '0' && OP <= '5') break;
		else printf("Opção inválida.\n");
		
	}
	
	/* Returns -1 for exit, 0 for download, etc */
	OP -= '0';
	return OP-!OP;
	
	ERROR:
		return -2;
	
}


struct client_info* cl_menu_download(struct client_info* list, int list_size, int* chosenfiles_info_size){
	
	if(!list || !list_size || !chosenfiles_info_size)
		return NULL;
	
	char input[CL_INPUT_MAX];
	struct client_info *chosenfiles_info = NULL;
	int files_n = 0, *chosenfiles = NULL, chosenfiles_size = 0;

	for(int i = 0; i < list_size; i++)
		if(list[i].ID != CL_DEFAULT_ID)
			files_n += list[i].files_n;
	
	*chosenfiles_info_size = 0;
	while(1){

		printf("============================================\n");
		printf("||########################################||\n");
		printf("||########### Baixar Arquivo(s) ##########||\n");
		printf("||########################################||\n");
		printf("============================================\n");
		for(int i = 0, j = 1; i < list_size; i++){
			if(list[i].ID == CL_DEFAULT_ID) continue;
			printf("||%sCliente %d%s:\n", CL_COLOR_GREEN, list[i].ID, CL_COLOR_NORMAL);
			for(int k = 0; k < list[i].files_n; j++, k++)
				printf("||\t[%d] %s%s%s\n", j, CL_COLOR_BLUE, list[i].filename[k], CL_COLOR_NORMAL);
			printf("||\n");
		}
		if(!files_n) printf("||Não há outros clientes conectados no momento\n");
		printf("============================================\n");
		printf("||[0]: cancelar operação\n");
		if(files_n) printf("||[1~%d]: arquivos disponíveis para baixar\n", files_n);
		printf("============================================\n");
		printf("||Input: ");
		if(!fgets(input, sizeof(input), stdin)) _goto(FINISH, errno);
		cl_menu_clrscr();

		if(input[0] == '0')
			_goto(FINISH, ERR_CANCEL);
		
		if((chosenfiles = cl_menu_parseInput(input, files_n, &chosenfiles_size)))
			break;
		else
			printf("%s\n", chosenfiles_size ? "||Algum arquivo escolhido não é válido" :
											  "||Escolha algum(ns) arquivo(s) para baixar, \
											   ou cancele a operação");
		
	}

	if(!(chosenfiles_info = cl_menu_fillInfo(list, list_size, chosenfiles, chosenfiles_info_size, 1)))
		_goto(FINISH, ERR_UNKNOWN);

	FINISH:;

	/* Returns set up client_info struct*, or NULL on failure */
	free(chosenfiles);
	return chosenfiles_info;
	
}


struct client_info* cl_menu_upload(struct client_info* list, int list_size, int* chosenfiles_info_size){
	
	if(!list || !list_size || !chosenfiles_info_size)
		return NULL;
	
	char input[CL_INPUT_MAX], **files;
	struct client_info *chosenfiles_info = NULL;
	int *chosenfiles = NULL, *chosenIDs = NULL;
	int firstID = 0, lastID = 0, chosen_size = 0, files_n = 0;

	if((files_n = cl_files_getLocalFiles(".", &files, &files_n)) < 0)
		_goto(FINISH, ERR_UNKNOWN);

	*chosenfiles_info_size = 0;
	while(1){

		printf("============================================\n");
		printf("||########################################||\n");
		printf("||########### Enviar Arquivo(s) ##########||\n");
		printf("||########################################||\n");
		printf("============================================\n");
		printf("||%s\n", list_size == 1 ? "Não há outros clientes conectados no momento" : "Clientes conectados:");
		for(int i = 0; i < list_size; i++){
			if(list[i].ID == CL_DEFAULT_ID) continue;
			if(!firstID) firstID = list[i].ID;
			lastID = list[i].ID;
			printf("||\t%sCliente [%d]%s\n", CL_COLOR_GREEN, list[i].ID, CL_COLOR_NORMAL);
		}
		printf("||\n");
		printf("||Arquivos locais:\n");
		for(int i = 0; i < files_n; i++)
			printf("||\t[%d] %s%s%s\n", i+1, CL_COLOR_BLUE, files[i], CL_COLOR_NORMAL);
		printf("===============================================================\n");
		printf("||[0]: cancelar operação\n");
		if(files_n && list_size > 1)
			printf("||[%d~%d]%c[1~%d]: clientes e arquivos disponíveis para enviar\n", firstID, lastID, CL_CLIENT_DELIM, files_n);
		printf("===============================================================\n");
		printf("||Input: ");
		if(!fgets(input, sizeof(input), stdin)) _goto(FINISH, errno);
		cl_menu_clrscr();

		if(input[0] == '0')
			_goto(FINISH, ERR_CANCEL);
		
		if((chosenfiles = cl_menu_parseInputUpload(input, files_n, list, list_size, &chosenIDs, &chosen_size)))
			break;
		else printf("%s\n", chosen_size ? "||Algum arquivo escolhido não é válido. \
										   Exemplo de escolha: 1DELIM1 (cliente 1, arquivo 1)" :
										   "||Escolha algum(ns) arquivo(s) para enviar, \
										   ou cancele a operação");
		
	}
	
	if(!(chosenfiles_info = cl_menu_fillInfoUpload(files, chosenIDs, chosenfiles, chosen_size, chosenfiles_info_size)))
		_goto(FINISH, ERR_UNKNOWN);

	FINISH:;

	for(int i = 0; i < files_n; i++)
		free(files[i]);
	free(files);
	free(chosenfiles);
	free(chosenIDs);
	
	/* Returns set up client_info struct*, or NULL on failure */
	return chosenfiles_info;
	
}


struct client_info* cl_menu_delete(struct client_info* list, int list_size, int* chosenfiles_info_size){
	
	if(!list || !list_size || !chosenfiles_info_size) return NULL;
	
	char input[CL_INPUT_MAX];
	struct client_info *chosenfiles_info = NULL;
	int files_n = 0, *chosenfiles = NULL, chosenfiles_size;

	for(int i = 0; i < list_size; i++)
		files_n += list[i].files_n;
	
	*chosenfiles_info_size = 0;
	while(1){

		printf("=============================================\n");
		printf("||#########################################||\n");
		printf("||########### Deletar Arquivo(s) ##########||\n");
		printf("||#########################################||\n");
		printf("=============================================\n");
		for(int i = 0, j = 1; i < list_size; i++){
			printf("||%sCliente %d%s:\n", ((list[i].ID == CL_DEFAULT_ID) ? CL_COLOR_RED : CL_COLOR_GREEN), list[i].ID, CL_COLOR_NORMAL);
			for(int k = 0; k < list[i].files_n; j++, k++)
				printf("||\t[%d] %s%s%s\n", j, CL_COLOR_BLUE, list[i].filename[k], CL_COLOR_NORMAL);
			printf("||\n");
		}
		if(!files_n) printf("||Não há outros clientes conectados no momento\n");
		printf("============================================\n");
		printf("||[0]: cancelar operação\n");
		if(files_n) printf("||[1~%d]: arquivos disponíveis para deletar\n", files_n);
		printf("============================================\n");
		printf("||Input: ");
		if(!fgets(input, sizeof(input), stdin)) _goto(FINISH, errno);
		cl_menu_clrscr();

		if(input[0] == '0')
			_goto(FINISH, ERR_CANCEL);
		
		if((chosenfiles = cl_menu_parseInput(input, files_n, &chosenfiles_size)))
			break;
		else
			printf("%s\n", chosenfiles_size ? "||Algum arquivo escolhido não é válido" :
											  "||Escolha algum(ns) arquivo(s) para deletar, \
											   ou cancele a operação");
		
	}

	if(!(chosenfiles_info = cl_menu_fillInfo(list, list_size, chosenfiles, chosenfiles_info_size, 0)))
		_goto(FINISH, ERR_UNKNOWN);

	FINISH:;

	/* Returns set up client_info struct*, or NULL on failure */
	free(chosenfiles);
	return chosenfiles_info;
	
}


char cl_menu_status(char* status){
	
	if(!status) return ERR_UNKNOWN;

	char *infoend;

	printf("=================================================\n");
	printf("||#############################################||\n");
	printf("||########### Informações do Sistema ##########||\n");
	printf("||#############################################||\n");
	printf("=================================================\n||\n");
	while((infoend = strchr(status, '\n'))){
		
		*infoend = '\0';
		printf("|| %s\n", status);
		status = infoend+1;
		
	}
	printf("||\n");
	
	return 0;
	
}


char cl_menu_list(struct client_info* list, int list_size){
	
	if(!list) return ERR_UNKNOWN;
	
	printf("==============================================\n");
	printf("||##########################################||\n");
	printf("||########### Arquivos e Clientes ##########||\n");
	printf("||##########################################||\n");
	printf("==============================================\n");
	if(!list_size) printf("|| Nenhum cadastrado no momento\n");
	for(int i = 0; i < list_size; i++){
		printf("||%sCliente %d%s:\n", ((list[i].ID == CL_DEFAULT_ID) ? CL_COLOR_RED : CL_COLOR_GREEN), list[i].ID, CL_COLOR_NORMAL);
		for(int j = 0; j < list[i].files_n; j++)
			printf("||\t%s%s%s\n", CL_COLOR_BLUE, list[i].filename[j], CL_COLOR_NORMAL);
		printf("||\n");
	}
	
	return 0;
	
}


static int* cl_menu_parseInput(char* input, int files_n, int* chosenfiles_size){

	if(!input || !files_n || !chosenfiles_size) return NULL;

	char *substring;
	int file_ID, *chosenfiles = calloc(files_n, sizeof(int));
	
	
	*chosenfiles_size = 0;
	if(!(substring = strtok(input, " \n")))
		goto ERROR;
	
	do {
		
		if((file_ID = strtol(substring, NULL, 10)) <= 0 || file_ID > files_n)
			goto ERROR;
		/* Since the user chooses a file from 1 to x, we
		   need to decrement 1 so it's the actual index */
		chosenfiles[(*chosenfiles_size)++] = file_ID-1;
		
	} while((substring = strtok(NULL, " \n")) && (*chosenfiles_size <= files_n));
	
	/* Filters repeated file IDs */
	qsort(chosenfiles, *chosenfiles_size, sizeof(chosenfiles[0]), cl_menu_qsortComp);
	for(int i = 0, j = 1; j < *chosenfiles_size; i++, j++)
		if(chosenfiles[i] == chosenfiles[j]){
			for(int k = i, l = k+1; l < *chosenfiles_size; k++, l++)
				chosenfiles[k] = chosenfiles[l];
			(*chosenfiles_size)--;
		}
	
	return chosenfiles;
	
	ERROR:
		free(chosenfiles);
		return NULL;
	
}


static int* cl_menu_parseInputUpload(char* input, int files_n, struct client_info* list, int list_size, int** chosenIDs, int* chosen_size){
	
	if(!input || !files_n || !list || !list_size || !chosenIDs || !chosen_size)
		return NULL;

	char *delim, *substring, flag_safe;
	int file_ID, client_ID, *chosenfiles;
	chosenfiles = calloc(files_n*list_size, sizeof(int));
	*chosenIDs = calloc(files_n*list_size, sizeof(int));
	
	
	*chosen_size = 0;
	if(!(substring = strtok(input, " \n")))
		goto ERROR;
	
	do {
		
		flag_safe = 0;
		if(!(delim = strchr(substring, CL_CLIENT_DELIM)))
			goto ERROR;
		
		*delim = '\0';
		if((client_ID = strtol(substring, NULL, 10)) <= 0 || client_ID == CL_DEFAULT_ID)
			goto ERROR;
		
		for(int i = 0; i < list_size; i++)
			if(client_ID == list[i].ID){
				flag_safe = 1;
				break;
			}
		if(!flag_safe) goto ERROR;
		
		substring += strlen(substring)+1;
		if((file_ID = strtol(substring, NULL, 10)) <= 0 || file_ID > files_n)
			goto ERROR;
		
		/* Since the user chooses a file from 1 to x, we
		   need to decrement 1 so it's the actual index,
		   but client's ID must be as is */
		
		(*chosenIDs)[(*chosen_size)++]  = client_ID;
		chosenfiles[(*chosen_size)-1] = file_ID-1;
		
	} while((substring = strtok(NULL, " \n")) && (*chosen_size <= files_n*list_size));
	
	/* Filter same ID and fileID input */
	for(int i = 0; i < *chosen_size; i++)
		for(int j = i+1; j < *chosen_size; j++)
			if((*chosenIDs)[i] == (*chosenIDs)[j])
				if(chosenfiles[i] == chosenfiles[j]){
					for(int k = i, l = k+1; l < *chosen_size; k++, l++){
						(*chosenIDs)[k] = (*chosenIDs)[l];
						chosenfiles[k] = chosenfiles[l];
					}
					(*chosen_size)--;
				}

	return chosenfiles;
	
	ERROR:
		free(chosenfiles);
		free(*chosenIDs);
		*chosenIDs = NULL;
		return NULL;
	
}


static struct client_info* cl_menu_fillInfo(struct client_info* list, int list_size, int* chosenfiles, int* chf_size, char ignoreSameID){
	
	if(!list || !list_size || !chosenfiles || !chf_size) return NULL;
	
	char flag_first = 1;
	struct client_info *chosenfiles_info = calloc(1, sizeof(struct client_info));
	
	*chf_size = 0;
	for(int i = 0, j = 0, k = 0; i < list_size; i++){
		
		if(ignoreSameID && list[i].ID == CL_DEFAULT_ID) continue;
		
		for(int l = 0; l < list[i].files_n; k++, l++)
			if(chosenfiles[j] == k){

				if(list[i].ID != chosenfiles_info[*chf_size].ID){
					
					if(!flag_first){
						(*chf_size)++;
						chosenfiles_info = realloc(chosenfiles_info, ((*chf_size)+1)*sizeof(struct client_info));
					}
					
					chosenfiles_info[*chf_size].files_n = 0;
					chosenfiles_info[*chf_size].ID = list[i].ID;
					chosenfiles_info[*chf_size].filename = calloc(1, sizeof(char*));
					chosenfiles_info[*chf_size].filedata = calloc(1, sizeof(char*));
					
					flag_first = 0;
					
				}
				
				chosenfiles_info[*chf_size].files_n++;
				if(chosenfiles_info[*chf_size].files_n > 1){
					chosenfiles_info[*chf_size].filename = realloc(chosenfiles_info[*chf_size].filename, (chosenfiles_info[*chf_size].files_n)*sizeof(char*));
					chosenfiles_info[*chf_size].filedata = realloc(chosenfiles_info[*chf_size].filedata, (chosenfiles_info[*chf_size].files_n)*sizeof(char*));
				}
				
				chosenfiles_info[*chf_size].filename[chosenfiles_info[*chf_size].files_n-1] = strcpy(calloc(strlen(list[i].filename[l])+1, sizeof(char)), list[i].filename[l]);
				chosenfiles_info[*chf_size].filedata[chosenfiles_info[*chf_size].files_n-1] = calloc(1, sizeof(char));
				
				j++;
				
			}
		
	}
	(*chf_size)++;
	
	return chosenfiles_info;
	
}


static struct client_info* cl_menu_fillInfoUpload(char** list, int* chosenIDs, int* chosenfiles, int chosen_size, int* chosenfiles_info_size){
	
	if(!list || !chosenIDs || !chosenfiles || !chosen_size || !chosenfiles_info_size)
		return NULL;
	
	int max = 0, unique_n = 1;
	long filedata_size;
	
	/* Gets unique IDs count */
    for(int outer = 1, is_unique = 1; outer < chosen_size; outer++, is_unique = 1){
		
        for(int inner = 0; is_unique && inner < outer; inner++)
            if(chosenIDs[inner] == chosenIDs[outer]) is_unique = 0;
        if(is_unique) unique_n++;
		
    }
	*chosenfiles_info_size = unique_n;
	
	/* Gets max ID number */
	for(int i = 0; i < chosen_size; i++)
		if(chosenIDs[i] > max)
			max = chosenIDs[i];
	
	/* Maps clients' IDs to a sequenced order */
	int buffer[max+1];
	memset(buffer, max+1, sizeof(buffer));
	for(int i = 0, c = 0; i < chosen_size; i++){
		if(buffer[chosenIDs[i]] >= c) //maps repeated IDs to the least index number
			buffer[chosenIDs[i]] = c++;
	}
	
	
	struct client_info *chosenfiles_info = calloc(*chosenfiles_info_size, sizeof(struct client_info));
	for(int i = 0; i < *chosenfiles_info_size; i++){
		chosenfiles_info[i].filename = calloc(1, sizeof(char*));
		chosenfiles_info[i].filedata = calloc(1, sizeof(char*));
	}
	
	for(int i = 0, idx, files_n; i < chosen_size; i++){
		
		idx = buffer[chosenIDs[i]];
		files_n = chosenfiles_info[idx].files_n;
		
		chosenfiles_info[idx].ID = chosenIDs[i];
		chosenfiles_info[idx].filename[files_n] = strcpy(calloc(strlen(list[chosenfiles[i]])+1, sizeof(char)), list[chosenfiles[i]]);
		chosenfiles_info[idx].filedata[files_n] = cl_files_readFile(cl_files_getFileFullPath(".", list[chosenfiles[i]]), &filedata_size);
		chosenfiles_info[idx].files_n++;
		
		chosenfiles_info[idx].filename = realloc(chosenfiles_info[idx].filename, (files_n+2)*sizeof(char*));
		chosenfiles_info[idx].filedata = realloc(chosenfiles_info[idx].filedata, (files_n+2)*sizeof(char*));
		
	}
	
	return chosenfiles_info;
	
}


static void cl_menu_clrscr(void){
	if(NW_LOG == 1) return;
    #if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
        system("clear");
		return;
    #endif
    #if defined(_WIN32) || defined(_WIN64)
        system("cls");
    #endif
	
}


static int cl_menu_qsortComp(const void* x, const void* y){
	
	return *(int*)x < *(int*)y ? -1 : (*(int*)x > *(int*)y);
	
}