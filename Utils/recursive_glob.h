#ifndef RECURSIVE_GLOB_H
#define RECURSIVE_GLOB_H

#ifndef RECURSIVE_GLOB_API
#define RECURSIVE_GLOB_API static
#endif
// Requires that Marray(StringView) is defined.
RECURSIVE_GLOB_API void recursive_glob_suffix(LongString directory, StringView suffix, Marray(StringView)* entries);

#endif
