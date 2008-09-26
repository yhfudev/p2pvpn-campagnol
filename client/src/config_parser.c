/*
 * Pseudo INI file parser
 *
 * Copyright (C) 2008 Florent Bondoux
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

#include "campagnol.h"
#include "config_parser.h"
#include "log.h"

#include <search.h>
#include <string.h>
#include <pthread.h>

/* GNU getline replacement */
#ifndef HAVE_GETLINE
ssize_t campagnol_getline(char **lineptr, size_t *n, FILE *stream) {
    ASSERT(lineptr);
    ASSERT(n);
    ASSERT(stream);
    char *p;

    if (*lineptr == NULL || *n == 0) {
        *n = 120;
        *lineptr = (char *) malloc(*n);
        if (*lineptr == NULL) {
            return -1;
        }
    }
    p = *lineptr;
    for (;;p++) {
        if (p - *lineptr == *n) {
            *lineptr = (char *)realloc(*lineptr, *n+120);
            p = *lineptr + *n;
            *n = *n + 120;
        }
        *p = fgetc(stream);
        if (*p == EOF) {
            p--;
            break;
        }
        if (*p == '\n') {
            break;
        }
    }

    if ((p - *lineptr + 1) == 0) return -1;

    p++;
    if (p - *lineptr == *n) {
        *lineptr = (char *)realloc(*lineptr, *n+120);
        p = *lineptr + *n;
        *n = *n + 120;
    }
    *p = '\0';

    return p - *lineptr+1;
}

#   define getline campagnol_getline
#endif

/* GNU tdestroy replacement */
#ifndef HAVE_TDESTROY
/* POSIX doesn't let us know about the node structure
 * so we simply use tdelete.
 * It's not efficient and we have to know the comparison routine
 */
void campagnol_tdestroy(void *root, void (*free_node)(void *nodep), int (*compar)(const void *, const void *)) {
    void *node;
    while (root != NULL) {
        node = *(void **)root;
        tdelete(node, &root, compar);
        free_node(node);
    }
}

#   define tdestroy(root,free_node) campagnol_tdestroy(root,free_node,parser_compare)
#endif



/* internal comparison function (strncmp) */
static int parser_compare(const void *itema, const void *itemb) {
    return strncmp((const char *) itema, (const char *) itemb, CONF_NAME_LENGTH);
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

    // get or create section
    tmp = tsearch(section, &parser->data, parser_compare);
    if (*(void **) tmp == section) {
        item_section = (item_section_t *) malloc(sizeof(item_section_t));
        *(void **) tmp = item_section;
        strncpy(item_section->name, section, CONF_NAME_LENGTH);
        item_section->values_tree = NULL;
        item_section->parser = parser;
    }
    else {
        item_section = *(void **) tmp;
    }

    // get or create option
    tmp = tsearch(option, &item_section->values_tree, parser_compare);
    if (*(void **) tmp == option) {
        item_value = (item_value_t *) malloc(sizeof(item_value_t));
        *(void **) tmp = item_value;
        strncpy(item_value->name, option, CONF_NAME_LENGTH);
    }
    else {
        item_value = *(void **) tmp;
    }

    // set value
    strncpy(item_value->value, value, CONF_VALUE_LENGTH);
    item_value->nline = nline;
    item_value->section = item_section;
}

/* Create a new empty section
 * or do nothing if it already exists*/
void parser_add_section(const char *section, parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;

    tmp = tsearch(section, &parser->data, parser_compare);
    if (*(void **) tmp == section) {
        item_section = (item_section_t *) malloc(sizeof(item_section_t));
        *(void **) tmp = item_section;
        strncpy(item_section->name, section, CONF_NAME_LENGTH);
        item_section->values_tree = NULL;
        item_section->parser = parser;
    }
}

/* Tell whether section exists */
int parser_has_section(const char *section, parser_context_t *parser) {
    void *tmp;

    tmp = tfind(section, &parser->data, parser_compare);
    return tmp != NULL;
}

/* Indicate whether [section] option is defined */
int parser_has_option(const char *section, const char *option,
        parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;

    tmp = tfind(section, &parser->data, parser_compare);
    if (tmp == NULL)
        return 0;

    item_section = *(void **) tmp;

    tmp = tfind(option, &item_section->values_tree, parser_compare);

    return tmp != NULL;
}

/* Return the value of [section] option
 * If section or option is not defined, then return NULL
 * if nline is not NULL, copy the line number into nline
 */
