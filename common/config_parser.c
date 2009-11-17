/*
 * This file is part of Campagnol VPN.
 * Copyright (C) 2009  Florent Bondoux
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <search.h>
#include <string.h>
#include <limits.h>

#include "config_parser.h"
#include "log.h"

#ifndef HAVE_TDESTROY
#   include "../lib/tdestroy.h"
#   define tdestroy(root,free_node) campagnol_tdestroy(root,free_node,parser_compare)
#endif

/*
 * Dummy item, used to search a key or a section
 */
struct item_common {
    const char *name;
};
typedef struct item_common item_common_t;

/* internal comparison function (strcmp) */
static int parser_compare(const void *itema, const void *itemb) {
    return strcmp(((const item_common_t *) itema)->name,
            ((const item_common_t *) itemb)->name);
}

/* Free an item_value_t item */
static void free_value(void *p) {
    item_value_t *value = (item_value_t *) p;
    free(value->s);
    strlib_free(&value->expanded);
    free(value);
}

/* Free an item_key_t item and its childs */
static void free_key(void *p) {
    item_key_t *key = (item_key_t *) p;
    item_value_t *value, *value_tmp;
    free(key->name);
    if (key->n_values != 0) {
        TAILQ_FOREACH_SAFE(value, &key->values_list, tailq, value_tmp) {
            TAILQ_REMOVE(&key->values_list, value, tailq);
            free_value(value);
        }
    }
    free(key);
}

/* Free an item_section_t item and its child */
static void free_section(void *p) {
    item_section_t *section = (item_section_t *) p;
    item_key_t *key, *key_tmp;
    free(section->name);
    if (!TAILQ_EMPTY(&section->keys_list)) {
        TAILQ_FOREACH_SAFE(key, &section->keys_list, tailq, key_tmp) {
            TAILQ_REMOVE(&section->keys_list, key, tailq);
        }
        tdestroy(section->keys_tree, free_key);
        section->keys_tree = NULL;
    }
    free(section);
}

/* Init the parser */
void parser_init(parser_context_t *parser, int allow_default,
        int allow_empty_value) {
    parser->allow_default = allow_default;
    parser->allow_empty_value = allow_empty_value;
    parser->sections_tree = NULL;
    TAILQ_INIT(&parser->sections_list);
}

/* Free the parser */
void parser_free(parser_context_t *parser) {
    item_section_t *section, *section_tmp;
    if (!TAILQ_EMPTY(&parser->sections_list)) {
        TAILQ_FOREACH_SAFE(section, &parser->sections_list, tailq, section_tmp) {
            TAILQ_REMOVE(&parser->sections_list, section, tailq);
        }
        tdestroy(parser->sections_tree, free_section);
        parser->sections_tree = NULL;
    }
}

/* Add or get a section */
item_section_t *parser_section_add(const char *section,
        parser_context_t *parser) {
    void *tmp;
    item_common_t item_tmp;
    item_section_t *item_section;

    item_tmp.name = section;
    tmp = CHECK_ALLOC_FATAL(
            tsearch(&item_tmp, &parser->sections_tree, parser_compare));
    if (*(void **) tmp == &item_tmp) {
        item_section = CHECK_ALLOC_FATAL(malloc(sizeof(item_section_t)));
        *(void **) tmp = item_section;
        item_section->name = CHECK_ALLOC_FATAL(strdup(section));
        item_section->keys_tree = NULL;
        TAILQ_INIT(&item_section->keys_list);
        TAILQ_INSERT_TAIL(&parser->sections_list, item_section, tailq);
        return item_section;
    }
    else {
        return *(void **) tmp;
    }
}

/* Get a section by its name */
item_section_t *parser_section_get(const char *section,
        parser_context_t *parser) {
    void *tmp;
    item_common_t item_tmp;

    item_tmp.name = section;
    tmp = tfind(&item_tmp, &parser->sections_tree, parser_compare);
    if (tmp == NULL)
        return NULL;
    return *(void **) tmp;
}

/* Remove a section */
void parser_section_remove(item_section_t *section, parser_context_t *parser) {
    if (section) {
        tdelete(section, &parser->sections_tree, parser_compare);
        TAILQ_REMOVE(&parser->sections_list, section, tailq);
        free_section(section);
    }
}

/* Add or get a key */
item_key_t *parser_key_add(const char *key, item_section_t *section) {
    void *tmp;
    item_common_t item_tmp;
    item_key_t *item_key;

    item_tmp.name = key;
    tmp = CHECK_ALLOC_FATAL(
            tsearch(&item_tmp, &section->keys_tree, parser_compare));
    if (*(void **) tmp == &item_tmp) {
        item_key = CHECK_ALLOC_FATAL(malloc(sizeof(item_key_t)));
        *(void **) tmp = item_key;
        item_key->name = CHECK_ALLOC_FATAL(strdup(key));
        item_key->n_values = 0;
        TAILQ_INIT(&item_key->values_list);
        TAILQ_INSERT_TAIL(&section->keys_list, item_key, tailq);
        return item_key;
    }
    else {
        return *(void **) tmp;
    }
}

