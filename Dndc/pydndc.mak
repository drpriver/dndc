civenv:
	$(PYTHON) -m venv civenv
	. civenv/bin/activate && python -m pip install cibuildwheel

.PHONY: wheels
ifeq ($(UNAME),Darwin)
# macos you need to build twice
wheels: civenv
	. civenv/bin/activate && CIBW_SKIP='pp*' cibuildwheel --platform macos --archs x86_64 .
	. civenv/bin/activate && cibuildwheel --platform macos --archs arm64 .
else
ifeq ($(UNAME), Linux)
wheels: civenv
	. civenv/bin/activate && CIBW_SKIP='pp*' cibuildwheel --platform linux --archs x86_64 .
else

#TODO: windows

endif
endif


