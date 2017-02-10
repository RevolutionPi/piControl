/*
 * Copyright (C) 2009 Vincent Hanquez <vincent@snarc.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 or version 3.0 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <project.h>
#include <common_define.h>

#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/slab.h>    // included for KERN_INFO
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/segment.h>

#include "json.h"
#include <piControl.h>
#include <piConfig.h>
#include <piDIOComm.h>

#define TOKEN_DEVICES       "Devices"
#define TOKEN_TYPE          "productType"
#define TOKEN_POSITION      "position"
#define TOKEN_INPUT         "inp"
#define TOKEN_OUTPUT        "out"
#define TOKEN_MEMORY        "mem"
#define TOKEN_CONFIG        "config"
#define TOKEN_OFFSET        "offset"

struct json_val_elem {
    char *key;
    uint32_t key_length;
    struct json_val *val;
};

typedef struct json_val {
    int type;
    int length;
    union {
        char *data;
        struct json_val **array;
        struct json_val_elem **object;
    } u;
} json_val_t;



char *indent_string = NULL;

char *string_of_errors[] =
{
    [JSON_ERROR_NO_MEMORY] = "out of memory",
    [JSON_ERROR_BAD_CHAR] = "bad character",
    [JSON_ERROR_POP_EMPTY] = "stack empty",
    [JSON_ERROR_POP_UNEXPECTED_MODE] = "pop unexpected mode",
    [JSON_ERROR_NESTING_LIMIT] = "nesting limit",
    [JSON_ERROR_DATA_LIMIT] = "data limit",
    [JSON_ERROR_COMMENT_NOT_ALLOWED] = "comment not allowed by config",
    [JSON_ERROR_UNEXPECTED_CHAR] = "unexpected char",
    [JSON_ERROR_UNICODE_MISSING_LOW_SURROGATE] = "missing unicode low surrogate",
    [JSON_ERROR_UNICODE_UNEXPECTED_LOW_SURROGATE] = "unexpected unicode low surrogate",
    [JSON_ERROR_COMMA_OUT_OF_STRUCTURE] = "error comma out of structure",
    [JSON_ERROR_CALLBACK] = "error in a callback"
};


struct file *open_filename(const char *filename, int flags)
{
    struct file *input;
    input = filp_open(filename, flags, 0);

#ifdef DEBUG_CONFIG
    DF_PRINTK("filp_open %s %x  %d %x\n", filename, (int)filename, (int)input, (int)input);
#endif

    if (IS_ERR(input))
    {
        DF_PRINTK("error: cannot open file %s\n", filename);
        return NULL;
    }
    return input;
}

void close_filename(struct file *file)
{
    filp_close(file, NULL);
}

int process_file(json_parser *parser, struct file *input, int *retlines, int *retcols)
{
    int ret = 0;
    int32_t read;
    uint32_t lines, col, i;
    char *buffer;
    mm_segment_t oldfs;

    buffer = kmalloc(4096, GFP_KERNEL);
    if (buffer == NULL)
    {
        DF_PRINTK("process file: out of memory\n");
        return -1;
    }

    oldfs = get_fs();
    set_fs (KERNEL_DS);

    lines = 1;
    col = 0;
    while (1)
    {
        uint32_t processed;
        read = vfs_read(input, buffer, 4096, &input->f_pos);
        if (read <= 0)
        {
            DF_PRINTK("vfs_read returned %d: %x, %ld\n", read, (int)input, (long int)input->f_pos);
            break;
        }
        ret = json_parser_string(parser, buffer, read, &processed);
        for (i = 0; i < processed; i++) {
            if (buffer[i] == '\n') { col = 0; lines++; }
            else col++;
        }
        if (ret)
            break;
    }
    if (retlines) *retlines = lines;
    if (retcols) *retcols = col;
    set_fs(oldfs);
    kfree(buffer);
    return ret;
}

static void *tree_create_structure(int is_object)
{
    json_val_t *v = kmalloc(sizeof(json_val_t), GFP_KERNEL);
    if (v) {
        /* instead of defining a new enum type, we abuse the
         * meaning of the json enum type for array and object */
        if (is_object) 
        {
            v->type = JSON_OBJECT_BEGIN;
            v->u.object = NULL;
        }
        else 
        {
            v->type = JSON_ARRAY_BEGIN;
            v->u.array = NULL;
        } 
        
        v->length = 0;
    }
    return v;
}

