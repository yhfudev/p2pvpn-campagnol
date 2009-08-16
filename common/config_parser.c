/*
 * Pseudo INI file parser
 *
 * Copyright (C) 2008-2009 Florent Bondoux
 *
 * This file is part of Campagnol.
 *
 * Campagnol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Campagnol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Campagnol.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "config_parser.h"
#include "../common/log.h"

#include <search.h>
#include <string.h>
#include <pthread.h>

#ifndef HAVE_GETLINE
#   include "../lib/getline.h"
#endif
#ifndef HAVE_TDESTROY
#   include "../lib/tdestroy.h"
#   define tdestroy(root,free_node) campagnol_tdestroy(root,free_node,parser_compare)
#endif

/* internal comparison function (strcmp) */
static int parser_compare(const void *itema, const void *itemb) {
    return strcmp(((const item_common_t *) itema)->name,
            ((const item_common_t *) itemb)->name);
}

/* set [section] option=value @line number = nline
 * Create section if it doesn't exist
 * It will override a previously defined value
 */
void parser_set(const char *section, const char *option, const char *value,
        int nline, parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;
    item_value_t *item_value;
    const_item_common_t tmp_item;

    // get or create section
    tmp_item.name = section;
    tmp = CHECK_ALLOC_FATAL(tsearch(&tmp_item, &parser->data, parser_compare));
    if (*(void **) tmp == &tmp_item) {
        item_section = (item_section_t *) CHECK_ALLOC_FATAL(malloc(sizeof(item_section_t)));
        *(void **) tmp = item_section;
        item_section->name = CHECK_ALLOC_FATAL(strdup(section));
        item_section->values_tree = NULL;
        item_section->parser = parser;
        item_section->n_values = 0;
    }
    else {
        item_section = *(void **) tmp;
    }

    // get or create option
    tmp_item.name = option;
    tmp = CHECK_ALLOC_FATAL(tsearch(&tmp_item, &item_section->values_tree, parser_compare));
    if (*(void **) tmp == &tmp_item) {
        item_value = (item_value_t *) CHECK_ALLOC_FATAL(malloc(sizeof(item_value_t)));
        *(void **) tmp = item_value;
        item_value->name = CHECK_ALLOC_FATAL(strdup(option));
        item_section->n_values++;
    }
    else {
        item_value = *(void **) tmp;
        free(item_value->value);
        free(item_value->expanded_value);
    }

    // set value
    item_value->value = CHECK_ALLOC_FATAL(strdup(value));
    item_value->nline = nline;
    item_value->section = item_section;
    item_value->expanded_value = NULL;
    item_value->expanding = 0;
}

/* Create a new empty section
 * or do nothing if it already exists*/
void parser_add_section(const char *section, parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;
    const_item_common_t tmp_section;

    tmp_section.name = section;
    tmp = CHECK_ALLOC_FATAL(tsearch(&tmp_section, &parser->data, parser_compare));
    if (*(void **) tmp == &tmp_section) {
        item_section = (item_section_t *) CHECK_ALLOC_FATAL(malloc(sizeof(item_section_t)));
        *(void **) tmp = item_section;
        item_section->name = CHECK_ALLOC_FATAL(strdup(section));
        item_section->values_tree = NULL;
        item_section->parser = parser;
        item_section->n_values = 0;
    }
}

/* Tell whether section exists */
int parser_has_section(const char *section, parser_context_t *parser) {
    void *tmp;
    const_item_common_t tmp_section;

    tmp_section.name = section;
    tmp = tfind(&tmp_section, &parser->data, parser_compare);
    return tmp != NULL;
}

/* Return the number of keys in a section, or -1 if it doesn't exist */
int parser_section_count(const char *section, parser_context_t *parser) {
    void *tmp;
    item_section_t *item;
    const_item_common_t tmp_section;

    tmp_section.name = section;
    tmp = tfind(&tmp_section, &parser->data, parser_compare);
    if (tmp != NULL) {
        item = *(void **) tmp;
        return item->n_values;
    }
    return -1;
}

