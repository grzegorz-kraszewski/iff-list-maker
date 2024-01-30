#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>

#include <dos/dos.h>

extern struct Library *SysBase, *DOSBase;

struct Library *IFFParseBase;

struct App
{
	LONG IFFType;
	struct IFFHandle *IFFIn;
	struct IFFHandle *IFFOut;
	LONG Error;
	UBYTE *CopyBuf;
};

STRPTR IFFErrors[11] = {
	"reached end of file",
	"reached end of context",
	"no scope for property",
	"out of memory",
	"file read error",
	"file write error",
	"file seek error",
	"corrupted data",
	"IFF syntax error",
	"not an IFF file",
	"callback hook missing"
};

/* indexes in argument array */

#define ARG_TO       0
#define ARG_TYPE     1
#define ARG_PROP     2
#define ARG_FROM     3
#define ARG_FILES    4

#define COPYBUFSIZE  8192

/*---------------------------------------------------------------------------*/

STRPTR GetIFFErrorStr(LONG error)
{
	error = -error - 1;   /* [-1 .. -11] translated to [0 .. 10] */

	if ((error >= 0) && (error < 11)) return IFFErrors[error];
	else return "unknown error";
}

/*---------------------------------------------------------------------------*/

STRPTR GetDOSErrorStr(LONG error)
{
	static UBYTE buf[128];

	Fault(error, "", buf, 128);
	return buf + 2;               /* skips colon and space */
}

/*---------------------------------------------------------------------------*/

void PrintError(STRPTR message, LONG code)
{
	if (code) PrintFault(code, "IFFListMaker");
	else if (message) Printf("IFFListMaker: %s\n", message);
}

/*---------------------------------------------------------------------------*/

void PrintErrorIFF(STRPTR message, LONG ifferr)
{
	ifferr = -ifferr - 1;   /* [-1 .. -11] translated to [0 .. 10] */
	PutStr("IFFListMaker: ");
	PutStr(message);
	PutStr(", ");

	if ((ifferr >= 0) && (ifferr < 11))
	{
		PutStr(IFFErrors[ifferr]);
		PutStr("\n");
	}
	else PutStr("unknown error\n"); 
}

/*---------------------------------------------------------------------------*/

void RTrim(STRPTR s)
{
	STRPTR t = s;

	while (*t) t++;

	while (--t >= s)
	{
		if (*t == 0x0A) *t = 0x00;
		else break;
	}
}

/*---------------------------------------------------------------------------*/

