
ifeq ($(UNAME),Darwin)
RPATH:=-rpath @executable_path
else
RPATH:=
endif

$(BINDIR)/dndc$(EXE): Dndc/dndc_cli.c $(DEPDIR)/dndc.dep  opt.mak $(VENDOBJDIR)/libquickjs.o | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) -g $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc.dep $< -o $@ $(VENDOBJDIR)/libquickjs.o $(LINK_FLAGS) $(RPATH)
.PHONY: dndc
dndc: $(BINDIR)/dndc$(EXE)

$(OBJDIR)/dndc.o: Dndc/dndc.c $(DEPDIR)/dndc_o.dep opt.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc_o.dep $< -c -o $@


$(BINDIR)/dndcfuzz$(EXE): Dndc/dndcfuzz.c $(DEPDIR)/dndcfuzz.dep $(VENDOBJDIR)/libquickjs.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) -O1 -g $(DEPFLAGS) $(DEPDIR)/dndcfuzz.dep $< -o $@ $(VENDOBJDIR)/libquickjs.o $(LINK_FLAGS) -fsanitize=fuzzer,address,undefined
.PHONY: dndcfuzz
dndcfuzz: $(BINDIR)/dndcfuzz$(EXE)

FUZZDIR=FuzzCorpus
$(FUZZDIR): ; @$(MKDIR) -p $@


$(BINDIR)/TestDndc_fast$(EXE): Dndc/TestDndc.c $(DEPDIR)/TestDndc_fast.dep $(BINDIR)/libquickjs$(SO) | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(FAST_FLAGS) $(DEPFLAGS) $(DEPDIR)/TestDndc_fast.dep $< -o $@ -g  $(LINK_FLAGS) -DQJS_SHARED_LIBRARY $(BINDIR)/libquickjs$(SOLIB) $(RPATH)
$(BINDIR)/TestDndc_debug$(EXE): Dndc/TestDndc.c $(DEPDIR)/TestDndc_debug.dep $(BINDIR)/libquickjs$(SO) | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(DEBUG_FLAGS) $(DEPFLAGS) $(DEPDIR)/TestDndc_debug.dep $< -o $@ -g  $(LINK_FLAGS) -DQJS_SHARED_LIBRARY $(BINDIR)/libquickjs$(SOLIB) $(RPATH)

$(TESTDIR)/TestDndc_debug: $(BINDIR)/TestDndc_debug$(EXE)
	$< --tee $@
tests: $(TESTDIR)/TestDndc_debug
$(TESTDIR)/TestDndc_fast: $(BINDIR)/TestDndc_fast$(EXE)
	$< --tee $@
tests: $(TESTDIR)/TestDndc_fast

TestDndc: $(TESTDIR)/TestDndc_debug $(TESTDIR)/TestDndc_fast
.PHONY: TestDndc

$(BINDIR)/TestDndcAst_fast$(EXE): Dndc/TestDndcAst.c $(DEPDIR)/TestDndcAst_fast.dep $(BINDIR)/libquickjs$(SO) | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(FAST_FLAGS) $(DEPFLAGS) $(DEPDIR)/TestDndcAst_fast.dep $< -o $@ -g  $(LINK_FLAGS) -DQJS_SHARED_LIBRARY $(BINDIR)/libquickjs$(SOLIB) $(RPATH)
$(BINDIR)/TestDndcAst_debug$(EXE): Dndc/TestDndcAst.c $(DEPDIR)/TestDndcAst_debug.dep $(BINDIR)/libquickjs$(SO) | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(DEBUG_FLAGS) $(DEPFLAGS) $(DEPDIR)/TestDndcAst_debug.dep $< -o $@ -g  $(LINK_FLAGS) -DQJS_SHARED_LIBRARY $(BINDIR)/libquickjs$(SOLIB) $(RPATH)

$(TESTDIR)/TestDndcAst_debug: $(BINDIR)/TestDndcAst_debug$(EXE)
	$< --tee $@
tests: $(TESTDIR)/TestDndcAst_debug
$(TESTDIR)/TestDndcAst_fast: $(BINDIR)/TestDndcAst_fast$(EXE)
	$< --tee $@
tests: $(TESTDIR)/TestDndcAst_fast

TestDndcAst: $(TESTDIR)/TestDndcAst_debug $(TESTDIR)/TestDndcAst_fast
.PHONY: TestDndcAst

$(BINDIR)/pydndc$(PYEXTENSION): Dndc/pydndc.c $(VENDOBJDIR)/libquickjs.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) $(PYCFLAGS) -O3 -g $(DEPFLAGS) $(DEPDIR)/pydndc.dep $(PYEXTFLAGS) $< -o $@  $(VENDOBJDIR)/libquickjs.o $(PYLDFLAGS) $(LINK_FLAGS)
.PHONY: pydndc
pydndc: pydndc/pydndc$(PYEXTENSION) $(BINDIR)/pydndc$(PYEXTENSION) PyDndEdit/pydndc$(PYEXTENSION) PyDndEdit/jsdoc.dnd PyDndEdit/dndc_js_api.d.ts