static char *memalloc_copy_length(const char *src, uint32_t n)
{
    char *dest;

    dest = kmalloc((n + 1) * sizeof(char), GFP_KERNEL);
    if (dest)
    {
        memcpy(dest, src, n);
        dest[n] = 0;
    }
    return dest;
}

static void *tree_create_data(int type, const char *data, uint32_t length)
{
    json_val_t *v;

    v = kmalloc(sizeof(json_val_t), GFP_KERNEL);
    if (v) {
        v->type = type;
        v->length = length;
        v->u.data = memalloc_copy_length(data, length);
        if (!v->u.data) {
            kfree(v);
            return NULL;
        }
    }
    return v;
}

static int tree_append(void *structure, char *key, uint32_t key_length, void *obj)
{
    json_val_t *parent = structure;
    if (key) {
        struct json_val_elem *objelem;

        if (parent->length == 0) {
            parent->u.object = kmalloc((1 + 1) * sizeof(json_val_t *), GFP_KERNEL); /* +1 for null */
            if (!parent->u.object)
                return 1;
            memset(parent->u.object, 0, 2*sizeof(json_val_t *));
        }
        else {
            uint32_t newsize = parent->length + 1 + 1; /* +1 for null */
            void *newptr;

            newptr = krealloc(parent->u.object, newsize * sizeof(json_val_t *), GFP_KERNEL);
            if (!newptr)
                return -1;
            parent->u.object = newptr;
        }

        objelem = kmalloc(sizeof(struct json_val_elem), GFP_KERNEL);
        if (!objelem)
            return -1;

        objelem->key = memalloc_copy_length(key, key_length);
        objelem->key_length = key_length;
        objelem->val = obj;
        parent->u.object[parent->length++] = objelem;
        parent->u.object[parent->length] = NULL;
    }
    else {
        if (parent->length == 0) {
            parent->u.array = kmalloc((1 + 1) * sizeof(json_val_t *), GFP_KERNEL); /* +1 for null */
            if (!parent->u.array)
                return 1;
            memset(parent->u.array, 0, (1 + 1) * sizeof(json_val_t *));
        }
        else {
            uint32_t newsize = parent->length + 1 + 1; /* +1 for null */
            void *newptr;

            newptr = krealloc(parent->u.object, newsize * sizeof(json_val_t *), GFP_KERNEL);
            if (!newptr)
                return -1;
            parent->u.array = newptr;
        }
        parent->u.array[parent->length++] = obj;
        parent->u.array[parent->length] = NULL;
    }
    return 0;
}

static int do_tree(json_config *config, const char *filename, json_val_t **root_structure)
{
    struct file *input;
    json_parser parser;
    json_parser_dom dom;
    int ret;
    int col, lines;

    input = open_filename(filename, O_RDONLY);
    if (!input)
        return 2;

    ret = json_parser_dom_init(&dom, tree_create_structure, tree_create_data, tree_append);
    if (ret)
    {
        DF_PRINTK("error: initializing helper failed: [code=%d] %s\n", ret, string_of_errors[ret]);
        close_filename(input);
        return ret;
    }

    ret = json_parser_init(&parser, config, json_parser_dom_callback, &dom);
    if (ret)
    {
        DF_PRINTK("error: initializing parser failed: [code=%d] %s\n", ret, string_of_errors[ret]);
        close_filename(input);
        return ret;
    }

    ret = process_file(&parser, input, &lines, &col);
    if (ret)
    {
        DF_PRINTK("line %d, col %d: [code=%d] %s\n",
            lines,
            col,
            ret,
            string_of_errors[ret]);
        close_filename(input);
        return 1;
    }

    ret = json_parser_is_done(&parser);
    if (!ret)
    {
        DF_PRINTK("syntax error: offset %d  state %d\n", parser.stack_offset, parser.state);
        close_filename(input);
        return 1;
    }

    if (root_structure)
        *root_structure = dom.root_structure;

    /* cleanup */
    json_parser_free(&parser);
    json_parser_dom_free(&dom);
    
    close_filename(input);
    return 0;
}

