#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <getopt.h>

#include <alpm.h>
#include <alpm_list.h>

#include "termio.h"

enum table_entry_type {
    ENTRY_REPOSITORY,
    ENTRY_NAME,
    ENTRY_VERSION,
    ENTRY_DESCRIPTION,
    ENTRY_ARCHITECTURE,
    ENTRY_URL,
    ENTRY_LICENSES,
    ENTRY_GROUPS,
    ENTRY_PROVIDES,
    ENTRY_DEPENDS,
    ENTRY_PACKAGER,
    LAST_ENTRY
};

static size_t max_padding(const char *entries[static LAST_ENTRY])
{
    int i;
    size_t max = 0;

    for (i = 0; i < LAST_ENTRY; ++i) {
        if (entries[i]) {
            size_t len = grapheme_count(entries[i]);
            if (len > max)
                max = len;
        }
    }

    return max;
}

static void print_list(alpm_list_t *list, unsigned short offset)
{
    if(list == NULL) {
        printf("None");
    } else {
        alpm_list_t *i;
        unsigned short cidx = 0;

        for(i = list; i; i = alpm_list_next(i)) {
            const char *entry = i->data;
            cidx = indentprint_r(entry, offset, cidx);
            cidx = indentpad_r(2, cidx);
        }
    }
}

static void print_deplist(alpm_list_t *list, unsigned short offset)
{
    if(list == NULL) {
        printf("None");
    } else {
        alpm_list_t *i;
        unsigned short cidx = 0;

        for(i = list; i; i = alpm_list_next(i)) {
            const alpm_depend_t *dep = i->data;
            char *entry = alpm_dep_compute_string(dep);

            cidx = indentprint_r(entry, offset, cidx);
            cidx = indentpad_r(2, cidx);
            free(entry);
        }
    }
}

static void print_table(const char *table[static LAST_ENTRY], alpm_pkg_t *pkg)
{
    int i;
    size_t max_width = max_padding(table);

    for(i = 0; i < LAST_ENTRY; ++i) {
        if (!table[i])
            continue;

        size_t width = max_width;
        printf("\033[1m%-*s\033[0m : ", (int)width, table[i]);
        width += 3;

        switch(i) {
        case ENTRY_REPOSITORY:
            indentprint_r(alpm_db_get_name(alpm_pkg_get_db(pkg)), width, 0);
            break;
        case ENTRY_NAME:
            indentprint_r(alpm_pkg_get_name(pkg), width, 0);
            break;
        case ENTRY_VERSION:
            indentprint_r(alpm_pkg_get_version(pkg), width, 0);
            break;
        case ENTRY_DESCRIPTION:
            indentprint_r(alpm_pkg_get_desc(pkg), width, 0);
            break;
        case ENTRY_ARCHITECTURE:
            indentprint_r(alpm_pkg_get_arch(pkg), width, 0);
            break;
        case ENTRY_URL:
            indentprint_r(alpm_pkg_get_url(pkg), width, 0);
            break;
        case ENTRY_LICENSES:
            print_list(alpm_pkg_get_licenses(pkg), width);
            break;
        case ENTRY_GROUPS:
            print_list(alpm_pkg_get_groups(pkg), width);
            break;
        case ENTRY_PROVIDES:
            print_deplist(alpm_pkg_get_provides(pkg), width);
            break;
        case ENTRY_DEPENDS:
            print_deplist(alpm_pkg_get_depends(pkg), width);
            break;
        case ENTRY_PACKAGER:
            indentprint_r(alpm_pkg_get_packager(pkg), width, 0);
            break;
        default:
            break;
        }

        putchar('\n');
    }

    putchar('\n');
}

static void dump_db(alpm_db_t *db, const char *table[static LAST_ENTRY])
{
    alpm_list_t *i, *cache = alpm_db_get_pkgcache(db);

    for(i = cache; i; i = alpm_list_next(i)) {
        print_table(table, (alpm_pkg_t *)i->data);
    }
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

    static const char *table[LAST_ENTRY] = {
        [ENTRY_REPOSITORY]   = "Repository",
        [ENTRY_NAME]         = "Name",
        [ENTRY_VERSION]      = "Version",
        [ENTRY_DESCRIPTION]  = "Description",
        [ENTRY_ARCHITECTURE] = "Architecture",
        [ENTRY_URL]          = "URL",
        [ENTRY_LICENSES]     = "Licenses",
        [ENTRY_GROUPS]       = "Groups",
        [ENTRY_PROVIDES]     = "Provides",
        [ENTRY_DEPENDS]      = "Depends On",
        [ENTRY_PACKAGER]     = "Packager"
    };

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
}
