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
#include <ctype.h>
#include <string.h>

#include "config_parser.h"
#include "log.h"

/* The position in the file */
struct position {
    size_t line; // line
    size_t c; // column
};
typedef struct position position_t;

/*
 * Reading structure with a 1 byte readahead
 * The readahead is used to transform \r\n into \n while reading
 * and to handle the octal and hexadecimal escape sequences
 */
struct read_buf {
    int next; //next char
    position_t *pos; // Current position
    FILE *f; // stream
};
typedef struct read_buf read_buf_t;

static void read_buf_init(read_buf_t *rb, position_t *pos, FILE *f);
static inline int read_buf_next(read_buf_t *rb);
static inline int read_buf_peek(read_buf_t *rb);

static void position_init(position_t *pos);
static inline void position_inc_line(position_t *pos);
static inline void position_inc_char(position_t *pos);

void read_buf_init(read_buf_t *rb, position_t *pos, FILE *f) {
    rb->f = f;
    rb->pos = pos;
    rb->next = fgetc(f);
}

/*
 * Read the next char and convert \r\n sequences to \n
 * Update the position structure
 */
int read_buf_next(read_buf_t *rb) {
    int c;
    c = rb->next;
    if (c != EOF) {
        rb->next = fgetc(rb->f);
        if (c == '\r' && rb->next == '\n') {
            rb->next = fgetc(rb->f);
            c = '\n';
        }
        if (c == '\n')
            position_inc_line(rb->pos);
        else
            position_inc_char(rb->pos);
    }
    return c;
}

/*
 * Get the next character on the stream without removing that char
 */
int read_buf_peek(read_buf_t *rb) {
    return rb->next;
}

void position_init(position_t *pos) {
    pos->line = 1;
    pos->c = 1;
}

void position_inc_line(position_t *pos) {
    pos->line++;
    pos->c = 1;
}

void position_inc_char(position_t *pos) {
    pos->c++;
}

/*
 * Convert an hexadecimal digit to its int value.
 * Assume that isxdigit(c) == true
 */
static inline int hextoint(int c) {
    if (isdigit(c))
        return c - '0';
    else if (islower(c))
        return c - 'a' + 10;
    else
        return c - 'A' + 10;
}

/* checks for octal digits */
static inline int isoctdigit(int c) {
    return c >= '0' && c <= '7';
}

/*
 * Convert an octal digit to its int value.
 * Assume that isoctdigit(c) == true
 */
static inline int octtoint(int c) {
    return c - '0';
}

/*
 * Interprete an escaped char c, write the output into sb.
 * Support the C escape sequences and \[ \] \; \# \=
 */
static inline void char_unescape(strlib_buf_t *sb, int c, read_buf_t *rb) {
    int v;
    switch (c) {
        case '\\':
        case '\"':
        case '\'':
        case '?':
            strlib_push(sb, (char) c);
            break;
        case 'n':
            strlib_push(sb, '\n');
            break;
        case 'r':
            strlib_push(sb, '\r');
            break;
        case 'b':
            strlib_push(sb, '\b');
            break;
        case 't':
            strlib_push(sb, '\t');
            break;
        case 'f':
            strlib_push(sb, '\f');
            break;
        case 'a':
            strlib_push(sb, '\a');
            break;
        case 'v':
            strlib_push(sb, '\v');
            break;
        case 'x':
            // C hexadecimal value, read up to 2 digits
            c = read_buf_peek(rb);
            if (isxdigit(c)) {
                v = hextoint(c);
                read_buf_next(rb);
                c = read_buf_peek(rb);
                if (isxdigit(c)) {
                    v <<= 4;
                    v += hextoint(c);
                    read_buf_next(rb);
                }
                strlib_push(sb, (char) v);
            }
            else {
                strlib_push(sb, '\\');
                strlib_push(sb, 'x');
                break;
            }
            break;
        case '#':
        case ';':
        case '[':
        case ']':
        case '=':
            strlib_push(sb, (char) c);
            break;
        default:
            // C octal value, read up to 3 digits
            if (isoctdigit(c)) {
                v = octtoint(c);
                c = read_buf_peek(rb);
                if (isoctdigit(c)) {
                    read_buf_next(rb);
                    v <<= 3;
                    v += octtoint(c);
                    c = read_buf_peek(rb);
                    if (isoctdigit(c)) {
                        read_buf_next(rb);
                        v <<= 3;
                        v += octtoint(c);
                    }
                }
                strlib_push(sb, (char) v);
            }
            else {
                strlib_push(sb, '\\');
                strlib_push(sb, (char) c);
            }
            break;
    }
}