/* Get a key */
item_key_t *parser_key_get(const char *key, item_section_t *section) {
    void *tmp;
    item_common_t item_tmp;

    item_tmp.name = key;
    tmp = tfind(&item_tmp, &section->keys_tree, parser_compare);
    if (tmp == NULL)
        return NULL;
    return *(void **) tmp;
}

/* Remove a key */
void parser_key_remove(item_key_t *key, item_section_t *section) {
    if (key) {
        tdelete(key, &section->keys_tree, parser_compare);
        TAILQ_REMOVE(&section->keys_list, key, tailq);
        free_key(key);
    }
}

/* The number of values stored in this key */
int parser_key_get_nvalues(item_key_t *key) {
    return key->n_values;
}

/* Append a new value to a key */
item_value_t * parser_value_add(const char *value, size_t nline,
        item_key_t *key) {
    item_value_t *item;
    item = CHECK_ALLOC_FATAL(malloc(sizeof(item_value_t)));
    item->s = CHECK_ALLOC_FATAL(strdup(value));
    strlib_init(&item->expanded);
    item->nline = nline;
    item->expanding = 0;
    TAILQ_INSERT_TAIL(&key->values_list, item, tailq);
    key->n_values++;
    return item;
}

/* Get a value from a key
 * if n == -1, get the last value
 * otherwhise get the value number n
 */
item_value_t * parser_value_get(int n, item_key_t *key) {
    int i = 0;
    item_value_t *value;

    if (!key)
        return NULL;

    if (key->n_values == 0 || n > key->n_values)
        return NULL;
    if (n == -1) {
        return TAILQ_LAST(&key->values_list, item_value_list);
    }
    else {
        TAILQ_FOREACH(value, &key->values_list, tailq) {
            if (i == n)
                return value;
            i++;
        }
    }
    return NULL;
}

/* Remove a value from a key */
void parser_value_remove(item_value_t *value, item_key_t *key) {
    TAILQ_REMOVE(&key->values_list, value, tailq);
    free_value(value);
}

/* Add [section] key = value
 * Create the section and the key if they do not exist
 */
void parser_set(const char *section, const char *key, const char *value,
        size_t nline, parser_context_t *parser) {
    item_section_t *item_section;
    item_key_t *item_key;

    item_section = parser_section_add(section, parser);
    item_key = parser_key_add(key, item_section);
    parser_value_add(value, nline, item_key);
}

/*
 * Expand the ${...} sequences of the values using the keys in section
 * Unknown sequences are kept
 */
char * parser_value_expand(item_section_t *section, item_value_t *value) {
    int escaped = 0;
    char *src, *end, *rplc;
    item_key_t *rplc_key;
    item_value_t *rplc_value;
    const char *error_str = "[RECURSION ERROR]";

    if (value->expanding == 1) {
        // Already expanding this value, recursion error
        return NULL;
    }

    strlib_reset(&value->expanded);
    src = value->s;
    value->expanding = 1;

    while (*src) {
        if (!escaped) {
            if (*src == '$') {
                escaped = 1;
            }
            else {
                strlib_push(&value->expanded, *src);
            }
        }
        else {
            if (*src == '{' && (end = strchr(src + 2, '}')) != NULL) {
                *end = '\0';
                rplc_key = parser_key_get(src + 1, section);
                rplc_value = parser_value_get(-1, rplc_key);
                if (rplc_value) {
                    rplc = parser_value_expand(section, rplc_value);
                    if (rplc) {
                        strlib_append(&value->expanded, rplc, strlen(rplc));
                    }
                    else {
                        strlib_append(&value->expanded, error_str, strlen(
                                error_str));
                    }
                }
                else {
                    strlib_append(&value->expanded, src - 1, strlen(src - 1));
                    strlib_push(&value->expanded, '}');
                }
                src = end;
                *end = '}';
            }
            else if (*src == '$') {
                strlib_push(&value->expanded, *src);
            }
            else {
                strlib_push(&value->expanded, '$');
                strlib_push(&value->expanded, *src);
            }
            escaped = 0;
        }

        src++;
    }

    value->expanding = 0;

    return value->expanded.s;
}

/*
 * Get [section] key value number n
 * if n == -1, get the last value
 * If expand is true, the value is expanded
 */
item_value_t * parser_get(const char *section, const char *key, int n,
        int expand, parser_context_t *parser) {
    item_section_t *item_section;
    item_key_t *item_key;
    item_value_t *item_value;

    item_section = parser_section_get(section, parser);
    if (!item_section)
        return NULL;
    item_key = parser_key_get(key, item_section);
    if (!item_key)
        return NULL;
    item_value = parser_value_get(n, item_key);
    if (!item_value)
        return NULL;
    if (expand)
        parser_value_expand(item_section, item_value);
    return item_value;
}

