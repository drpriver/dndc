DNDCVERSION:=1.2.1
DNDC_COMPAT_VERSION:=1.0.0
include defs.mak
include Vendored/quickjs.mak
include Dndc/dndc.mak
include Dndc/pydndc.mak
ifeq ($(QTGUI),true)
include qt.mak
endif

DNDC:=$(BINDIR)/dndc$(EXE)

#%.html: %.dnd | $(DNDC) $(DIRECTORIES)
#	$(DNDC) $< -o $@ -d $(DEPDIR)/$*.dep

$(DOCDIR)/OVERVIEW.html: Documentation/OVERVIEW.dnd
	$(DNDC) $< -o $@ -d $(DEPDIR)/OVERVIEW.dep
$(DOCDIR)/REFERENCE.html: Documentation/REFERENCE.dnd
	$(DNDC) $< -o $@ -d $(DEPDIR)/REFERENCE.dep
$(DOCDIR)/jsdoc.html: Dndc/jsdoc.dnd Dndc/dndc_js_api.d.ts
	$(DNDC) Dndc/jsdoc.dnd -o $@ -d $(DEPDIR)/jsdoc.dep
$(DOCDIR)/changelog.html: PyDndEdit/PyDndEdit/changelog.dnd
	$(DNDC) $< -o $@ -d $(DEPDIR)/changelog.dep
$(DOCDIR)/gui-manual.html: PyDndEdit/PyDndEdit/Manual.dnd
	$(DNDC) $< -o $@ -d $(DEPDIR)/gui-manual.dep
$(DOCDIR)/index.html: Documentation/index.dnd
	$(DNDC) $< -o $@ -d $(DEPDIR)/docs-index.dep

.PHONY: docs
DOCS=$(addprefix $(DOCDIR)/,OVERVIEW.html REFERENCE.html jsdoc.html changelog.html gui-manual.html index.html dndc.html)
$(DOCS): | $(DNDC) $(DIRECTORIES)
docs: $(DOCS)

$(EXAMPLEDIR)/Examples/%.html : Examples/%.dnd | $(DIRECTORIES)
	$(DNDC) $< -o $@ -d $(DEPDIR)/$(subst /,-,$*).dep
