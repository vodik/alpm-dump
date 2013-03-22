#ifndef _PM_TERMIO_H
#define _PM_TERMIO_H

#include <stddef.h>

unsigned short getcols(int fd);

size_t grapheme_count(const char *s);

void indentprint_r(const char *str, unsigned short indent, unsigned short cols, size_t *saveidx);
void indentpad_r(int pad, unsigned short cols, size_t *saveidx);

#endif