/* Indicate whether [section] option is defined */
int parser_has_option(const char *section, const char *option,
        parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;
    const_item_common_t tmp_item;

    tmp_item.name = section;
    tmp = tfind(&tmp_item, &parser->data, parser_compare);
    if (tmp == NULL)
        return 0;

    item_section = *(void **) tmp;

    tmp_item.name = option;
    tmp = tfind(&tmp_item, &item_section->values_tree, parser_compare);

    return tmp != NULL;
}

/* Perform option value substitution
 * return a newly allocated string */
static char *parser_substitution(const char *section, const char *value,
        parser_context_t *parser) {
    size_t len, len_written;
    const char *src;
    char *end, *v;
    char *new_value, *dst;
    int escaped;
    size_t i;

    len = 0;
    src = value;
    escaped = 0;

    // compute the length of value minus the expanded parts
    // len includes the final null byte and the \$ -> $ transformation
    for (;; src++) {
        if (!escaped) {
            // escape character
            if (*src == '\\')
                escaped = 1;
            // ${ ... }, skip it
            else if (*src == '$' && *(src + 1) == '{' && (end = strchr(src + 2,
                    '}')) != NULL) {
                src = end;
            }
            else
                // normal character
                len++;
        }
        else {
            if (*src != '$')
                len++;
            len++;
            escaped = 0;
        }
        if (*src == '\0')
            break;
    }

    // Now create the exanded value into new_value

    len_written = 0; // number of chars already written
    new_value = CHECK_ALLOC_FATAL(malloc(len));
    dst = new_value; // dst pointer
    *dst = '\0';
    src = value;
    escaped = 0;
    for (;; src++) {
        if (!escaped) {
            if (*src == '\\')
                escaped = 1;
            // ${ ... }
            else if (*src == '$' && *(src + 1) == '{' && (end = strchr(src + 2,
                    '}')) != NULL) {
                *end = '\0';
                v = parser_get(section, src + 2, NULL, parser);
                if (v != NULL) { // replace ${...} by it's value after a realloc
                    len += strlen(v);
                    new_value = CHECK_ALLOC_FATAL(realloc(new_value, len));
                    dst = new_value + len_written;
                    for (i = 0; i < strlen(v); i++) {
                        dst[i] = v[i];
                        len_written++;
                    }
                    dst = new_value + len_written;
                    src = end;
                }
                else { // leave the ${...} in place
                    len += (end - src + 1);
                    new_value = CHECK_ALLOC_FATAL(realloc(new_value, len));
                    dst = new_value + len_written;
                    for (i = 0; i < (unsigned int) (end - src); i++) {
                        *dst = src[i];
                        len_written++;
                        dst++;
                    }
                    *dst = '}';
                    len_written++;
                    dst++;
                    src = end;

                }
                *end = '}';
            }
            else {
                *dst = *src;
                len_written++;
                dst++;
            }
        }
        else {
            if (*src != '$') {
                *dst = '\\';
                len_written++;
                dst++;
            }
            *dst = *src;
            len_written++;
            dst++;
            escaped = 0;
        }

        if (*src == '\0')
            break;
    }

    ASSERT(len == len_written);

    return new_value;
}

static char *parser_get_expanded(item_value_t *item_value) {
    char *new_value;
    // do we need to expand item_value->value
    if (!item_value->section->parser->allow_value_expansions) {
        return item_value->value;
    }
    else if (item_value->expanding) {
        // value is beeing expanded
        return item_value->section->parser->parser_error_string;
    }
    else {
        item_value->expanding = 1;
        new_value = parser_substitution(item_value->section->name,
                item_value->value, item_value->section->parser);
        free(item_value->expanded_value);
        item_value->expanded_value = new_value;
        item_value->expanding = 0;
        return item_value->expanded_value;
    }
}

