DNDCVERSION = 0.7.0

$(BINDIR)/libquickjs$(SO): Vendored/quickjs/libquickjs$(SO)
	cp $< $@
$(OBJDIR)/libquickjs.a: Vendored/quickjs/libquickjs.a
	cp $< $@
ifeq ($(UNAME),Darwin)
RPATH:=-rpath @executable_path
else
RPATH:=
endif
$(BINDIR)/dndc$(EXE): Dndc/dndc.c $(DEPDIR)/dndc.dep  $(OBJDIR)/frozenstdlib.o opt.mak $(BINDIR)/libquickjs$(SO) | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -DDNDCMAIN $(BINDIR)/libquickjs$(SO) $(RPATH)
dndc: $(BINDIR)/dndc$(EXE)

$(OBJDIR)/dndc.o: Dndc/dndc.c $(DEPDIR)/dndc_o.dep opt.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc_o.dep $< -c -o $@


BENCHMARKITERS ?= 10000
$(BINDIR)/dndcbench$(EXE): Dndc/dndc.c $(DEPDIR)/dndcbench.dep Dndc/dndc.mak $(OBJDIR)/frozenstdlib.o $(BINDIR)/libquickjs$(SO)
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) -O1 -g $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/dndcbench.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -DBENCHMARKING -DBENCHMARKITERS=$(BENCHMARKITERS) -DDNDCMAIN $(BINDIR)/libquickjs$(SO) $(RPATH)
dndcbench: $(BINDIR)/dndcbench$(EXE)

$(BINDIR)/dndcfuzz$(EXE): Dndc/dndcfuzz.c $(DEPDIR)/dndcfuzz.dep $(OBJDIR)/frozenstdlib.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) -O1 -g $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/dndcfuzz.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -fsanitize=fuzzer,address,undefined
dndcfuzz: $(BINDIR)/dndcfuzz$(EXE)

FUZZDIR=FuzzCorpus
$(FUZZDIR): ; @$(MKDIR) -p $@


$(BINDIR)/TestDndc_fast$(EXE): Dndc/TestDndc.c $(DEPDIR)/TestDndc_fast.dep $(OBJDIR)/frozenstdlib.o $(BINDIR)/libquickjs$(SO) | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(FAST_FLAGS) $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/TestDndc_fast.dep $(OBJDIR)/frozenstdlib.o $< -o $@ -g  $(LINK_FLAGS) $(PYLDFLAGS) $(BINDIR)/libquickjs$(SO) $(RPATH)
	$@
$(BINDIR)/TestDndc_debug$(EXE): Dndc/TestDndc.c $(DEPDIR)/TestDndc_debug.dep $(OBJDIR)/frozenstdlib.o $(BINDIR)/libquickjs$(SO) | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(DEBUG_FLAGS) $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/TestDndc_debug.dep $(OBJDIR)/frozenstdlib.o $< -o $@ -g  $(LINK_FLAGS) $(PYLDFLAGS) $(BINDIR)/libquickjs$(SO) $(RPATH)
	$@
TestDndc: $(BINDIR)/TestDndc_debug$(EXE) $(BINDIR)/TestDndc_fast$(EXE)

$(BINDIR)/pydndc$(PYEXTENSION): Dndc/pydndc.c $(OBJDIR)/libquickjs.a
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) $(PYCFLAGS) -O2 $(DEPFLAGS) $(DEPDIR)/pydndc.dep $(PYEXTFLAGS) $< -o $@ $(LINK_FLAGS) $(PYLDFLAGS) $(OBJDIR)/libquickjs.a
pydndc: $(BINDIR)/pydndc$(PYEXTENSION) PyGdndc/pydndc$(PYEXTENSION) PyGdndc/pydndc.pyi

PyGdndc/pydndc.pyi: Dndc/pydndc.pyi
	$(CP) $< $@
PyGdndc/pydndc$(PYEXTENSION): $(BINDIR)/pydndc$(PYEXTENSION)
	$(CP) $< $@


RELEASEFILES = $(BINDIR)/pydndc$(PYEXTENSION) PyGdndc/pygdndc.pyw PyGdndc/changelog.dnd PyGdndc/install_deps.py PyGdndc/README.txt EXAMPLE.dnd PyGdndc/Manual.dnd
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
	$(BINDIR)/dndc $< -o $@ -d $(DEPDIR)/demo.html.dep --use-site
