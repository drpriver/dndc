CC=clang
PYTHON:=python3
PYCFLAGS:=-F/Library/Frameworks -isystem /Library/Frameworks/Python.framework/Headers
PYLDFLAGS:=-framework Python
PLATFORM_FLAGS=-DDARWIN -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
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

$(BINDIR)/gdndc: Platform/MacOS/gdndc.m $(OBJDIR)/dndc.o $(OBJDIR)/frozenstdlib.o Platform/MacOS/Info.plist Platform/MacOS/app_icon.png
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/gdndc.dep $< $(OBJDIR)/frozenstdlib.o $(OBJDIR)/dndc.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -framework Cocoa -framework WebKit -fobjc-arc -Wl,-sectcreate,__TEXT,__info_plist,Platform/MacOS/Info.plist
gdndc: $(BINDIR)/gdndc
install-gdndc: $(BINDIR)/gdndc
	$(INSTALL) -C $< $(INSTALLDIR)/gdndc
PYEXTENSION=.cpython-38-darwin.so
PYEXTFLAGS=-bundle -bundle_loader /usr/local/bin/python3

all: gdndc
