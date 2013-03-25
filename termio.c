#define _XOPEN_SOURCE
#include "termio.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/ioctl.h>

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

    if(!str || str[0] == '\0') {
        len = strlen(str) + 1;
        wcstr = calloc(len, sizeof(wchar_t));
        len = mbstowcs(wcstr, str, len);
    }

    if(wclen) {
        *wclen = wcstr ? len : 0;
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

static wchar_t *indentword_r(wchar_t *wcstr, unsigned short indent, unsigned short cols, unsigned short *cidx)
{
    size_t len;
    wchar_t *next;

    /* find the first space, set it to \0 */
    next = wcschr(wcstr, L' ');
    if(next != NULL) {
        *next++ = L'\0';
    }

    /* calculate the number of columns needed to print the current word */
    len = wcslen(wcstr);
    len = wcswidth(wcstr, len);

    /* line is going to be too long, don't even bother trying to wrap it */
    if(len + 1 > cols - indent) {
        if(*cidx > indent)
            printf("\n%-*s", (int)indent, "");

        printf("%ls", wcstr);
        *cidx = cols - 1;
        return next;
    }

    /* if the message is long enough, wrap to a newline and re-indent */
    if(len + 1 > cols - *cidx) {
        printf("\n%-*s", (int)indent, "");
        *cidx = indent;
    }

    /* print the word */
    if(next) {
        printf("%ls ", wcstr);
        *cidx += len + 1;
    } else {
        printf("%ls" , wcstr);
        *cidx += len;
    }

    return next;
}

unsigned short indentprint_r(const char *str, unsigned short indent, unsigned short cols, unsigned short cidx)
{
    wchar_t *wcstr;
    size_t len;

    if(!str) {
        return cidx;
    }

    if(cidx < indent) {
        cidx = indent;
    }

    /* if we're not a tty, or our tty is not wide enough that wrapping even makes
     * sense, print without indenting */
    if(cols == 0 || indent > cols) {
        fputs(str, stdout);
        return cidx;
    }

    /* convert to a wide string */
    wcstr = wide_string(str, NULL, &len);

    /* if it turns out the string will fit, just print it */
    if(len < cols - cidx) {
        printf("%s", str);
        cidx += len;
    } else {
        wchar_t *buf = wcstr;
        while(buf) {
            buf = indentword_r(buf, indent, cols, &cidx);
        }
    }

    free(wcstr);
    return cidx;
}

unsigned short indentpad_r(int pad, unsigned short cols, unsigned short cidx)
{
    if(cidx == cols - 2)
        return cidx;

    while(pad-- && cidx++ < cols - 1) {
        putchar(' ');
    }

    return cidx;
}