/*
 * Write c into sb, escaping the char if necessary
 */
static inline void char_escape(strlib_buf_t *sb, char c) {
    switch (c) {
        case '\\':
        case '\"':
            //        case '\'':
            //        case '?':
            strlib_appendf(sb, "\\%c", c);
            break;
        case '\n':
            strlib_append(sb, "\\n", 2);
            break;
        case '\r':
            strlib_append(sb, "\\r", 2);
            break;
        case '\b':
            strlib_append(sb, "\\b", 2);
            break;
        case '\t':
            strlib_append(sb, "\\t", 2);
            break;
        case '\f':
            strlib_append(sb, "\\f", 2);
            break;
        case '\a':
            strlib_append(sb, "\\a", 2);
            break;
        case '\v':
            strlib_append(sb, "\\v", 2);
            break;
        case '#':
        case ';':
        case '[':
        case ']':
        case '=':
            strlib_appendf(sb, "\\%c", c);
            break;
        default:
            strlib_push(sb, c);
            break;
    }
}

/*
 * Read the next identifier (section or key).
 * Return -1 on error, 0 on EOF, 1 on section, 2 on key
 * The identifier is copied into sb
 * The position in the stream is updated
 * When reading a section name, stop reading after the end of the line
 * When reading a key name, stop reading after the = char. The value must be
 * retrieved with nparser_get_next_value
 */
