/*
 * Basic INI file parser
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

/*
 * This module handle INI configuration files.
 *
 * Syntax :
 * - Sections
 *   - sections are defined between square brackets : [section]
 *   - sections ends at the next section declaration or at EOF
 *   - section's names are case sensitive
 *   - whitespaces around the section's name are stripped
 *   - the name can contain whitespaces
 * - Options
 *   - the only valid syntax is : name = value
 *   - whitespaces around the option's name and around the value are stripped
 *   - options are case sensitive
 *   - options can contain whitespaces
 *   - options may optionally be defined before the first section mark. They will be put in a "DEFAULT" section.
 *   - options may optionally accept empty values
 * - Comments
 *   - comments start with a ';' or a '#' and continue to the end of the line
 * - Blank lines are allowed
 * - Values can be quoted between "
 * - Escaped characters
 *   - \t \n \r
 *   - \# for #
 *   - \; for ;
 *   - \\ for \
 *   - \" for "
 *   - '\ ' (space) for ' '
 * - No hierarchy within sections
 *
 * The parser context is stored in a parser_context_t value.
 * The context must be initialized with parser_init and is passed
 * to every functions.
 * The context is freed with parser_free.
 */

#ifndef CONFIG_PARSER_H_
#define CONFIG_PARSER_H_

#include "configuration.h"
#include <stdio.h>

typedef void (*parser_action_t)(const char*, const char*, const char*, int);

struct parser_context {
    int allow_default; // if true, options defined before the first section mark
                       // are put in a "DEFAULT" section
    int allow_empty_value; // allow option with empty value

    void *data; // tree
    FILE *dump_file; // tmp value
    parser_action_t forall_function; // tmp value
};
typedef struct parser_context parser_context_t;

#define __ITEM_COMMON \
    char *name

struct item_section {
    __ITEM_COMMON;
    void *values_tree;
    parser_context_t *parser; // pointer to its parser_context
};
typedef struct item_section item_section_t;

struct item_value {
    __ITEM_COMMON;
    char *value;
    int nline;
    item_section_t *section; // pointer to its section item
};
typedef struct item_value item_value_t;

struct item_common {
    __ITEM_COMMON;
};
typedef struct item_common item_common_t;

/* Initialize a parser_context_t structure */
extern void parser_init(parser_context_t *parser, int allow_default_section,
        int allow_empty_value);
/* Remove all the data associated with this context */
extern void parser_free(parser_context_t *parser);


/* Parse the file */
extern void parser_read(const char *file, parser_context_t *parser);
/* Set a default option. Create the section if it doesn't exist */
extern void parser_set(const char *section, const char *option,
        const char *value, int nline, parser_context_t *parser);
/* return the value of [section] option or NULL if it doesn't exist */
extern char *parser_get(const char *section, const char *option, int *nline,
        parser_context_t *parser);
/* return the value of [section] option as an integer into "value".
 * return 1: success  0: error */
extern int parser_getint(const char *section, const char *option, int *value,
        char **raw, int *nline, parser_context_t *parser);
/* return the value of [section] option as an unsigned integer into "value".
 * return 1: success  0: error */
extern int parser_getuint(const char *section, const char *option,
        unsigned int *value, char **raw, int *nline, parser_context_t *parser);
/* return the value of [section] option as boolean into "value".
 * return 1: success  0: error
 * valid values are "yes", "on", "1", "true", "no", "off", "0", "false"
 * and are case insensitive
 */
extern int parser_getboolean(const char *section, const char *option,
        int *value, char **raw, int *nline, parser_context_t *parser);


/* Add a new section. Do nothing if the section exists */
extern void parser_add_section(const char *section, parser_context_t *parser);
/* Indicates whether the section exist */
extern int parser_has_section(const char *section, parser_context_t *parser);
/* Indicates whether the option is defined */
extern int parser_has_option(const char *section, const char *option,
        parser_context_t *parser);
/* Remove an option */
extern void parser_remove_option(const char *section, const char *option,
        parser_context_t *parser);
/* Remove a complete section */
extern void
        parser_remove_section(const char *section, parser_context_t *parser);


/* Write this configuration into file. */
extern void parser_write(FILE *file, parser_context_t *parser);

/* Execute action(section, option, value) for all options in the parser. */
extern void parser_forall(parser_action_t, parser_context_t *parser);

#endif /* CONFIG_PARSER_H_ */