/* Return the value of [section] option
 * If section or option is not defined, then return NULL
 * if nline is not NULL, copy the line number into nline
 */
char *parser_get(const char *section, const char *option, int *nline,
        parser_context_t *parser) {
    item_section_t *item_section;
    item_value_t *item_value;
    const_item_common_t tmp_item;
    void *tmp;

    tmp_item.name = section;
    tmp = tfind(&tmp_item, &parser->data, parser_compare);
    if (tmp == NULL) {
        return NULL;
    }
    item_section = *(void **) tmp;

    tmp_item.name = option;
    tmp = tfind(&tmp_item, &item_section->values_tree, parser_compare);
    if (tmp == NULL) {
        return NULL;
    }
    item_value = *(void **) tmp;

    if (nline != NULL)
        *nline = item_value->nline;

    return parser_get_expanded(item_value);
}

/* Parse [section] option into value as a long integer
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not an integer, -1 if the
 * option is not defined
 */
int parser_getlong(const char *section, const char *option, long int *value,
        char **raw, int *nline, parser_context_t *parser) {
    char *v = parser_get(section, option, nline, parser);
    if (v == NULL) {
        return -1;
    }

    if (raw != NULL) {
        *raw = v;
    }

    return sscanf(v, "%ld", value);
}

/* Parse [section] option into value as an unsigned long integer
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not an integer, -1 if the
 * option is not defined
 */
int parser_getulong(const char *section, const char *option,
        unsigned long int *value, char **raw, int *nline, parser_context_t *parser) {
    char *v = parser_get(section, option, nline, parser);
    if (v == NULL) {
        return -1;
    }

    if (raw != NULL) {
        *raw = v;
    }

    return sscanf(v, "%lu", value);
}

/* Parse [section] option into value as an integer
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not an integer, -1 if the
 * option is not defined
 */
int parser_getint(const char *section, const char *option, int *value,
        char **raw, int *nline, parser_context_t *parser) {
    long int tmp;
    int r;
    r = parser_getlong(section, option, &tmp, raw, nline, parser);
    if (r == 1) {
        if (tmp <= INT_MAX && tmp >= INT_MIN) {
            *value = (int) tmp;
        }
        else
            return 0;
    }
    return r;
}

/* Parse [section] option into value as an unsigned integer
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not an integer, -1 if the
 * option is not defined
 */
int parser_getuint(const char *section, const char *option,
        unsigned int *value, char **raw, int *nline, parser_context_t *parser) {
    unsigned long int tmp;
    int r;
    r = parser_getulong(section, option, &tmp, raw, nline, parser);
    if (r == 1) {
        if (tmp <= UINT_MAX) {
            *value = (unsigned int) tmp;
        }
        else
            return 0;
    }
    return r;
}

/* Parse [section] option into value as an unsigned short
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not an integer, -1 if the
 * option is not defined
 */
int parser_getushort(const char *section, const char *option,
        unsigned short *value, char **raw, int *nline, parser_context_t *parser) {
    unsigned long int tmp;
    int r;
    r = parser_getulong(section, option, &tmp, raw, nline, parser);
    if (r == 1) {
        if (tmp <= USHRT_MAX) {
            *value = (unsigned short) tmp;
        }
        else
            return 0;
    }
    return r;
}

/* Parse [section] option into value as a float
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not a float, -1 if the option
 * is not defined
 */
int parser_getfloat(const char *section, const char *option, float *value,
        char **raw, int *nline, parser_context_t *parser) {
    char *v = parser_get(section, option, nline, parser);
    if (v == NULL) {
        return -1;
    }

    if (raw != NULL) {
        *raw = v;
    }

    return sscanf(v, "%f", value);
}

/* Parse [section] option into value as a boolean
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not valid, -1 if the option
 * is not defined
 * valid values are "yes", "on", "1", "true"
 *                  "no", "off", "0", "false"
 * Values are case insensitive
 */