char *parser_get(const char *section, const char *option, int *nline,
        parser_context_t *parser) {
    item_section_t *item_section;
    item_value_t *item_value;
    void *tmp;

    tmp = tfind(section, &parser->data, parser_compare);
    if (tmp == NULL) {
        return NULL;
    }
    item_section = *(void **) tmp;

    tmp = tfind(option, &item_section->values_tree, parser_compare);
    if (tmp == NULL) {
        return NULL;
    }
    item_value = *(void **) tmp;

    if (nline != NULL)
        *nline = item_value->nline;

    return item_value->value;
}

/* Parse [section] option into value as an integer
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not an integer, -1 if the option is not defined
 */
int parser_getint(const char *section, const char *option, int *value,
        char **raw, int *nline, parser_context_t *parser) {
    char *v = parser_get(section, option, nline, parser);
    if (v == NULL) {
        return -1;
    }

    if (raw != NULL) {
        *raw = v;
    }

    return sscanf(v, "%d", value);
}

/* Parse [section] option into value as an unsigned integer
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not an integer, -1 if the option is not defined
 */
int parser_getuint(const char *section, const char *option,
        unsigned int *value, char **raw, int *nline, parser_context_t *parser) {
    char *v = parser_get(section, option, nline, parser);
    if (v == NULL) {
        return -1;
    }

    if (raw != NULL) {
        *raw = v;
    }

    return sscanf(v, "%u", value);
}

/* Parse [section] option into value as a boolean
 * if nline is not NULL, copy the line number into nline
 * if raw is not NULL, copy the string value pointer into *raw
 * Return 1 in case of success, 0 if the option is not valid, -1 if the option is not defined
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

    if (strncasecmp(v, "yes", CONF_VALUE_LENGTH) == 0 || strncasecmp(v, "1",
            CONF_VALUE_LENGTH) == 0
            || strncasecmp(v, "true", CONF_VALUE_LENGTH) == 0 || strncasecmp(v,
            "on", CONF_VALUE_LENGTH) == 0) {
        *value = 1;
        return 1;
    }
    if (strncasecmp(v, "no", CONF_VALUE_LENGTH) == 0 || strncasecmp(v, "0",
            CONF_VALUE_LENGTH) == 0 || strncasecmp(v, "false",
            CONF_VALUE_LENGTH) == 0 || strncasecmp(v, "off", CONF_VALUE_LENGTH)
            == 0) {
        *value = 0;
        return 1;
    }

    return 0;
}

/* internal function used to free an item_value_t */
static void free_value(void *p) {
    free(p);
}

/* internal function used to free an item_section_t
 * It will destroy its options.
 */
static void free_section(void *p) {
    item_section_t *section = (item_section_t *) p;
    if (section->values_tree != NULL) {
        tdestroy(section->values_tree, free_value);
    }
    free(p);
}

/* Clean the parser */
void parser_free(parser_context_t *parser) {
    tdestroy(parser->data, free_section);
    parser->data = NULL;
}


/* Initialize a new parser */
void parser_init(parser_context_t *parser, int allow_default_section,
        int allow_empty_value) {
    parser->data = NULL;
    parser->allow_default = allow_default_section;
    parser->allow_empty_value = allow_empty_value;
}

/* Remove a [section] option
 * or do nothing if it doesn't exist
 */
void parser_remove_option(const char *section, const char *option,
        parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;
    item_value_t *item_value;

    tmp = tfind(section, &parser->data, parser_compare);
    if (tmp == NULL)
        return;

    item_section = *(void **) tmp;

    tmp = tfind(option, &item_section->values_tree, parser_compare);

    if (tmp != NULL) {
        item_value = *(void **) tmp;
        tdelete(option, &item_section->values_tree, parser_compare);
        free_value(item_value);
    }
}

/* Remove a whole section and its options
 * or do nothing if it doesn't exist
 */
void parser_remove_section(const char *section, parser_context_t *parser) {
    void *tmp;
    item_section_t *item_section;

    tmp = tfind(section, &parser->data, parser_compare);
    if (tmp == NULL)
        return;
    item_section = *(void **) tmp;
    tdelete(section, &parser->data, parser_compare);
    free_section(item_section);
}



/* value expansion, 2 passes
 *
 * if pass == 1
 * expand characters \# \; \t \n \r \\
 * remove comments # and ;
 *
 * if pass == 2
 * expand \"
 */
