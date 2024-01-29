#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>

#include <dos/dos.h>

extern struct Library *SysBase, *DOSBase;

struct Library *IFFParseBase;

struct App
{
	LONG IFFType;
	struct IFFHandle *IFFOut;
};

const UBYTE* const IFFErrors[11] = {
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

LONG PushFormsFromFileList(struct App *app, STRPTR filename)
{
	PrintError(0, ERROR_NOT_IMPLEMENTED);
	return RETURN_ERROR;
}
/*---------------------------------------------------------------------------*/

LONG PushFormsFromArgumentList(struct App *app, STRPTR *list)
{
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

LONG CheckIFFType(LONG *argtab)
{
	long result = RETURN_ERROR;
	struct App app;

	app.IFFType = *(LONG*)argtab[ARG_TYPE];

	if (GoodType(app.IFFType))
	{
		result = OpenOutput(&app, argtab);
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
	LONG argtab[5];

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