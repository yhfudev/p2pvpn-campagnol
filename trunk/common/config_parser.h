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

/*
 * This module handles INI configuration files.
 *
 * Syntax :
 * - Sections
 *   - sections are defined between square brackets : [section]
 *   - sections end at the next section declaration or at EOF
 *   - section's names are case sensitive
 *   - whitespaces around the section's name are stripped
 *   - the name can contain whitespaces
 * - Keys
 *   - the only valid syntax is : key = value
 *   - whitespaces around the key's name are stripped
 *   - keys are case sensitive
 *   - keys can contain whitespaces
 *   - keys may optionally be defined before the first section mark. They will be put in a "DEFAULT" section.
 *   - keys may optionally accept empty values
 *   - if a key is used more than once in the same section, all the values are
 *     stored and can be retrived
 * - Comments
 *   - comments start with a ';' or a '#' and continue to the end of the line
 * - Blank lines are allowed
 * - Values
 *   - values can be quoted between "". A quoted value can expand to several
 *     lines.
 *   - quotted and unquotted values can be mixed on the same line:
 *     aze" rty" uio "p"
 *     is equivalent to
 *     "aze rtyuiop"
 *   - A '\' at the end of a line is treated as a line continuation
 *     even inside a quoted value.
 *   - values can reference other values from the same section:
 *       [SECTION]
 *       dir = /home/foo/
 *       file = ${dir}file
 *     file is equivalent to "/home/foo/file"
 *   - If a ${.} sequence must not be expanded, it can be protected with $${.}
 * - Escaped characters:
 *   - the C escape sequences
 *   - \# for #
 *   - \; for ;
 *   - \[ for [
 *   - \] for ]
 *   - \= for =
 * - There is no hierarchy within sections
 *
 * The parser context is stored in a parser_context_t value.
 * The context must be initialized with parser_init
 * The context is freed with parser_free.
 */

#ifndef CONFIG_PARSER_H_
#define CONFIG_PARSER_H_

#include <sys/queue.h>
#include "strlib.h"

/* Glibc's sys/queue.h is out of date */
#ifndef TAILQ_FOREACH_SAFE
#   define TAILQ_FOREACH_SAFE(var, head, field, tvar)                      \
        for ((var) = TAILQ_FIRST((head));                               \
            (var) && ((tvar) = TAILQ_NEXT((var), field), 1);            \
            (var) = (tvar))
#endif

/*
 * A value in the configuration file
 */
struct item_value {
    char *s; // raw string
    size_t nline; // line number in the file
    strlib_buf_t expanded; // expanded string
    int expanding; // the value is being expanded
TAILQ_ENTRY(item_value) tailq; // tail queue data
};
typedef struct item_value item_value_t;
TAILQ_HEAD(item_value_list, item_value);

/*
 * A key in the configuration file
 */
struct item_key {
    char *name; // name
    int n_values; // number of values
    struct item_value_list values_list; // tail queue of values
TAILQ_ENTRY(item_key) tailq; // tail queue data
};
typedef struct item_key item_key_t;
TAILQ_HEAD(item_key_list, item_key);

/*
 * A section in the configuration file
 */
struct item_section {
    char *name; // name
    void *keys_tree; // tree with the keys
    struct item_key_list keys_list; // tail queue of the keys
TAILQ_ENTRY(item_section) tailq; // tail queue structure
};
typedef struct item_section item_section_t;
TAILQ_HEAD(item_section_list, item_section);

/*
 * Parser
 */
struct parser_context {
    int allow_default; // Allow the use of the DEFAULT section
    int allow_empty_value; // Allow empty values
    void *sections_tree; // tree with the sections
    struct item_section_list sections_list; // tail queue of the sections
};
typedef struct parser_context parser_context_t;

/*
 * Name of the default section
 */
#define SECTION_DEFAULT     "DEFAULT"

extern void parser_init(parser_context_t *parser, int allow_default,
        int allow_empty_value);
extern void parser_free(parser_context_t *parser);

extern void parser_read(const char *file, parser_context_t *parser, int debug);
extern void parser_write(FILE *f, parser_context_t *parser, int expanded);

extern item_section_t * parser_section_add(const char *section,
        parser_context_t *parser);
extern item_section_t * parser_section_get(const char *section,
        parser_context_t *parser);
extern void parser_section_remove(item_section_t *section,
        parser_context_t *parser);

extern item_key_t * parser_key_add(const char *key, item_section_t *section);
extern item_key_t * parser_key_get(const char *key, item_section_t *section);
extern void parser_key_remove(item_key_t *key, item_section_t *section);
extern int parser_key_get_nvalues(item_key_t *key);

extern item_value_t * parser_value_add(const char *value, size_t nline,
        item_key_t *key);
extern item_value_t * parser_value_get(int n, item_key_t *key);
extern void parser_value_remove(item_value_t *value, item_key_t *key);
extern char *parser_value_expand(item_section_t *section, item_value_t *value);

extern void parser_set(const char *section, const char *key, const char *value,
        size_t nline, parser_context_t *parser);
extern item_value_t * parser_get(const char *section, const char *key, int n,
        int expand, parser_context_t *parser);
extern char * parser_get_expanded(const char *section, const char *key, int n,
        parser_context_t *parser);


/*
 * Conversion functions
 * return -1 if the section/key is unknown, 0 on error, 1 on success
 */
extern int
        parser_get_ulong(const char *section, const char *key, int n,
                unsigned long int *value, item_value_t **item,
                parser_context_t *parser);
extern int parser_get_long(const char *section, const char *key, int n,
        long int *value, item_value_t **item, parser_context_t *parser);
extern int parser_get_uint(const char *section, const char *key, int n,
        unsigned int *value, item_value_t **item, parser_context_t *parser);
extern int parser_get_int(const char *section, const char *key, int n,
        int *value, item_value_t **item, parser_context_t *parser);
extern int parser_get_ushort(const char *section, const char *key, int n,
        unsigned short *value, item_value_t **item, parser_context_t *parser);
extern int parser_get_short(const char *section, const char *key, int n,
        short *value, item_value_t **item, parser_context_t *parser);
extern int parser_get_float(const char *section, const char *key, int n,
        float *value, item_value_t **item, parser_context_t *parser);
extern int parser_get_bool(const char *section, const char *key, int n,
        int *value, item_value_t **item, parser_context_t *parser);

#endif /* CONFIG_PARSER_H_ */