static int free_tree(json_val_t *element)
{
    int i;
    if (!element) {
        DF_PRINTK("error: no element in print tree\n");
        return -1;
    }
    
    switch (element->type) {
    case JSON_OBJECT_BEGIN:
        for (i = 0; i < element->length; i++) {
            free_tree(element->u.object[i]->val);
            kfree(element->u.object[i]->key);
            kfree(element->u.object[i]);
        }
        kfree(element->u.object);
        break;
    case JSON_ARRAY_BEGIN:
        for (i = 0; i < element->length; i++) {
            free_tree(element->u.array[i]);
        }
        kfree(element->u.array);
        break;
    case JSON_FALSE:
    case JSON_TRUE:
    case JSON_NULL:
    case JSON_INT:
    case JSON_STRING:
    case JSON_FLOAT:
        kfree(element->u.data);
        break;
    default:
        break;
    }
    
    kfree(element);
    
    return 0;
}

#if 0
static void appendEntry(piEntry **first, piEntry *p)
{
    while (*first != NULL)
        first = &(*first)->next;
    
    *first = p;
}

static piDevice *extract_devices(json_val_t *element, int lvl)
{
    int i;
    piDevice *ret, *p, *last;
    
    ret = NULL;
    
    if (!element) 
    {
        DF_PRINTK("error: no element in print tree\n");
        return ret;
    }
    
    switch (element->type) {
    case JSON_OBJECT_BEGIN:
        for (i = 0; i < element->length; i++) 
        {
            if (lvl == 1 && strcmp(element->u.object[i]->key, TOKEN_DEVICES) == 0)
            {   // we found the devices list -> increase lvl
                if (ret != NULL)
                {
                    DF_PRINTK("error: there should by only one '%s' element\n", element->u.object[i]->key);
                    return NULL;
                }
                ret = extract_devices(element->u.object[i]->val, 100);
            }
            else if (lvl == 101)
            {   // we found a device, parse elements
                if (ret == NULL)
                {
                    ret = malloc(sizeof(piDevice));
                    memset(ret, 0, sizeof(piDevice));
                }
                if (strcmp(element->u.object[i]->key, TOKEN_TYPE) == 0)
                {
                    ret->dev.i16uModuleType = simple_strtoul(element->u.object[i]->val->u.data);
                }
                else if (strcmp(element->u.object[i]->key, TOKEN_POSITION) == 0)
                {
                    ret->dev.i8uAddress = simple_strtoul(element->u.object[i]->val->u.data);
                }
                else if (strcmp(element->u.object[i]->key, TOKEN_INPUT) == 0)
                {
                    int j;
                    int idx = 0;
                    piEntry *first, *p;
                    json_val_t *ent = element->u.object[i]->val;
                   
                    if (ent->type != JSON_OBJECT_BEGIN || ent->u.object == NULL)
                    {
                        DF_PRINTK("error: element '%s' must contain an Object\n", element->u.object[i]->key);
                        return NULL;
                    }
                    first = NULL;
                    for (j = 0; j < ent->length; j++)
                    {
                        if (ent->u.object[j]->val->type == JSON_ARRAY_BEGIN)
                        {
                            struct json_val **array = ent->u.object[j]->val->u.array;
                            p = malloc(sizeof(piEntry));
                            memset(p, 0, sizeof(piEntry));
                                
                            p->ent.i8uAddress = ret->dev.i8uAddress;
                            p->ent.i8uType = 1; // input
                            p->ent.i16uIndex = idx++;
                            p->ent.i16uLength = simple_strtoul(array[2]->u.data) / 8;
                            p->ent.i16uOffset = simple_strtoul(array[3]->u.data);
                            p->ent.i32uDefault = simple_strtoul(array[1]->u.data);
                            
                            appendEntry(&first, p);
                        }
                    }
                    
                    appendEntry(&ret->entry, first);
                }
                
            }
            else
            {
                ret = extract_devices(element->u.object[i]->val, lvl+1);
            }
        }
        break;
    case JSON_ARRAY_BEGIN:
        if (lvl == 100)
        {
            for (i = 0; i < element->length; i++) 
            {
                p = extract_devices(element->u.array[i], lvl + 1);
                if (i == 0)
                {
                    ret = p;
                }
                else
                {
                    last->next = p;
                }
                last = p;
            }
        }
        else
        {
            for (i = 0; i < element->length; i++) 
            {
                ret = extract_devices(element->u.array[i], lvl + 1);
            }
        }
        break;
    case JSON_FALSE:
    case JSON_TRUE:
    case JSON_NULL:
    case JSON_INT:
    case JSON_STRING:
    case JSON_FLOAT:
        break;
    default:
        break;
    }
    
    return ret;
}
#endif

