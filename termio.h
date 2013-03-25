#ifndef _PM_TERMIO_H
#define _PM_TERMIO_H

#include <stddef.h>

unsigned short getcols(int fd);

size_t grapheme_count(const char *s);

unsigned short indentprint_r(const char *str, unsigned short indent, unsigned short cols, unsigned short cidx);
unsigned short indentpad_r(int pad, unsigned short cols, unsigned short cidx);

#endif /* _PM_TERMIO_H */

/* vim: set ts=2 sw=2 noet: */
