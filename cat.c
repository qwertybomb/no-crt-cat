#include <windows.h>

/* global variables */
HANDLE stdout = NULL;
HANDLE stdin = NULL;
char *input_buffer = NULL;
CONSOLE_READCONSOLE_CONTROL crc = { .nLength = sizeof(crc), .dwCtrlWakeupMask = 1 << '\n' };
char *output_buffer = NULL;
DWORD output_capacity = 0;

/* There is only CommandLineToArgvW so a version for ascii is needed */
LPSTR *CommandLineToArgvA(LPSTR lpCmdLine, INT *pNumArgs)
{
	int retval;
	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, NULL, 0);
	if (!SUCCEEDED(retval))
		return NULL;

	LPWSTR lpWideCharStr = HeapAlloc(GetProcessHeap(), 0, retval * sizeof(WCHAR));
	if (lpWideCharStr == NULL)
		return NULL;

	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, lpWideCharStr, retval);
	if (!SUCCEEDED(retval)) {
		HeapFree(GetProcessHeap(), 0, lpWideCharStr);
		return NULL;
	}

	int numArgs;
	LPWSTR *args;
	args = CommandLineToArgvW(lpWideCharStr, &numArgs);
	HeapFree(GetProcessHeap(), 0, lpWideCharStr);
	if (args == NULL)
		return NULL;

	int storage = numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i) {
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, NULL, 0, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval)) {
			LocalFree(args);
			return NULL;
		}

		storage += retval;
	}

	LPSTR *result = (LPSTR *)LocalAlloc(LMEM_FIXED, storage);
	if (result == NULL) {
		LocalFree(args);
		return NULL;
	}

	int bufLen = storage - numArgs * sizeof(LPSTR);
	LPSTR buffer = ((LPSTR)result) + numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i) {
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, buffer, bufLen, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval)) {
			LocalFree(result);
			LocalFree(args);
			return NULL;
		}

		result[i] = buffer;
		buffer += retval;
		bufLen -= retval;
	}

	LocalFree(args);

	*pNumArgs = numArgs;
	return result;
}


static void lmemcpy(char *dest, const char *src, DWORD len)
{
	/* copy 4 bytes at once */
	for (; len > 3; len -= 4, dest += 4, src += 4)
		*(long *)dest = *(long *)src;
	while (len--)
		*dest++ = *src++;
}

static void catstdin(void)
{
	DWORD chars_read = 0;
	ReadConsoleA(stdin, input_buffer, 2048, &chars_read, &crc);
	WriteConsoleA(stdout, input_buffer, chars_read, NULL, NULL);
}

static void catfile(char *filepath)
{
	HANDLE filehandle = CreateFileA(filepath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (filehandle == INVALID_HANDLE_VALUE) {
		WriteConsoleA(stdout, "Error could not open file: ", 27, NULL, NULL);
		WriteConsoleA(stdout, filepath, lstrlenA(filepath), NULL, NULL);
		ExitProcess(GetLastError());
	}
	DWORD filelength = GetFileSize(filehandle, NULL);
	if (filelength > output_capacity) { /* see if we need to allocate more memory */
		char *new_buffer = HeapAlloc(GetProcessHeap(), 0, output_capacity * 2); /* copy the data from the old memory to the new memory */
		lmemcpy(new_buffer, output_buffer, output_capacity);
		HeapFree(GetProcessHeap(), 0, output_buffer); /* free old memory */
		output_capacity *= 2;
		++output_capacity;
		output_buffer = new_buffer;
	}

	ReadFile(filehandle, output_buffer, filelength, NULL, NULL);
	WriteConsoleA(stdout, output_buffer, filelength, NULL, NULL);
	CloseHandle(filehandle); /* close file */
}

void __cdecl mainCRTStartup(void)
{
	/* setup global variables */
	stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	stdin = GetStdHandle(STD_INPUT_HANDLE);
	input_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 2048);
	output_buffer = HeapAlloc(GetProcessHeap(), 0, 2048);
	output_capacity = 2048;

	/* get argc and argv */
	int argc;
	char **argv = CommandLineToArgvA(GetCommandLineA(), &argc) + 1;
	argc--; /* the first arg is always the program name */

	switch (argc) {
		case 0:
			for (;;) catstdin();
			break;
		default:
			for (int i = 0; i < argc; ++i) {
				if (!lstrcmpA(argv[i], "-"))
					catstdin();
				else
					catfile(argv[i]);
			}
	}

	/* free memory */
	HeapFree(GetProcessHeap(), 0, input_buffer);
	HeapFree(GetProcessHeap(), 0, output_buffer);
	LocalFree(argv);

	/* exit */
	ExitProcess(0);
}
