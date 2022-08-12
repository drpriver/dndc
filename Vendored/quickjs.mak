ifeq ($(UNAME), Windows)
	QUICKJS_CEXTRA=-mrdseed
	QUICKJS_LDEXTRA=
else ifeq ($(UNAME), Rpi)
	QUICKJS_CEXTRA=-fPIC
	QUICKJS_LDEXTRA=-lm -lpthread -fPIC
else ifeq ($(UNAME), Linux)
	QUICKJS_CEXTRA=-fPIC
	QUICKJS_LDEXTRA=-lm -lpthread
else ifeq ($(UNAME), Darwin)
	QUICKJS_CEXTRA=
	QUICKJS_LDEXTRA=-install_name @rpath/$(notdir $@) -compatibility_version $(DNDC_COMPAT_VERSION) -current_version $(DNDCVERSION)
endif

# For some reason with just this target, the dep file will be newer than the .o file.
# So to work around that we touch the .o file its mtime is newer. It's annoying.
$(VENDOBJDIR)/libquickjs.o: Vendored/libquickjs.c $(DEPDIR)/libquickjs_o.dep | $(DIRECTORIES)
	$(CC) $(FLAGS) -O2 $(PLATFORM_FLAGS) $(QUICKJS_CEXTRA) $(DEPFLAGS) $(DEPDIR)/libquickjs_o.dep -fvisibility=hidden -c -o $@ Vendored/libquickjs.c -Wno-sign-compare
	$(TOUCH) $@

$(BINDIR)/libquickjs$(SO): Vendored/libquickjs.c $(DEPDIR)/libquickjs_so.dep | $(DIRECTORIES)
	$(CC) $(FLAGS) -O2 $(PLATFORM_FLAGS) $(QUICKJS_CEXTRA) $(DEPFLAGS) $(DEPDIR)/libquickjs_so.dep -shared -fvisibility=hidden -o $@ Vendored/libquickjs.c -DBUILDING_SHARED_OBJECT=1 $(QUICKJS_LDEXTRA) -Wno-sign-compare
	$(TOUCH) $@
