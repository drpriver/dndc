CC=gcc
CXX=g++
PYTHON:=python3
PYCFLAGS?=$(shell pkg-config --cflags python3)
PYLDFLAGS?=$(shell pkg-config --libs python3)

PLATFORM_FLAGS=-march=native -fPIC -mfpu=neon -mtune=native -DRPI4=1
DEBUG_FLAGS=-fsanitize=undefined\
	 -fsanitize=address\
	 -static-libasan\
	 -O0\
	 -g
DEV_FLAGS=-O0\
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
$(BINDIR)/libdndc.$(DNDCVERSION).so: $(OBJDIR)/dndc.o $(VENDOBJDIR)/libquickjs.o
	$(CC) $^ -o $@ -Wl,-undefined,error -shared -g
