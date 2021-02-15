$(BINDIR)/dndc: $(DEPDIR)/dndc.dep DndC/dndc.c $(OBJDIR)/frozenstdlib.o opt.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(OPT_FLAGS) $(PYCFLAGS) $(PLATFORM_FLAGS) $(DEPFLAGS) $(DEPDIR)/dndc.dep DndC/dndc.c $(OBJDIR)/frozenstdlib.o -o $(BINDIR)/dndc$(EXE) $(LINK_FLAGS) $(PYLDFLAGS) 
dndc: $(BINDIR)/dndc
