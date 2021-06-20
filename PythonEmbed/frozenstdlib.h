#ifndef FROZENSTDLIB_H
#define FROZENSTDLIB_H
extern
void
set_frozen_modules(void);
struct FrozenPyVersion{
    int major, minor;
};
extern
struct FrozenPyVersion
get_frozen_version(void);
#endif
