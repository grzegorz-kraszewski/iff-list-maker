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

LONG CopyFormData(struct App *app, LONG what)
{
	LONG error = 0;
	LONG result = RETURN_ERROR;
	UBYTE idbuf[5];

	IDtoStr(what, idbuf);

	if (!(error = PushChunk(app->IFFOut, app->IFFType, what, IFFSIZE_UNKNOWN)))
	{
		result = CopyLoop(app);

		if (error = PopChunk(app->IFFOut))
		{
			Printf("\tFailed to pop %s chunk from output stack: %s.\n", idbuf, GetIFFErrorStr(error));
			result = RETURN_ERROR;
		}
		else PutStr("\tDone.\n");
	}
	else Printf("/tFailed to push %s chunk to output stack: %s.\n", idbuf, GetIFFErrorStr(error));

	return result;
}

/*---------------------------------------------------------------------------*/

LONG PushFromFile(struct App *app, STRPTR filename, LONG what)
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
						result = CopyFormData(app, what);
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
				result = PushFromFile(app, buf, ID_FORM);
			}

			if ((result == RETURN_OK) && (doserr = IoErr()))
			{
				Printf("Error reading list file: %s.\n", GetDOSErrorStr(doserr));
			}

			Close(file);
		}
		else Printf("Failed to open list file: %s.\n", GetDOSErrorStr(IoErr()));

		FreeMem(buf, 1024);
	}
	else PutStr("No memory for list file read buffer.\n");

	return RETURN_ERROR;
}

/*---------------------------------------------------------------------------*/

LONG PushFormsFromArgumentList(struct App *app, STRPTR *list)
{
	STRPTR filename;
	LONG result = RETURN_OK;

	while ((filename = *list++) && (result == RETURN_OK))
	{
		result = PushFromFile(app, filename, ID_FORM);
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
	BOOL result = RETURN_OK;

	if (argtab[ARG_PROP]) result = PushFromFile(app, (STRPTR)argtab[ARG_PROP], ID_PROP);
	if (result == RETURN_OK) result = PushForms(app, argtab);
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
		if ((ifferr = PopChunk(app->IFFOut)) != 0) Printf("Failed to pop LIST chunk "
		"from the output stack: %s.\n", GetIFFErrorStr(ifferr));
	}
	else Printf("Failed to push LIST chunk to the output stack: %s.\n",
	GetIFFErrorStr(ifferr));

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
			else Printf("Failed to open output IFF stream: %s.\n", GetIFFErrorStr(ifferr));

			FreeIFF(app->IFFOut);
		}
		else PutStr("No memory for output IFF handle.\n");

		Close(outfile);
	}
	else Printf("Failed to open output file: %s.\n", GetDOSErrorStr(IoErr()));

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
	else PutStr("No memory for copy buffer.\n");

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
	else Printf("'%s' is not a valid IFF FORM type.\n", argtab[ARG_TYPE]);

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
	else Printf("Failed to parse arguments: %s.\n", GetDOSErrorStr(IoErr()));

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
	else PutStr("Failed to open iffparse.library v39+.\n");

	return result;
}