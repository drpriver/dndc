GENDIR=Generated

ifeq ($(QTMOC),)
$(error QTMOC command not defined)
endif

# Idk if these need to be set.
QTFLAGS=-fPIC

ifeq ($(UNAME),Darwin)
# MacOS
QTINCLUDE=-F$(QTFRAMEWORKDIR)
QTFRAMEWORKS=-framework QtCore \
	     -framework QtGui  \
	     -framework QtWidgets \
	     -framework QtWebEnginecore \
	     -framework QtWebEngineWidgets
QTLINK=$(QTFRAMEWORKS) -rpath $(QTFRAMEWORKDIR)
DNDCLINK=-F/Library/Frameworks -framework Python
ifeq ($(QTFRAMEWORKDIR),)
$(error QTFRAMEWORKDIR not defined)
endif
else ifeq ($(UNAME),Windows)
# untested
QTINCLUDE=-isystem$(QTINCLUDEDIR)
QTLINK=$(QTLIBDIR)/Qt$(QTVERSION)Core.dll \
	$(QTLIBDIR)/Qt$(QTVERSION)Gui.dll \
	$(QTLIBDIR)/Qt$(QTVERSION)Widgets.dll \
	$(QTLIBDIR)/Qt$(QTVERSION)WebEngineCore.dll \
	$(QTLIBDIR)/Qt$(QTVERSION)WebEngineWidgets.dll
DNDCLINK=$(PYLDFLAGS)
else ifeq ($(UNAME),Linux)
QTINCLUDE=-isystem$(QTINCLUDEDIR)
QTLINK=$(QTLIBDIR)/libQt$(QTVERSION)Core.so \
	$(QTLIBDIR)/libQt$(QTVERSION)Gui.so \
	$(QTLIBDIR)/libQt$(QTVERSION)Widgets.so \
	$(QTLIBDIR)/libQt$(QTVERSION)WebEngineCore.so \
	$(QTLIBDIR)/libQt$(QTVERSION)WebEngineWidgets.so \
	-lpthread
DNDCLINK=$(PYLDFLAGS)
else
$(error Unsupported platform for qt gui)
endif

$(GENDIR): ; @$(MKDIR) -p $@


$(GENDIR)/moc_%.cpp: QtDndcEdit/%.h | $(GENDIR)
	$(QTMOC) $< -o $@
DNDCOBJS=$(OBJDIR)/dndc.o $(OBJDIR)/libquickjs.o


$(BINDIR)/DndcEdit: QtDndcEdit/DndcEdit.cpp $(GENDIR)/moc_DndcEdit.cpp $(DNDCOBJS) $(DEPDIR)/DndcEdit.dep | $(GENDIR) $(BINDIR) $(DEPDIR) $(OBJDIR)
	$(CXX) \
		-O3 \
		$(QTFLAGS) \
		$(QTINCLUDE) \
		$< $(GENDIR)/moc_DndcEdit.cpp \
		$(DNDCOBJS) \
		$(DEPFLAGS) $(DEPDIR)/DndcEdit.dep \
		-o $@ \
		-std=gnu++17 \
		$(DNDCLINK) \
		$(QTLINK)
		
.PHONY: DndcEdit
DndcEdit: $(BINDIR)/DndcEdit
