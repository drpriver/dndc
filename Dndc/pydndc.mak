DNDEDITFILES:=PyDndEdit/PyDndEdit/Manual.dnd \
	      PyDndEdit/PyDndEdit/changelog.dnd \
	      PyDndEdit/PyDndEdit/dndedit.py \
	      PyDndEdit/PyDndEdit/__main__.py \
	      Dndc/jsdoc.dnd \
	      Dndc/dndc_js_api.d.ts \
	      Documentation/REFERENCE.dnd

.PHONY: dndeditfolder
dndeditfolder:
	$(RM) -rf dndeditfolder
	$(MKDIR) -p dndeditfolder/PyDndEdit
	$(CP) $(DNDEDITFILES) dndeditfolder/PyDndEdit
	$(CP) PyDndEdit/setup.py dndeditfolder

.PHONY: dndedit-wheel
.PHONY: wheels
ifeq ($(UNAME),Darwin)
civenv:
	$(PYTHON) -m venv civenv
	. civenv/bin/activate && python -m pip install cibuildwheel && python -m pip install twine

# macos you need to build multiple times
wheels: civenv
	$(RM) -f wheelhouse/*.whl
	. civenv/bin/activate && CIBW_SKIP='pp*' cibuildwheel --platform macos --archs x86_64 .
	. civenv/bin/activate && CIBW_SKIP='pp*' cibuildwheel --platform macos --archs arm64 .
	. civenv/bin/activate && CIBW_SKIP='pp*' cibuildwheel --platform macos --archs universal2 .

dndedit-wheel: civenv dndeditfolder
	. civenv/bin/activate && python3 -m pip install build && cd dndeditfolder && python3 -m build --wheel
	$(CP) dndeditfolder/dist/PyDndEdit-*-py3-none-any.whl wheelhouse
	$(RM) -rf dndeditfolder

endif

ifeq ($(UNAME),Linux)
civenv:
	$(PYTHON) -m venv civenv
	. civenv/bin/activate && python -m pip install cibuildwheel && python -m pip install twine

wheels: civenv
	$(RM) -f wheelhouse/*.whl
	. civenv/bin/activate && CIBW_SKIP='{pp*,*musl*}' cibuildwheel --platform linux --archs x86_64 .

dndedit-wheel: civenv dndeditfolder
	. civenv/bin/activate && python3 -m pip install build && cd dndeditfolder && python3 -m build --wheel
	$(CP) dndeditfolder/dist/PyDndEdit-*-py3-none-any.whl wheelhouse
	$(RM) -rf dndeditfolder
endif

ifeq ($(UNAME),Windows)
civenv:
	$(PYTHON) -m venv civenv
	civenv\Scripts\activate && py -m pip install cibuildwheel && py -m pip install twine

wheels: civenv
	civenv\Scripts\activate && cmd /V /C "SET CIBW_SKIP=pp*&& cibuildwheel --platform windows --archs AMD64 ."
dndedit-wheel: civenv dndeditfolder
	civenv\Scripts\activate && py -m pip install build && cd dndeditfolder && py -m build --wheel
	$(CP) dndeditfolder/dist/PyDndEdit-*-py3-none-any.whl wheelhouse
	$(RM) -rf dndeditfolder
endif


.PHONY: ci-upload
pypi-upload: archive-wheels
	. civenv/bin/activate && python3 -m twine upload wheelhouse/*

.PHONY: archive-wheels
archive-wheels: | ArchivedWheels
	$(CP) wheelhouse/*.whl ArchivedWheels
ArchivedWheels: ; $(MKDIR) -p $@
