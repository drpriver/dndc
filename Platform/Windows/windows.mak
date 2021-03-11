PYTHON:=py
CC=clang
PLATFORM_FLAGS=-DWINDOWS -D_CRT_NONSTDC_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -Wno-c++98-compat -Wno-gnu-empty-initializer -Wno-gnu-auto-type -Wno-nullability-extension -Wno-gnu-statement-expression -Wno-sign-conversion -Wno-extra-semi -Wno-reserved-id-macro -Wno-implicit-int-float-conversion -Wno-shorten-64-to-32 -Wno-implicit-int-conversion -Wno-gnu-case-range -Wno-format-nonliteral -Wno-language-extension-token -Wno-alloca -Wno-implicit-fallthrough -Wno-undef -Wno-gnu -Wno-pointer-arith -Wno-enum-float-conversion -Wno-switch-enum -Wno-missing-variable-declarations -Wno-float-conversion -Wno-c++-compat -Wno-four-char-constants -Wno-missing-prototypes -Wno-extra-semi-stmt -Wno-unused-function -Wno-format-pedantic
INCLUDE_FLAGS+=-IPlatform\Windows
DEBUG_FLAGS=-DLOG_LEVEL=4\
	 -DDEBUG\
	 -fsanitize=nullability\
	 -fsanitize=undefined\
	 -fsanitize=address\
	 -O0\
	 -g
RM=$(PYTHON) -m Scripts.win_utils rm -r
MV=$(PYTHON) -m Scripts.win_utils mv
TOUCH=$(PYTHON) -m Scripts.win_utils touch
MKDIR=$(PYTHON) -m Scripts.win_utils mkdir
CP=$(PYTHON) -m Scripts.win_utils cp
EXE=.exe
INSTALL=$(PYTHON) -m Scripts.win_utils install

ifeq ($(PYCFLAGS), )
PYCFLAGS=-I$(LOCALAPPDATA)\Programs\Python\Python38\include -Wno-visibility
PYLDFLAGS=$(LOCALAPPDATA)\Programs\Python\Python38\libs\python38.lib
PYDLL=$(LOCALAPPDATA)\Programs\Python\Python38\python38.dll
endif

$(BINDIR)\python38.dll: $(PYDLL)
	$(CP) $< $@
$(BINDIR)\dndc.exe: $(BINDIR)\python38.dll
TestDndC: | $(BINDIR)\python38.dll
# idk
PYEXTENSION=.dll
PYEXTFLAGS=-shared -fPIC
