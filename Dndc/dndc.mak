DNDCVERSION=0.10.3
DNDC_COMPAT_VERSION=0.10.2

ifeq ($(UNAME),Darwin)
RPATH:=-rpath @executable_path
else
RPATH:=
endif

$(BINDIR)/dndc$(EXE): Dndc/dndc_cli.c $(DEPDIR)/dndc.dep  opt.mak $(VENDOBJDIR)/libquickjs.o | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc.dep $< -o $@ $(VENDOBJDIR)/libquickjs.o $(LINK_FLAGS) $(RPATH)
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

$(BINDIR)/pydndc$(PYEXTENSION): Dndc/pydndc.c $(VENDOBJDIR)/libquickjs.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) $(PYCFLAGS) -O2 $(DEPFLAGS) $(DEPDIR)/pydndc.dep $(PYEXTFLAGS) $< -o $@  $(VENDOBJDIR)/libquickjs.o $(PYLDFLAGS) $(LINK_FLAGS)
.PHONY: pydndc
pydndc: $(BINDIR)/pydndc$(PYEXTENSION) PyGdndc/pydndc$(PYEXTENSION) PyGdndc/pydndc.pyi PyGdndc/jsdoc.dnd PyGdndc/dndc_js_api.d.ts
TestResults/testpydndc: $(BINDIR)/pydndc$(PYEXTENSION) Dndc/testpydndc.py
	$(PYTHON) Dndc/testpydndc.py --extension-directory $(BINDIR) --tee $@
tests: TestResults/testpydndc

PyGdndc/pydndc.pyi: Dndc/pydndc.pyi
	$(CP) $< $@
PyGdndc/pydndc$(PYEXTENSION): $(BINDIR)/pydndc$(PYEXTENSION)
	$(CP) $< $@
PyGdndc/jsdoc.dnd: Dndc/jsdoc.dnd
	$(CP) $< $@
PyGdndc/dndc_js_api.d.ts: Dndc/dndc_js_api.d.ts
	$(CP) $< $@


RELEASEFILES = $(BINDIR)/dndc$(EXE) $(BINDIR)/pydndc$(PYEXTENSION) PyGdndc/pygdndc.pyw PyGdndc/dndbatch.pyw PyGdndc/changelog.dnd PyGdndc/install_deps.py PyGdndc/README.txt Documentation/OVERVIEW.dnd PyGdndc/Manual.dnd Dndc/jsdoc.dnd Dndc/dndc_js_api.d.ts Documentation/REFERENCE.dnd
.PHONY: release
release: $(RELEASEFILES)
	$(RM) -f Release/Dndc Release/Dndc.$(DNDCVERSION).zip
	$(MKDIR) -p Release/Dndc
	$(CP) $(RELEASEFILES) Release/Dndc
	$(CP) -r Examples Release/Dndc
	$(PYTHON) -m zipfile -c Release/Dndc.$(DNDCVERSION).zip Release/Dndc
	$(RM) Release/Dndc

include Platform/Wasm/wasm.mak

$(OBJDIR)/dndc.wasm: Platform/Wasm/dndc_wasm.c Dndc/dndc.mak Platform/Wasm/wasm.mak $(DEPDIR)/dndc_wasm.dep | $(DIRECTORIES)
	$(WASMCC) -MD -MP -MF $(DEPDIR)/dndc_wasm.dep $(WASMCFLAGS) $(INCLUDE_FLAGS) $< -o $@

$(BINDIR)/demo.html: Platform/Wasm/demo.dnd $(OBJDIR)/dndc.wasm | $(DIRECTORIES) $(BINDIR)/dndc
	$(BINDIR)/dndc $< -o $@ -d $(DEPDIR)/demo.html.dep


$(BINDIR)/dndc-browse$(EXE): Bin/libdndc$(SO) Dndc/dndc_browse.c
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc_browse.dep Dndc/dndc_browse.c -o $@ Bin/libdndc$(SOLIB) $(LINK_FLAGS) $(RPATH)
.PHONY: dndc-browse
dndc-browse: $(BINDIR)/dndc-browse$(EXE)
all: dndc-browse
