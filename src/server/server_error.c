#include "server_utils.h"


const char* sv_error_assess(char ret_code){
	
	switch(ret_code){
		case ERR_CNFOUND:
			return "O cliente solicitado não foi encontrado no banco de dados\n";
		case ERR_FNFOUND:
			return "O arquivo solicitado pelo cliente não existe no cliente-alvo\n";
		case ERR_SRVUPDATE:
			return "Houveram atualizações no servidor que impediram a conclusão da operação\n";
		default:
			return nw_assessError(ret_code);
	}
	
}