static piDevices *find_devices(json_val_t *element, SDeviceInfo *pDev, int lvl)
{
    int i;
    piDevices *ret = NULL;
    
    if (!element) {
        DF_PRINTK("error: no element in print tree\n");
        return NULL;
    }
    
    switch (element->type) {
    case JSON_OBJECT_BEGIN:
        if (lvl == 200)
        {
            pDev->i16uEntries += element->length;
        }
        else
        {
            for (i = 0; i < element->length; i++) 
            {
                if (lvl == 1 && strcmp(element->u.object[i]->key, TOKEN_DEVICES) == 0)
                {   // we found the devices list -> increase lvl
                    if (ret != NULL)
                    {
                        DF_PRINTK("error: there should by only one '%s' element\n", element->u.object[i]->key);
                        return NULL;
                    }
                    ret = find_devices(element->u.object[i]->val, NULL, 100);
                }
                else if (lvl == 101)
                {   // we found a device, parse elements
                    if (strcmp(element->u.object[i]->key, TOKEN_TYPE) == 0)
                    {
                        pDev->i16uModuleType = simple_strtoul(element->u.object[i]->val->u.data, NULL, 0);
                    }
                    else if (strcmp(element->u.object[i]->key, TOKEN_POSITION) == 0)
                    {
                        pDev->i8uAddress = simple_strtoul(element->u.object[i]->val->u.data, NULL, 0);
                    }
                    else if (strcmp(element->u.object[i]->key, TOKEN_OFFSET) == 0)
                    {
                        pDev->i16uBaseOffset = simple_strtoul(element->u.object[i]->val->u.data, NULL, 0);
                    }
                    else if (strcmp(element->u.object[i]->key, TOKEN_INPUT) == 0)
                    {
                        ret = find_devices(element->u.object[i]->val, pDev, 200);
                    }
                    else if (strcmp(element->u.object[i]->key, TOKEN_OUTPUT) == 0)
                    {
                        ret = find_devices(element->u.object[i]->val, pDev, 200);
                    }
                    else if (strcmp(element->u.object[i]->key, TOKEN_MEMORY) == 0)
                    {
                        ret = find_devices(element->u.object[i]->val, pDev, 200);
                    }
                    else if (strcmp(element->u.object[i]->key, TOKEN_CONFIG) == 0)
                    {
                        ret = find_devices(element->u.object[i]->val, pDev, 200);
                    }
                    else
                    {
                        ret = find_devices(element->u.object[i]->val, NULL, lvl + 1);
                    }
                }
                else
                {
                    ret = find_devices(element->u.object[i]->val, NULL, lvl + 1);
                }
            }
        }
        break;
    case JSON_ARRAY_BEGIN:
        if (lvl == 100)
        {
            int entries = 0;
            ret = kmalloc(sizeof(piDevices) + element->length * sizeof(SDeviceInfo), GFP_KERNEL);
            memset(ret, 0, sizeof(piDevices) + element->length * sizeof(SDeviceInfo));
            ret->i16uNumDevices = element->length;
            for (i = 0; i < element->length; i++) 
            {
                ret->dev[i].i16uFirstEntry = entries;
                find_devices(element->u.array[i], &ret->dev[i], lvl + 1);
                entries += ret->dev[i].i16uEntries;
            }
        }
        else
        {
            for (i = 0; i < element->length; i++) 
            {
                ret = find_devices(element->u.array[i], 0, lvl + 1);
            }
        }
        break;
        break;
    case JSON_FALSE:
    case JSON_TRUE:
    case JSON_NULL:
        break;
    case JSON_INT:
        break;
    case JSON_STRING:
        break;
    case JSON_FLOAT:
        break;
    default:
        break;
    }
    return ret;
}


