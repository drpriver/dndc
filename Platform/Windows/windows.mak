PYTHON:=py
CC=clang
CXX=clang++
# These flags were copied from another project, probably excessive.
PLATFORM_FLAGS=-D_CRT_NONSTDC_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -Wno-gnu-empty-initializer -Wno-gnu-auto-type -Wno-nullability-extension -Wno-gnu-statement-expression -Wno-extra-semi -Wno-gnu-case-range  -Wno-language-extension-token -Wno-gnu -Wno-extra-semi-stmt
INCLUDE_FLAGS+=-IPlatform\Windows
DEBUG_FLAGS=-DLOG_LEVEL=4\
	 -DDEBUG\
	 -fsanitize=nullability\
	 -fsanitize=undefined\
	 -fsanitize=address\
	 -O0\
	 -g
# We have to have python anyway, so emulate the unix utilities.
RM=$(PYTHON) -m Scripts.win_utils rm -r
MV=$(PYTHON) -m Scripts.win_utils mv
TOUCH=$(PYTHON) -m Scripts.win_utils touch
MKDIR=$(PYTHON) -m Scripts.win_utils mkdir
CP=$(PYTHON) -m Scripts.win_utils cp
EXE=.exe
SO=.dll
SOLIB=.lib
INSTALL=$(PYTHON) -m Scripts.win_utils install

ifeq ($(PYCFLAGS), )
PYCFLAGS=-I$(LOCALAPPDATA)\Programs\Python\Python39\include -Wno-visibility
PYLDFLAGS=$(LOCALAPPDATA)\Programs\Python\Python39\libs\python39.lib
PYDLL=$(LOCALAPPDATA)\Programs\Python\Python39\python39.dll
endif

$(BINDIR)\python39.dll: $(PYDLL)
	$(CP) $< $@
$(BINDIR)/dndc.exe: $(BINDIR)\python39.dll
TestDndc: | $(BINDIR)\python39.dll
# idk
PYEXTENSION=.pyd
PYEXTFLAGS=-shared

# This is messy and hasn't been worked on in a while. I was trying to build a
# gdndc using just native os APIs, but if I had to do it now I would probably
# just use QT.
# It'll probably still do something if you install WIL and WebView2.
GDNDCINCLUDES=-ID:\WebView2Samples\GettingStartedGuides\Win32_GettingStarted\packages\Microsoft.Web.WebView2.1.0.774.44\build\native\include -ID:\WebView2Samples\GettingStartedGuides\Win32_GettingStarted\packages\Microsoft.Windows.ImplementationLibrary.1.0.210204.1\include
GDNDCLINKER=-LD:\WebView2Samples\GettingStartedGuides\Win32_GettingStarted\packages\Microsoft.Web.WebView2.1.0.774.44\build\native\x64
gdndc: $(BINDIR)/gdndc.exe
$(BINDIR)/gdndc.exe: Platform/Windows/gdndc.cpp $(OBJDIR)/dndc.o  Platform/Windows/windows.mak $(BINDIR)/libquickjs$(SO)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PLATFORM_FLAGS) $(GDNDCINCLUDES) $(DEPFLAGS) $(DEPDIR)/gdndc.dep $< $(OBJDIR)/dndc.o -o $@ $(LINK_FLAGS) $(GDNDCLINKER) -std=c++17 -Wno-deprecated-dynamic-exception-spec -Wno-missing-noreturn -Wno-c99-designator -DQJS_SHARED_LIBRARY $(BINDIR)/libquickjs$(SOLIB)
#this is untested
$(OBJDIR)/dndc.lib: $(OBJDIR)/dndc.o
	llvm-ar crs $@ $^
$(BINDIR)/libdndc.dll: $(OBJDIR)/dndc.o $(VENDOBJDIR)/libquickjs.o
	$(CC) $^ -o $@ -shared
