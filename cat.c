#include <windows.h>

#define nullptr (0)
HANDLE stdout = nullptr;
HANDLE stderr = nullptr;
HANDLE stdin = nullptr;

/* how much of a file to read at once */
enum { chunksize = 1 << 16	 };

static void catstdin(void)
{
	char ch = 0;
	DWORD bytes_read = 0;

	/* read till eof or newline */
	do {
		if (ReadFile(stdin, &ch, sizeof(ch), &bytes_read, nullptr) == 0) {
			WriteFile(stderr, "Error: could not read stdin", 27, nullptr, nullptr);
			ExitProcess(GetLastError());
		}
		
		if (WriteFile(stdout, &ch, bytes_read, nullptr, nullptr) == 0) {
			WriteFile(stderr, "Error: could not write to stdout", 32, nullptr, nullptr);
			ExitProcess(GetLastError());
		}
	} while (bytes_read != 0 && ch != '\n');
}

static void catfile(wchar_t *filepath)
{
	HANDLE filehandle = CreateFileW(filepath, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (filehandle == INVALID_HANDLE_VALUE) {
		/* set console to red text */
		SetConsoleTextAttribute(stderr, FOREGROUND_RED);

		/* write error message */
		WriteConsoleA(stderr, "Error: could not open ", 22, nullptr, nullptr);
		WriteConsoleW(stderr, filepath, lstrlenW(filepath) , nullptr, nullptr);
		
		/* set console color back to white */
		SetConsoleTextAttribute(stderr, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);
		ExitProcess(GetLastError());
	}

	DWORD bytes_read = 1;
	char output_buffer[chunksize];

	/* read the file in chunks */
	while (bytes_read != 0) {
		if (ReadFile(filehandle, output_buffer, chunksize, &bytes_read, nullptr) == 0) {
			WriteConsoleA(stderr, "Error: could not read ", 22, nullptr, nullptr);
			WriteConsoleW(stderr, filepath, lstrlenW(filepath), nullptr, nullptr);
			ExitProcess(GetLastError());
		}

		if (WriteFile(stdout, output_buffer, bytes_read, nullptr, nullptr) == 0) {
			WriteConsoleA(stderr, "Error: could not write to ", 26, nullptr, nullptr);
			WriteConsoleW(stderr, filepath, lstrlenW(filepath), nullptr, nullptr);
			ExitProcess(GetLastError());
		}
	}

	CloseHandle(filehandle); /* close file */
}

void __cdecl mainCRTStartup(void)
{
	/* setup global variables */
	stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	stderr = GetStdHandle(STD_ERROR_HANDLE);
	stdin = GetStdHandle(STD_INPUT_HANDLE);

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
	LocalFree(argv - 1);

	ExitProcess(GetLastError());
}