static void find_entries(json_val_t *element, piEntries *pEnt, int *pIdxEntry, int devAddr, int type, int lvl)
{
    int i;
    
    if (!element) {
        DF_PRINTK("error: no element in print tree\n");
        return;
    }
    
    switch (element->type) {
    case JSON_OBJECT_BEGIN:
        for (i = 0; i < element->length; i++) 
        {
            if (lvl == 1 && strcmp(element->u.object[i]->key, TOKEN_DEVICES) == 0)
            {   // we found the devices list -> increase lvl
                find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, type, 100);
            }
            else if (lvl == 101)
            {   // we found a device, parse elements
                if (strcmp(element->u.object[i]->key, TOKEN_POSITION) == 0)
                {
                    devAddr = simple_strtoul(element->u.object[i]->val->u.data, NULL, 0);
                }
                else if (strcmp(element->u.object[i]->key, TOKEN_INPUT) == 0)
                {
                    find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, 1, 200);
                }
                else if (strcmp(element->u.object[i]->key, TOKEN_OUTPUT) == 0)
                {
                    find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, 2, 200);
                }
                else if (strcmp(element->u.object[i]->key, TOKEN_MEMORY) == 0)
                {
                    find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, 3, 200);
                }
                else if (strcmp(element->u.object[i]->key, TOKEN_CONFIG) == 0)
                {
                    find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, 4, 200);
                }
                else
                {
                    find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, type, lvl+1);
                }
            }
            else if (lvl == 200)
            {
                if (element->u.object[i]->val->type == JSON_ARRAY_BEGIN)
                {
                    struct json_val **array = element->u.object[i]->val->u.array;

                    if (*pIdxEntry >= pEnt->i16uNumEntries)
                    {
                        DF_PRINTK("error: wrong entry index\n");
                        return;
                    }
                    pEnt->ent[*pIdxEntry].i8uAddress = devAddr;
                    pEnt->ent[*pIdxEntry].i8uType = type;
                    pEnt->ent[*pIdxEntry].i16uIndex = i;
                    pEnt->ent[*pIdxEntry].i16uBitLength = simple_strtoul(array[2]->u.data, NULL, 0);
                    if (pEnt->ent[*pIdxEntry].i16uBitLength == 1)
                    {
                        pEnt->ent[*pIdxEntry].i8uBitPos = simple_strtoul(array[7]->u.data, NULL, 0);
                    }
                    else
                    {
                        pEnt->ent[*pIdxEntry].i8uBitPos = 0; // default for whole bytes
                    }
                    pEnt->ent[*pIdxEntry].i16uOffset = simple_strtoul(array[3]->u.data, NULL, 0);
                    pEnt->ent[*pIdxEntry].i32uDefault = simple_strtoul(array[1]->u.data, NULL, 0);
                    //DF_PRINTK("export: >%s< t %d l %d\n", array[4]->u.data, array[4]->type, array[4]->length);
                    if (array[4]->type == JSON_TRUE ||
                        (array[4]->type == JSON_STRING && strcmp(array[4]->u.data, "true") == 0)
                       )
                    {
                        pEnt->ent[*pIdxEntry].i8uType |= 0x80;      // flag for exported variables
                    }
                    strcpy(pEnt->ent[*pIdxEntry].strVarName, array[0]->u.data);
                    (*pIdxEntry)++;
                }
                else
                {
                    find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, type, lvl + 1);
                }
            }
            else
            {
                find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, type, lvl+1);
            }
        }
        break;
    case JSON_ARRAY_BEGIN:
        for (i = 0; i < element->length; i++) 
        {
            find_entries(element->u.array[i], pEnt, pIdxEntry, devAddr, type, lvl + 1);
        }
        break;
    case JSON_FALSE:
    case JSON_TRUE:
    case JSON_NULL:
        break;
    case JSON_INT:
        break;
    case JSON_STRING:
        break;
    case JSON_FLOAT:
        break;
    default:
        break;
    }
}

#ifdef VERBOSE
static int print_tree_iter(json_val_t *element, int lvl)
{
    int i;
    char sLvl[] = "############";

    if (!element)
    {
        DF_PRINTK("error: no element in print tree\n");
        return -1;
    }
    
    sLvl[lvl] = 0;

    switch (element->type) {
    case JSON_OBJECT_BEGIN:
        DF_PRINTK("%s object begin (%d element)\n", sLvl, element->length);
        for (i = 0; i < element->length; i++) {
            DF_PRINTK("%s key: %s\n", sLvl, element->u.object[i]->key);
            print_tree_iter(element->u.object[i]->val, lvl + 1);
        }
        DF_PRINTK("%s object end\n", sLvl);
        break;
    case JSON_ARRAY_BEGIN:
        DF_PRINTK("%s array begin\n", sLvl);
        for (i = 0; i < element->length; i++) {
            print_tree_iter(element->u.array[i], lvl + 1);
        }
        DF_PRINTK("%s array end\n", sLvl);
        break;
    case JSON_FALSE:
    case JSON_TRUE:
    case JSON_NULL:
        DF_PRINTK("%s constant\n", sLvl);
        break;
    case JSON_INT:
        DF_PRINTK("%s integer: %s\n", sLvl, element->u.data);
        break;
    case JSON_STRING:
        DF_PRINTK("%s string: %s\n", sLvl, element->u.data);
        break;
    case JSON_FLOAT:
        DF_PRINTK("%s float: %s\n", sLvl, element->u.data);
        break;
    default:
        break;
    }
    return 0;
}
#endif

