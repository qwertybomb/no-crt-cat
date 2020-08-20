#include <windows.h>

/* global variables */
HANDLE stdout = NULL;
HANDLE stdin = NULL;
char *input_buffer = NULL;
CONSOLE_READCONSOLE_CONTROL crc = { .nLength = sizeof(crc),  .dwCtrlWakeupMask = 1 << '\n' };
char *output_buffer = NULL;
DWORD output_capacity = 0;

/* There is only CommandLineToArgvW so version for ascii is needed */
static LPSTR *CommandLineToArgvA(_In_opt_ LPCSTR lpCmdLine, _Out_ int *pNumArgs)
{
    DWORD argc;
    LPSTR *argv;
    LPSTR s;
    LPSTR d;
    LPSTR cmdline;
    int qcount, bcount;

    if (!pNumArgs || *lpCmdLine == 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    /* --- First count the arguments */
    argc = 1;
    s = lpCmdLine;
    /* The first argument, the executable path, follows special rules */
    if (*s == '"') {
        /* The executable path ends at the next quote, no matter what */
        s++;
        while (*s)
            if (*s++ == '"')
                break;
    }
    else {
        /* The executable path ends at the next space, no matter what */
        while (*s && *s != ' ' && *s != '\t')
            s++;
    }
    /* skip to the first argument, if any */
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s)
        argc++;

    /* Analyze the remaining arguments */
    qcount = bcount = 0;
    while (*s) {
        if ((*s == ' ' || *s == '\t') && qcount == 0) {
            /* skip to the next argument and count it if any */
            while (*s == ' ' || *s == '\t')
                s++;
            if (*s)
                argc++;
            bcount = 0;
        }
        else if (*s == '\\') {
            /* '\', count them */
            bcount++;
            s++;
        }
        else if (*s == '"') {
            /* '"' */
            if ((bcount & 1) == 0)
                qcount++; /* unescaped '"' */
            s++;
            bcount = 0;
            /* consecutive quotes, see comment in copying code below */
            while (*s == '"') {
                qcount++;
                s++;
            }
            qcount = qcount % 3;
            if (qcount == 2)
                qcount = 0;
        }
        else {
            /* a regular character */
            bcount = 0;
            s++;
        }
    }

    /* Allocate in a single lump, the string array, and the strings that go
     * with it. This way the caller can make a single LocalFree() call to free
     * both, as per MSDN.
     */
    
    argv = LocalAlloc(LMEM_FIXED, (argc + 1) * sizeof(LPSTR) + (lstrlenA(lpCmdLine) + 1) * sizeof(char));
    if (!argv)
        return NULL;
    cmdline = (LPSTR)(argv + argc + 1);
    lstrcpyA(cmdline, lpCmdLine);

    /* --- Then split and copy the arguments */
    argv[0] = d = cmdline;
    argc = 1;
    /* The first argument, the executable path, follows special rules */
    if (*d == '"') {
        /* The executable path ends at the next quote, no matter what */
        s = d + 1;
        while (*s) {
            if (*s == '"') {
                s++;
                break;
            }
            *d++ = *s++;
        }
    }
    else {
        /* The executable path ends at the next space, no matter what */
        while (*d && *d != ' ' && *d != '\t')
            d++;
        s = d;
        if (*s)
            s++;
    }
    /* close the executable path */
    *d++ = 0;
    /* skip to the first argument and initialize it if any */
    while (*s == ' ' || *s == '\t')
        s++;
    if (!*s) {
        /* There are no parameters so we are all done */
        argv[argc] = NULL;
        *pNumArgs = argc;
        return argv;
    }

    /* Split and copy the remaining arguments */
    argv[argc++] = d;
    qcount = bcount = 0;
    while (*s) {
        if ((*s == ' ' || *s == '\t') && qcount == 0) {
            /* close the argument */
            *d++ = 0;
            bcount = 0;

            /* skip to the next one and initialize it if any */
            do {
                s++;
            } while (*s == ' ' || *s == '\t');
            if (*s)
                argv[argc++] = d;
        }
        else if (*s == '\\') {
            *d++ = *s++;
            bcount++;
        }
        else if (*s == '"') {
            if ((bcount & 1) == 0) {
                /* Preceded by an even number of '\', this is half that
                 * number of '\', plus a quote which we erase.
                 */
                d -= bcount / 2;
                qcount++;
            }
            else {
                /* Preceded by an odd number of '\', this is half that
                 * number of '\' followed by a '"'
                 */
                d = d - bcount / 2 - 1;
                *d++ = '"';
            }
            s++;
            bcount = 0;
            /* Now count the number of consecutive quotes. Note that qcount
             * already takes into account the opening quote if any, as well as
             * the quote that lead us here.
             */
            while (*s == '"') {
                if (++qcount == 3) {
                    *d++ = '"';
                    qcount = 0;
                }
                s++;
            }
            if (qcount == 2)
                qcount = 0;
        }
        else {
            /* a regular character */
            *d++ = *s++;
            bcount = 0;
        }
    }
    *d = '\0';
    argv[argc] = NULL;
    *pNumArgs = argc;

    return argv;
}

static void lmemcpy(char *dest, const char *src, DWORD len)
{
    /* copy 4 bytes at once */
    for (; len > 3; len -= 4, dest += 4, src += 4)
        *(long*)dest = *(long*)src;
    while (len--)
        *dest++ = *src++;
}

static void catstdin(void)
{
	DWORD chars_read = 0;
	ReadConsoleA(stdin, input_buffer, 1024 * sizeof(wchar_t), &chars_read, &crc);
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
	input_buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 1024 * sizeof(wchar_t));

	/* get argc and argv */
	int argc;
	char **argv = CommandLineToArgvA(GetCommandLineA(), &argc) + 1;
	argc--; /* the first arg is always the program name */

    switch (argc) {
        case 0:
            for (;; catstdin());
            break;
        default:
            for (int i = 0; i < argc; ++i) {
                if (!lstrcmpA(argv[i], "-")) catstdin();
                else catfile(argv[i]);
            }
    }

	/* free memory */
    HeapFree(GetProcessHeap(), 0, input_buffer);
    HeapFree(GetProcessHeap(), 0, output_buffer);
    LocalFree(argv);

    /* exit */
    ExitProcess(0);
}