LONG CopyLoop(struct App *app)
{
	LONG r = 0;

	for (;;)
	{
		r = ReadChunkBytes(app->IFFIn, app->CopyBuf, COPYBUFSIZE);
		if (r <= 0) break;
		r = WriteChunkBytes(app->IFFOut, app->CopyBuf, r);
		if (r <= 0)	break;
	}

	if (r < 0)
	{
		Printf("\tCopying FORM failed: %s.\n", GetIFFErrorStr(r));
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

/*---------------------------------------------------------------------------*/

LONG CopyFormData(struct App *app)
{
	LONG error = 0;
	LONG result = RETURN_ERROR;

	if (!(error = PushChunk(app->IFFOut, app->IFFType, ID_FORM, IFFSIZE_UNKNOWN)))
	{
		result = CopyLoop(app);

		if (error = PopChunk(app->IFFOut))
		{
			Printf("\tFailed to pop FORM chunk from output stack: %s.\n", GetIFFErrorStr(error));
			result = RETURN_ERROR;
		}
		else PutStr("\tDone.\n");
	}
	else Printf("/tFailed to push FORM chunk to output stack: %s.\n", GetIFFErrorStr(error));

	return result;
}

/*---------------------------------------------------------------------------*/

LONG PushFormFromFile(struct App *app, STRPTR filename)
{
	LONG result = RETURN_ERROR;
	BPTR file;

	Printf("%s:\n", filename);

	if (app->IFFIn = AllocIFF())
	{
		if (file = Open(filename, MODE_OLDFILE))
		{
			LONG ifferr;

			app->IFFIn->iff_Stream = file;
			InitIFFasDOS(app->IFFIn);

			if ((ifferr = OpenIFF(app->IFFIn, IFFF_READ)) == 0)
			{
				if ((ifferr = StopChunk(app->IFFIn, app->IFFType, ID_FORM)) == 0)
				{
					if ((ifferr = ParseIFF(app->IFFIn, IFFPARSE_SCAN)) == 0)
					{
						result = CopyFormData(app);
					}
					else Printf("IFF FORM chunk not found: %s.\n", (LONG)GetIFFErrorStr(ifferr));
				}
				else Printf("Failed to set FORM chunk handler: %s.\n", (LONG)GetIFFErrorStr(ifferr));

				CloseIFF(app->IFFIn);
			}
			else Printf("\tFailed to open input as IFF: %s.\n", (LONG)GetIFFErrorStr(ifferr));

			Close(file);
		}
		else Printf("\tFailed to open input file: %s.\n", (LONG)GetDOSErrorStr(IoErr()));

		FreeIFF(app->IFFIn);
	}
	else PutStr("\tOut of memory.\n");

	return result;
}

/*---------------------------------------------------------------------------*/

LONG PushFormsFromFileList(struct App *app, STRPTR filename)
{
	STRPTR buf;
	BPTR file;
	LONG result = RETURN_OK;

	if (buf = AllocMem(1024, MEMF_ANY))
	{
		if (file = Open(filename, MODE_OLDFILE))
		{
			LONG doserr;

			while ((result == RETURN_OK) && FGets(file, buf, 1024))
			{
				RTrim(buf);
				result = PushFormFromFile(app, buf);
			}

			if (doserr = IoErr()) app->Error = doserr;
			PrintError(NULL, app->Error);

			Close(file);
		}
		else PrintError(0, IoErr());

		FreeMem(buf, 1024);
	}
	else PrintError(0, ERROR_NO_FREE_STORE);

	return RETURN_ERROR;
}
/*---------------------------------------------------------------------------*/

LONG PushFormsFromArgumentList(struct App *app, STRPTR *list)
{
	STRPTR filename;
	LONG result = RETURN_OK;

	while ((filename = *list++) && (result == RETURN_OK))
	{
		result = PushFormFromFile(app, filename);
	}

	return result;
}

/*---------------------------------------------------------------------------*/

LONG PushForms(struct App *app, LONG *argtab)
{
	LONG result;

	if (argtab[ARG_FROM]) result = PushFormsFromFileList(app, (STRPTR)argtab[ARG_FROM]);
	else result = PushFormsFromArgumentList(app, (STRPTR*)argtab[ARG_FILES]);

	return result; 
}

/*---------------------------------------------------------------------------*/

LONG PushProperties(struct App *app, LONG *argtab)
{
	BOOL result = RETURN_ERROR;
	LONG ifferr;

	if (argtab[ARG_PROP])
	{
		PrintError(0, ERROR_NOT_IMPLEMENTED);
	}
	else result = PushForms(app, argtab);

	return result;
}


/*---------------------------------------------------------------------------*/

LONG PushListHeader(struct App *app, LONG *argtab)
{
	BOOL result = RETURN_ERROR;
	LONG ifferr;

	if ((ifferr = PushChunk(app->IFFOut, app->IFFType, ID_LIST, IFFSIZE_UNKNOWN)) == 0)
	{
		result = PushProperties(app, argtab);
		if ((ifferr = PopChunk(app->IFFOut)) != 0) PrintErrorIFF("output", ifferr);
	}
	else PrintErrorIFF("output", ifferr);

	return result;
}

/*---------------------------------------------------------------------------*/

LONG OpenOutput(struct App *app, LONG *argtab)
{
	LONG result = RETURN_ERROR;
	LONG ifferr;

	BPTR outfile;

	if (outfile = Open((STRPTR)argtab[ARG_TO], MODE_NEWFILE))
	{
		if (app->IFFOut = AllocIFF())
		{
			app->IFFOut->iff_Stream = outfile;
			InitIFFasDOS(app->IFFOut);

			if ((ifferr = OpenIFF(app->IFFOut, IFFF_WRITE)) == 0)
			{
				result = PushListHeader(app, argtab);
				CloseIFF(app->IFFOut);
			}
			else PrintErrorIFF("output", ifferr);

			FreeIFF(app->IFFOut);
		}
		else PrintError(0, ERROR_NO_FREE_STORE);

		Close(outfile);
	}
	else PrintError(0, IoErr());

	return result;
}

/*---------------------------------------------------------------------------*/

LONG GetCopyBuffer(struct App *app, LONG *argtab)
{
	LONG result = RETURN_FAIL;

	if (app->CopyBuf = AllocMem(COPYBUFSIZE, MEMF_ANY))
	{
		result = OpenOutput(app, argtab);
		FreeMem(app->CopyBuf, COPYBUFSIZE);
	}

	return result;
}

/*---------------------------------------------------------------------------*/


LONG CheckIFFType(LONG *argtab)
{
	long result = RETURN_ERROR;
	struct App app;

	app.IFFType = *(LONG*)argtab[ARG_TYPE];

	if (GoodType(app.IFFType))
	{
		result = GetCopyBuffer(&app, argtab);
	}
	else
	{
		PutStr("IFFListMaker: '");
		PutStr((STRPTR)argtab[ARG_TYPE]);
		PutStr("' is not a valid IFF FORM type\n");
	}

	return result;
}

/*---------------------------------------------------------------------------*/

LONG ParseArguments(void)
{
	LONG result = RETURN_ERROR;
	struct RDArgs *args;
	LONG argtab[5] = { 0, 0, 0, 0, 0 };

	if (args = ReadArgs("TO/K/A,TYPE/K/A,PROP/K,FROM/K,FILES/M", argtab, NULL))
	{
		result = CheckIFFType(argtab);
		FreeArgs(args);
	}
	else PrintError(NULL, IoErr());

	return result;
}

/*---------------------------------------------------------------------------*/

LONG Main(void)
{
	LONG result = RETURN_FAIL;

	if (IFFParseBase = OpenLibrary("iffparse.library", 39))
	{
		result = ParseArguments();
		CloseLibrary(IFFParseBase);
	}
	else PrintError("failed to open iffparse.library v39+", 0);

	return result;
}