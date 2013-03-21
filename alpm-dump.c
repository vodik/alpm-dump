#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <getopt.h>

#include <alpm.h>
#include <alpm_list.h>

enum {
    ROW_STRING,
    ROW_LIST
};

struct table {
    unsigned short width, cols;
    alpm_list_t *table;
};

typedef const char *(*fetch_string_fn)(alpm_pkg_t *pkg);
typedef alpm_list_t *(*fetch_list_fn)(alpm_pkg_t *pkg);

struct table_row {
    const char *title;

    int id;
    fetch_string_fn string_fn;
    fetch_list_fn   list_fn;
};

static size_t string_length(const char *s)
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

struct indenter {
    unsigned short indent, cols;
    size_t cidx;
};

void indentprint_r(struct indenter **i, const char *str, unsigned short indent, unsigned short cols)
{
    wchar_t *wcstr;
    const wchar_t *p;
    size_t len;

    if(!str) {
        return;
    }

    /* TODO: CLEANUP, maybe size_t *cidx = ... */
    if(*i == NULL) {
        *i = malloc(sizeof(struct indenter));
        (*i)->indent = indent;
        (*i)->cols = cols;
        (*i)->cidx = indent;
    } else {
        indent = (*i)->indent;
        cols = (*i)->cols;
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

    /* if it turns out the string will fit, just print it */
    if(len < cols - indent - (*i)->cidx) {
        fputs(str, stdout);
        free(wcstr);
        (*i)->cidx += len;
        return;
    }

    p = wcstr;

    if(!p || !len) {
        return;
    }

    while(*p) {
        if(*p == L' ') {
            const wchar_t *q, *next;
            p++;

            if(p == NULL || *p == L' ') {
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

            if(len + 1 > cols - (*i)->cidx) {
                /* wrap to a newline and reindent */
                printf("\n%-*s", (int)indent, "   ");
                (*i)->cidx = indent;
            } else {
                printf(" ");
                (*i)->cidx++;
            }
        } else {
            printf("%lc", (wint_t)*p++);
            (*i)->cidx += wcwidth(*p);
        }
    }
    free(wcstr);
}

/* TODO: actually implement */
static void indentpad_r(struct indenter **i, int __attribute__((unused)) pad)
{
    if(*i == NULL)
        return;
    indentprint_r(i, "  ", 0, 0);
}

static struct table *new_table(void)
{
    struct table *table = malloc(sizeof(struct table));

    table->table = NULL;
    table->width = 0;
    table->cols = getcols(0);

    return table;
}

static struct table_row *new_row(struct table *table, const char *title)
{
    struct table_row *row = malloc(sizeof(struct table_row));
    size_t len = string_length(title);

    row->title = title;
    if(len > table->width)
        table->width = len;

    table->table = alpm_list_add(table->table, row);
    return row;
}

static void add_string_row(struct table *table, const char *title, fetch_string_fn fetch)
{
    struct table_row *row = new_row(table, title);

    row->string_fn = fetch;
    row->id = ROW_STRING;
}

static void add_list_row(struct table *table, const char *title, fetch_list_fn fetch)
{
    struct table_row *row = new_row(table, title);

    row->list_fn = fetch;
    row->id = ROW_LIST;
}

static void print_list(struct table *table, alpm_list_t *list)
{
    if(list == NULL) {
        printf("None\n");
    } else {
        alpm_list_t *i;
        struct indenter *state = NULL;

        for(i = list; i; i = alpm_list_next(i)) {
            const char *entry = i->data;
            indentprint_r(&state, entry, table->width + 3, table->cols);
            indentpad_r(&state, 2);
        }
        printf("\n");
    }
}

static void print_table(struct table *table, alpm_pkg_t *pkg)
{
    alpm_list_t *i;

    for(i = table->table; i; i = alpm_list_next(i)) {
        const struct table_row *row = i->data;
        struct indenter *state = NULL;

        switch(row->id) {
        case ROW_STRING:
            printf("%-*s : ", (int)table->width, row->title);
            indentprint_r(&state, row->string_fn(pkg), table->width + 3, table->cols);
            /* indentprint_r(&state, row->string_fn(pkg), 0, 0); */
            printf("\n");
            break;
        case ROW_LIST:
            printf("%-*s : ", (int)table->width, row->title);
            print_list(table, row->list_fn(pkg));
            break;
        }
    }
    printf("\n");
}

static void dump_db(alpm_db_t *db, struct table *table)
{
    alpm_list_t *i, *cache = alpm_db_get_pkgcache(db);

    for(i = cache; i; i = alpm_list_next(i)) {
        print_table(table, i->data);
    }
}

static const char *alpm_pkg_get_dbname(alpm_pkg_t *pkg)
{
    return alpm_db_get_name(alpm_pkg_get_db(pkg));
}

int main(int argc, char *argv[])
{
    int sync = 0;
    static const struct option opts[] = {
        { "sync",  no_argument, 0, 'S' },
        { "query", no_argument, 0, 'Q' },
        { 0, 0, 0, 0 }
    };

    while (true) {
        int opt = getopt_long(argc, argv, "SQ", opts, NULL);
        if (opt == -1)
            break;

        switch (opt) {
        case 'S':
            sync = 1;
            break;
        case 'Q':
            sync = 0;
            break;
        }
    }

    alpm_handle_t *handle = alpm_initialize("/", "/var/lib/pacman/", NULL);
    alpm_list_t *sync_dbs = sync_dbs = alpm_get_syncdbs(handle);

    struct table *table = new_table();
    add_string_row(table, "Repository",   alpm_pkg_get_dbname);
    add_string_row(table, "Name",         alpm_pkg_get_name);
    add_string_row(table, "Version",      alpm_pkg_get_version);
    add_string_row(table, "Description",  alpm_pkg_get_desc);
    add_string_row(table, "Architecture", alpm_pkg_get_arch);
    add_string_row(table, "URL",          alpm_pkg_get_url);
    add_list_row(table,   "Licenses",     alpm_pkg_get_licenses);
    add_list_row(table,   "Groups",       alpm_pkg_get_groups);

    if(sync) {
        const char *dbs[] = {
            "testing",
            "core",
            "extra",
            "community-testing",
            "community",
            "multilib-testing",
            "multilib",
            "heftig",
            NULL
        };

        /* dump out the databases */
        const char **i;
        for(i = dbs; *i != NULL; ++i) {
            alpm_db_t *db = alpm_register_syncdb(handle, *i, ALPM_SIG_USE_DEFAULT);
            dump_db(db, table);
        }
    } else {
        alpm_db_t *db = alpm_get_localdb(handle);
        dump_db(db, table);
    }

    /* free(table); */
}
