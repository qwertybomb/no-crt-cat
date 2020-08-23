#include <windows.h>

/* global variables */
HANDLE stdout = NULL;
HANDLE stderr = NULL;
HANDLE stdin = NULL;
char *output_buffer = NULL;

/* how much of a file to read at once */
#define chunksize (1 << 16)

static void catstdin(void)
{
	char ch = 0;
	DWORD bytes_read = 0;
	BOOL result = 0;

	/* read till eof or newline */
	do { 
		result = !!ReadFile(stdin, &ch, sizeof(ch), &bytes_read, NULL);
		result &= !!WriteFile(stdout, &ch, bytes_read, NULL, NULL);
	} while (result && bytes_read != 0 && ch != '\n');
}

static void catfile(wchar_t *filepath)
{
	HANDLE filehandle = CreateFileW(filepath, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (filehandle == INVALID_HANDLE_VALUE) {
		/* set console to red text */
		SetConsoleTextAttribute(stderr, FOREGROUND_RED);
		WriteFile(stderr, "Error: could not open ", 22, NULL, NULL);
		while (*filepath) WriteFile(stderr, filepath++, 1, NULL, NULL);
		/* set console color back to white */
		SetConsoleTextAttribute(stderr, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);
		ExitProcess(GetLastError());
	}
	DWORD filelength = GetFileSize(filehandle, NULL);
	DWORD bytes_read = 1;
	BOOL result = 1;

	/* read the file in chunks */
	while (result && bytes_read != 0) {
		result = !!ReadFile(filehandle, output_buffer, chunksize, &bytes_read, NULL);
		result &= !!WriteFile(stdout, output_buffer, bytes_read, NULL, NULL);
	}

	CloseHandle(filehandle); /* close file */
}

void __cdecl mainCRTStartup(void)
{
	/* setup global variables */
	stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	stderr = GetStdHandle(STD_ERROR_HANDLE);
	stdin = GetStdHandle(STD_INPUT_HANDLE);
	output_buffer = HeapAlloc(GetProcessHeap(), 0, chunksize);

	
	/* get argc and argv */
	int argc;
	wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc) + 1;
	argc--; /* the first arg is always the program name */

	switch (argc) {
		case 0:
			for (;;) catstdin();
			break;
		default:
			for (int i = 0; i < argc; ++i) {
				if (!lstrcmpW(argv[i], L"-"))
					catstdin();
				else
					catfile(argv[i]);
			}
	}

	/* free memory */
	HeapFree(GetProcessHeap(), 0, output_buffer);
	LocalFree(argv);

	/* exit */
	ExitProcess(0);
}
