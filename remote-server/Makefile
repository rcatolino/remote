CC=gcc #Compiler
EDL=gcc #Linker
AR=ar #Archiver
ARFLAGS=rcs
CCFLAGS=-Wall #Compiler options
EDLFLAGS=-Wall #Linker options
EXE=remote-serv #Binary name
DEFINES=DEBUG #Preprocessor definitions
ECHO=@echo

EOBJ=tcpserver.o remote-base.o config.o
LIBS=-ljansson

$(EXE): $(EOBJ)
	@echo building $<
	$(EDL) -o $(EXE) $(EDLFLAGS) $(EOBJ) $(LIBS)
	@echo done

%.o : %.c *.h
	@echo building $< ...
	$(CC) $(CCFLAGS) -c -D $(DEFINES) $<
	@echo done

clean:
	@echo -n cleaning repository...
	-@rm -rf *.o
	-@rm -rf *.so*
	-@rm -rf *~
	@echo cleaned.
