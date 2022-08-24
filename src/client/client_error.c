#include "client_utils.h"


const char* cl_error_assess(char ret_code){

	switch(ret_code){
		case ERR_CANCEL:
			return "Operação cancelada\n";
		case ERR_FEXIST:
			return "Um arquivo com o nome provido já existe localmente\n";
		case ERR_FNFOUND:
			return "O arquivo escolhido não foi encontrado\n";
		case ERR_WRITE:
			return "Falha ao salvar o arquivo localmente\n";
		case ERR_FREMOVE:
			return "Falha ao deletar o arquivo\n";
		case ERR_FOPEN:
			return "Falha ao abrir o arquivo\n";
		default:
			return nw_assessError(ret_code);
	}

}