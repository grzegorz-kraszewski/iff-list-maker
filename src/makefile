CC = gcc
LD = gcc
CFLAGS = -nostdlib -O2 -fbaserel -mregparm -D__NOLIBBASE__ -DUSE_INLINE_STDARG
LDFLAGS = -nostdlib -nostartfiles -fbaserel
LIBS =
OBJS = start.o main.o
EXE = IFFListMaker

all: $(OBJS)
	@echo "Linking $(EXE)..."
	@$(LD) $(LDFLAGS) -o $(EXE).db $^ $(LIBS)
	@strip $(EXE).db -o $(EXE) --strip-unneeded
	@Protect $(EXE) +E
	List $(EXE)

clean:
	rm -vf *.o $(EXE) $(EXE).db

#start.o: start.c
#	@echo "Compiling $@..."
#	@$(CC) $(CFLAGS) -fwritable-strings -c -o $@ $<

%.o: %.c
	@echo "Compiling $@..."
	@$(CC) $(CFLAGS) -c -o $@ $<

# dependencies

main.o: main.c
start.o: start.c
