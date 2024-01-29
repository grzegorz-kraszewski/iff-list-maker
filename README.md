# IFFListMaker
IFFListMaker is a console based tool for AmigaOS. It creates compound IFF LIST files from a set of IFF FORMs of the same type. A PROP chunk with global list properties may be also injected.

# Usage

`IFFListMaker <TO=<output LIST file>> <TYPE=<id>> [PROP=<prop file>] [FROM=<path>] <FORM 1> <FORM 2> ...`
- TO – Output IFF LIST file. Required.
- TYPE - IFF type of resulting LIST. All merged FORMs and optional PROP FORM **must** have this type. Required.
- PROP – Optional IFF FORM file. All chunks inside the FORM are copied to PROP of the LIST.
- FROM – Optional plain text file containing list of files to be merged (one per line). File paths in the list should be either absolute or relative to the current dir.
- files containing IFF FORMs to be merged. Order of FORMs in the LIST is preserved. If FROM argument is specified, single input files passed as IFFListMaker arguments are ignored.
