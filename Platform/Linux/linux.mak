CC=clang
PYTHON:=python3
PYCFLAGS?=-I/usr/include/python3.8
PYLDFLAGS?=-L/usr/lib/python3.8/config-3.8-x64_64-linux-gnu -lpython3.8

# I think there is a bug with clang and _FORTIFY_SOURCE
PLATFORM_FLAGS=-DLINUX -U_FORTIFY_SOURCE
DEBUG_FLAGS=-DDEBUG\
	 -DLOG_LEVEL=4\
	 -fsanitize=undefined\
	 -fsanitize=address\
	 -O0\
	 -g
DEV_FLAGS=-DLOG_LEVEL=4\
	 -DDEBUG\
	 -O0\
	 -g
ifeq ($(CC),clang)
DEBUG_FLAGS+=-fsanitize=nullability
endif
LINK_FLAGS+=-lm -lpthread
RM=rm -rf
MV=mv
TOUCH=touch
MKDIR=mkdir
CP=cp
EXE=
INSTALL=install
