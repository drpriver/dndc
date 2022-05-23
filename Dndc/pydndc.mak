
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
endif

ifeq ($(UNAME),Linux)
civenv:
	$(PYTHON) -m venv civenv
	. civenv/bin/activate && python -m pip install cibuildwheel && python -m pip install twine

wheels: civenv
	$(RM) -f wheelhouse/*.whl
	. civenv/bin/activate && CIBW_SKIP='{pp*,*musl*}' cibuildwheel --platform linux --archs x86_64 .
endif

ifeq ($(UNAME),Windows)
civenv:
	$(PYTHON) -m venv civenv
	civenv/bin/activate && py -m pip install cibuildwheel && py -m pip install twine

wheels: civenv
	civenv/bin/activate && CIBW_SKIP='pp*' cibuildwheel --platform windows --archs AMD64 .
endif


.PHONY: ci-upload
pypi-upload: archive-wheels
	. civenv/bin/activate && python3 -m twine upload wheelhouse/*

.PHONY: archive-wheels
archive-wheels: | ArchivedWheels
	$(CP) wheelhouse/*.whl ArchivedWheels
ArchivedWheels: ; $(MKDIR) -p $@
