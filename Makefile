include defs.mak
include PythonEmbed/pythonembed.mak
include DndC/dndc.mak

DNDC:=$(BINDIR)/dndc$(EXE)

%/README.html: %/README.dnd | $(DNDC)
	mkdir -p Depends/$*
	$(DNDC) $< $@ -d $(DEPDIR)/$*

README.html: README.dnd | $(DNDC)
	$(DNDC) $< $@ -d $(DEPDIR)

# Assumes libclang is installed.
tags: $(wildcard *.h *.c **/*.c **/*.h) Scripts/tag_and_syntax.py compile_commands.json
	$(PYTHON) -m Scripts.tag_and_syntax

.PHONY: clean clean-tests clean-depends deep-clean run-tests strip convert directories install compile_commands fuzz

# Assumes compiledb is installed.
compile_commands.json:
	$(MAKE) clean
	$(PYTHON) -m compiledb make
clean:
	@$(RM)  -f $(OBJDIR)/*
	@$(RM)  -f $(BINDIR)/*

clean-tests:
	@$(RM) -f $(BINDIR)/Test*

clean-depends:
	@$(RM) -f $(DEPDIR)/*

deep-clean: clean clean-tests clean-depends
	@$(RM) -rf PythonEmbed/frozen
	@$(RM) -r $(DIRECTORIES)

directories: $(DIRECTORIES)

# Strips trailing whitespace from source files.
strip:
	$(PYTHON) -m Scripts.convert --strip_only

# Renames identifiers.
convert:
	$(PYTHON) -m Scripts.convert --extensions .h .c

run-tests: clean-tests tests

all: tests dndc dndcbench README.html

install: $(DNDC)
	@$(INSTALL) -C $< $(INSTALLDIR)/dndc$(EXE)

fuzz: $(BINDIR)/dndcfuzz$(EXE) | $(FUZZDIR)
	$< $(FUZZDIR) -fork=4 -only_ascii=1

.DEFAULT_GOAL := dndc
