CC:=gcc #Compiler
EDL:=gcc #Linker
AR:=ar #Archiver
ARFLAGS:=rcs
CCFLAGS:=-Wall -Wextra -Wno-unused-parameter -g #Compiler options
EDLFLAGS:=-Wall -g #Linker options
EXE=remote-server #Binary name
DEFINES:=-D NODEBUG #Preprocessor definitions
ECHO:=@echo
INCLUDES:=-I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include/\
				 -I/usr/lib/glib-2.0/include/

EOBJ:=tcpserver.o remote-base.o config.o dbusif.o mpris.o mpris-command.o\
			service-broadcast.o
LIBS:=libsmdp.a -ljansson -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lgthread-2.0 -lmnl
HOST:=$(shell uname)-$(shell uname -m)
SMDP:=../smdp/src/$(HOST)/libsmdp.a
L_SMDP:=./libsmdp.a

.PHONY: setup smdp

$(EXE): setup $(EOBJ)
	@echo building $@
	$(EDL) -o $(EXE) $(EDLFLAGS) $(EOBJ) $(LIBS)
	@echo done

%.o : %.c *.h
	@echo building $@ ...
	$(CC) $(CCFLAGS) -c $(DEFINES) $(INCLUDES) $<
	@echo done

clean:
	@echo -n cleaning repository...
	-@rm -rf *.o
	-@rm -rf *.so*
	-@rm -rf *~
	@echo cleaned.

setup : $(L_SMDP)

$(L_SMDP) : $(SMDP)
	@echo installing $@
	cp $(SMDP) ./;
	cp ../smdp/src/smdp.h ./;

$(SMDP) :
	@echo building $@
	cd ../smdp; make > /dev/null;

