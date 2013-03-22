#define _XOPEN_SOURCE
#include "termio.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define NOCOLOR "\033[0m"
#define RED     "\033[0;31m"
#define YELLOW  "\033[0;33m"
#define BLUE    "\033[0;34m"
#define BOLDRED "\033[1;31m"

size_t string_length(const char *s)
{
    int len;
    wchar_t *wcstr;

    if(!s || s[0] == '\0') {
        return 0;
    }

    /* len goes from # bytes -> # chars -> # cols */
    len = strlen(s) + 1;
    wcstr = calloc(len, sizeof(wchar_t));
    len = mbstowcs(wcstr, s, len);
    len = wcswidth(wcstr, len);
    free(wcstr);

    return len;
}

unsigned short getcols(int fd)
{
    const unsigned short default_tty = 80;
    const unsigned short default_notty = 0;
    unsigned short termwidth = 0;

    if(!isatty(fd)) {
        return default_notty;
    }

    struct winsize win;
    if(ioctl(fd, TIOCGWINSZ, &win) == 0) {
        termwidth = win.ws_col;
    }

    return termwidth == 0 ? default_tty : termwidth;
}

void indentprint_r(const char *str, unsigned short indent, unsigned short cols, size_t *saveidx)
{
    wchar_t *wcstr;
    const wchar_t *p;
    size_t len;
    size_t cidx = saveidx ? *saveidx : 0;

    if(!str) {
        return;
    }

    if(cidx < indent) {
        cidx = indent;
    }

    /* if we're not a tty, or our tty is not wide enough that wrapping even makes
     * sense, print without indenting */
    if(cols == 0 || indent > cols) {
        fputs(str, stdout);
        return;
    }

    len = strlen(str) + 1;
    wcstr = calloc(len, sizeof(wchar_t));
    len = mbstowcs(wcstr, str, len);
    len = wcswidth(wcstr, len);

    /* if it turns out the string will fit, just print it */
    if(len + 1 < cols - cidx) {
        /* fputs(str, stdout); */
        printf(YELLOW "%s" NOCOLOR, str);
        if(saveidx)
            *saveidx = cidx + len;
        free(wcstr);
        return;
    }

    p = wcstr;

    if(!p || !len) {
        return;
    }

    const wchar_t *q, *next;

    next = wcschr(p, L' ');
    if(next == NULL) {
        next = p + wcslen(p);
    }

    len = 0;
    q = p;
    for(q = p; q < next; q++) {
        len += wcwidth(*q);
    }

    if(len + 1 > cols - indent) {
        /* line is going to be too long, don't even bother trying to
         * wrap it */
        if(cidx > indent)
            printf(BOLDRED "\n%-*s" NOCOLOR, (int)indent, "-->");

        printf(BLUE "%s" NOCOLOR, str);
        if(saveidx)
            *saveidx = cols - 1;
        free(wcstr);
        return;
    } else if(len + 1 > cols - cidx) {
        /* wrap to a newline and reindent */
        printf(BOLDRED "\n%-*s" NOCOLOR, (int)indent, "-->");
        cidx = indent;
    }

    while(*p) {
        if(*p == L' ') {
            const wchar_t *q, *next;
            p++;

            if(p == NULL) {
            /* if(p == NULL || *p == L' ') { */
                continue;
            }

            next = wcschr(p, L' ');
            if(next == NULL) {
                next = p + wcslen(p);
            }

            len = 0;
            q = p;
            for(q = p; q < next; q++) {
                len += wcwidth(*q);
            }

            if(len + 1 > cols - cidx) {
                /* wrap to a newline and reindent */
                printf(BOLDRED "\n%-*s" NOCOLOR, (int)indent, "-->");
                cidx = indent;
            } else {
                printf(" ");
                cidx++;
            }
        } else {
            printf(RED "%lc" NOCOLOR, (wint_t)*p++);
            cidx += wcwidth(*p);
        }
    }

    if(saveidx)
        *saveidx = cidx;
    free(wcstr);
}

void indentpad_r(int pad, unsigned short cols, size_t *saveidx)
{
    size_t cidx = saveidx ? *saveidx : 0;

    if(cidx == cols - 2)
        return;

    while(cidx < cols - 1 && pad > 0) {
        ++cidx;
        --pad;
        fputc('_', stdout);
    }

    if(saveidx)
        *saveidx = cidx;
}

