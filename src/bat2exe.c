#include "common.h"
#include <io.h>
#include <direct.h>
#include "resource.h"
#include "bat2exeres.h"

static char* exe_name = NULL;

static void exit_usage(int status)
{
	out("Usage: %s [[--temp|-t]|[--local|-l]] [-h|--help] file.bat file.exe\n\n"
		"  --local, -l    Execute the bat file from the same file as exe. (Affects %%0)\n"
		"  --temp, -t     Execute the bat file from temporary folder. (Affects %%0)\n"
		"  --help, -h     Print this message\n", exe_name);
	exit(status);
}

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

static bool end_update(HANDLE hExe, char* inexe)
{

	if(!EndUpdateResource(hExe, FALSE)) {
		error("Could not update %s.", inexe);
		return false;
	}
	return true;
}

static bool change_exec_location(HANDLE hExe, char* inexe, int exec_type)
{
	const char* exec_flag = exec_type ? "0" : "1";
	if(!UpdateResource(hExe, "FLAGS", MAKEINTRESOURCE(IDR_EXECLOCAL_FLAG), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPVOID)exec_flag, (DWORD)sizeof(char) * 2)) {
		error("Could not update resource in %s.", inexe);
		return false;
	}
	return true;
}

static bool change_batchfile(HANDLE hExe, char* inexe, char* inbat)
{
	FILE* fwork;
	char* output;
	char* input;
	size_t output_length;
	size_t input_length;
	
	if((fwork = fopen(inbat, "rb")) == NULL) {
		error("Failed to open input file.");
		return false;
	}
	
	if((input_length = get_file_length(fwork)) == -1) {
		error("Could not get filesize of %s.", inbat);
		return false;
	}
	
	input = (char*)malloc((input_length+1)*sizeof(char));
	memset((void*)input, 0, (input_length+1)*sizeof(char));
	if(input == NULL) {
		error("Could not allocate buffer for uncompressed data.");
		return false;
	}
	
	if(!fread((void*)input, sizeof(char), input_length, fwork)) {
		free(input);
		error("Failed to read input file.");
		return false;
	}
	
	fclose(fwork);
	output_length = snappy_max_compressed_length(input_length);
	output = (char*)malloc(output_length);
	if(output == NULL) {
		free(input);
		error("Could not allocate buffer for compressed data.");
		return false;
	}
	
	if (snappy_compress(input, input_length, output, &output_length) == SNAPPY_OK) {
		free(input);
		outl("Successfully compressed data.");
		if(!UpdateResource(
			hExe,	// update resource handle 
			"EXEC",	// type
			MAKEINTRESOURCE(IDR_DEFAULT_EXEC),	// name
			MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),  // language
			(LPVOID)output,		// ptr to resource info 
			(DWORD)output_length)) {// size of resource info.
				free(output);
				error("Could not update resource in %s.", inexe);
				return false;
		}
	} else {
		free(input);
		free(output);
		error("Failed to compress stub file.");
		return false;
	}
	
	free(output);
	return end_update(hExe, inexe);
}

int main(int argc, char* argv[])
{
	HANDLE hExe;
	int exec_type = -1;
	FILE* fwork;
	char* inexe = NULL;
	char* inbat = NULL;
	const char* data = NULL;
	HRSRC hrsrc;
	HGLOBAL hglb;
	LPBYTE bindata;
	size_t data_len = 0;
	int i = 0;
	
	exe_name = argv[0];
	for(i = 1; i < argc; i++) {
		if((_stricmp(argv[i], "--local") == 0) || (_stricmp(argv[i], "-l") == 0)) {
			exec_type = 0;
		} else if((_stricmp(argv[i], "--temp") == 0) || (_stricmp(argv[i], "-t") == 0)) {
			exec_type = 1;
		} else if((_stricmp(argv[i], "--help") == 0)||(_stricmp(argv[i], "-h") == 0)) {
			exit_usage(EXIT_SUCCESS);
		} else if(inbat == NULL) {
			inbat = argv[i];
		} else if(inexe == NULL) {
			inexe = argv[i];
		} else {
			error("Extra argument added.");
			exit_usage(EXIT_FAILURE);
		}
	}
	if(((inbat == NULL) || (inexe == NULL)) && (exec_type == -1)) {
		error("Missing an input.");
		exit_usage(EXIT_FAILURE);
	}
	
	if(_access(inbat, 00) != 0) {
		fatal("%s does not exist.", inbat);
	}
	
	if(_access(inexe, 00) != 0) {
		hrsrc = FindResource(NULL, "IDR_CONSOLE_BAT", "STUBS");
		if (hrsrc && (hglb = LoadResource(NULL, hrsrc))) {
			bindata = (LPBYTE)LockResource(hglb);
			data_len = (size_t)SizeofResource(NULL, hrsrc);
			data = (const char*)malloc(sizeof(const char) * data_len);
			if(data == NULL) {
				fatal("Could not allocate buffer for compressed data.");
			}
			memcpy((void*)data, (const void*)bindata, data_len);
			if((fwork = fopen(inexe, "wb")) == NULL) {
				free((void*)data);
				fatal("Could not open new exe file: %s", inexe);
			}
			if(!fwrite((const void*)data, sizeof(char), data_len / sizeof(char), fwork)) {
				fclose(fwork);
				free((void*)data);
				fatal("Failed to write to exe file: %s.", inexe);
			}
			fclose(fwork);
			free((void*)data);
		} else {
			fatal("Could not find and load packaged data.");
		}
	}
	
	hExe = BeginUpdateResource(inexe, TRUE);
	if(hExe == NULL) {
		fatal("Could not open %s for updating.", inexe);
	}
	
	if(exec_type != -1)
		if(!change_exec_location(hExe, inexe, exec_type))
			return EXIT_FAILURE;
	
	if(!change_batchfile(hExe, inexe, inbat))
		return EXIT_FAILURE;
	out("Successsfully created exe.\n");
	return EXIT_SUCCESS;
}