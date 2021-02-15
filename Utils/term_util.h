#ifndef TERM_UTIL_H
#define TERM_UTIL_H
#include <stdlib.h>

typedef struct TermSize {
    int columns, rows;
} TermSize;
// Returns the size of the terminal.
// On error, we return 80 columns and 24 rows.
static inline TermSize get_terminal_size(void);
// Have a terminal util header to handle isatty, enable_vt, etc.
#ifdef WINDOWS
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#endif
#include <io.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
// punting for now
static inline
TermSize
get_terminal_size(void){
    return (TermSize){80, 24};
    }
#else
#include <unistd.h>
#include <sys/ioctl.h>

static inline
TermSize
get_terminal_size(void){
    struct winsize w;
    int err = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    if(err < -1){
        char* cols_s = getenv("COLUMNS");
        if(!cols_s)
            goto err;
        char* rows_s = getenv("ROWS");
        if(!rows_s)
            goto err;
        int cols = atoi(cols_s);
        if(!cols)
            goto err;
        int rows = atoi(rows_s);
        if(!rows)
            goto err;
        return (TermSize){cols, rows};
        err:
        return (TermSize){80, 24};
        }
    return (TermSize){w.ws_col, w.ws_row};
    }
#endif


#endif
