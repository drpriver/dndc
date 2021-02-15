PYTHON:=py
CC=clang
PLATFORM_FLAGS=-DWINDOWS -D_CRT_NONSTDC_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS -Wno-c++98-compat -Wno-gnu-empty-initializer -Wno-gnu-auto-type -Wno-nullability-extension -Wno-gnu-statement-expression -Wno-sign-conversion -Wno-extra-semi -Wno-reserved-id-macro -Wno-implicit-int-float-conversion -Wno-shorten-64-to-32 -Wno-implicit-int-conversion -Wno-gnu-case-range -Dalloca=__builtin_alloca -Wno-format-nonliteral -Wno-language-extension-token -Wno-alloca -Wno-implicit-fallthrough -Wno-undef -Wno-gnu -Wno-pointer-arith -Wno-enum-float-conversion -Wno-switch-enum -Xclang -std=gnu17 -Wno-missing-variable-declarations -Wno-float-conversion -Wno-c++-compat -Wno-four-char-constants -Wno-missing-prototypes -Wno-extra-semi-stmt -Wno-unused-function -Wno-format-pedantic -mrdseed
foo:
	echo $(COMMON_FLAGS)
INCLUDE_FLAGS+=-ID:\Libs\SDL2-2.0.12\include -ID:\Libs\SDL2_mixer-2.0.4\include -ID:\Libs\SDL2_ttf-2.0.15\include -ID:\Libs\SDL2_image-2.0.5\include
SDL_FLAGS=D:\Libs\SDL2-2.0.12\lib\x64\SDL2main.lib -LD:\Libs\SDL2-2.0.12\lib\x64 -LD:\Libs\SDL2_image-2.0.5\lib\x64 -LD:\Libs\SDL2_mixer-2.0.4\lib\x64 -LD:\Libs\SDL2_ttf-2.0.15\lib\x64 -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -LD:\Libs\SDL2-2.0.12\lib\x64\SDL2main.lib
DEBUG_FLAGS=-DLOG_LEVEL=4\
	 -DDEBUG\
	 -fsanitize=nullability\
	 -fsanitize=undefined\
	 -fsanitize=address\
	 -O0\
	 -g
DEV_FLAGS=-DLOG_LEVEL=4\
	 -DDEBUG\
	 -O0\
	 -g
LOADER_FLAGS=-DHOT_RELOAD
DYNAMIC_FLAGS=-DHOT_RELOAD
NATIVE_UI_INCLUDES=
NATIVE_UI_LINKER=
PCRE_INCLUDE=
PCRE_LIB=
RM=$(PYTHON) -m Scripts.win_utils rm -r
MV=$(PYTHON) -m Scripts.win_utils mv
TOUCH=$(PYTHON) -m Scripts.win_utils touch
MKDIR=$(PYTHON) -m Scripts.win_utils mkdir
CP=$(PYTHON) -m Scripts.win_utils cp
EXE=.exe
SO=.dll
PYCFLAGS=-IC:\Users\David\AppData\Local\Programs\Python\Python38\include -LC:\Users\David\AppData\Local\Programs\Python\Python38\libs -Wno-visibility
PYLDFLAGS=-LC:\Users\David\AppData\Local\Programs\Python\Python38\libs C:\Users\David\AppData\Local\Programs\Python\Python38\libs\python38.lib
$(NATIVEUIPLAT): Objs/%.o : Platform/Windows/%.c
	$(CC) $(LINKERFREE_FLAGS) $(NATIVE_UI_INCLUDES) -c -o $@ $<

DLLS:=$(wildcard ExternalArtifacts/DLL/*.dll)
copy-dlls:
	$(CP) $(DLLS) Bin
