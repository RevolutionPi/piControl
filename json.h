/*=======================================================================================
 *
 *	       KK    KK   UU    UU   NN    NN   BBBBBB    UU    UU    SSSSSS
 *	       KK   KK    UU    UU   NNN   NN   BB   BB   UU    UU   SS
 *	       KK  KK     UU    UU   NNNN  NN   BB   BB   UU    UU   SS
 *	+----- KKKKK      UU    UU   NN NN NN   BBBBB     UU    UU    SSSSS
 *	|      KK  KK     UU    UU   NN  NNNN   BB   BB   UU    UU        SS
 *	|      KK   KK    UU    UU   NN   NNN   BB   BB   UU    UU        SS
 *	|      KK    KKK   UUUUUU    NN    NN   BBBBBB     UUUUUU    SSSSSS     GmbH
 *	|
 *	|            [#]  I N D U S T R I A L   C O M M U N I C A T I O N
 *	|             |
 *	+-------------+
 *
 *---------------------------------------------------------------------------------------
 *
 * Copyright (C) 2009 Vincent Hanquez <vincent@snarc.org>
 * (C) KUNBUS GmbH, Heerweg 15C, 73770 Denkendorf, Germany
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License V2 as published by
 * the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  For licencing details see COPYING
 *
 *=======================================================================================
 */

#ifndef JSON_H
#define JSON_H

#include <linux/types.h>


#define JSON_MAJOR 	0
#define JSON_MINOR	7
#define JSON_VERSION	(JSON_MAJOR * 100 + JSON_MINOR)

typedef enum
{
    JSON_NONE,
    JSON_ARRAY_BEGIN,
    JSON_OBJECT_BEGIN,
    JSON_ARRAY_END,
    JSON_OBJECT_END,
    JSON_INT,
    JSON_FLOAT,
    JSON_STRING,
    JSON_KEY,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL,
} json_type;

typedef enum
{
    /* SUCCESS = 0 */
    /* running out of memory */
    JSON_ERROR_NO_MEMORY = 1,
    /* character < 32, except space newline tab */
    JSON_ERROR_BAD_CHAR,
    /* trying to pop more object/array than pushed on the stack */
    JSON_ERROR_POP_EMPTY,
    /* trying to pop wrong type of mode. popping array in object mode, vice versa */
    JSON_ERROR_POP_UNEXPECTED_MODE,
    /* reach nesting limit on stack */
    JSON_ERROR_NESTING_LIMIT,
    /* reach data limit on buffer */
    JSON_ERROR_DATA_LIMIT,
    /* comment are not allowed with current configuration */
    JSON_ERROR_COMMENT_NOT_ALLOWED,
    /* unexpected char in the current parser context */
    JSON_ERROR_UNEXPECTED_CHAR,
    /* unicode low surrogate missing after high surrogate */
    JSON_ERROR_UNICODE_MISSING_LOW_SURROGATE,
    /* unicode low surrogate missing without previous high surrogate */
    JSON_ERROR_UNICODE_UNEXPECTED_LOW_SURROGATE,
    /* found a comma not in structure (array/object) */
    JSON_ERROR_COMMA_OUT_OF_STRUCTURE,
    /* callback returns error */
    JSON_ERROR_CALLBACK,
} json_error;

#define LIBJSON_DEFAULT_STACK_SIZE 256
#define LIBJSON_DEFAULT_BUFFER_SIZE 4096

typedef int (*json_parser_callback)(void *userdata, int type, const char *data, uint32_t length);

typedef struct {
    uint32_t buffer_initial_size;
    uint32_t max_nesting;
    uint32_t max_data;
    int allow_c_comments;
    int allow_yaml_comments;
    void * (*user_calloc)(size_t nmemb, size_t size);
    void * (*user_realloc)(void *ptr, size_t size);
} json_config;

typedef struct json_parser {
    json_config config;

    /* SAJ callback */
    json_parser_callback callback;
    void *userdata;

    /* parser state */
    uint8_t state;
    uint8_t save_state;
    uint8_t expecting_key;
    uint16_t unicode_multi;
    json_type type;

    /* state stack */
    uint8_t *stack;
    uint32_t stack_offset;
    uint32_t stack_size;

    /* parse buffer */
    char *buffer;
    uint32_t buffer_size;
    uint32_t buffer_offset;
} json_parser;


/** json_parser_init initialize a parser structure taking a config,
 * a config and its userdata.
 * return JSON_ERROR_NO_MEMORY if memory allocation failed or SUCCESS.  */
int json_parser_init(json_parser *parser, json_config *cfg,
		     json_parser_callback callback, void *userdata);

/** json_parser_free freed memory structure allocated by the parser */
int json_parser_free(json_parser *parser);

/** json_parser_string append a string s with a specific length to the parser
 * return 0 if everything went ok, a JSON_ERROR_* otherwise.
 * the user can supplied a valid processed pointer that will
 * be fill with the number of processed characters before returning */
int json_parser_string(json_parser *parser, const char *string,
		       uint32_t length, uint32_t *processed);

/** json_parser_char append one single char to the parser
 * return 0 if everything went ok, a JSON_ERROR_* otherwise */
int json_parser_char(json_parser *parser, unsigned char next_char);

/** json_parser_is_done return 0 is the parser isn't in a finish state. !0 if it is */
int json_parser_is_done(json_parser *parser);


/** callback from the parser_dom callback to create object and array */
typedef void * (*json_parser_dom_create_structure)(int);

/** callback from the parser_dom callback to create data values */
typedef void * (*json_parser_dom_create_data)(int, const char *, uint32_t);

/** callback from the parser helper callback to append a value to an object or array value
 * append(parent, key, key_length, val); */
typedef int (*json_parser_dom_append)(void *, char *, uint32_t, void *);

/** the json_parser_dom permits to create a DOM like tree easily through the
 * use of 3 callbacks where the user can choose the representation of the JSON values */
typedef struct json_parser_dom
{
    /* object stack */
    struct stack_elem { void *val; char *key; uint32_t key_length; } *stack;
    uint32_t stack_size;
    uint32_t stack_offset;

    /* overridable memory allocator */
    void * (*user_calloc)(size_t nmemb, size_t size);
    void * (*user_realloc)(void *ptr, size_t size);

    /* returned root structure (object or array) */
    void *root_structure;

    /* callbacks */
    json_parser_dom_create_structure create_structure;
    json_parser_dom_create_data create_data;
    json_parser_dom_append append;
} json_parser_dom;

/** initialize a parser dom structure with the necessary callbacks */
int json_parser_dom_init(json_parser_dom *helper,
			 json_parser_dom_create_structure create_structure,
			 json_parser_dom_create_data create_data,
			 json_parser_dom_append append);
/** free memory allocated by the DOM callback helper */
int json_parser_dom_free(json_parser_dom *ctx);

/** helper to parser callback that arrange parsing events into comprehensive JSON data structure */
int json_parser_dom_callback(void *userdata, int type, const char *data, uint32_t length);

#endif /* JSON_H */