static int nparser_get_next_idt(read_buf_t *rb, const char *name,
        position_t *pos, strlib_buf_t *sb) {
    int r = 0;
    enum idt_states {
        ST_IDT_START, // Start reading
        ST_IDT_BEGIN_SECTION, // After a [
        ST_IDT_READ_SECTION, // Reading a section name
        ST_IDT_READ_SECTION_ESC, // Escaped char in a section name
        ST_IDT_AFTER_SECTION, // After a ]
        ST_IDT_READ_NAME, // Reading a key name
        ST_IDT_READ_NAME_ESC, // Escaped char in a key name
        ST_IDT_COMMENT, // The line contains a comment
        ST_IDT_AFTER_COMMENT, // Comment after an identifier
        ST_IDT_SKIP, // Skip until the end of the line
        ST_IDT_END // Stop reading
    } state = ST_IDT_START;
    int c;
    int eol, eof, comment;

    strlib_reset(sb);

    while (state != ST_IDT_END) {
        c = read_buf_next(rb);

        eof = (c == EOF);
        eol = (c == '\n');
        comment = (c == '#' || c == ';');

        switch (state) {
            case ST_IDT_START:
                if (eof)
                    state = ST_IDT_END;
                else if (c == '[')
                    state = ST_IDT_BEGIN_SECTION;
                else if (comment) {
                    state = ST_IDT_COMMENT;
                }
                else if (c == ']') {
                    log_message(
                            "[%s:%zu:%zu] Syntax error: section declaration",
                            name, pos->line, pos->c);
                    state = ST_IDT_SKIP;
                    r = -1;
                }
                else if (c == '=') {
                    log_message("[%s:%zu:%zu] Syntax error: empty name", name,
                            pos->line, pos->c);
                    state = ST_IDT_SKIP;
                    r = -1;
                }
                else if (!isblank(c) && !eol) {
                    strlib_push(sb, (char) c);
                    state = ST_IDT_READ_NAME;
                }
                break;
            case ST_IDT_BEGIN_SECTION:
                if (eol || eof) {
                    log_message(
                            "[%s:%zu:%zu] Syntax error: section declaration",
                            name, pos->line, pos->c);
                    state = ST_IDT_END;
                    r = -1;
                }
                else if (comment) {
                    log_message(
                            "[%s:%zu:%zu] Syntax error: section declaration",
                            name, pos->line, pos->c);
                    state = ST_IDT_SKIP;
                    r = -1;
                }
                else if (c == ']') {
                    log_message("[%s:%zu:%zu] Syntax error: empty section",
                            name, pos->line, pos->c);
                    state = ST_IDT_SKIP;
                    r = -1;
                }
                else if (!isblank(c)) {
                    strlib_push(sb, (char) c);
                    state = ST_IDT_READ_SECTION;
                }
                break;
            case ST_IDT_READ_SECTION:
                if (eol || eof) {
                    log_message(
                            "[%s:%zu:%zu] Syntax error: section declaration",
                            name, pos->line, pos->c);
                    state = ST_IDT_END;
                    r = -1;
                }
                else if (c == '\\') {
                    state = ST_IDT_READ_SECTION_ESC;
                }
                else if (comment) {
                    log_message(
                            "[%s:%zu:%zu] Syntax error: section declaration",
                            name, pos->line, pos->c);
                    state = ST_IDT_SKIP;
                    r = -1;
                }
                else if (c == ']') {
                    state = ST_IDT_AFTER_SECTION;
                    r = 1;
                }
                else {
                    strlib_push(sb, (char) c);
                }
                break;
            case ST_IDT_READ_SECTION_ESC:
                if (eol || eof) {
                    log_message(
                            "[%s:%zu:%zu] Syntax error: line continuation not allowed in identifiers",
                            name, pos->line, pos->c);
                    state = ST_IDT_END;
                    r = -1;
                }
                else {
                    char_unescape(sb, c, rb);
                    state = ST_IDT_READ_SECTION;
                }
                break;
            case ST_IDT_AFTER_SECTION:
                if (eol || eof) {
                    state = ST_IDT_END;
                }
                else if (comment) {
                    state = ST_IDT_AFTER_COMMENT;
                }
                else if (!isblank(c)) {
                    log_message(
                            "[%s:%zu:%zu] Syntax error: ignoring the text after the declaration",
                            name, pos->line, pos->c);
                    state = ST_IDT_SKIP;
                }
                break;
            case ST_IDT_READ_NAME:
                if (eol || eof) {
                    log_message("[%s:%zu:%zu] Syntax error: no value", name,
                            pos->line, pos->c);
                    state = ST_IDT_END;
                    r = -1;
                }
                else if (c == '\\') {
                    state = ST_IDT_READ_NAME_ESC;
                }
                else if (comment) {
                    log_message("[%s:%zu:%zu] Syntax error: no value", name,
                            pos->line, pos->c);
                    state = ST_IDT_SKIP;
                    r = -1;
                }
                else if (c == '=') {
                    state = ST_IDT_END;
                    r = 2;
                }
                else {
                    strlib_push(sb, (char) c);
                }
                break;
            case ST_IDT_READ_NAME_ESC:
                if (eol || eof) {
                    log_message(
                            "[%s:%zu:%zu] Syntax error: line continuation not allowed in identifiers",
                            name, pos->line, pos->c);
                    state = ST_IDT_END;
                    r = -1;
                }
                else {
                    char_unescape(sb, c, rb);
                    state = ST_IDT_READ_NAME;
                }
                break;
            case ST_IDT_COMMENT:
                if (eol) {
                    state = ST_IDT_START;
                }
                else if (eof) {
                    state = ST_IDT_END;
                }
                break;
            case ST_IDT_AFTER_COMMENT:
                if (eol || eof) {
                    state = ST_IDT_END;
                }
                break;
            case ST_IDT_SKIP:
                if (eol || eof) {
                    state = ST_IDT_END;
                }
                break;
            case ST_IDT_END:
                break;
        }
    }
    if (r == 1 || r == 2) {
        strlib_rstrip(sb);
    }

    return r;
}

