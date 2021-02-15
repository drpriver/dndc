include defs.mak
include PythonEmbed/pythonembed.mak
include DndC/dndc.mak

%/README.html: %/README.dnd | $(BINDIR)/dndc
	mkdir -p Depends/$*
	$(BINDIR)/dndc $< $@ -d $(DEPDIR)/$*

tags: $(wildcard *.h *.c **/*.c **/*.h) Scripts/tag_and_syntax.py compile_commands.json
	$(PYTHON) -m Scripts.tag_and_syntax

.PHONY: clean clean-tests clean-depends deep-clean run-tests strip convert directories install
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

strip:
	$(PYTHON) -m Scripts.convert --strip_only
convert:
	$(PYTHON) -m Scripts.convert --extensions .h .c

run-tests: clean-tests tests

all: dndc

install: $(BINDIR)/dndc
	install -vdC $< $(INSTALLDIR)/dndc

.DEFAULT_GOAL := all
