ifeq ($(UNAME), Windows)
	QUICKJS_CEXTRA=-mrdseed
	QUICKJS_LDEXTRA=
else ifeq ($(UNAME), Rpi)
	QUICKJS_CEXTRA=
	QUICKJS_LDEXTRA=-lm -lpthread -fPIC
else ifeq ($(UNAME), Linux)
	QUICKJS_CEXTRA=
	QUICKJS_LDEXTRA=-lm -lpthread -fPIC
else ifeq ($(UNAME), Darwin)
	QUICKJS_CEXTRA=
	QUICKJS_LDEXTRA=-install_name @rpath/$(notdir $@) -compatibility_version $(DNDC_COMPAT_VERSION) -current_version $(DNDCVERSION)
endif

$(OBJDIR)/libquickjs.o: Vendored/libquickjs.c $(DEPDIR)/libquickjs_o.dep | $(DIRECTORIES)
	$(CC) $(FLAGS) -O2 -g $(PLATFORM_FLAGS) $(QUICKJS_CEXTRA) $(DEPFLAGS) $(DEPDIR)/libquickjs_o.dep -fvisibility=hidden -c -o $@ Vendored/libquickjs.c

$(BINDIR)/libquickjs$(SO): Vendored/libquickjs.c $(DEPDIR)/libquickjs_so.dep | $(DIRECTORIES)
	$(CC) $(FLAGS) -O2 -g $(PLATFORM_FLAGS) $(QUICKJS_CEXTRA) $(DEPFLAGS) $(DEPDIR)/libquickjs_so.dep -shared -fvisibility=hidden -o $@ Vendored/libquickjs.c -DBUILDING_SHARED_OBJECT=1 $(QUICKJS_LDEXTRA)
