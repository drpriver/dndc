CC=clang $(ARCHES)
CXX=clang++ $(ARCHES)
PYTHON:=python3
PYCFLAGS:=-F/Library/Frameworks -isystem /Library/Frameworks/Python.framework/Headers
# bleh. I can't figure out how to tell the linker that symbols will be resolved
# using frameworks loaded by the bundle_loader instead of the symbols being in
# the bundle_loader itself. This flag means we turn what should be link errors
# into runtime errors. Oh well. At least we have python tests now to catch
# this.
PYLDFLAGS:=-undefined dynamic_lookup
# I get codegen bugs in MStringBuilder with this flag
# It isn't able to properly track the size of buffers
PLATFORM_FLAGS=-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
#PLATFORM_FLAGS=-U_FORTIFY_SOURCE
DEBUG_FLAGS=-DLOG_LEVEL=4\
	 -DDEBUG\
	 -fsanitize=nullability\
	 -fsanitize=undefined\
	 -fsanitize=address\
	 -O0\
	 -g
# LINK_FLAGS+=-Wl,-dead_strip
RM=rm -rf
MV=mv
TOUCH=touch
MKDIR=mkdir
CP=cp
EXE=
INSTALL=install
SO=.dylib
SOLIB=.dylib

$(BINDIR)/gdndc: Platform/MacOS/gdndc.m Platform/MacOS/Info.plist Platform/MacOS/app_icon.png opt.mak $(VENDOBJDIR)/libquickjs.o
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/gdndc.dep $< -o $@ $(LINK_FLAGS) -framework Cocoa -framework WebKit -fobjc-arc -Wl,-sectcreate,__TEXT,__info_plist,Platform/MacOS/Info.plist $(VENDOBJDIR)/libquickjs.o -Wno-sign-compare
.PHONY: gdndc
gdndc: $(BINDIR)/gdndc
.PHONY: install-gdndc
install-gdndc: $(BINDIR)/gdndc
	$(INSTALL) -C $< $(INSTALLDIR)/gdndc

$(BINDIR)/dndbr: Platform/MacOS/dndbr.m Platform/MacOS/dndbr_app_icon.png Platform/MacOS/DndBrInfo.plist opt.mak
	$(CC) $(FLAGS) $(OPT_FLAGS) $(DEPFLAGS) $(DEPDIR)/gdndc.dep $< Dndc/dndc_local_server.c -o $@ $(LINK_FLAGS) -framework Cocoa -fobjc-arc -Wl,-sectcreate,__TEXT,__info_plist,Platform/MacOS/DndBrInfo.plist $(BINDIR)/libdndc.dylib $(RPATH)
.PHONY: dndbr
dndbr: $(BINDIR)/dndbr

$(OBJDIR)/libdndc.a: $(OBJDIR)/dndc.o
	ar crs $@ $^
$(BINDIR)/libdndc.dylib: $(OBJDIR)/dndc.o $(VENDOBJDIR)/libquickjs.o
	$(CC) $^ -o $@ $(OPT_FLAGS) -Wl,-dead_strip_dylibs -Wl,-headerpad_max_install_names -Wl,-undefined,error -shared -install_name @rpath/libdndc.$(DNDCVERSION).dylib -compatibility_version $(DNDC_COMPAT_VERSION) -current_version $(DNDCVERSION) -g

PYEXTENSION=$(shell python3-config --extension-suffix)
PYEXTFLAGS=-bundle -bundle_loader /Library/Frameworks/Python.framework/Versions/3.10/bin/python3 -arch arm64

all: gdndc $(OBJDIR)/libdndc.a $(BINDIR)/libdndc.dylib
all: dndbr