TestResults/testpydndc: $(BINDIR)/pydndc$(PYEXTENSION) Dndc/testpydndc.py
	$(PYTHON) Dndc/testpydndc.py --extension-directory $(BINDIR) --tee $@
tests: TestResults/testpydndc

PyDndEdit/pydndc$(PYEXTENSION): $(BINDIR)/pydndc$(PYEXTENSION)
	$(CP) $< $@
PyDndEdit/jsdoc.dnd: Dndc/jsdoc.dnd
	$(CP) $< $@
PyDndEdit/dndc_js_api.d.ts: Dndc/dndc_js_api.d.ts
	$(CP) $< $@
pydndc/pydndc$(PYEXTENSION): $(BINDIR)/pydndc$(PYEXTENSION)
	$(RM) -rf $@
	$(CP) $< $@


RELEASEFILES = $(BINDIR)/dndc$(EXE) $(BINDIR)/pydndc$(PYEXTENSION) PyDndEdit/dndedit.pyw PyDndEdit/changelog.dnd PyDndEdit/install_deps.py PyDndEdit/README.txt Documentation/OVERVIEW.dnd PyDndEdit/Manual.dnd Dndc/jsdoc.dnd Dndc/dndc_js_api.d.ts Documentation/REFERENCE.dnd pydndc/pydndc.pyi
.PHONY: release
release: $(RELEASEFILES)
	$(RM) -rf Release/Dndc Release/Dndc.$(DNDCVERSION).zip Release/Dndc.$(DNDCVERSION)
	$(MKDIR) -p Release/Dndc
	$(CP) $(RELEASEFILES) Release/Dndc
	$(CP) -r Examples Release/Dndc
	$(PYTHON) -m zipfile -c Release/Dndc.$(DNDCVERSION).zip Release/Dndc
	$(RM) Release/Dndc
ifeq ($(UNAME),Darwin)
MACFILES=$(BINDIR)/dndc $(BINDIR)/DndEdit $(BINDIR)/dndbr Documentation/OVERVIEW.dnd Dndc/jsdoc.dnd \
	 Dndc/dndc_js_api.d.ts Documentation/REFERENCE.dnd $(BINDIR)/libdndc.$(DNDCVERSION)$(SO) $(BINDIR)/dndc-tag
macrelease: $(MACFILES)
	$(RM) -rf Release/MacDndc Release/MacDndc.$(DNDCVERSION).zip Release/Dndc.$(DNDCVERSION)
	$(MKDIR) -p Release/MacDndc
	$(CP) $(MACFILES) Release/MacDndc
	$(CP) -r Examples Release/MacDndc
	$(PYTHON) -m zipfile -c Release/MacDndc.$(DNDCVERSION).zip Release/MacDndc
	$(RM) Release/MacDndc
endif

include Platform/Wasm/wasm.mak

$(OBJDIR)/dndc.wasm: Platform/Wasm/dndc_wasm.c Dndc/dndc.mak Platform/Wasm/wasm.mak $(DEPDIR)/dndc_wasm.dep | $(DIRECTORIES)
	$(WASMCC) -MD -MP -MF $(DEPDIR)/dndc_wasm.dep $(WASMCFLAGS) $(INCLUDE_FLAGS) $< -o $@

$(BINDIR)/demo.html: Platform/Wasm/demo.dnd $(OBJDIR)/dndc.wasm | $(DIRECTORIES) $(BINDIR)/dndc$(EXE)
	$(BINDIR)/dndc $< -o $@ -d $(DEPDIR)/demo.html.dep


$(BINDIR)/dndc-browse$(EXE): Bin/libdndc.$(DNDCVERSION)$(SO) Dndc/dndc_browse.c
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc_browse.dep Dndc/dndc_browse.c -o $@ Bin/libdndc.$(DNDCVERSION)$(SOLIB) $(LINK_FLAGS) $(RPATH)
.PHONY: dndc-browse
dndc-browse: $(BINDIR)/dndc-browse$(EXE)
all: dndc-browse

$(BINDIR)/dndc-tag$(EXE): Bin/libdndc.$(DNDCVERSION)$(SO) Dndc/dndc_tag.c
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc_tag.dep Dndc/dndc_tag.c -o $@ Bin/libdndc.$(DNDCVERSION)$(SOLIB) $(LINK_FLAGS) $(RPATH)
.PHONY: dndc-tag
dndc-tag: $(BINDIR)/dndc-tag$(EXE)
all: dndc-tag