.PHONY: example-dnd
example-dnd: $(addsuffix .html,$(basename $(addprefix $(EXAMPLEDIR)/,$(wildcard Examples/*/*/*.dnd Examples/*/*.dnd Examples/*.dnd))))

# Assumes libclang is installed.
tags: $(wildcard *.h *.c **/*.c **/*.h **/*.m) Scripts/tag_and_syntax.py compile_commands.json
	$(PYTHON) -m Scripts.tag_and_syntax
# Assumes compiledb is installed.
compile_commands.json:
	$(MAKE) clean
	$(PYTHON) -m compiledb make
.PHONY: clean
clean:
	@$(RM) -f $(OBJDIR)/*
	@$(RM) -f $(BINDIR)/*
.PHONY: clean-tests
clean-tests:
	@$(RM) -f $(BINDIR)/Test*
clean-vendored:
	@$(RM) -f $(VENDOBJDIR)/*
.PHONY: clean-depends
clean-depends:
	@$(RM) -f $(DEPDIR)/*
.PHONY: deep-clean
deep-clean: clean clean-tests clean-depends
	@$(RM) -r $(DIRECTORIES)
.PHONY: clean-docs
clean-docs:
	@$(RM) -r $(DOCDIR)
.PHONY: clean-examples
clean-examples:
	@$(RM) -r $(EXAMPLEDIR)

.PHONY: directories
directories: $(DIRECTORIES)

# Strips trailing whitespace from source files.
.PHONY: strip
strip:
	$(PYTHON) -m Scripts.convert --strip_only

# Renames identifiers.
.PHONY: convert
convert:
	$(PYTHON) -m Scripts.convert --extensions .h .c

.PHONY: run-tests
run-tests: clean-tests tests

.PHONY: all
all: tests dndc pydndc docs dndc-browse

.PHONY: install
install: $(DNDC)
	$(INSTALL) -C $< $(INSTALLDIR)/dndc$(EXE)

install-pydndc: pydndc
	$(PYTHON) -m pip install -e . --user

.PHONY: fuzz
fuzz: $(BINDIR)/dndcfuzz$(EXE) | $(FUZZDIR)
	$< $(FUZZDIR) -fork=4 -only_ascii=1
.PHONY: fuzzformat
fuzzformat: $(BINDIR)/dndcfuzzformat$(EXE) | $(FUZZDIR)
	$< $(FUZZDIR) -fork=4 -only_ascii=1
.PHONY: fuzzmd
fuzzmd: $(BINDIR)/dndcfuzzmd$(EXE) | $(FUZZDIR)
	$< $(FUZZDIR) -fork=4 -only_ascii=1

ifneq ($(UNAME),Windows) # shells out to unix commands
.PHONY: list
list:
	@LC_ALL=C $(MAKE) -npRrq : 2>/dev/null \
		| awk -v RS= -F: '{if ($$1 !~ "^[#.]") {print $$1}}' \
		| sort \
		| uniq \
		| egrep -v \
			-e '^[^[:alnum:]]' \
			-e '^$@$$' \
			-e '.(dnd|dep|[ch])$$' \
			-e '^(Bin|Depends|Objs|VendObjs|Generated|FuzzCorpus)' \
			-e '^(RenderedExamples|RenderedDocs|TestResults)' \
			-e '^(Dndc/)'
			
endif

$(DOCDIR)/%.h.html: %.h | $(DOCDIR)
	$(PYTHON) -m Scripts.cdoc $< -o $(DOCDIR) -d $(DEPDIR)/$<.html.dep --cflags -iquote .
$(DOCDIR)/Utils/Marray.h.html: Utils/Marray.h | $(DOCDIR)
	$(PYTHON) -m Scripts.cdoc $< -o $(DOCDIR) -d $(DEPDIR)/$<.html.dep --cflags -iquote . -DMARRAY_T=int
$(DOCDIR)/Utils/Rarray.h.html: Utils/Rarray.h | $(DOCDIR)
	$(PYTHON) -m Scripts.cdoc $< -o $(DOCDIR) -d $(DEPDIR)/$<.html.dep --cflags -iquote . -DRARRAY_T=int
$(DOCDIR)/Dndc/pyhead.h.html: Dndc/pyhead.h | $(DOCDIR)
	$(PYTHON) -m Scripts.cdoc $< -o $(DOCDIR) -d $(DEPDIR)/$<.html.dep --cflags $(PYCFLAGS)
# these have trouble with generating docs
NODOC=Utils/dsort.h \
      Utils/dsort_test_strings.h \
      QtDndcEdit/DndcEdit.h
CDOCS=$(addprefix $(DOCDIR)/,$(addsuffix .html,$(filter-out $(NODOC),$(wildcard */*.h))))
.PHONY: cdocs
cdocs: $(CDOCS) $(DOCDIR)/cdocindex.html
$(DOCDIR)/cdocindex.html: | $(CDOCS)
	$(PYTHON) -m Scripts.make_cdoc_index $(DOCDIR)

.PHONY: docset
docset: dndc.docset

docsetfiles=$(DOCDIR)/Dndc/dndc_ast.h.html $(DOCDIR)/Dndc/dndc.h.html $(DOCDIR)/jsdoc.html

dndc.docset: $(docsetfiles)
	$(PYTHON) -m Scripts.make_docset $(docsetfiles) -o $@

README.md: README.dnd | $(BINDIR)/dndc$(EXE)
	$(BINDIR)/dndc$(EXE) $< -o $@ --md --no-css
Documentation/OVERVIEW.md: Documentation/OVERVIEW.dnd | $(BINDIR)/dndc$(EXE)
	$(BINDIR)/dndc$(EXE) $< -o $@ --md --no-css

.PHONY: coverage
coverage:
	$(RM) -r covdir
	meson setup covdir -Db_coverage=true
	meson compile -C covdir
	meson test -C covdir
	ninja coverage -C covdir