/*
 * Get [section] key value number n and convert the value to a unsigned long int
 * The result of the conversion is copied into value
 * The item_value_t is copied into item
 * Return -1 if the value does not exist
 *        0 on conversion error
 *        1 on success
 */
int parser_get_ulong(const char *section, const char *key, int n,
        unsigned long int *value, item_value_t **item, parser_context_t *parser) {
    item_value_t *item_value;
    item_value = parser_get(section, key, n, 1, parser);
    unsigned long int i;
    char *endptr;

    if (item_value == NULL)
        return -1;

    if (item != NULL)
        *item = item_value;

    errno = 0;
    i = strtoul(item_value->expanded.s, &endptr, 10);
    if (errno != 0 || endptr == item_value->expanded.s)
        return 0;
    *value = i;
    return 1;
}

/*
 * Conversion to long int
 */
int parser_get_long(const char *section, const char *key, int n,
        long int *value, item_value_t **item, parser_context_t *parser) {
    item_value_t *item_value;
    item_value = parser_get(section, key, n, 1, parser);
    long int i;
    char *endptr;

    if (item_value == NULL)
        return -1;

    if (item != NULL)
        *item = item_value;

    errno = 0;
    i = strtol(item_value->expanded.s, &endptr, 10);
    if (errno != 0 || endptr == item_value->expanded.s)
        return 0;
    *value = i;
    return 1;
}

/*
 * Conversion to unsigned int
 */
int parser_get_uint(const char *section, const char *key, int n,
        unsigned int *value, item_value_t **item, parser_context_t *parser) {
    int r;
    unsigned long int tmp;

    r = parser_get_ulong(section, key, n, &tmp, item, parser);
    if (r == 1) {
        if (tmp <= UINT_MAX) {
            *value = (unsigned int) tmp;
        }
        else
            return 0;
    }
    return r;
}

/*
 * Conversion to int
 */
int parser_get_int(const char *section, const char *key, int n, int *value,
        item_value_t **item, parser_context_t *parser) {
    int r;
    long int tmp;

    r = parser_get_long(section, key, n, &tmp, item, parser);
    if (r == 1) {
        if (tmp <= INT_MAX && tmp >= INT_MIN) {
            *value = (int) tmp;
        }
        else
            return 0;
    }
    return r;
}

/*
 * Conversion to unsigned short int
 */
int parser_get_ushort(const char *section, const char *key, int n,
        unsigned short *value, item_value_t **item, parser_context_t *parser) {
    int r;
    unsigned long int tmp;

    r = parser_get_ulong(section, key, n, &tmp, item, parser);
    if (r == 1) {
        if (tmp <= USHRT_MAX) {
            *value = (unsigned short) tmp;
        }
        else
            return 0;
    }
    return r;
}

/*
 * Conversion to short int
 */
int parser_get_short(const char *section, const char *key, int n, short *value,
        item_value_t **item, parser_context_t *parser) {
    int r;
    long int tmp;

    r = parser_get_long(section, key, n, &tmp, item, parser);
    if (r == 1) {
        if (tmp <= SHRT_MAX && tmp >= SHRT_MIN) {
            *value = (short) tmp;
        }
        else
            return 0;
    }
    return r;
}

/*
 * Conversion to float
 */
int parser_get_float(const char *section, const char *key, int n, float *value,
        item_value_t **item, parser_context_t *parser) {
    item_value_t *item_value;
    item_value = parser_get(section, key, n, 1, parser);
    float f;
    char *endptr;

    if (item_value == NULL)
        return -1;

    if (item != NULL)
        *item = item_value;

    errno = 0;
    f = strtof(item_value->expanded.s, &endptr);
    if (errno != 0 || endptr == item_value->expanded.s)
        return 0;
    *value = f;
    return 1;
}

/*
 * Conversion to a boolean
 * Valid values are "yes", "on", "1", "true"  -> 1
 *                  "no", "off", "0", "false" -> 0
 * Values are case insensitive
 */
int parser_get_bool(const char *section, const char *key, int n, int *value,
        item_value_t **item, parser_context_t *parser) {
    item_value_t *item_value;
    item_value = parser_get(section, key, n, 1, parser);
    char *v;

    if (item_value == NULL)
        return -1;

    if (item != NULL)
        *item = item_value;

    v = item_value->expanded.s;
    if (strcasecmp(v, "yes") == 0 || strcasecmp(v, "1") == 0 || strcasecmp(v,
            "true") == 0 || strcasecmp(v, "on") == 0) {
        *value = 1;
        return 1;
    }
    if (strcasecmp(v, "no") == 0 || strcasecmp(v, "0") == 0 || strcasecmp(v,
            "false") == 0 || strcasecmp(v, "off") == 0) {
        *value = 0;
        return 1;
    }

    return 0;
}
