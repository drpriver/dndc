#ifndef DNDC_H
#define DNDC_H
#include "long_string.h"
#include "dndc_flags.h"
//
// Returns 0 on success, an error code otherwise.
// You must call dndc_init_python beforehand.
//
extern
int
dndc_make_html(LongString source_text, Nonnull(LongString*)output);

extern
int
dndc_format(LongString source_text, Nonnull(LongString*)output);

extern
int
dndc_init_python(void);

#endif