int parser_getboolean(const char *section, const char *option, int *value,
        char **raw, int *nline, parser_context_t *parser) {
    char *v = parser_get(section, option, nline, parser);
    if (v == NULL) {
        return -1;
    }

    if (raw != NULL) {
        *raw = v;
    }

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

/* internal function used to free an item_value_t */
static void free_value(void *p) {
    item_value_t *v = (item_value_t *) p;
    free(v->value);
    free(v->expanded_value);
    free(v->name);
    free(v);
}

/* internal function used to free an item_section_t
 * It will destroy its options.
 */
static void free_section(void *p) {
    item_section_t *section = (item_section_t *) p;
    if (section->values_tree != NULL) {
        tdestroy(section->values_tree, free_value);
    }
    free(section->name);
    free(p);
}

/* Clean the parser */
void parser_free(parser_context_t *parser) {
    tdestroy(parser->data, free_section);
    parser->data = NULL;
    free(parser->parser_error_string);
}

/* Initialize a new parser */
void parser_init(parser_context_t *parser, int allow_default_section,
        int allow_empty_value, int allow_value_expansions) {
    parser->data = NULL;
    parser->allow_default = allow_default_section;
    parser->allow_empty_value = allow_empty_value;
    parser->allow_value_expansions = allow_value_expansions;
    parser->parser_error_string = CHECK_ALLOC_FATAL(strdup("[RECURSION ERROR]"));
}

/* Remove a [section] option
 * or do nothing if it doesn't exist
 */
void parser_remove_option(const char *section, const char *option,
        parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;
    item_value_t *item_value;
    const_item_common_t tmp_item;

    tmp_item.name = section;
    tmp = tfind(&tmp_item, &parser->data, parser_compare);
    if (tmp == NULL)
        return;

    item_section = *(void **) tmp;

    tmp_item.name = option;
    tmp = tfind(&tmp_item, &item_section->values_tree, parser_compare);

    if (tmp != NULL) {
        item_value = *(void **) tmp;
        tdelete(option, &item_section->values_tree, parser_compare);
        item_section->n_values--;
        free_value(item_value);
    }
}

/* Remove a whole section and its options
 * or do nothing if it doesn't exist
 */
void parser_remove_section(const char *section, parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;
    const_item_common_t tmp_item;

    tmp_item.name = section;
    tmp = tfind(&tmp_item, &parser->data, parser_compare);
    if (tmp == NULL)
        return;
    item_section = *(void **) tmp;
    tdelete(section, &parser->data, parser_compare);
    free_section(item_section);
}

/* remove comments # and ; */
static int remove_comments(char *line) {
    const char *src = line;
    char *dst = line;

    int escaped = 0;

    for (;; src++) {
        if (!escaped) {
            if (*src == '\\')
                escaped = 1;
            else if (*src == '#' || *src == ';') {
                *dst = '\0';
                dst++;
                break;
            }
            else {
                *dst = *src;
                dst++;
            }
        }
        else {
            *dst = '\\';
            dst++;
            *dst = *src;
            dst++;
            escaped = 0;
        }

        if (*src == '\0')
            break;
    }

    return (dst - line) - 1;
}

/* value expansion
 *
 * expand characters \# \; \t \n \r \\ and \"
 * Remove the quotes around the value
 *
 * continued is set to 1 if the token ends with '\'<newline>
 * quoted must be set to 1 if we are inside a quoted value and will be
 * set accordingly.
 * return the length of token after expansion or -1 if the syntax of the line
 * continuation is wrong.
 */
static int expand_token(char *token, int *quoted, int *continued) {
    const char *src = token;
    char *dst = token;
    int escaped = 0;
    char *escape_pos = NULL;
    int end_quotes = 0;

    *continued = 0;

    if (*src == '"' && !*quoted) {
        *quoted = 1;
        src++;
    }

    for (;; src++) {
        if (!escaped) {
            if (*src == '\\') { // escape char
                escaped = 1;
                escape_pos = dst;
            }
            else if (*src == '"' && *quoted) { // closing quote
                *dst = '\0';
                dst++;
                end_quotes = 1;
                *quoted = 0;
                break;
            }
            else { // normal char
                *dst = *src;
                dst++;
            }
        }
        else {
            switch (*src) {
                case '#': *dst = '#'; break;
                case ';': *dst = ';'; break;
                case '\\': *dst = '\\'; break;
                case 't': *dst = '\t'; break;
                case 'n': *dst = '\n'; break;
                case 'r': *dst = '\r'; break;
                case '"': *dst = '"'; break;
                default:
                    *dst = '\\';
                    dst++;
                    *dst = *src;
                    break;
            }
            dst++;
            if (*src != '\0') // keep escaped=1 at the end of line
                escaped = 0;
        }

        if (*src == '\0')
            break;
    }

    if (end_quotes) { // last char was a closing ", search for line continuation
        src++;
        while (*src) {
            switch (*src) {
                case ' ':
                    if (*continued == 1) {
                        return -1;
                    }
                    break;
                case '\\':
                    if (*continued == 1) {
                        return -1;
                    }
                    *continued = 1;
                    break;
                default:
                    return -1;
                    break;
            }
            src++;
        }
    }
    else if (escaped) { // last char was a '\'
        *continued = 1;
        *escape_pos = '\0'; // remove last '\'
        dst = escape_pos + 1;
    }

    //ASSERT((dst-token-1) == strlen(token));
    return (dst - token) - 1;
}

/* Parse a file */
void parser_read(const char *confFile, parser_context_t *parser, int debug) {
    FILE *conf = fopen(confFile, "r");
    if (conf == NULL) {
        log_error(errno, confFile);
        exit(EXIT_FAILURE);
    }

    char * line = NULL; // last read line
    size_t line_len = 0; // length of the line buffer
    ssize_t r; // length of the line
    char *token; // word
    char *name = NULL; // option name
    char *value = NULL; // option value
    char *section = NULL; // current section
    size_t name_length = 0, value_length = 0, section_length = 0;

    int continued, r_continued;
    int quoted;
    char *token_end;
    char *line_eq;
    char *eol;
    int nline = 0;

    /* Read the configuration file */
    while ((r = getline(&line, &line_len, conf)) != -1) {
        nline++;

        quoted = 0;

        // end of line
        eol = strstr(line, "\r\n");
        if (eol == NULL)
            eol = strchr(line, '\n');
        if (eol != NULL) {
            *eol = '\0';
        }

        token = line;
        // remove leading spaces:
        while (*token == ' ' || *token == '\t')
            token++;

        // remove comments
        r = remove_comments(token);

        // empty line:
        if (r == 0)
            continue;

        // section ?
        if (token[0] == '[') {
            token++;
            // strip leading spaces
            while (*token == ' ' || *token == '\t')
                token++;
            token_end = strrchr(token, ']');
            if (token_end == NULL) {
                log_message("[%s:%d] Syntax error, section declaration",
                        confFile, nline);
                continue;
            }
            if (token_end == token) {
                log_message("[%s:%d] Syntax error, empty section", confFile,
                        nline);
                continue;
            }
            // strip trailing spaces
            while (*(token_end - 1) == ' ' || *(token_end - 1) == '\t')
                token_end--;
            *token_end = '\0';
            ASSERT(token_end > token);

            if (section_length < (unsigned int) (token_end - token + 1)) {
                section_length = token_end - token + 1;
                free(section);
                section = CHECK_ALLOC_FATAL(malloc(section_length));
            }
            ASSERT(section_length >= strlen(token) + 1);
            strcpy(section, token);

            parser_add_section(section, parser);

            continue;
        }

        // something outside of a section ?
        if (section == NULL) {
            if (parser->allow_default) {
                section = CHECK_ALLOC_FATAL(strdup(SECTION_DEFAULT));
                section_length = strlen(SECTION_DEFAULT) + 1;
            }
            else {
                log_message(
                        "[%s:%d] Syntax error, option outside of a section",
                        confFile, nline);
                continue;
            }
        }

        // find first equal sign:
        line_eq = strchr(token, '=');
        token_end = line_eq;
        // no '=' or empty token:
        if (token_end == NULL || token_end == token) {
            log_message("[%s:%d] Syntax error", confFile, nline);
            continue;
        }
        // remove trailing spaces:
        while (*(token_end - 1) == ' ' || *(token_end - 1) == '\t')
            token_end--;
        // copy name:
        *token_end = '\0';
        ASSERT(token_end > token);

        if (name_length < (unsigned int) (token_end - token + 1)) {
            name_length = token_end - token + 1;
            free(name);
            name = CHECK_ALLOC_FATAL(malloc(name_length));
        }
        ASSERT(name_length >= strlen(token) + 1);
        strcpy(name, token);

        // get the value
        token = line_eq + 1;
        // strip leading spaces
        while (*token == ' ' || *token == '\t')
            token++;
        token_end = token + strlen(token);
        if (!parser->allow_empty_value && token_end == token) {
            log_message("[%s:%d] Syntax error, empty value", confFile, nline);
            continue;
        }
        // strip trailing spaces
        while (*(token_end - 1) == ' ' || *(token_end - 1) == '\t')
            token_end--;
        *token_end = '\0';

        /* remove quotes and expand escaped chars. The return value doesn't
        include the terminating null byte */
        r = expand_token(token, &quoted, &continued);

        if (r == -1) {
            log_message("[%s:%d] Syntax error, line continuation", confFile,
                    nline);
            continue;
        }

        // inside a quotation: append \n to the token
        if (quoted && !continued)
            r++;

        // copy what we have into value
        ASSERT(r >= 0);
        if (value_length < (unsigned int) (r + 1)) {
            value_length = r + 1;
            free(value);
            value = CHECK_ALLOC_FATAL(malloc(value_length));
        }
        ASSERT(value_length >= strlen(token) + 1 + (quoted && !continued));
        strcpy(value, token);

        // append \n if necessary
        if (quoted && !continued)
            strcat(value, "\n");

        // continue the reading if the line ends with a line continuation
        // or if we are inside a quotation
        while (continued || quoted) {
            r_continued = getline(&line, &line_len, conf);
            if (r_continued == -1) {
                log_message("[%s:%d] End of file, line continuation error",
                        confFile, nline);
                break;
            }
            nline++;

            // end of line
            eol = strstr(line, "\r\n");
            if (eol == NULL)
                eol = strchr(line, '\n');
            if (eol != NULL) {
                *eol = '\0';
            }

            token = line;
            if (!quoted) {
                // remove leading spaces:
                while (*token == ' ' || *token == '\t')
                    token++;

                // remove comments
                r_continued = remove_comments(token);

                if (r_continued == 0)
                    break;

                token_end = token + strlen(token);

                // strip trailing spaces
                while (*(token_end - 1) == ' ' || *(token_end - 1) == '\t')
                    token_end--;
                *token_end = '\0';

            }

            // remove quotes and expand escaped chars
            r_continued = expand_token(token, &quoted, &continued);

            if (r_continued == -1) {
                log_message("[%s:%d] Syntax error, line continuation",
                        confFile, nline);
                break;
            }

            if (quoted && !continued)
                r_continued++;

            // append the line to value
            if (r_continued != 0) {
                r += r_continued;
                if (value_length < (unsigned int) (r + 1)) {
                    value_length = r + 1;
                    value = CHECK_ALLOC_FATAL(realloc(value, value_length));
                }
                ASSERT(value_length >= strlen(token) + 1 + (quoted && !continued));
                strcat(value, token);
                if (quoted && !continued)
                    strcat(value, "\n");
            }
        }

        if (!parser->allow_empty_value && r == 0) {
            log_message("[%s:%d] Syntax error, empty value", confFile, nline);
            continue;
        }

        if (debug) {
            printf("[%s:%d] [%s] '%s' = '%s'\n", confFile, nline, section,
                    name, value);
        }

        ASSERT(strlen(value) == (size_t) r);
        parser_set(section, name, value, nline, parser);
    }

    if (line) {
        free(line);
    }
    if (section)
        free(section);
    if (name)
        free(name);
    if (value)
        free(value);

    fclose(conf);
}

/* internal, write an option and it's value into parser->dump_file
 * escape # ; \ " \n \t \r
 */
static void parser_write_option(const void *nodep, const VISIT which,
        const int depth __attribute__((unused))) {
    const item_value_t *item;
    char *c;
    if (which == postorder || which == leaf) {
        item = *(item_value_t const * const *) nodep;
        fprintf(item->section->parser->dump_file, "%s = \"", item->name);
        for (c = item->value; *c != '\0'; c++) {
            switch (*c) {
                case '#':
                case ';':
                case '\\':
                case '"':
                    fprintf(item->section->parser->dump_file, "\\%c", *c);
                    break;
                case '\t':
                    fprintf(item->section->parser->dump_file, "\\t");
                    break;
                case '\n':
                    fprintf(item->section->parser->dump_file, "\\n");
                    break;
                case '\r':
                    fprintf(item->section->parser->dump_file, "\\r");
                    break;
                default:
                    fprintf(item->section->parser->dump_file, "%c", *c);
                    break;
            }
        }
        fprintf(item->section->parser->dump_file, "\"\n");
    }
}

/* internal, write a complete section into parser->dump_file */
static void parser_write_section(const void *nodep, const VISIT which,
        const int depth __attribute__((unused))) {
    const item_section_t *item;
    switch (which) {
        case postorder:
        case leaf:
            item = *(item_section_t const * const *) nodep;
            fprintf(item->parser->dump_file, "[ %s ]\n", item->name);
            twalk(item->values_tree, parser_write_option);
            fprintf(item->parser->dump_file, "\n");
            break;
        default:
            break;
    }
}

/* write the content of parser into file */
void parser_write(FILE *file, parser_context_t *parser) {
    parser->dump_file = file;
    twalk(parser->data, parser_write_section);
}

static void parser_forall_option(const void *nodep, const VISIT which,
        const int depth __attribute__((unused))) {
    item_value_t *item;
    char *v;
    switch (which) {
        case postorder:
        case leaf:
            item = *(item_value_t * const *) nodep;
            v = parser_get_expanded(item);
            item->section->parser->forall_function(item->section->name,
                    item->name, v, item->nline);
            break;
        default:
            break;
    }
}

static void parser_forall_sec(const void *nodep, const VISIT which,
        const int depth __attribute__((unused))) {
    const item_section_t *item;
    switch (which) {
        case postorder:
        case leaf:
            item = *(item_section_t const * const *) nodep;
            twalk(item->values_tree, parser_forall_option);
            break;
        default:
            break;
    }
}

/* execute f on each option */
void parser_forall(parser_action_t f, parser_context_t *parser) {
    parser->forall_function = f;
    twalk(parser->data, parser_forall_sec);
}

/* execute f on each option of the given section */
void parser_forall_section(const char *section, parser_action_t f,
        parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;
    const_item_common_t tmp_section;

    tmp_section.name = section;
    tmp = tfind(&tmp_section, &parser->data, parser_compare);
    if (tmp == NULL) {
        return;
    }

    item_section = *(void **) tmp;
    parser->forall_function = f;
    twalk(item_section->values_tree, parser_forall_option);
}
