PYTHON:=py
CC=clang
PLATFORM_FLAGS=-DWINDOWS -D_CRT_NONSTDC_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -Wno-c++98-compat -Wno-gnu-empty-initializer -Wno-gnu-auto-type -Wno-nullability-extension -Wno-gnu-statement-expression -Wno-sign-conversion -Wno-extra-semi -Wno-reserved-id-macro -Wno-implicit-int-float-conversion -Wno-shorten-64-to-32 -Wno-implicit-int-conversion -Wno-gnu-case-range -Wno-format-nonliteral -Wno-language-extension-token -Wno-alloca -Wno-implicit-fallthrough -Wno-undef -Wno-gnu -Wno-pointer-arith -Wno-enum-float-conversion -Wno-switch-enum -Xclang -Wno-missing-variable-declarations -Wno-float-conversion -Wno-c++-compat -Wno-four-char-constants -Wno-missing-prototypes -Wno-extra-semi-stmt -Wno-unused-function -Wno-format-pedantic
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

# super unportable
ifeq ($(PYCFLAGS), )
PYCFLAGS=-IC:\Users\David\AppData\Local\Programs\Python\Python38\include -LC:\Users\David\AppData\Local\Programs\Python\Python38\libs -Wno-visibility
PYLDFLAGS=-LC:\Users\David\AppData\Local\Programs\Python\Python38\libs C:\Users\David\AppData\Local\Programs\Python\Python38\libs\python38.lib
endif
