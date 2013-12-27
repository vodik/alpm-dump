#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>

#include <alpm.h>
#include <alpm_list.h>

#include "termio.h"

static alpm_handle_t *handle;

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
    ENTRY_OPTIONAL,
    ENTRY_REQUIRED,
    ENTRY_OPTIONAL_FOR,
    ENTRY_CONFLICTS,
    ENTRY_REPLACES,
    ENTRY_DOWNLOAD_SIZE,
    ENTRY_INSTALL_SIZE,
    ENTRY_PACKAGER,
    ENTRY_BUILD_DATE,
    ENTRY_INSTALL_DATE,
    ENTRY_INSTALL_REASON,
    ENTRY_INSTALL_SCRIPT,
    ENTRY_VALIDATED,
    LAST_ENTRY
};

static const char *local_table[LAST_ENTRY] = {
    [ENTRY_NAME]           = "Name",
    [ENTRY_VERSION]        = "Version",
    [ENTRY_DESCRIPTION]    = "Description",
    [ENTRY_ARCHITECTURE]   = "Architecture",
    [ENTRY_URL]            = "URL",
    [ENTRY_LICENSES]       = "Licenses",
    [ENTRY_GROUPS]         = "Groups",
    [ENTRY_PROVIDES]       = "Provides",
    [ENTRY_OPTIONAL]       = "Optional Deps",
    [ENTRY_DEPENDS]        = "Depends On",
    [ENTRY_REQUIRED]       = "Required By",
    [ENTRY_INSTALL_SIZE]   = "Install Size",
    [ENTRY_OPTIONAL_FOR]   = "Optional For",
    [ENTRY_CONFLICTS]      = "Conflicts With",
    [ENTRY_REPLACES]       = "Replaces",
    [ENTRY_PACKAGER]       = "Packager",
    [ENTRY_BUILD_DATE]     = "Build Date",
    [ENTRY_INSTALL_DATE]   = "Install Date",
    [ENTRY_INSTALL_REASON] = "Install Reason",
    [ENTRY_INSTALL_SCRIPT] = "Install Script",
    [ENTRY_VALIDATED]      = "Validation",
};

static const char *sync_table[LAST_ENTRY] = {
    [ENTRY_REPOSITORY]    = "Repository",
    [ENTRY_NAME]          = "Name",
    [ENTRY_VERSION]       = "Version",
    [ENTRY_DESCRIPTION]   = "Description",
    [ENTRY_ARCHITECTURE]  = "Architecture",
    [ENTRY_URL]           = "URL",
    [ENTRY_LICENSES]      = "Licenses",
    [ENTRY_GROUPS]        = "Groups",
    [ENTRY_PROVIDES]      = "Provides",
    [ENTRY_DEPENDS]       = "Depends On",
    [ENTRY_OPTIONAL]      = "Optional Deps",
    [ENTRY_CONFLICTS]     = "Conflicts With",
    [ENTRY_REPLACES]      = "Replaces",
    [ENTRY_DOWNLOAD_SIZE] = "Download Size",
    [ENTRY_INSTALL_SIZE]  = "Install Size",
    [ENTRY_PACKAGER]      = "Packager",
    [ENTRY_BUILD_DATE]    = "Build Date",
    [ENTRY_VALIDATED]     = "Validated By",
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
        return;
    }

    alpm_list_t *i;
    unsigned short cidx = 0;

    for(i = list; i; i = alpm_list_next(i)) {
        cidx = indentprint_r(i->data, offset, cidx);
        cidx = indentpad_r(2, cidx);
    }
}

static void print_deplist(alpm_list_t *list, unsigned short offset)
{
    if(list == NULL) {
        printf("None");
        return;
    }

    alpm_list_t *i;
    unsigned short cidx = 0;

    for(i = list; i; i = alpm_list_next(i)) {
        char *entry = alpm_dep_compute_string(i->data);

        cidx = indentprint_r(entry, offset, cidx);
        cidx = indentpad_r(2, cidx);
        free(entry);
    }
}

static void print_optentry(alpm_pkg_t *pkg, alpm_depend_t *optdep, unsigned short offset)
{
    char *depstring = alpm_dep_compute_string(optdep);
    int cidx = indentprint_r(depstring, offset, 0);

    if(alpm_pkg_get_origin(pkg) == ALPM_PKG_FROM_LOCALDB &&
       alpm_db_get_pkg(alpm_get_localdb(handle), optdep->name))
        indentprint_r(" [installed]", offset, cidx);
}

static void print_optdeplist(alpm_pkg_t *pkg, unsigned short offset)
{
    alpm_list_t *i = alpm_pkg_get_optdepends(pkg);

    if (i == NULL) {
        printf("None");
        return;
    }

    print_optentry(pkg, i->data, offset);
    for(i = alpm_list_next(i); i; i = alpm_list_next(i)) {
        printf("\n%-*s", offset, "");
        print_optentry(pkg, i->data, offset);
    }
}

static void print_bool(int b, unsigned short offset)
{
    indentprint_r(b ? "Yes" : "No", offset, 0);
}

