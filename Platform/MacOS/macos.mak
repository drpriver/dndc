CC=clang
PYTHON:=python3
PYCFLAGS:=-F/Library/Frameworks -isystem /Library/Frameworks/Python.framework/Headers
PYLDFLAGS:=-framework Python
PLATFORM_FLAGS=-DDARWIN -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
DEBUG_FLAGS=-DLOG_LEVEL=4\
	 -DDEBUG\
	 -fsanitize=nullability\
	 -fsanitize=undefined\
	 -fsanitize=address\
	 -O0\
	 -g
RM=rm -rf
MV=mv
TOUCH=touch
MKDIR=mkdir
CP=cp
EXE=
# I am confused about whether or not this should be .so or .dylib
# But we are defining this for compatibility with windows
# Hello, there's even a bundle type and a loader type on macos?
# Should I use those so that I can get symbol checking at compile time?
SO=.so
