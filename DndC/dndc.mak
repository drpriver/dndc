$(BINDIR)/dndc$(EXE): DndC/dndc.c $(DEPDIR)/dndc.dep  $(OBJDIR)/frozenstdlib.o opt.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS)
dndc: $(BINDIR)/dndc$(EXE)


BENCHMARKINPUTPATH ?= surface.dnd
BENCHMARKOUTPUTPATH ?= foo.html
BENCHMARKDIRECTORY ?= .
$(BINDIR)/dndcbench: DndC/dndc.c $(DEPDIR)/dndcbench.dep DndC/dndc.mak $(OBJDIR)/frozenstdlib.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) -O1 -g $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/dndcbench.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -DBENCHMARKING -DBENCHMARKINPUTPATH='"$(BENCHMARKINPUTPATH)"' -DBENCHMARKOUTPUTPATH='"$(BENCHMARKOUTPUTPATH)"' -DBENCHMARKDIRECTORY='"$(BENCHMARKDIRECTORY)"' -DBENCHMARKITERS=1
dndcbench: $(BINDIR)/dndcbench

$(BINDIR)/dndcfuzz: DndC/dndcfuzz.c $(DEPDIR)/dndcfuzz.dep $(OBJDIR)/frozenstdlib.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) -O1 -g $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/dndcfuzz.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -DNOMAIN -fsanitize=fuzzer,address,undefined
dndcfuzz: $(BINDIR)/dndcfuzz

FUZZDIR=FuzzCorpus
$(FUZZDIR): ; @$(MKDIR) -p $@


