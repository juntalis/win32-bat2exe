#include "common.h"
#include "compile.h"
#include <io.h>
#include <direct.h>


#define access _access
#define fgetcnl _fgetc_nolock
#define ftellnl _ftell_nolock
#define fseeknl _fseek_nolock
size_t get_file_length(FILE* pFile)
{
	long lCurrPos, lEndPos;
	size_t result = -1;
	lCurrPos = ftellnl(pFile);
	if(lCurrPos == -1)
		return result;
	if(fseeknl(pFile, 0L, SEEK_END) == -1)
		return result;
	lEndPos = ftellnl(pFile);
	if(lEndPos == -1)
		return result;
	result = (size_t)(lEndPos - lCurrPos);
	if(fseeknl(pFile, 0L, SEEK_SET) == -1)
		return -1;
	return result;
}

int main(int argc, char* argv[])
{
	char* output;
	char* input;
	FILE* fwork;
	size_t output_length;
	size_t input_length;
	size_t execlen = 0;
	int result = 0;
	char* cflags = NULL;
	char* exec_cmd = NULL;
	char rc_cmd[128] = "\0";
	bool no_compression = false;
	bool exec_temp = false;
	int i = 0;
	
	if(_access("obj", 00) != 0) {
		if(_mkdir("obj") != 0) {
			fatal("Could not create intermediate folder.");
		}
	}
	
	for(i = 1; i < argc; i++) {
		if((_stricmp(argv[i], "--no-compression") == 0) || (_stricmp(argv[i], "-n") == 0)){
			no_compression = true;
		} else if((_stricmp(argv[i], "--no-local") == 0) || (_stricmp(argv[i], "-l") == 0)) {
			exec_temp = true;
		} else if((_stricmp(argv[i], "--help") == 0)||(_stricmp(argv[i], "-h") == 0)) {
			out("Usage: %s [--no-compression|-n] [--no-local|-l] [-h|--help]\n", argv[0]);
			exit(0);
		}
	}
	
	if(_access(STUB_OUT, 00) == 0) _unlink(STUB_OUT);
	
	if((fwork = fopen(STUB_IN, "rb")) == NULL) {
		fatal("Failed to open input file.");
	}
	
	if((input_length = get_file_length(fwork)) == -1) {
		fatal("Could not get filesize of " STUB_IN ".");
	}
	
	input = (char*)malloc((input_length+1)*sizeof(char));
	memset((void*)input, 0, (input_length+1)*sizeof(char));
	if(input == NULL) {
		fatal("Could not allocate buffer for uncompressed data.");
	}
	
	if(!fread((void*)input, sizeof(char), input_length, fwork)) {
		free(input);
		fatal("Failed to read input file.");
	}
	
	fclose(fwork);
	output_length = snappy_max_compressed_length(input_length);
	output = (char*)malloc(output_length);
	if(output == NULL) {
		free(input);
		fatal("Could not allocate buffer for compressed data.");
	}
	
	if (snappy_compress(input, input_length, output, &output_length) == SNAPPY_OK) {
		free(input);
		outl("Compressed data.");
		if((fwork = fopen(STUB_OUT, "wb")) == NULL) {
			fatal("Failed to open output file.");
		}
		if(!fwrite((const void*)output, sizeof(char), output_length / sizeof(char), fwork)) {
			fatal("Failed to write to output file.");
		}
		fclose(fwork);
	} else {
		free(input);
		free(output);
		fatal("Failed to compress stub file.");
	}
	free(output);

	if(_access(STUB_OBJ, 00) == 0) _unlink(STUB_OBJ);
	if(_access(EDIT_OBJ, 00) == 0) _unlink(EDIT_OBJ);
	if(_access(STUB_RES, 00) == 0) _unlink(STUB_RES);
	if(_access(EDIT_RES, 00) == 0) _unlink(EDIT_RES);
	
	// Compile stub.rc
	outl("\nCompiling resource file.");
	strcpy(rc_cmd, RC_CMD);
	if(!exec_temp) strcat(rc_cmd, RC_EXEC_LOCAL);
	if(no_compression) strcat(rc_cmd, RC_NO_COMPRESS);
	strcat(rc_cmd, STUB_RC);
	
	if((result = system(rc_cmd)) != EXIT_SUCCESS) {
		fatal("Failed to compile " STUB_RC "\n\nrc.exe returned code %d", result);
	}
	
	
	// cl.exe -Foobj\stub.obj src\stub.c obj\stub.res -link -LTCG -OPT:REF -OPT:ICF\0
	execlen = strlen(CL_CMD CL_END);
	if((cflags = getenv("CFLAGS")) != NULL) {
		execlen += lstrlenA(cflags) + 1;
	}
	
	if(!exec_temp) execlen += 15;
	if(no_compression) execlen += 19;
	
	exec_cmd = (char*)malloc(sizeof(char) * execlen);
	if(exec_cmd == NULL) {
		fatal("Could not allocate buffer for command-line.");
	}
	
	memset((void*)exec_cmd, 0, sizeof(char) * execlen);
	strcpy(exec_cmd, CL_CMD);
	if(cflags != NULL) {
		strcat(exec_cmd, cflags);
		strcat(exec_cmd, " ");
	}
	if(!exec_temp) {
		out("Enabling local-exec.\n");
		strcat(exec_cmd, CL_EXEC_LOCAL);
	}
	if(no_compression) {
		out("Enabling resource compression.\n");
		strcat(exec_cmd, CL_NO_COMPRESS);
	}
	
	strcat(exec_cmd, CL_END);

	outl("Compiling source files.\n");
	if((result = system(exec_cmd)) != EXIT_SUCCESS) {
		free(exec_cmd);
		fatal("Failed to compile stub.c\n\ncl.exe returned code %d", result);
	}
	free(exec_cmd);
	
	// Compile bat2exe.rc
	strcpy(rc_cmd, "");
	outl("\nCompiling resource file.");
	strcpy(rc_cmd, RC_EDIT_CMD);
	if(!exec_temp) strcat(rc_cmd, RC_EXEC_LOCAL);
	if(no_compression) strcat(rc_cmd, RC_NO_COMPRESS);
	strcat(rc_cmd, EDIT_RC);
	
	if((result = system(rc_cmd)) != EXIT_SUCCESS) {
		fatal("Failed to compile " EDIT_RC "\n\nrc.exe returned code %d", result);
	}
	
	// cl.exe %CFLAGS% -Foobj\bat2exe.obj src\bat2exe.c obj\bat2exe.res -link -LTCG -OPT:REF -OPT:ICF\0
	execlen = strlen(CL_EDIT_CMD CL_EDIT_END);
	if((cflags = getenv("CFLAGS")) != NULL) {
		execlen += lstrlenA(cflags) + 1;
	}
	
	if(!exec_temp) execlen += 15;
	if(no_compression) execlen += 19;
	
	exec_cmd = (char*)malloc(sizeof(char) * execlen);
	if(exec_cmd == NULL) {
		fatal("Could not allocate buffer for command-line.");
	}
	memset((void*)exec_cmd, 0, sizeof(char) * execlen);
	strcpy(exec_cmd, CL_EDIT_CMD);
	if(cflags != NULL) {
		strcat(exec_cmd, cflags);
		strcat(exec_cmd, " ");
	}
	if(!exec_temp) {
		out("Enabling local-exec.\n");
		strcat(exec_cmd, CL_EXEC_LOCAL);
	}
	if(no_compression) {
		out("Enabling resource compression.\n");
		strcat(exec_cmd, CL_NO_COMPRESS);
	}
	strcat(exec_cmd, CL_EDIT_END);
	if((result = system(exec_cmd)) != EXIT_SUCCESS) {
		free(exec_cmd);
		fatal("Failed to compile batcompile.c\n\ncl.exe returned code %d", result);
	}
	free(exec_cmd);
	return 0;
}