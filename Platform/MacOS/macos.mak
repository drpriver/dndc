CC=clang $(ARCHES)
PYTHON:=python3
PYCFLAGS:=-F/Library/Frameworks -isystem /Library/Frameworks/Python.framework/Headers
PYLDFLAGS:=-framework Python
# I get codegen bugs in MStringBuilder with this flag
# It isn't able to properly track the size of buffers
#PLATFORM_FLAGS=-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
PLATFORM_FLAGS=-U_FORTIFY_SOURCE
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

$(BINDIR)/gdndc: Platform/MacOS/gdndc.m $(OBJDIR)/frozenstdlib.o Platform/MacOS/Info.plist Platform/MacOS/app_icon.png opt.mak $(BINDIR)/libquickjs.dylib
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/gdndc.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -framework Cocoa -framework WebKit -fobjc-arc -Wl,-sectcreate,__TEXT,__info_plist,Platform/MacOS/Info.plist $(BINDIR)/libquickjs.dylib -rpath @executable_path
gdndc: $(BINDIR)/gdndc
install-gdndc: $(BINDIR)/gdndc
	$(INSTALL) -C $< $(INSTALLDIR)/gdndc
$(OBJDIR)/libdndc.a: $(OBJDIR)/dndc.o $(OBJDIR)/frozenstdlib.o
	ar crs $@ $^
$(OBJDIR)/libdndc.dylib: $(OBJDIR)/dndc.o $(OBJDIR)/frozenstdlib.o $(OBJDIR)/libquickjs.a
	$(CC) $^ -o $@ -Wl,-dead_strip_dylibs -Wl,-headerpad_max_install_names -Wl,-undefined,error -shared -install_name @rpath/libdndc.0.6.2.dylib -compatibility_version 0.6.0 -current_version 0.6.2 -g -F/Library/Frameworks -framework Python -install_name $(OBJDIR)/libquickjs.a

PYEXTENSION=.cpython-38-darwin.so
PYEXTFLAGS=-bundle -bundle_loader /usr/local/bin/python3 -arch x86_64

all: gdndc $(OBJDIR)/libdndc.a $(OBJDIR)/libdndc.dylib
