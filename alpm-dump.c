#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>

#include <alpm.h>
#include <alpm_list.h>

#include "termio.h"

enum {
    ROW_STRING,
    ROW_LIST,
    ROW_DEPLIST
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

static void add_deplist_row(struct table *table, const char *title, fetch_list_fn fetch)
{
    struct table_row *row = new_row(table, title);

    row->list_fn = fetch;
    row->id = ROW_DEPLIST;
}

static void print_list(struct table *table, alpm_list_t *list)
{
    if(list == NULL) {
        printf("None\n");
    } else {
        alpm_list_t *i;
        size_t cidx = 0;

        for(i = list; i; i = alpm_list_next(i)) {
            const char *entry = i->data;
            indentprint_r(entry, table->width + 3, table->cols, &cidx);
            indentpad_r(2, table->cols, &cidx);
        }
        printf("\n");
    }
}

static void print_deplist(struct table *table, alpm_list_t *list)
{
    if(list == NULL) {
        printf("None\n");
    } else {
        alpm_list_t *i;
        size_t cidx = 0;

        for(i = list; i; i = alpm_list_next(i)) {
            const alpm_depend_t *dep = i->data;
            const char *entry = alpm_dep_compute_string(dep);

            indentprint_r(entry, table->width + 3, table->cols, &cidx);
            indentpad_r(2, table->cols, &cidx);
        }
        printf("\n");
    }
}

static void print_table(struct table *table, alpm_pkg_t *pkg)
{
    alpm_list_t *i;

    for(i = table->table; i; i = alpm_list_next(i)) {
        const struct table_row *row = i->data;

        printf("%-*s : ", (int)table->width, row->title);
        switch(row->id) {
        case ROW_STRING:
            indentprint_r(row->string_fn(pkg), table->width + 3, table->cols, NULL);
            printf("\n");
            break;
        case ROW_LIST:
            print_list(table, row->list_fn(pkg));
            break;
        case ROW_DEPLIST:
            print_deplist(table, row->list_fn(pkg));
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
    add_string_row(table,  "Repository",   alpm_pkg_get_dbname);
    add_string_row(table,  "Name",         alpm_pkg_get_name);
    add_string_row(table,  "Version",      alpm_pkg_get_version);
    add_string_row(table,  "Description",  alpm_pkg_get_desc);
    add_string_row(table,  "Architecture", alpm_pkg_get_arch);
    add_string_row(table,  "URL",          alpm_pkg_get_url);
    add_list_row(table,    "Licenses",     alpm_pkg_get_licenses);
    add_list_row(table,    "Groups",       alpm_pkg_get_groups);
    add_string_row(table,  "Packager",     alpm_pkg_get_packager);
    add_deplist_row(table, "Provides",     alpm_pkg_get_provides);
    add_deplist_row(table, "Depends On",   alpm_pkg_get_depends);

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

    free(table);
}
