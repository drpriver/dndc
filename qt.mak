GENDIR=Generated

ifeq ($(QTMOC),)
$(error QTMOC command not defined)
endif

# Idk if these need to be set.

ifeq ($(UNAME),Darwin)
# MacOS
QTFLAGS=-fPIC
QTINCLUDE=-F$(QTFRAMEWORKDIR) -isystem $(QTINCLUDEDIR)
QTFRAMEWORKS=-framework QtCore \
	     -framework QtGui  \
	     -framework QtWidgets \
	     -framework QtWebEnginecore \
	     -framework QtWebEngineWidgets
QTLINK=$(QTFRAMEWORKS) -rpath $(QTFRAMEWORKDIR)
DNDCLINK=
DNDCINCLUDE=-isystem.
ifeq ($(QTFRAMEWORKDIR),)
$(error QTFRAMEWORKDIR not defined)
endif
else ifeq ($(UNAME),Windows)
# untested
QTFLAGS=
QTINCLUDE=-isystem$(QTINCLUDEDIR)
QTLINK=$(QTLIBDIR)\Qt$(QTVERSION)Core.lib \
	$(QTLIBDIR)\Qt$(QTVERSION)Gui.lib \
	$(QTLIBDIR)\Qt$(QTVERSION)Widgets.lib \
	$(QTLIBDIR)\Qt$(QTVERSION)WebEngineCore.lib \
	$(QTLIBDIR)\Qt$(QTVERSION)WebEngineWidgets.lib
DNDCINCLUDE=-isystem.
DNDCLINK=
else ifeq ($(UNAME),Linux)
QTFLAGS=-fPIC
QTINCLUDE=-isystem$(QTINCLUDEDIR)
QTLINK=$(QTLIBDIR)/libQt$(QTVERSION)Core.so \
	$(QTLIBDIR)/libQt$(QTVERSION)Gui.so \
	$(QTLIBDIR)/libQt$(QTVERSION)Widgets.so \
	$(QTLIBDIR)/libQt$(QTVERSION)WebEngineCore.so \
	$(QTLIBDIR)/libQt$(QTVERSION)WebEngineWidgets.so \
	-lpthread
DNDCLINK=
DNDCINCLUDE=-isystem.
else
$(error Unsupported platform for qt gui)
endif

$(GENDIR): ; @$(MKDIR) -p $@


$(GENDIR)/moc_%.cpp: QtDndcEdit/%.h | $(GENDIR)
	$(QTMOC) $< -o $@
DNDCOBJS=$(OBJDIR)/dndc.o $(VENDOBJDIR)/libquickjs.o

ifneq ($(findstring clang,$(CXX)),)
QTFLAGS+=-fvisibility=hidden
endif
ifneq ($(findstring g++,$(CXX)),)
QTFLAGS+=-fvisibility=hidden
endif
$(BINDIR)/DndcEdit$(EXE): QtDndcEdit/DndcEdit.cpp $(GENDIR)/moc_DndcEdit.cpp $(DNDCOBJS) $(DEPDIR)/DndcEdit.dep | $(GENDIR) $(BINDIR) $(DEPDIR) $(OBJDIR)
	$(CXX) \
		-O3 \
		$(QTFLAGS) \
		$(QTINCLUDE) \
		$(DNDCINCLUDE) \
		$< $(GENDIR)/moc_DndcEdit.cpp \
		$(DNDCOBJS) \
		$(DEPFLAGS) $(DEPDIR)/DndcEdit.dep \
		-o $@ \
		-std=gnu++17 \
		$(DNDCLINK) \
		$(QTLINK)

.PHONY: DndcEdit
DndcEdit: $(BINDIR)/DndcEdit
