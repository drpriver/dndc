FREEZE=$(BINDIR)/freeze_py_module
FROZENDIR:=PythonEmbed/frozen
$(FROZENDIR): ; @$(MKDIR) -p $@
DIRECTORIES+=$(FROZENDIR)
PYVENDOR=PythonEmbed/vendored

$(FREEZE): PythonEmbed/freeze_py_module.c $(DEPDIR)/freeze_py_module.dep  PythonEmbed/pythonembed.mak | $(DIRECTORIES)
	$(CC) $(FLAGS) $(PLATFORM_FLAGS) $(FAST_FLAGS) $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/freeze_py_module.dep $< -o $@ $(LINK_FLAGS) $(PYLDFLAGS)
freeze_py_module: $(FREEZE)

frozen-modules: $(FROZEN_MODULES)

FROZEN_MODULES:=$(addprefix $(FROZENDIR)/,\
	abc.py.h\
	importlib.py.h \
	importlib_external.py.h \
	zipimport.py.h \
	io.py.h \
	codecs.py.h \
	encodings.py.h \
	encodings.latin_1.py.h \
	encodings.utf_8.py.h \
	stat.py.h\
	_collections_abc.py.h\
	genericpath.py.h\
	posixpath.py.h\
	ntpath.py.h\
	os.py.h\
	operator.py.h\
	keyword.py.h\
	heapq.py.h\
	reprlib.py.h\
	collections.py.h\
	collections.abc.py.h\
	types.py.h\
	functools.py.h\
	weakref.py.h\
	copy.py.h\
	copyreg.py.h\
	typing.py.h\
	contextlib.py.h\
	enum.py.h\
	sre_constants.py.h\
	sre_compile.py.h\
	sre_parse.py.h\
	re.py.h\
	string.py.h\
	cmd.py.h\
	fnmatch.py.h\
	token.py.h\
	tokenize.py.h\
	linecache.py.h\
	dis.py.h\
	opcode.py.h\
	importlib.machinery.py.h\
	inspect.py.h\
	bdb.py.h\
	code.py.h\
	codeop.py.h\
	traceback.py.h\
	__future__.py.h\
	glob.py.h\
	pprint.py.h\
	signal.py.h\
	shlex.py.h\
	pdb.py.h\
	)

$(OBJDIR)/frozenstdlib.o: $(DEPDIR)/frozenstdlib.dep PythonEmbed/frozenstdlib.c $(FROZEN_MODULES) | $(DIRECTORIES)
	$(CC) $(INCLUDE_FLAGS) $(PLATFORM_FLAGS) $(FAST_FLAGS) $(PYCFLAGS) $(DEPFLAGS) $(DEPDIR)/frozenstdlib.dep PythonEmbed/frozenstdlib.c -c -o $(OBJDIR)/frozenstdlib.o

$(FROZENDIR)/%.py.h: $(PYVENDOR)/%.py | $(FREEZE) $(FROZENDIR)
	$(FREEZE) $* $< $@

# needs weird name
$(FROZENDIR)/importlib.py.h: $(PYVENDOR)/importlib_bootstrap.py
	$(FREEZE) importlib_bootstrap $< $(FROZENDIR)/importlib.py.h
$(FROZENDIR)/importlib_external.py.h: $(PYVENDOR)/importlib_bootstrap_external.py
	$(FREEZE) importlib_bootstrap_external $< $(FROZENDIR)/importlib_external.py.h