static int expand_line(char *line, int pass) {
    const char *src = line;
    char *dst = line;

    int escaped = 0;

    for (;;src++) {
        if (!escaped) {
            if (*src == '\\')
                escaped = 1;
            else if (pass == 1 && (*src == '#' || *src == ';')) {
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
            if (pass == 1) {
                switch (*src) {
                    case '#': *dst = '#'; break;
                    case ';': *dst = ';'; break;
                    case '\\': *dst = '\\'; break;
                    case 't': *dst = '\t'; break;
                    case 'n': *dst = '\n'; break;
                    case 'r': *dst = '\r'; break;
                    default :
                        *dst = '\\';
                        dst++;
                        *dst = *src;
                        break;
                }
            }
            else if (pass == 2) {
                switch (*src) {
                    case '"': *dst = '"'; break;
                    default :
                        *dst = '\\';
                        dst++;
                        *dst = *src;
                        break;
                }
            }
            dst++;
            escaped = 0;
        }

        if (*src == '\0')
            break;
    }

    return (dst - line);
}

/* Parse a file */
void parser_read(const char *confFile, parser_context_t *parser) {
    FILE *conf = fopen(confFile, "r");
    if (conf == NULL) {
        log_error(confFile);
        exit(EXIT_FAILURE);
    }

    char * line = NULL; // last read line
    size_t line_len = 0; // length of the line buffer
    ssize_t r; // length of the line
    char *token; // word
    char name[CONF_NAME_LENGTH]; // option name
    char value[CONF_VALUE_LENGTH]; // option value
    char section[CONF_NAME_LENGTH]; // current section

    //    int res;
    char *token_end;
    char *line_eq;
    char *eol;
    int nline = 0;
    section[0] = '\0';

    /* Read the configuration file */
    while ((r = getline(&line, &line_len, conf)) != -1) {
        nline++;

        // end of line
        eol = strstr(line, "\r\n");
        if (eol == NULL)
            eol = index(line, '\n');
        if (eol != NULL) {
            *eol = '\0';
        }

        // expands everything but \" and removes comments
        r = expand_line(line, 1);

        token = line;
        // remove leading spaces:
        while (*token == ' ' || *token == '\t')
            token++;
        // empty line:
        if (*token == '\0')
            continue;

        // section ?
        if (token[0] == '[') {
            token_end = index(token, ']');
            if (token_end == NULL || token_end == token + 1) {
                log_message("[%s:%d] Syntax error, empty section", confFile,
                        nline);
                continue;
            }
            *token_end = '\0';
            strncpy(section, token + 1, CONF_NAME_LENGTH);

            continue;
        }

        // something outside of a section ?
        if (*section == '\0') {
            if (parser->allow_default)
                strncpy(section, "DEFAULT", CONF_NAME_LENGTH);
            else {
                log_message(
                        "[%s:%d] Syntax error, option outside of a section",
                        confFile, nline);
                continue;
            }
        }

        parser_add_section(section, parser);

        // find first equal sign:
        line_eq = index(token, '=');
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
        strncpy(name, token, CONF_NAME_LENGTH);

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

        // quotted value, remove quotes
        if (*token == '"' && *(token_end - 1) == '"') {
            token++;
            token_end--;
            *token_end = '\0';
        }
        if (!parser->allow_empty_value && token_end == token) {
            log_message("[%s:%d] Syntax error, empty value", confFile, nline);
            continue;
        }

        // expands \"
        expand_line(token, 2);


        strncpy(value, token, CONF_VALUE_LENGTH);

        if (config.debug) {
            printf("[%s:%d] [%s] '%s' = '%s'\n", confFile, nline, section,
                    name, value);
        }

        parser_set(section, name, value, nline, parser);
    }

    if (line) {
        free(line);
    }

    fclose(conf);
}

/* internal, write an option and it's value into parser->dump_file */
static void parser_write_option(const void *nodep, const VISIT which, const int depth) {
    item_value_t *item;
    switch (which) {
        case postorder:
        case leaf:
            item = *(item_value_t **) nodep;
            fprintf(item->section->parser->dump_file, "%s = %s\n", item->name, item->value);
            break;
        default:
            break;
    }
}

/* internal, write a complete section into parser->dump_file */
static void parser_write_section(const void *nodep, const VISIT which, const int depth) {
    item_section_t *item;
    switch (which) {
        case postorder:
        case leaf:
            item = *(item_section_t **) nodep;
            fprintf(item->parser->dump_file, "[%s]\n", item->name);
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



static void parser_forall_option(const void *nodep, const VISIT which, const int depth) {
    item_value_t *item;
    switch (which) {
        case postorder:
        case leaf:
            item = *(item_value_t **) nodep;
            item->section->parser->forall_function(item->section->name, item->name, item->value, item->nline);
            break;
        default:
            break;
    }
}

static void parser_forall_section(const void *nodep, const VISIT which, const int depth) {
    item_section_t *item;
    switch (which) {
        case postorder:
        case leaf:
            item = *(item_section_t **) nodep;
            twalk(item->values_tree, parser_forall_option);
            break;
        default:
            break;
    }
}

/* execute f on each option */
void parser_forall(parser_action_t f, parser_context_t *parser) {
    parser->forall_function = f;
    twalk(parser->data, parser_forall_section);
}