/*
 * Read the next value
 * Return -1 on error (EOF), 0 on success
 * The value is copied into sb and the position in the stream is updated
 * The parser stops after the final end of line.
 */
static int nparser_get_next_value(read_buf_t *rb, const char *name,
        position_t *pos, strlib_buf_t *sb) {
    enum val_states {
        ST_VAL_START, // Start reading
        ST_VAL_IN_QUOTE, // Read a quoted value
        ST_VAL_IN_QUOTE_ESC, // Escaped char in a quoted value
        ST_VAL_IN_UNQUOTED, // Read an unquoted value
        ST_VAL_IN_UNQUOTED_ESC, // Escaped char in an unquoted value
        ST_VAL_COMMENT, // Read a comment
        ST_VAL_END // Stop reading
    } state = ST_VAL_START;
    int c;
    int r = 0;
    int eol, eof, comment;

    strlib_reset(sb);

    while (state != ST_VAL_END) {
        c = read_buf_next(rb);

        eof = (c == EOF);
        eol = (c == '\n');
        comment = (c == '#' || c == ';');

        switch (state) {
            case ST_VAL_START:
                if (eol || eof) {
                    state = ST_VAL_END;
                }
                else if (comment) {
                    state = ST_VAL_COMMENT;
                }
                else if (isblank(c)) {
                }
                else if (c == '\"') {
                    state = ST_VAL_IN_QUOTE;
                }
                else if (c == '\\') {
                    state = ST_VAL_IN_UNQUOTED_ESC;
                }
                else {
                    state = ST_VAL_IN_UNQUOTED;
                    strlib_push(sb, (char) c);
                }
                break;
            case ST_VAL_IN_QUOTE:
                if (eof) {
                    state = ST_VAL_END;
                    r = -1;
                }
                else if (eol) {
                    strlib_push(sb, '\n');
                }
                else if (c == '\"') {
                    strlib_setmark(sb, sb->len);
                    state = ST_VAL_START;
                }
                else if (c == '\\') {
                    state = ST_VAL_IN_QUOTE_ESC;
                }
                else {
                    strlib_push(sb, (char) c);
                }
                break;
            case ST_VAL_IN_QUOTE_ESC:
                if (eof) {
                    state = ST_VAL_END;
                    r = -1;
                }
                else {
                    if (!eol) {
                        char_unescape(sb, c, rb);
                    }
                    state = ST_VAL_IN_QUOTE;
                }
                break;
            case ST_VAL_IN_UNQUOTED:
                if (eol || eof) {
                    strlib_rstrip(sb);
                    state = ST_VAL_END;
                }
                else if (c == '\\') {
                    state = ST_VAL_IN_UNQUOTED_ESC;
                }
                else if (c == '\"') {
                    strlib_rstrip(sb);
                    state = ST_VAL_IN_QUOTE;
                }
                else {
                    strlib_push(sb, (char) c);
                }
                break;
            case ST_VAL_IN_UNQUOTED_ESC:
                if (eof) {
                    state = ST_VAL_END;
                    r = -1;
                }
                else if (eol) {
                    strlib_rstrip(sb);
                    state = ST_VAL_START;
                }
                else {
                    char_unescape(sb, c, rb);
                    state = ST_VAL_IN_UNQUOTED;
                }
                break;
            case ST_VAL_COMMENT:
                if (eol || eof) {
                    state = ST_VAL_END;
                }
                break;
            case ST_VAL_END:
                break;
        }
    }

    if (r == -1) {
        log_message("[%s:%zu:%zu] Syntax error: unexpected end of file", name,
                pos->line, pos->c);
    }
    return r;
}

