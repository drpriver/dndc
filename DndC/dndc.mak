$(BINDIR)/dndc$(EXE): DndC/dndc.c $(DEPDIR)/dndc.dep  $(OBJDIR)/frozenstdlib.o opt.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -DDNDCMAIN
dndc: $(BINDIR)/dndc$(EXE)

$(OBJDIR)/dndc.o: DndC/dndc.c $(DEPDIR)/dndc_o.dep opt.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc_o.dep $< -c -o $@


BENCHMARKITERS ?= 10000
$(BINDIR)/dndcbench$(EXE): DndC/dndc.c $(DEPDIR)/dndcbench.dep DndC/dndc.mak $(OBJDIR)/frozenstdlib.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) -O1 -g $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/dndcbench.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -DBENCHMARKING -DBENCHMARKITERS=$(BENCHMARKITERS) -DDNDCMAIN
dndcbench: $(BINDIR)/dndcbench$(EXE)

$(BINDIR)/dndcfuzz$(EXE): DndC/dndcfuzz.c $(DEPDIR)/dndcfuzz.dep $(OBJDIR)/frozenstdlib.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) -O1 -g $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/dndcfuzz.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -fsanitize=fuzzer,address,undefined
dndcfuzz: $(BINDIR)/dndcfuzz$(EXE)

FUZZDIR=FuzzCorpus
$(FUZZDIR): ; @$(MKDIR) -p $@


$(BINDIR)/TestDndC_fast$(EXE): DndC/TestDndC.c $(DEPDIR)/TestDndC_fast.dep $(OBJDIR)/frozenstdlib.o | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(FAST_FLAGS) $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/TestDndC_fast.dep $(OBJDIR)/frozenstdlib.o $< -o $@ -g  $(LINK_FLAGS) $(PYLDFLAGS)
	$@
$(BINDIR)/TestDndC_debug$(EXE): DndC/TestDndC.c $(DEPDIR)/TestDndC_debug.dep $(OBJDIR)/frozenstdlib.o | $(DIRECTORIES)
	$(CC) $(TEST_FLAGS) $(FLAGS) $(DEBUG_FLAGS) $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/TestDndC_debug.dep $(OBJDIR)/frozenstdlib.o $< -o $@ -g  $(LINK_FLAGS) $(PYLDFLAGS)
	$@
TestDndC: $(BINDIR)/TestDndC_debug$(EXE) $(BINDIR)/TestDndC_fast$(EXE)

$(BINDIR)/pydndc$(PYEXTENSION): DndC/pydndc.c
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) $(PYCFLAGS) -O2 $(DEPFLAGS) $(DEPDIR)/pydndc.dep $(PYEXTFLAGS) $< -o $@ $(LINK_FLAGS) $(PYLDFLAGS)
pydndc: $(BINDIR)/pydndc$(PYEXTENSION) PyGdndc/pydndc$(PYEXTENSION) PyGdndc/pydndc.pyi

PyGdndc/pydndc.pyi: DndC/pydndc.pyi
	$(CP) $< $@
PyGdndc/pydndc$(PYEXTENSION): $(BINDIR)/pydndc$(PYEXTENSION)
	$(CP) $< $@

DNDCVERSION = 0.4.7

RELEASEFILES = $(BINDIR)/pydndc$(PYEXTENSION) PyGdndc/pygdndc.pyw PyGdndc/changelog.dnd PyGdndc/install_deps.py PyGdndc/README.txt EXAMPLE.dnd PyGdndc/Manual.dnd
.PHONY: release
release: $(RELEASEFILES)
	$(RM) -f Release/DndC Release/DndC.$(DNDCVERSION).zip
	$(MKDIR) -p Release/DndC
	$(CP) $(RELEASEFILES) Release/DndC
	$(CP) -r Examples Release/DndC
	$(PYTHON) -m zipfile -c Release/DndC.$(DNDCVERSION).zip Release/DndC
	$(RM) Release/DndC

include Platform/Wasm/wasm.mak

$(OBJDIR)/dndc.wasm: Platform/Wasm/dndc_wasm.c DndC/dndc.mak Platform/Wasm/wasm.mak $(DEPDIR)/dndc_wasm.dep | $(DIRECTORIES)
	$(WASMCC) -MD -MP -MF $(DEPDIR)/dndc_wasm.dep $(WASMCFLAGS) $(INCLUDE_FLAGS) $< -o $@

$(BINDIR)/demo.html: Platform/Wasm/demo.dnd $(OBJDIR)/dndc.wasm | $(DIRECTORIES) $(BINDIR)/dndc
	$(BINDIR)/dndc $< -o $@ -d $(DEPDIR)/demo.html.dep --use-site
