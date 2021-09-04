CC=clang
CXX=clang++
PYTHON:=python3
PYCFLAGS?=$(shell pkg-config --cflags python3)
PYLDFLAGS?=$(shell pkg-config --libs python3)

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
SO=.so
SOLIB=.so
INSTALL=install
PYEXTENSION=.so
PYEXTFLAGS=-shared -fPIC
