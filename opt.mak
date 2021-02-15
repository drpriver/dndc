# Options go here.
# Putting them in a makefile avoids issues of compiling with inconsistent
# flags.

# Valid choices are DEBUG, FAST, DEV
# Targets should all depend on this file so that
# We don't end up with a mix of optimization levels.
# DEBUG: -O0, -g, and sanitizers
# DEV: -O0, -g
# FAST: -OFast
SPEED?=DEV
INSTALLDIR?=/usr/local/bin
