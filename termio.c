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

static wchar_t *wide_string(const char *str, size_t *wclen, size_t *graphemes)
{
    wchar_t *wcstr = NULL;
    size_t len = 0;

    if(str && str[0] != '\0') {
        len = strlen(str) + 1;
        wcstr = calloc(len, sizeof(wchar_t));
        len = mbstowcs(wcstr, str, len);
    }

    if(wclen) {
        *wclen = len;
    }

    if(graphemes) {
        *graphemes = len ? wcswidth(wcstr, len) : 0;
    }

    return wcstr;
}

size_t grapheme_count(const char *str)
{
    wchar_t *wcstr;
    size_t graphemes;

    wcstr = wide_string(str, NULL, &graphemes);

    free(wcstr);
    return graphemes;
}

/* XXX: deal with len here */
static wchar_t *indentword_t(wchar_t *wcstr, size_t len, unsigned short indent, unsigned short cols, unsigned short *cidx)
{
    /* find the first space, set it to \0 */
    wchar_t *next = wcschr(wcstr, L' ');
    if(next != NULL) {
        *next = L'\0';
    }

    /* calculate the number of columns needed to print the current word */
    len = wcswidth(wcstr, next ? wcstr - next : len);

    if(len + 1 > cols - indent) {
        /* line is going to be too long, don't even bother trying to
         * wrap it */
        if(*cidx > indent)
            printf(BOLDRED "\n%-*s" NOCOLOR, (int)indent, "-->");

        printf(BLUE "%ls" NOCOLOR, wcstr);
        *cidx = cols - 1;
        return next ? next + 1 : NULL;
    } else if(len + 1 > cols - *cidx) {
        /* if the message is long enough, wrap to a newline and re-indent */
        printf(BOLDRED "\n%-*s" NOCOLOR, (int)indent, "-->");
        *cidx = indent;
    }

    /* print the word */
    if(next) {
        printf(RED "%ls " NOCOLOR, wcstr);
        *cidx += len + 1;
        return next + 1;
    }

    printf(RED "%ls" NOCOLOR, wcstr);
    *cidx += len;
    return NULL;
}

void indentprint_r(const char *str, unsigned short indent, unsigned short cols, unsigned short *saveidx)
{
    wchar_t *wcstr;
    size_t len;
    unsigned short cidx = saveidx ? *saveidx : 0;

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

    /* convert to a wide string */
    wcstr = wide_string(str, NULL, &len);

    /* if it turns out the string will fit, just print it */
    if(len < cols - cidx) {
        printf(YELLOW "%s" NOCOLOR, str);
        cidx += len;
        goto cleanup;
    }

    /* print out message word by word */
    while(wcstr) {
        wcstr = indentword_t(wcstr, len, indent, cols, &cidx);
    }

cleanup:
    if(saveidx)
        *saveidx = cidx;
    free(wcstr);
}

void indentpad_r(int pad, unsigned short cols, unsigned short *saveidx)
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
