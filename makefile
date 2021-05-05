# Makefile for 64-bit modules

CC=cl.exe
RC=rc.exe

INCDIRS = -I\code\lab\OceanOptics\SeaBreeze\include

OSTYPE = NT
ARCHITECTURE = i486
COMPILER = MSC70
_VC_EMBED_MANIFEST = mt.exe -manifest $@.manifest -outputresource:$@;1 -nologo

# ---------------------------------------------------------------------------
# -G5   - optimize for 586 machines
# -MT   - multithreaded
# -W3   - Warning level 3
# -O2   - optimize on
# /D_CRT_SECURE_NO_DEPRECATE
# ---------------------------------------------------------------------------
WARNS  = 
GOPTS  = /nologo /MD /W3 /O2 $(WARNS) $(DBGOPTS)
CCOPT  = 
SYSOPT = -D_POSIX_

DEFS = $(DBG) -D$(OSTYPE) -D$(ARCHITECTURE) -D$(COMPILER)

CL=
CFLAGS = $(GOPTS) $(CCOPT) $(INCDIRS) $(SYSOPT) $(DEFS)

LIBS = tfoc.lib

SYSLIBS = user32.lib comctl32.lib gdi32.lib comdlg32.lib WS2_32.lib

ALL: FilmMeasure.exe test.exe

INSTALL: z:\lab\exes\FilmMeasure.exe

CLEAN:
	rm *.obj *.exe *.res win32ex.c win32ex.h graph.c graph.h server_support.c server_support.h

OBJS = FilmMeasure.obj spec_client.obj server_support.obj win32ex.obj graph.obj curfit.obj FilmMeasure.res 

FilmMeasure.exe : $(OBJS)
	$(CC) -FeFilmMeasure.exe $(CFLAGS) $(OBJS) $(LIBS) $(SYSLIBS) /link  /NODEFAULTLIB:LIBCMT

test.exe : test.obj spec_client.obj server_support.obj 
	$(CC) -Fetest.exe $(CFLAGS) test.obj spec_client.obj server_support.obj $(SYSLIBS)

.c.obj:
	$(CC) $(CFLAGS) -c -Fo$@ $<

.rc.res:
	$(RC) -r -fo $@ $<


# Distribution for the lab use
z:\lab\exes\FilmMeasure.exe : FilmMeasure.exe
	copy $** $@

# ---------------------------------------------------------------------------
# Support modules 
# ---------------------------------------------------------------------------
graph.h : \code\Window_Classes\graph\graph.h
	copy $** $@

graph.c : \code\Window_Classes\graph\graph.c
	copy $** $@

win32ex.h : \code\Window_Classes\win32ex\win32ex.h
	copy $** $@

win32ex.c : \code\Window_Classes\win32ex\win32ex.c
	copy $** $@

server_support.c : \code\lab\Server_Support\server_support.c
	copy $** $@

server_support.h : \code\lab\Server_Support\server_support.h
	copy $** $@

spec.h : ..\spec\spec.h
	copy $** $@

spec_client.h : ..\spec\spec_client.h
	copy $** $@

# ---------------------------------------------------------------------------
# dependencies
# ---------------------------------------------------------------------------
FilmMeasure.obj : filmmeasure.h spec.h spec_client.h graph.h win32ex.h server_support.h resource.h tfoc.h

FilmMeasure.res : FilmMeasure.rc resource.h

graph.obj : graph.h

server_support.obj : server_support.h

win32ex.obj : win32ex.h 

curfit.obj : curfit.h
