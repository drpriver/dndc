include defs.mak
include Vendored/quickjs.mak
include Dndc/dndc.mak
ifeq ($(QTGUI),true)
include qt.mak
endif

DNDC:=$(BINDIR)/dndc$(EXE)

#%.html: %.dnd | $(DNDC) $(DIRECTORIES)
#	$(DNDC) $< -o $@ -d $(DEPDIR)/$*.dep

$(DOCDIR)/OVERVIEW.html: Docs/OVERVIEW.dnd
	$(DNDC) $< -o $@ -d $(DEPDIR)/OVERVIEW.dep
$(DOCDIR)/REFERENCE.html: Docs/REFERENCE.dnd
	$(DNDC) $< -o $@ -d $(DEPDIR)/REFERENCE.dep
$(DOCDIR)/jsdoc.html: Dndc/jsdoc.dnd Dndc/dndc_js_api.d.ts
	$(DNDC) Dndc/jsdoc.dnd -o $@ -d $(DEPDIR)/jsdoc.dep
$(DOCDIR)/changelog.html: PyGdndc/changelog.dnd
	$(DNDC) $< -o $@ -d $(DEPDIR)/changelog.dep
$(DOCDIR)/gui-manual.html: PyGdndc/Manual.dnd
	$(DNDC) $< -o $@ -d $(DEPDIR)/gui-manual.dep

.PHONY: docs
DOCS= $(addprefix $(DOCDIR)/,OVERVIEW.html REFERENCE.html jsdoc.html changelog.html gui-manual.html)
$(DOCS): | $(DNDC) $(DIRECTORIES)
docs: $(DOCS)

# Assumes libclang is installed.
tags: $(wildcard *.h *.c **/*.c **/*.h **/*.m) Scripts/tag_and_syntax.py compile_commands.json
	$(PYTHON) -m Scripts.tag_and_syntax

.PHONY: clean clean-tests clean-depends deep-clean run-tests strip convert directories install compile_commands fuzz

# Assumes compiledb is installed.
compile_commands.json:
	$(MAKE) clean
	$(PYTHON) -m compiledb make
clean:
	@$(RM) -f $(OBJDIR)/*
	@$(RM) -f $(BINDIR)/*

clean-tests:
	@$(RM) -f $(BINDIR)/Test*

clean-depends:
	@$(RM) -f $(DEPDIR)/*

deep-clean: clean clean-tests clean-depends
	@$(RM) -r $(DIRECTORIES)

directories: $(DIRECTORIES)

# Strips trailing whitespace from source files.
strip:
	$(PYTHON) -m Scripts.convert --strip_only

# Renames identifiers.
convert:
	$(PYTHON) -m Scripts.convert --extensions .h .c

run-tests: clean-tests tests

all: tests dndc pydndc docs

install: $(DNDC)
	$(INSTALL) -C $< $(INSTALLDIR)/dndc$(EXE)

fuzz: $(BINDIR)/dndcfuzz$(EXE) | $(FUZZDIR)
	$< $(FUZZDIR) -fork=4 -only_ascii=1

ifneq ($(UNAME),Windows) # shells out to unix commands
.PHONY: list
list:
	@LC_ALL=C $(MAKE) -npRrq : 2>/dev/null \
		| awk -v RS= -F: '{if ($$1 !~ "^[#.]") {print $$1}}' \
		| sort \
		| egrep -v \
			-e '^[^[:alnum:]]' \
			-e '^$@$$' \
			-e '.(dnd|dep|[ch])$$'
endif
.DEFAULT_GOAL:=all