/* {{{ */
static double simple_pow(int base, int exp)
{
    double result = 1.0;
    for(; exp > 0; exp--) {
        result *= base;
    }
    return result;
}

static double humanize_size(off_t bytes, const char target_unit, int precision,
                            const char **label)
{
    static const char *labels[] = {"B", "KiB", "MiB", "GiB",
        "TiB", "PiB", "EiB", "ZiB", "YiB"};
    static const int unitcount = sizeof(labels) / sizeof(labels[0]);

    double val = (double)bytes;
    int index;

    for(index = 0; index < unitcount - 1; index++) {
        if(target_unit != '\0' && labels[index][0] == target_unit) {
            break;
        } else if(target_unit == '\0' && val <= 2048.0 && val >= -2048.0) {
            break;
        }
        val /= 1024.0;
    }

    if(label) {
        *label = labels[index];
    }

    /* do not display negative zeroes */
    if(precision >= 0 && val < 0.0 &&
       val > (-0.5 / simple_pow(10, precision))) {
        val = 0.0;
    }

    return val;
}
/* }}} */

static void print_filesize(off_t size, unsigned short offset)
{
    const char *label;
    double _size = humanize_size(size, '\0', 2, &label);
    printf("%6.2f %s", _size, label);
}

static void print_date(time_t date, unsigned short offset)
{
    char datestr[50];
    strftime(datestr, 50, "%c", localtime(&date));
    indentprint_r(datestr, offset, 0);
}

static void print_reason(alpm_pkgreason_t r, unsigned short offset)
{
    const char *reason;

    switch(r) {
    case ALPM_PKG_REASON_EXPLICIT:
        reason = "Explicitly installed";
        break;
    case ALPM_PKG_REASON_DEPEND:
        reason = "Installed as a dependency for another package";
        break;
    default:
        reason = "Unknown";
        break;
    }

    indentprint_r(reason, offset, 0);
}

static void print_validation(alpm_pkgvalidation_t v, unsigned short offset)
{
    alpm_list_t *validation = NULL;

    if(v) {
        if(v & ALPM_PKG_VALIDATION_NONE) {
            validation = alpm_list_add(validation, "None");
        } else {
            if(v & ALPM_PKG_VALIDATION_MD5SUM) {
                validation = alpm_list_add(validation, "MD5 Sum");
            }
            if(v & ALPM_PKG_VALIDATION_SHA256SUM) {
                validation = alpm_list_add(validation, "SHA256 Sum");
            }
            if(v & ALPM_PKG_VALIDATION_SIGNATURE) {
                validation = alpm_list_add(validation, "Signature");
            }
        }
    } else {
        validation = alpm_list_add(validation, "Unknown");
    }

    print_list(validation, offset);
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
        case ENTRY_OPTIONAL:
            print_optdeplist(pkg, width);
            break;
        case ENTRY_REQUIRED:
            print_list(alpm_pkg_compute_requiredby(pkg), width);
            break;
        case ENTRY_DOWNLOAD_SIZE:
            print_filesize(alpm_pkg_get_size(pkg), width);
            break;
        case ENTRY_INSTALL_SIZE:
            print_filesize(alpm_pkg_get_isize(pkg), width);
            break;
        case ENTRY_OPTIONAL_FOR:
            print_list(alpm_pkg_compute_optionalfor(pkg), width);
            break;
        case ENTRY_CONFLICTS:
            print_deplist(alpm_pkg_get_conflicts(pkg), width);
            break;
        case ENTRY_REPLACES:
            print_deplist(alpm_pkg_get_replaces(pkg), width);
            break;
        case ENTRY_PACKAGER:
            indentprint_r(alpm_pkg_get_packager(pkg), width, 0);
            break;
        case ENTRY_BUILD_DATE:
            print_date((time_t)alpm_pkg_get_builddate(pkg), width);
            break;
        case ENTRY_INSTALL_DATE:
            print_date((time_t)alpm_pkg_get_installdate(pkg), width);
            break;
        case ENTRY_INSTALL_REASON:
            print_reason(alpm_pkg_get_reason(pkg), width);
            break;
        case ENTRY_INSTALL_SCRIPT:
            print_bool(alpm_pkg_has_scriptlet(pkg), width);
            break;
        case ENTRY_VALIDATED:
            print_validation(alpm_pkg_get_validation(pkg), width);
            break;
        default:
            indentprint_r("Unknown field", width, 0);
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
        print_table(table, i->data);
    }
}

int main(int argc, char *argv[])
{
    int sync = 0;
    const char **table = local_table;
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
            table = sync_table;
            sync = 1;
            break;
        case 'Q':
            sync = 0;
            break;
        }
    }

    handle = alpm_initialize("/", "/var/lib/pacman/", NULL);
    alpm_list_t *sync_dbs = sync_dbs = alpm_get_syncdbs(handle);

    if(sync) {
        const char *dbs[] = {
            "testing",
            "core",
            "extra",
            "community-testing",
            "community",
            "multilib-testing",
            "multilib",
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
