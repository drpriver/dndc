.SUFFIXES:
# cribbed from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEPDIR := Depends
OBJDIR := Objs
BINDIR := Bin
VENDOBJDIR:= VendObjs
TESTDIR := TestResults
DEPFLAGS = -MT $@ -MMD -MP -MF
DOCDIR := RenderedDocs
EXAMPLEDIR := RenderedExamples
# Templates require an extra $
TDEPFLAGS = -MT $$@ -MMD -MP -MF
$(DEPDIR):  ; @$(MKDIR) -p $@
$(OBJDIR):  ; @$(MKDIR) -p $@
$(VENDOBJDIR): ; @$(MKDIR) -p $@
$(BINDIR):  ; @$(MKDIR) -p $@
$(DOCDIR):  ; @$(MKDIR) -p $@
$(TESTDIR): ; @$(MKDIR) -p $@
$(EXAMPLEDIR):
	@$(MKDIR) -p $(EXAMPLEDIR)/Examples/Calendar
	@$(MKDIR) -p $(EXAMPLEDIR)/Examples/KrugsBasement
	@$(MKDIR) -p $(EXAMPLEDIR)/Examples/Rules
	@$(MKDIR) -p $(EXAMPLEDIR)/Examples/Wiki/Inner
DIRECTORIES= $(DEPDIR) $(OBJDIR) $(BINDIR) $(DOCDIR) $(TESTDIR) $(EXAMPLEDIR) $(VENDOBJDIR)
%.dep: ;
DEPFILES:= $(wildcard Depends/*.dep)
include $(DEPFILES)

WARNING_FLAGS=-Wall\
	-Wbad-function-cast\
	-Wextra \
	-Wvla\
	-Wmissing-noreturn\
	-Wcast-qual\
	-Wdeprecated\
	-Wdouble-promotion\
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
INCLUDE_FLAGS=-iquote .

LINK_FLAGS=
FAST_FLAGS=-O3
DEV_FLAGS=-O0 -g
# DEBUG_FLAGS is set in the platform makefile as sanitizers vary per platform.

# Don't tolerate warnings for tests.
TEST_FLAGS=-Werror

opt.mak: | opt.mak.template
	@echo "Creating default $@"
	@$(CP) opt.mak.template $@

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

ifneq ($(findstring $(CC),clang),)
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
	-Wuninitialized\
	-Wconditional-uninitialized\
	-Werror=undefined-internal\
	-Wcomma
endif
ifneq ($(findstring $(CC),gcc),)
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
FLAGS=$(INCLUDE_FLAGS) $(PLATFORM_FLAGS)



# Testing


# TestDndc needs special link arguments, so define it manually.
# TestDsort hangs gcc for some reason.
TESTS:=$(filter-out Dndc/TestDndc.c Dndc/TestDndcAst.c Dndc/TestDndcAlloc.c Dndc/TestQJS.c Utils/TestDsort.c,$(wildcard **/Test*.c))

ifneq (,$(findstring s,$(MAKEFLAGS)))
TESTQUIET=-s
else
TESTQUIET=
endif

# This template defines how to build and run a test. Tests are automatically
# discovered by convention: a test program is a C file starting with 'Test'.
#
# All of the tests are compiled both in fast and debug mode as bugs
# sometimes will present themselves in one and not the other.
#
# Tests will write the results of their test to the TestResults directory.
# If the test fails, the result is deleted. Maybe we should add another file
# for telling make the test passed so you can review the tests results, but
# meh. Just run the test again without using make. It'll be in the Bin folder.
# You can even debug it using a debugger, imagine that.
define TEST_template
$(BINDIR)/$(notdir $(basename $(1)))_fast$(EXE): $(DEPDIR)/$(notdir $(1))_fast.dep defs.mak | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(FAST_FLAGS) $(TDEPFLAGS) $(DEPDIR)/$(notdir $(1))_fast.dep $(1) -o $$@ -g  $(LINK_FLAGS)

$(BINDIR)/$(notdir $(basename $(1)))_debug$(EXE): $(DEPDIR)/$(notdir $(1))_debug.dep defs.mak | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(DEBUG_FLAGS) $(TDEPFLAGS) $(DEPDIR)/$(notdir $(1))_debug.dep $(1) -o $$@ -g  $(LINK_FLAGS)

$(TESTDIR)/$(notdir $(basename $(1)))_debug: $(BINDIR)/$(notdir $(basename $(1)))_debug$(EXE)
	$$< --tee $$@ --shuffle $(TESTQUIET)

$(TESTDIR)/$(notdir $(basename $(1)))_fast: $(BINDIR)/$(notdir $(basename $(1)))_fast$(EXE)
	$$< --tee $$@ --shuffle $(TESTQUIET)

tests: $(TESTDIR)/$(notdir $(basename $(1)))_fast
tests: $(TESTDIR)/$(notdir $(basename $(1)))_debug
endef
$(foreach test, $(TESTS), $(eval $(call TEST_template, $(test))))

.PHONY: tests

# Delete output of targets that failed
.DELETE_ON_ERROR:
