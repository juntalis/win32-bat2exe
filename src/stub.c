#pragma comment(lib, "psapi.lib")
#include "common.h"
#include <psapi.h>
#include <time.h>
#include <process.h>
#include <io.h>
#include "resource.h"
#include <tlhelp32.h>

static char thisPath[_MAX_PATH] = "\0";

// Search each process in the snapshot for id.
static bool find_proc_id( HANDLE snap, DWORD id, LPPROCESSENTRY32 ppe )
{
	bool fOk = false;
	ppe->dwSize = sizeof(PROCESSENTRY32);
	for (fOk = Process32First( snap, ppe ); fOk; fOk = Process32Next( snap, ppe ))
		if (ppe->th32ProcessID == id)
			break;
		
	return fOk;
}

// Search each process in the snapshot for id.
static bool find_proc_name( HANDLE snap, DWORD id, LPMODULEENTRY32 pme )
{
	bool fOk = false;
	char myPath[_MAX_PATH];
	if(!GetModuleFileNameA(NULL, myPath, _MAX_PATH)) {
		fatal("Could not get path of current executable.");
	}
	pme->dwSize = sizeof(MODULEENTRY32);
	for (fOk = Module32First( snap, pme ); fOk; fOk = Module32Next( snap, pme )) {
		if (pme->th32ModuleID == id) {
			if (_stricmp(pme->szExePath, myPath) == 0) {
				fOk = false;
			}
			break;
		}
	}
	return fOk;
}

// Obtain the process and thread identifiers of the parent process.
static bool get_parent( LPPROCESS_INFORMATION ppi )
{
	HANDLE hSnap;
	PROCESSENTRY32 pe;
	THREADENTRY32	te;
	DWORD	id = GetCurrentProcessId();
	bool	 fOk;

	
	hSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS|TH32CS_SNAPTHREAD, id );
	if (hSnap == INVALID_HANDLE_VALUE)
		return false;
	
	find_proc_id( hSnap, id, &pe );
	if (!find_proc_id( hSnap, pe.th32ParentProcessID, &pe )) {
		CloseHandle( hSnap );
		return false;
	}
	
	while(_stricmp(pe.szExeFile, "cmd.exe") == 0) {
		if (!find_proc_id( hSnap, pe.th32ParentProcessID, &pe )) {
			CloseHandle( hSnap );
			return false;
		}
	}

	te.dwSize = sizeof(te);
	for (fOk = Thread32First( hSnap, &te ); fOk; fOk = Thread32Next( hSnap, &te ))
		if (te.th32OwnerProcessID == pe.th32ProcessID)
			break;

	CloseHandle(hSnap);
	ppi->dwProcessId = pe.th32ProcessID;
	ppi->dwThreadId	= te.th32ThreadID;

	return fOk;
}

static bool eliminate_recursion()
{
	HANDLE hProcess = NULL;
	PROCESS_INFORMATION pi;
	char parentPath[_MAX_PATH] = "\0";
	if (!get_parent(&pi)) {
		error("Could not get parent process.");
		return false;
	}
	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pi.dwProcessId);
	if( !hProcess ) {
		error("Couldn't open parent process.\n");
		return false;
	}
	GetModuleFileNameA(NULL, thisPath, _MAX_PATH);
	GetModuleFileNameExA(hProcess, 0, parentPath, _MAX_PATH);
	if (_stricmp(parentPath, thisPath) == 0) {
		debug("Eliminating recursion..");
		return false;
	}
	CloseHandle(hProcess);
	return true;
}

int main(int argc, char* argv[])
{
	size_t output_length;
	size_t input_length;
	HRSRC hrsrc;
	HGLOBAL hglb;
	LPBYTE bindata;
	FILE* fwork;
	const char* input;
	char* output;
	char tmpbuff[_MAX_PATH] = "\0";
	char* substr = NULL;
	DWORD tmpbufflen = _MAX_PATH;
	int result = EXIT_SUCCESS;
	bool execLocal = false;
#ifndef NO_COMPRESSION
	snappy_status status;
#endif

	if(!eliminate_recursion())
		return EXIT_FAILURE;
	
	hrsrc = FindResource(NULL, MAKEINTRESOURCE(IDR_EXECLOCAL_FLAG), "FLAGS");
	if (hrsrc && (hglb = LoadResource(NULL, hrsrc))) {
		bindata = (LPBYTE)LockResource(hglb);
		if(((char*)bindata)[0] == '1') execLocal = true;
	}
	
	hrsrc = FindResource(NULL, MAKEINTRESOURCE(IDR_DEFAULT_EXEC), "EXEC");
	if (hrsrc && (hglb = LoadResource(NULL, hrsrc))) {
		bindata = (LPBYTE)LockResource(hglb);
		input_length = (size_t)SizeofResource(NULL, hrsrc);
		input = (const char*)malloc(sizeof(const char) * input_length);
		if(input == NULL) {
			fatal("Could not allocate buffer for compressed data.");
		}
		memcpy((void*)input, (const void*)bindata, input_length);
	} else {
		fatal("Could not find and load packaged data.");
	}
	
	#ifndef NO_COMPRESSION
	if ((status = snappy_uncompressed_length(input, input_length, &output_length)) != SNAPPY_OK) {
		if(status == SNAPPY_INVALID_INPUT) {
			error("Invalid input.");
		} else if(status == SNAPPY_BUFFER_TOO_SMALL) {
			error("Buffer too small.");
		}
		free((void*)input);
		fatal("Could not find length of uncompressed data.");
	}

	output = (char*)malloc(output_length);
	if(output == NULL) {
		free((void*)input);
		fatal("Could not allocate buffer for uncompressed data.");
	}
	
	if ((status = snappy_uncompress((const char*)input, input_length, output, &output_length)) != SNAPPY_OK) {
		if(status == SNAPPY_INVALID_INPUT) {
			error("Invalid input.");
		} else if(status == SNAPPY_BUFFER_TOO_SMALL) {
			error("Buffer too small.");
		}
		error("Could not uncompress packaged data.");
		goto exception;
	}
	#else
	output = (char*)input;
	output_length = input_length;
	#endif
	
	if(execLocal) {
		strcpy(tmpbuff, thisPath);
		substr = strrchr((const char*)tmpbuff, '\\');
		while(*(substr++) != '\0')
			*(substr) = '\0';
	} else {
		if(!GetTempPathA(tmpbufflen, tmpbuff)) {
			error("Could not get temporary folder.");
			goto exception;
		}
	}
	
	srand(((unsigned int)time(NULL)) | 1);
	sprintf(tmpbuff, "%s%i.bat", tmpbuff, rand() % 10 + 1);
	
	debug("Writing temporary file to %s..", tmpbuff);
	if((fwork = fopen(tmpbuff, "wb")) == NULL) {
		error("Could not open temporary file.");
		goto exception;
	}

	if(!fwrite((const void*)output, sizeof(char), output_length / sizeof(char), fwork)) {
		fclose(fwork);
		error("Failed to write to temporary file.");
		goto exception;
	}
	fclose(fwork);
	argv[0] = tmpbuff;
	result = _spawnv(_P_WAIT, (const char*)tmpbuff, (const char* const*)argv);
	if(_access(tmpbuff, 00) == 0) _unlink(tmpbuff);
	
	goto finally;
exception:
	result = EXIT_FAILURE;
	goto finally;
finally:
	#ifndef NO_COMPRESSION
	free(output);
	#endif
	free((void*)input);
	return result;
}
