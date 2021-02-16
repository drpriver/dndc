$(BINDIR)/dndc$(EXE): DndC/dndc.c $(DEPDIR)/dndc.dep  $(OBJDIR)/frozenstdlib.o opt.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS)
dndc: $(BINDIR)/dndc$(EXE)


BENCHMARKINPUTPATH ?= surface.dnd
BENCHMARKOUTPUTPATH ?= foo.html
BENCHMARKDIRECTORY ?= .
BENCHMARKITERS ?= 1000
$(BINDIR)/dndcbench$(EXE): DndC/dndc.c $(DEPDIR)/dndcbench.dep DndC/dndc.mak $(OBJDIR)/frozenstdlib.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) -O1 -g $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/dndcbench.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -DBENCHMARKING -DBENCHMARKINPUTPATH='"$(BENCHMARKINPUTPATH)"' -DBENCHMARKOUTPUTPATH='"$(BENCHMARKOUTPUTPATH)"' -DBENCHMARKDIRECTORY='"$(BENCHMARKDIRECTORY)"' -DBENCHMARKITERS=$(BENCHMARKITERS)
dndcbench: $(BINDIR)/dndcbench$(EXE)

$(BINDIR)/dndcfuzz$(EXE): DndC/dndcfuzz.c $(DEPDIR)/dndcfuzz.dep $(OBJDIR)/frozenstdlib.o
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) -O1 -g $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/dndcfuzz.dep $< $(OBJDIR)/frozenstdlib.o -o $@ $(LINK_FLAGS) $(PYLDFLAGS) -DNOMAIN -fsanitize=fuzzer,address,undefined
dndcfuzz: $(BINDIR)/dndcfuzz$(EXE)

FUZZDIR=FuzzCorpus
$(FUZZDIR): ; @$(MKDIR) -p $@


