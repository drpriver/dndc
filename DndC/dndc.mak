$(BINDIR)/dndc$(EXE): DndC/dndc.c $(DEPDIR)/dndc.dep  $(OBJDIR)/frozenstdlib.o opt.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -DDNDCMAIN
dndc: $(BINDIR)/dndc$(EXE)

$(OBJDIR)/dndc.o: DndC/dndc.c $(DEPDIR)/dndc_o.dep opt.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc_o.dep $< -c -o $@


BENCHMARKITERS ?= 1000
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

$(BINDIR)/pydndc.cpython-38-darwin.so: DndC/pydndc.c

$(BINDIR)/pydndc$(PYEXTENSION): DndC/pydndc.c
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) $(PYCFLAGS) -g $(DEPFLAGS) $(DEPDIR)/pydndc.dep $(PYEXTFLAGS) $< -o $@ $(LINK_FLAGS) $(PYLDFLAGS)
pydndc: $(BINDIR)/pydndc$(PYEXTENSION)