int piConfigParse(const char *filename, piDevices **devs, piEntries **ent, piCopylist **cl)
{
    int ret = 0, i, cnt, d, idx[4], exported_outputs;
    json_config config;
    json_val_t *root_structure;

    memset(&config, 0, sizeof(json_config));
    config.max_nesting = 0;
    config.max_data = 0;
    config.allow_c_comments = 1;
    config.allow_yaml_comments = 1;

    *devs = NULL;
    *ent = NULL;
    *cl = NULL;

    ret = do_tree(&config, filename, &root_structure);
    if (ret)
        return ret;

#ifdef VERBOSE
    print_tree_iter(root_structure, 1);
#endif

    *devs = find_devices(root_structure, NULL, 1);
    
#if 1 //def DEBUG_CONFIG
    DF_PRINTK("%d devices found\n", (*devs)->i16uNumDevices);
#endif
    cnt = 0;
    for (i = 0; i < (*devs)->i16uNumDevices; i++)
    {
#ifdef DEBUG_CONFIG
        DF_PRINTK("device %d has %d entries, from %d. Offsets: Base=%3d"
                  //" In=%3d Out=%3d Conf=%3d"
                  "\n",
                  i, (*devs)->dev[i].i16uEntries, (*devs)->dev[i].i16uFirstEntry, (*devs)->dev[i].i16uBaseOffset
                  //, (*devs)->dev[i].i16uInputOffset, (*devs)->dev[i].i16uOutputOffset, (*devs)->dev[i].i16uConfigOffset
                  );
#endif
        cnt += (*devs)->dev[i].i16uEntries;
    }
#if 1 //def DEBUG_CONFIG
    DF_PRINTK("%d entries in total\n", cnt);
#endif

    *ent = kmalloc(sizeof(piEntries) + cnt * sizeof(SEntryInfo), GFP_KERNEL);
    memset(*ent, 0, sizeof(piEntries) + cnt * sizeof(SEntryInfo));
    (*ent)->i16uNumEntries = cnt;
    cnt = 0;
    find_entries(root_structure, *ent, &cnt, 0, 0, 1);

    piDIOComm_InitStart();

    // copy the config value into the module driver
    for (i = 0; i < (*devs)->i16uNumDevices; i++)
    {
#ifdef DEBUG_CONFIG
        DF_PRINTK("device %d typ %d has %d entries. Offsets: Base=%3d"
                  " In=%3d Out=%3d Conf=%3d"
                  "\n",
                  i, (*devs)->dev[i].i16uModuleType, (*devs)->dev[i].i16uEntries, (*devs)->dev[i].i16uBaseOffset
                  , (*devs)->dev[i].i16uInputOffset, (*devs)->dev[i].i16uOutputOffset, (*devs)->dev[i].i16uConfigOffset
                  );
#endif

        switch ((*devs)->dev[i].i16uModuleType)
        {
        case KUNBUS_FW_DESCR_TYP_PI_DIO_14:
        case KUNBUS_FW_DESCR_TYP_PI_DI_16:
        case KUNBUS_FW_DESCR_TYP_PI_DO_16:
            piDIOComm_Config((*devs)->dev[i].i8uAddress, (*devs)->dev[i].i16uEntries, &(*ent)->ent[(*devs)->dev[i].i16uFirstEntry]);
            break;
        }
    }

    // now correct the offsets with the base offset of the module
    d=0;
    i=0;
    exported_outputs = 0;
    idx[0] = idx[1] = idx[2] = idx[3] = 0;
    while (i < (*ent)->i16uNumEntries && d < (*devs)->i16uNumDevices)
    {
        if ((*ent)->ent[i].i8uAddress != (*devs)->dev[d].i8uAddress)
        {
            (*devs)->dev[d].i16uInputLength /= 8;
            (*devs)->dev[d].i16uOutputLength /= 8;
            (*devs)->dev[d].i16uConfigLength /= 8;

#ifdef DEBUG_CONFIG
            DF_PRINTK("device %d typ %d adjusted. Offsets: Base=%3d\n"
                      "offset In=%3d Out=%3d Conf=%3d\n"
                      "length In=%3d Out=%3d Conf=%3d\n",
                      d, (*devs)->dev[d].i16uModuleType, (*devs)->dev[d].i16uBaseOffset
                      , (*devs)->dev[d].i16uInputOffset, (*devs)->dev[d].i16uOutputOffset, (*devs)->dev[d].i16uConfigOffset
                      , (*devs)->dev[d].i16uInputLength, (*devs)->dev[d].i16uOutputLength, (*devs)->dev[d].i16uConfigLength
                      );
#endif
            d++;    // goto next device
            idx[0] = idx[1] = idx[2] = idx[3] = 0;
        }
        else
        {
            uint8_t type = (*ent)->ent[i].i8uType;
            if (type == 0x82)
                exported_outputs++;

            type &= 0x7f;   // remove export flag

            (*ent)->ent[i].i16uOffset += (*devs)->dev[d].i16uBaseOffset;
            if (type == 1)  // Input
            {
                if (idx[0] == 0 || (*devs)->dev[d].i16uInputOffset > (*ent)->ent[i].i16uOffset)
                {
                    idx[0]++;
                    (*devs)->dev[d].i16uInputOffset = (*ent)->ent[i].i16uOffset;
                }
                (*devs)->dev[d].i16uInputLength += (*ent)->ent[i].i16uBitLength;
            }
            else if (type == 2) // Output
            {
                if (idx[1] == 0 || (*devs)->dev[d].i16uOutputOffset > (*ent)->ent[i].i16uOffset)
                {
                    idx[1]++;
                    (*devs)->dev[d].i16uOutputOffset = (*ent)->ent[i].i16uOffset;
                }
                (*devs)->dev[d].i16uOutputLength += (*ent)->ent[i].i16uBitLength;
            }
            else if (type == 3) // Memory
            {
                if (idx[2] == 0 || (*devs)->dev[d].i16uConfigOffset > (*ent)->ent[i].i16uOffset)
                {
                    idx[2]++;
                    (*devs)->dev[d].i16uConfigOffset = (*ent)->ent[i].i16uOffset;
                }
                (*devs)->dev[d].i16uConfigLength += (*ent)->ent[i].i16uBitLength;
            }
            else if (type == 4) // Config
            {
                if (idx[0] == 0 || (*devs)->dev[d].i16uInputOffset > (*ent)->ent[i].i16uOffset)
                {
                    idx[3]++;
                    (*devs)->dev[d].i16uConfigOffset = (*ent)->ent[i].i16uOffset;
                }
                (*devs)->dev[d].i16uConfigLength += (*ent)->ent[i].i16uBitLength;
            }

            while ((*ent)->ent[i].i8uBitPos >= 8)
            {
                (*ent)->ent[i].i8uBitPos -= 8;
                (*ent)->ent[i].i16uOffset ++;
            }

#ifdef DEBUG_CONFIG
            DF_PRINTK("addr %2d  type %d  len %3d  offset %3d\n", (*ent)->ent[i].i8uAddress, (*ent)->ent[i].i8uType, (*ent)->ent[i].i16uBitLength, (*ent)->ent[i].i16uOffset);
#endif
            i++;
        }
    }
    if (d < (*devs)->i16uNumDevices)
    {
        (*devs)->dev[d].i16uInputLength /= 8;
        (*devs)->dev[d].i16uOutputLength /= 8;
        (*devs)->dev[d].i16uConfigLength /= 8;

#ifdef DEBUG_CONFIG
        DF_PRINTK("device %d typ %d adjusted. Offsets: Base=%3d\n"
                  "offset In=%3d Out=%3d Conf=%3d\n"
                  "length In=%3d Out=%3d Conf=%3d\n",
                  d, (*devs)->dev[d].i16uModuleType, (*devs)->dev[d].i16uBaseOffset
                  , (*devs)->dev[d].i16uInputOffset, (*devs)->dev[d].i16uOutputOffset, (*devs)->dev[d].i16uConfigOffset
                  , (*devs)->dev[d].i16uInputLength, (*devs)->dev[d].i16uOutputLength, (*devs)->dev[d].i16uConfigLength
                  );
#endif
    }

    // Generate Copy List
    *cl = kmalloc(sizeof(piCopylist) + exported_outputs * sizeof(piCopyEntry), GFP_KERNEL);
    (*cl)->i16uNumEntries = exported_outputs;
    d = 0;
    for (i=0; i < (*ent)->i16uNumEntries && d <= exported_outputs; i++)
    {
        if ((*ent)->ent[i].i8uType == 0x82)
        {
            if (d == exported_outputs)
            {
                printk("### internal error 1 ### %d %d  %d %d\n", i, (*ent)->i16uNumEntries, d, exported_outputs);
                exported_outputs = 0;
            }
            else
            {
                (*cl)->ent[d].i16uAddr = (*ent)->ent[i].i16uOffset;
                (*cl)->ent[d].i8uBitMask = (0xff >> (8-(*ent)->ent[i].i16uBitLength)) << (*ent)->ent[i].i8uBitPos;
                (*cl)->ent[d].i16uLength = (*ent)->ent[i].i16uBitLength;
                d++;
            }
        }
    }

#ifdef DEBUG_CONFIG
    DF_PRINTK("cl: %2d addr %2d  bit %02x  len %3d\n", i, (*cl)->ent[0].i16uAddr, (*cl)->ent[0].i8uBitMask, (*cl)->ent[0].i16uLength);
#endif
    // prüfe ob die Einträge aufsteigend sortiert sind
    for (d=1; d<exported_outputs; d++)
    {
        if (  ((*cl)->ent[d-1].i16uAddr > (*cl)->ent[d].i16uAddr)
           || (  ((*cl)->ent[d-1].i16uAddr == (*cl)->ent[d].i16uAddr)
              && ((*cl)->ent[d-1].i8uBitMask & (*cl)->ent[d].i8uBitMask)
              )
           )
        {
            printk("### internal error 2 ### %d\n", d);
            exported_outputs = 0;
            break;
        }
        else
        {
#ifdef DEBUG_CONFIG
            DF_PRINTK("cl: %2d addr %2d  bit %02x  len %3d\n", i, (*cl)->ent[d].i16uAddr, (*cl)->ent[d].i8uBitMask, (*cl)->ent[d].i16uLength);
#endif
        }
    }

    i = 0;
    for (d=1; d<exported_outputs; d++)
    {
        if (  (*cl)->ent[i].i16uAddr == (*cl)->ent[d].i16uAddr
           && (*cl)->ent[i].i16uLength < 8
           && (*cl)->ent[d].i16uLength < 8
           )
        {
            // fasse die beiden Einträge zusammen
            (*cl)->ent[i].i16uLength += (*cl)->ent[d].i16uLength;
            (*cl)->ent[i].i8uBitMask |= (*cl)->ent[d].i8uBitMask;
        }
        else if (  (*cl)->ent[i].i16uLength >= 8
                && (*cl)->ent[d].i16uLength >= 8
                && (*cl)->ent[d].i16uAddr == (*cl)->ent[i].i16uAddr + (*cl)->ent[i].i16uLength / 8
           )
        {
            // fasse die beiden Einträge zusammen
            (*cl)->ent[i].i16uLength += (*cl)->ent[d].i16uLength;
        }
        else
        {
            // gehe zum nächsten Eintrag
#ifdef DEBUG_CONFIG
            DF_PRINTK("cl-comp: %2d addr %2d  bit %02x  len %3d\n", i, (*cl)->ent[i].i16uAddr, (*cl)->ent[i].i8uBitMask, (*cl)->ent[i].i16uLength);
#endif
            i++;
            (*cl)->ent[i].i16uAddr = (*cl)->ent[d].i16uAddr;
            (*cl)->ent[i].i8uBitMask = (*cl)->ent[d].i8uBitMask;
            (*cl)->ent[i].i16uLength = (*cl)->ent[d].i16uLength;
        }
    }
    if (exported_outputs > 0)
    {
#ifdef DEBUG_CONFIG
        DF_PRINTK("cl-comp: %2d addr %2d  bit %02x  len %3d\n", i, (*cl)->ent[i].i16uAddr, (*cl)->ent[i].i8uBitMask, (*cl)->ent[i].i16uLength);
#endif
        i++;
    }

    DF_PRINTK("copylist has %d entries\n", i);
    (*cl)->i16uNumEntries = i;

    free_tree(root_structure);
    
    return ret;
}

