.SUFFIXES:
# cribbed from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEPDIR := Depends
OBJDIR := Objs
BINDIR := Bin
DEPFLAGS = -MT $@ -MMD -MP -MF
TDEPFLAGS = -MT $$@ -MMD -MP -MF
$(DEPDIR): ; @$(MKDIR) -p $@
$(OBJDIR): ; @$(MKDIR) -p $@
$(BINDIR): ; @$(MKDIR) -p $@
$(DOCDIR): ; @$(MKDIR) -p $@
DIRECTORIES= $(DEPDIR) $(OBJDIR) $(BINDIR) $(DOCDIR)
%.dep: ;
DEPFILES:= $(wildcard Depends/*.dep)
include $(DEPFILES)

WARNING_FLAGS:=-Wall\
	-Wbad-function-cast\
	-Wextra \
	-Wvla\
	-Wmissing-noreturn\
	-Wcast-qual\
	-Wdeprecated\
	-Wdouble-promotion\
	-Wno-multichar\
	-Wno-sign-compare\
	-Werror=int-conversion\
	-Werror=implicit-int\
	-Werror=implicit-function-declaration\
	-Werror=incompatible-pointer-types\
	-Werror=unused-result\
	-Werror=switch\
	-Werror=format\
	-Werror=return-type\

STD:=-std=gnu17

#TODO: this should be generated
INCLUDE_FLAGS=-I.\
	-IPlatform\
	-IUtils\
	-IPythonEmbed\
	-IAllocators\
	-IDndc\

LINK_FLAGS=
FAST_FLAGS=-Ofast
DEV_FLAGS=-O0 -g
# DEBUG_FLAGS is set in the platform makefile as sanitizers vary per platform.

# Don't tolerate warnings for tests.
TEST_FLAGS=-Werror

# Currently this matches opt.mak
# This will override the local version if the template is changed.
%: %.template
	@echo "Creating default $@"
	@$(CP) $< $@

include opt.mak

# Platform specific nastiness.
# After inclusion, these variables should be set:
# CC, PYTHON, PYCFLAGS, PYLDFLAGS,
# RM, CP, MKDIR, TOUCH, EXE,
# DEBUG_FLAGS, LINK_FLAGS

ifeq ($(OS),Windows_NT)
UNAME:=Windows
include Platform\Windows\windows.mak
else
# nasty hack, need a better way to detect 32 bit
ifeq ($(shell uname -m),armv7l)
UNAME:=Rpi
include Platform/RPi32/rpi.mak
else
UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
include Platform/MacOS/macos.mak
else
include Platform/Linux/linux.mak
endif
endif
endif


ifeq ($(CC),clang)
WARNING_FLAGS+=-Wassign-enum\
	-Wshadow \
	-Warray-bounds-pointer-arithmetic\
	-Wcovered-switch-default\
	-Wfor-loop-analysis\
	-Winfinite-recursion\
	-Wduplicate-enum\
	-Wmissing-field-initializers\
	-Werror=pointer-type-mismatch\
	-Werror=extra-tokens\
	-Werror=macro-redefined\
	-Werror=initializer-overrides\
	-Werror=sometimes-uninitialized\
	-Werror=unused-comparison\
	-Werror=undefined-internal\
	-Werror=non-literal-null-conversion\
	-Werror=nullable-to-nonnull-conversion\
	-Werror=nullability-completeness\
	-Werror=nullability\
	-Wno-c++17-extensions\
	-Wno-gnu-alignof-expression\
	-Wuninitialized\
	-Wconditional-uninitialized\
	-Wcomma
else ifeq ($(CC),gcc)
WARNING_FLAGS+=-Wno-missing-braces\
	-Wno-missing-field-initializers
endif

ifeq ($(SPEED),FAST)
WARNING_FLAGS=
OPT_FLAGS=$(FAST_FLAGS)
else ifeq ($(SPEED),DEBUG)
OPT_FLAGS=$(DEBUG_FLAGS)
else ifeq ($(SPEED),DEV)
OPT_FLAGS=$(DEV_FLAGS)
# Only error if opt.mak exists, as make will re-invoke itself
# after building opt.mak if it is missing.
else ifeq ($(wildcard opt.mak),opt.mak)
$(error SPEED must be one of DEBUG|FAST|DEV: is '$(SPEED)')
endif

# We finally have everything defined
# We leave out OPT_FLAGS, LINK_FLAGS
FLAGS=$(INCLUDE_FLAGS) $(WARNING_FLAGS) $(PLATFORM_FLAGS)

# TestDndc needs to link against the frozenstdlib
TESTS:=$(filter-out Dndc/TestDndc.c, $(wildcard **/Test*.c))
# All of the tests are compiled both in fast and debug mode as bugs can
# sometimes will present themselves in one and not the other.

define TEST_template
$(BINDIR)/$(notdir $(basename $(1)))_fast$(EXE): $(DEPDIR)/$(notdir $(1))_fast.dep defs.mak | $(DIRECTORIES)
	@$(CC) $(TEST_FLAGS) $(FLAGS) $(FAST_FLAGS) $(TDEPFLAGS) $(DEPDIR)/$(notdir $(1))_fast.dep $(1) -o $$@ -g  $(LINK_FLAGS)
	$$@
$(BINDIR)/$(notdir $(basename $(1)))_debug$(EXE): $(DEPDIR)/$(notdir $(1))_debug.dep defs.mak | $(DIRECTORIES)
	@$(CC) $(TEST_FLAGS) $(FLAGS) $(DEBUG_FLAGS) $(TDEPFLAGS) $(DEPDIR)/$(notdir $(1))_debug.dep $(1) -o $$@ -g  $(LINK_FLAGS)
	$$@
$(notdir $(basename $(1))): $(BINDIR)/$(notdir $(basename $(1)))_debug$(EXE) $(BINDIR)/$(notdir $(basename $(1)))_fast$(EXE)
endef
$(foreach test, $(TESTS), $(eval $(call TEST_template, $(test))))

tests: $(notdir $(basename $(TESTS))) TestDndc
