CC=gcc #Compiler
EDL=gcc #Linker
AR=ar #Archiver
ARFLAGS=rcs
CCFLAGS=-Wall -g #Compiler options
EDLFLAGS=-Wall -g #Linker options
EXE=remote-serv #Binary name
DEFINES=DEBUG #Preprocessor definitions
ECHO=@echo
INCLUDES=-I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include/

EOBJ=tcpserver.o remote-base.o config.o dbusif.o
LIBS=-ljansson -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lgthread-2.0

$(EXE): $(EOBJ)
	@echo building $<
	$(EDL) -o $(EXE) $(EDLFLAGS) $(EOBJ) $(LIBS)
	@echo done

%.o : %.c *.h
	@echo building $< ...
	$(CC) $(CCFLAGS) -c -D $(DEFINES) $(INCLUDES) $<
	@echo done

clean:
	@echo -n cleaning repository...
	-@rm -rf *.o
	-@rm -rf *.so*
	-@rm -rf *~
	@echo cleaned.
