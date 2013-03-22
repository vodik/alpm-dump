#ifndef _PM_TERMIO_H
#define _PM_TERMIO_H

#include <stddef.h>

size_t string_length(const char *s);
unsigned short getcols(int fd);

void indentprint_r(const char *str, unsigned short indent, unsigned short cols, size_t *saveidx);
void indentpad_r(int pad, unsigned short cols, size_t *saveidx);

#endif