/*
 * Read confFile and store the config into parser
 * debug: print the keys and values
 */
void parser_read(const char *confFile, parser_context_t *parser, int debug) {
    FILE *conf;
    position_t pos;
    read_buf_t rb;
    strlib_buf_t sb_idt, sb_val, sb_section;
    int r_idt, r_val;
    size_t line;

    conf = fopen(confFile, "r");
    if (conf == NULL) {
        log_error(errno, "%s", confFile);
        exit(EXIT_FAILURE);
    }

    position_init(&pos);
    read_buf_init(&rb, &pos, conf);
    strlib_init(&sb_idt);
    strlib_init(&sb_val);
    strlib_init(&sb_section);

    // Get the next identifier
    while ((r_idt = nparser_get_next_idt(&rb, confFile, &pos, &sb_idt)) != 0) {
        if (r_idt == 1) {
            // A section
            strlib_reset(&sb_section);
            strlib_appendbuf(&sb_section, &sb_idt);
        }
        else if (r_idt == 2) {
            // A key
            line = pos.line;
            // Get the value
            r_val = nparser_get_next_value(&rb, confFile, &pos, &sb_val);
            if (r_val == 0) {
                if (!parser->allow_empty_value && strlen(sb_val.s) == 0) {
                    log_message("[%s:%zu] Syntax error, empty value", confFile,
                            line);
                    continue;
                }
                else if (!parser->allow_default && sb_section.len == 0) {
                    log_message(
                            "[%s:%zu] Syntax error, option outside of a section",
                            confFile, line);
                    continue;
                }
                else if (parser->allow_default && sb_section.len == 0) {
                    strlib_append(&sb_section, SECTION_DEFAULT, strlen(
                            SECTION_DEFAULT));
                }
                if (debug) {
                    printf("[%s:%zu] [%s] '%s' = '%s'\n", confFile, line,
                            sb_section.s, sb_idt.s, sb_val.s);
                }
                parser_set(sb_section.s, sb_idt.s, sb_val.s, line, parser);
            }
        }
    }

    strlib_free(&sb_idt);
    strlib_free(&sb_val);
    strlib_free(&sb_section);

    fclose(conf);
}

/*
 * Write a valid configuration file with the content of the parser
 * If expanded is true, the values are expanded.
 */
void parser_write(FILE *f, parser_context_t *parser, int expanded) {
    item_section_t *section;
    item_key_t *key;
    item_value_t *value;
    strlib_buf_t sb_out;
    char *c;

    strlib_init(&sb_out);

    TAILQ_FOREACH(section, &parser->sections_list, tailq) {
        strlib_reset(&sb_out);
        strlib_append(&sb_out, "[ ", 2);
        for (c = section->name; *c != '\0'; c++)
            char_escape(&sb_out, *c);
        strlib_append(&sb_out, " ]\n", 3);

        if (fputs(sb_out.s, f) == EOF) {
            strlib_free(&sb_out);
            return;
        }

        TAILQ_FOREACH(key, &section->keys_list, tailq) {
            TAILQ_FOREACH(value, &key->values_list, tailq) {
                strlib_reset(&sb_out);
                strlib_append(&sb_out, "  ", 2);
                for (c = key->name; *c != '\0'; c++)
                    char_escape(&sb_out, *c);
                strlib_append(&sb_out, " = \"", 4);

                if (expanded)
                    c = parser_value_expand(section, value);
                else
                    c = value->s;
                for (; *c != '\0'; c++)
                    char_escape(&sb_out, *c);
                strlib_append(&sb_out, "\"\n", 2);

                if (fputs(sb_out.s, f) == EOF) {
                    strlib_free(&sb_out);
                    return;
                }
            }
        }

        if (fputs("\n", f) == EOF) {
            strlib_free(&sb_out);
            return;
        }
    }

    strlib_free(&sb_out);
}
