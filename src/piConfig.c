// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: 2016-2024 KUNBUS GmbH

#include <linux/fs.h>
#include <linux/slab.h>

#include "common_define.h"
#include "json.h"
#include "piAIOComm.h"
#include "piConfig.h"
#include "piDIOComm.h"
#include "project.h"
#include "revpi_compact.h"
#include "revpi_mio.h"
#include "revpi_ro.h"

#define TOKEN_DEVICES       "Devices"
#define TOKEN_CONNECTIONS   "Connections"
#define TOKEN_TYPE          "productType"
#define TOKEN_POSITION      "position"
#define TOKEN_INPUT         "inp"
#define TOKEN_OUTPUT        "out"
#define TOKEN_MEMORY        "mem"
#define TOKEN_CONFIG        "config"
#define TOKEN_OFFSET        "offset"
#define TOKEN_SRC_GUID      "srcGUID"
#define TOKEN_SRC_NAME      "srcAttrname"
#define TOKEN_DEST_GUID     "destGUID"
#define TOKEN_DEST_NAME     "destAttrname"

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

char *string_of_errors[] = {
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

	pr_info_config("filp_open %s %x  %d %x\n", filename, (int)filename, (int)input, (int)input);

	if (IS_ERR(input)) {
		pr_err("error: cannot open file %s\n", filename);
		return NULL;
	}
	return input;
}

void close_filename(struct file *file)
{
	filp_close(file, NULL);
}

int process_file(json_parser * parser, struct file *input, int *retlines, int *retcols)
{
#define BUFFLEN     4096
	int ret = 0;
	int32_t read;
	uint32_t lines, col, i, len;
	char *buffer;
	uint32_t processed;

	buffer = kmalloc(BUFFLEN, GFP_KERNEL);
	if (buffer == NULL) {
		pr_err("process file: out of memory\n");
		return JSON_ERROR_NO_MEMORY;
	}

	lines = 1;
	col = 0;
	processed = 0;
	len = BUFFLEN;

	while (1) {
		read = kernel_read(input, buffer + (BUFFLEN - len), len, &input->f_pos);
		if (read < 0) {
			pr_err("read file failed, ret=%d\n", read);
			break;
		}
		if (read == 0) {
			pr_debug("read file finished, f_pos=%lld\n",
							input->f_pos);
			break;
		}
		ret = json_parser_string(parser, buffer, read, &processed);
		//pr_err("json_parser_string returned %d: %d %u\n", ret, read, processed);
		for (i = 0; i < processed; i++) {
			if (buffer[i] == '\n') {
				col = 0;
				lines++;
			} else
				col++;
		}
		for (i = 0; i < (read - processed); i++) {
			buffer[i] = buffer[processed + i];
		}
		len = BUFFLEN - read + processed;

		if (ret) {
			// exit loop on error
			break;
		}
	}
	if (retlines)
		*retlines = lines;
	if (retcols)
		*retcols = col;
	kfree(buffer);
	return ret;
}

static void *tree_create_structure(int is_object)
{
	json_val_t *v = kmalloc(sizeof(json_val_t), GFP_KERNEL);
	if (v) {
		/* instead of defining a new enum type, we abuse the
		 * meaning of the json enum type for array and object */
		if (is_object) {
			v->type = JSON_OBJECT_BEGIN;
			v->u.object = NULL;
		} else {
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
	if (dest) {
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
			parent->u.object = kmalloc((1 + 1) * sizeof(json_val_t *), GFP_KERNEL);	/* +1 for null */
			if (!parent->u.object)
				return 1;
			memset(parent->u.object, 0, 2 * sizeof(json_val_t *));
		} else {
			uint32_t newsize = parent->length + 1 + 1;	/* +1 for null */
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
	} else {
		if (parent->length == 0) {
			parent->u.array = kmalloc((1 + 1) * sizeof(json_val_t *), GFP_KERNEL);	/* +1 for null */
			if (!parent->u.array)
				return 1;
			memset(parent->u.array, 0, (1 + 1) * sizeof(json_val_t *));
		} else {
			uint32_t newsize = parent->length + 1 + 1;	/* +1 for null */
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

static int do_tree(json_config *config,
			const char *filename,
			json_val_t **root_structure)
{
	struct file *input;
	json_parser parser;
	json_parser_dom dom;
	int ret;
	int col, lines;

	input = open_filename(filename, O_RDONLY);
	if (!input)
		return 2;

	ret = json_parser_dom_init(&dom,
					tree_create_structure,
					tree_create_data,
					tree_append);
	if (ret) {
		pr_err("error: initializing helper failed: [code=%d] %s\n",
						ret, string_of_errors[ret]);
		goto close_file;
	}

	ret = json_parser_init(&parser, config, json_parser_dom_callback, &dom);
	if (ret) {
		pr_err("error: initializing parser failed: [code=%d] %s\n",
						ret, string_of_errors[ret]);
		goto free_dom;
	}

	ret = process_file(&parser, input, &lines, &col);
	if (ret) {
		pr_err("line %d, col %d: [code=%d] %s\n",
					lines, col, ret, string_of_errors[ret]);
		goto free_parser;
	}

	if (!json_parser_is_done(&parser)) { /* parsing incomplete */
		if (parser.state == 0 && parser.stack_offset == 0)
			pr_err("config.rsc is empty! "
				"Probably needs to be configured in PiCtory\n");
		else
			pr_err("syntax error: offset %d  state %d\n",
					parser.stack_offset, parser.state);
		ret = 1;
		goto free_parser;
	}

	if (root_structure)
		*root_structure = dom.root_structure;

	/* cleanup */
free_parser:
	json_parser_free(&parser);
free_dom:
	json_parser_dom_free(&dom);

close_file:
	close_filename(input);

	return ret;
}

static int free_tree(json_val_t * element)
{
	int i;
	if (!element) {
		pr_err("error: no element in print tree\n");
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

static piDevices *find_devices(json_val_t * element, SDeviceInfo * pDev, int lvl)
{
	int i;
	piDevices *ret = NULL;

	if (!element) {
		pr_err("error: no element in print tree\n");
		return NULL;
	}

	pr_info_config("find_devices type %d   lvl %d\n", element->type, lvl);

	switch (element->type) {
	case JSON_OBJECT_BEGIN:
		if (lvl == 200) {
			pDev->i16uEntries += element->length;
		} else {
			for (i = 0; i < element->length; i++) {
				if (lvl == 1 && strcmp(element->u.object[i]->key, TOKEN_DEVICES) == 0) {	// we found the devices list -> increase lvl
					if (ret != NULL) {
						pr_err("error: there should by only one '%s' element\n",
							  element->u.object[i]->key);
						return NULL;
					}
					ret = find_devices(element->u.object[i]->val, NULL, 100);
				} else if (lvl == 101) {	// we found a device, parse elements
					if (strcmp(element->u.object[i]->key, TOKEN_TYPE) == 0) {
						if (kstrtou16(element->u.object[i]->val->u.data, 0, &pDev->i16uModuleType) != 0)
							pDev->i16uModuleType = 0;
					} else if (strcmp(element->u.object[i]->key, TOKEN_POSITION) == 0) {
						if (kstrtou8(element->u.object[i]->val->u.data, 0, &pDev->i8uAddress) != 0)
							pDev->i8uAddress = 0;
					} else if (strcmp(element->u.object[i]->key, TOKEN_OFFSET) == 0) {
						if (kstrtou16(element->u.object[i]->val->u.data, 0, &pDev->i16uBaseOffset) != 0)
							pDev->i16uBaseOffset = 0;
					} else if (strcmp(element->u.object[i]->key, TOKEN_INPUT) == 0) {
						ret = find_devices(element->u.object[i]->val, pDev, 200);
					} else if (strcmp(element->u.object[i]->key, TOKEN_OUTPUT) == 0) {
						ret = find_devices(element->u.object[i]->val, pDev, 200);
					} else if (strcmp(element->u.object[i]->key, TOKEN_MEMORY) == 0) {
						ret = find_devices(element->u.object[i]->val, pDev, 200);
					} else if (strcmp(element->u.object[i]->key, TOKEN_CONFIG) == 0) {
						ret = find_devices(element->u.object[i]->val, pDev, 200);
					} else {
						ret = find_devices(element->u.object[i]->val, NULL, lvl + 1);
					}
				} else {
					// the other objects are not processed
					//ret = find_devices(element->u.object[i]->val, NULL, lvl + 1);
				}
			}
		}
		break;
	case JSON_ARRAY_BEGIN:
		if (lvl == 100) {
			int entries = 0;
			ret = kmalloc(sizeof(piDevices) + element->length * sizeof(SDeviceInfo), GFP_KERNEL);
			memset(ret, 0, sizeof(piDevices) + element->length * sizeof(SDeviceInfo));
			ret->i16uNumDevices = element->length;
			for (i = 0; i < element->length; i++) {
				ret->dev[i].i16uFirstEntry = entries;
				find_devices(element->u.array[i], &ret->dev[i], lvl + 1);
				entries += ret->dev[i].i16uEntries;
			}
		} else {
			for (i = 0; i < element->length; i++) {
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
		pr_err("error: unhandled type %d\n", element->type);
		break;
	}
	return ret;
}

static void find_entries(json_val_t * element, piEntries * pEnt, int *pIdxEntry, int devAddr, int type, int lvl)
{
	int i;

	if (!element) {
		pr_err("error: no element in print tree\n");
		return;
	}

	switch (element->type) {
	case JSON_OBJECT_BEGIN:
		for (i = 0; i < element->length; i++) {
			if (lvl == 1 && strcmp(element->u.object[i]->key, TOKEN_DEVICES) == 0) {	// we found the devices list -> increase lvl
				find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, type, 100);
			} else if (lvl == 101) {	// we found a device, parse elements
				if (strcmp(element->u.object[i]->key, TOKEN_POSITION) == 0) {
					if (kstrtoint(element->u.object[i]->val->u.data, 0, &devAddr) != 0)
						devAddr = 0;
				} else if (strcmp(element->u.object[i]->key, TOKEN_INPUT) == 0) {
					find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, 1, 200);
				} else if (strcmp(element->u.object[i]->key, TOKEN_OUTPUT) == 0) {
					find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, 2, 200);
				} else if (strcmp(element->u.object[i]->key, TOKEN_MEMORY) == 0) {
					find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, 3, 200);
				} else if (strcmp(element->u.object[i]->key, TOKEN_CONFIG) == 0) {
					find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, 4, 200);
				} else {
					find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, type,
						     lvl + 1);
				}
			} else if (lvl == 200) {
				if (element->u.object[i]->val->type == JSON_ARRAY_BEGIN) {
					struct json_val **array = element->u.object[i]->val->u.array;

					if (*pIdxEntry >= pEnt->i16uNumEntries) {
						pr_err("error: wrong entry index\n");
						return;
					}
					pEnt->ent[*pIdxEntry].i8uAddress = devAddr;
					pEnt->ent[*pIdxEntry].i8uType = type;
					pEnt->ent[*pIdxEntry].i16uIndex = i;
					if (kstrtou16(array[2]->u.data, 0, &pEnt->ent[*pIdxEntry].i16uBitLength) != 0)
						pEnt->ent[*pIdxEntry].i16uBitLength = 0;
					if (pEnt->ent[*pIdxEntry].i16uBitLength == 1) {
						if (kstrtou8(array[7]->u.data, 0, &pEnt->ent[*pIdxEntry].i8uBitPos) != 0)
							pEnt->ent[*pIdxEntry].i8uBitPos = 0;
					} else {
						pEnt->ent[*pIdxEntry].i8uBitPos = 0;	// default for whole bytes
					}
					if (kstrtou16(array[3]->u.data, 0, &pEnt->ent[*pIdxEntry].i16uOffset) != 0)
						pEnt->ent[*pIdxEntry].i16uOffset = 0;
					if (kstrtou32(array[1]->u.data, 0, &pEnt->ent[*pIdxEntry].i32uDefault) != 0) {
						// if parsing as unsigned failed, try it as signed number
						if (kstrtos32(array[1]->u.data, 0, &pEnt->ent[*pIdxEntry].i32uDefault) != 0) {
							// try binary representation
							if (array[1]->u.data[0] == '0' && array[1]->u.data[1] == 'b') {
								if (kstrtou32(array[1]->u.data+2, 2, &pEnt->ent[*pIdxEntry].i32uDefault) != 0) {
									// failed also, use default value 0
									pEnt->ent[*pIdxEntry].i32uDefault = 0;
								}
							} else if (array[1]->u.data[0] == '-' && array[1]->u.data[1] == '0' && array[1]->u.data[2] == 'b') {
								if (kstrtou32(array[1]->u.data+3, 2, &pEnt->ent[*pIdxEntry].i32uDefault) != 0) {
									// failed also, use default value 0
									pEnt->ent[*pIdxEntry].i32uDefault = 0;
								}
							} else {
								// use default value 0
								pEnt->ent[*pIdxEntry].i32uDefault = 0;
							}
						}
					}
					//pr_info_config("export: >%s< t %d l %d\n", array[4]->u.data, array[4]->type, array[4]->length);
					if (array[4]->type == JSON_TRUE ||
					    (array[4]->type == JSON_STRING && strcmp(array[4]->u.data, "true") == 0)
					    ) {
						pEnt->ent[*pIdxEntry].i8uType |= 0x80;	// flag for exported variables
					}
					strncpy(pEnt->ent[*pIdxEntry].strVarName, array[0]->u.data,
						sizeof(pEnt->ent[*pIdxEntry].strVarName) - 1);
					pEnt->ent[*pIdxEntry].strVarName[sizeof(pEnt->ent[*pIdxEntry].strVarName) - 1] =
					    0;

					(*pIdxEntry)++;
				} else {
					find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, type,
						     lvl + 1);
				}
			} else {
				find_entries(element->u.object[i]->val, pEnt, pIdxEntry, devAddr, type, lvl + 1);
			}
		}
		break;
	case JSON_ARRAY_BEGIN:
		for (i = 0; i < element->length; i++) {
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

static SEntryInfo *search_entry(piEntries * ent, char *strName)
{
	int i;
	for (i = 0; i < ent->i16uNumEntries; i++) {
		if (strcmp(ent->ent[i].strVarName, strName) == 0)
			return &ent->ent[i];
	}
	return NULL;
}

static piConnectionList *find_connections(json_val_t * element, piDevices * devs, piEntries * ent, piConnection * conn,
					  int lvl)
{
	int i;
	piConnectionList *ret = NULL;

	if (!element) {
		pr_err("error: no element in print tree\n");
		return NULL;
	}

	pr_info_config("find_connections type %d   lvl %d\n", element->type, lvl);

	switch (element->type) {
	case JSON_OBJECT_BEGIN:
		if (lvl == 200) {

		} else {
			// The variable name are unique in th whole configuration, therefore it is not necessary to compare the GUIDs
			// also. This is guaranteed by PiCtory.
//			char strSrcGUID[50];
//			char strDstGUID[50];
			char strSrcName[32];
			char strDstName[32];
//			strSrcGUID[0] = 0;
//			strDstGUID[0] = 0;
			strSrcName[0] = 0;
			strDstName[0] = 0;

			for (i = 0; i < element->length; i++) {
				if (lvl == 1 && strcmp(element->u.object[i]->key, TOKEN_CONNECTIONS) == 0) {	// we found the connections list -> increase lvl
					if (ret != NULL) {
						pr_err("error: there should by only one '%s' element\n",
							  element->u.object[i]->key);
						return NULL;
					}
					ret = find_connections(element->u.object[i]->val, devs, ent, conn, 100);
				} else if (lvl == 101) {	// we found a connection, parse elements
//					if (strcmp(element->u.object[i]->key, TOKEN_SRC_GUID) == 0)
//					{
//						strncpy(strSrcGUID, element->u.object[i]->val->u.data, sizeof(strSrcGUID)-1);
//						strSrcGUID[sizeof(strSrcGUID)-1] = 0;
//					}
//					else if (strcmp(element->u.object[i]->key, TOKEN_DEST_GUID) == 0)
//					{
//						strncpy(strDstGUID, element->u.object[i]->val->u.data, sizeof(strDstGUID)-1);
//						strDstGUID[sizeof(strDstGUID)-1] = 0;
//					}
//					else
					if (strcmp(element->u.object[i]->key, TOKEN_SRC_NAME) == 0) {
						strncpy(strSrcName, element->u.object[i]->val->u.data,
							sizeof(strSrcName) - 1);
						strSrcName[sizeof(strSrcName) - 1] = 0;
					} else if (strcmp(element->u.object[i]->key, TOKEN_DEST_NAME) == 0) {
						strncpy(strDstName, element->u.object[i]->val->u.data,
							sizeof(strDstName) - 1);
						strDstName[sizeof(strDstName) - 1] = 0;
					}
				} else {
					// the other objects are not processed
					//ret = find_connections(element->u.object[i]->val, NULL, lvl + 1);
				}
			}

			if (lvl == 101) {
				if (	/*    strSrcGUID[0] != 0
					   &&  strDstGUID[0] != 0
					   && */ strSrcName[0] != 0
					   && strDstName[0] != 0) {
					SEntryInfo *pSrcEntry, *pDstEntry;
					pSrcEntry = search_entry(ent, strSrcName);
					if (pSrcEntry == NULL) {
						pr_err("error: connection variable %s unknown\n", strSrcName);
						return NULL;
					}
					pDstEntry = search_entry(ent, strDstName);
					if (pDstEntry == NULL) {
						pr_err("error: connection variable %s unknown\n", strDstName);
						return NULL;
					}
					conn->i16uSrcAddr = pSrcEntry->i16uOffset;
					conn->i16uDestAddr = pDstEntry->i16uOffset;
					conn->i8uLength = pSrcEntry->i16uBitLength;
					if (conn->i8uLength < 8) {
						conn->i8uSrcBit = pSrcEntry->i8uBitPos;
						conn->i8uDestBit = pDstEntry->i8uBitPos;
					} else {
						conn->i8uSrcBit = 0;
						conn->i8uDestBit = 0;
					}
					return NULL;	// return value is not used in this recursive call
				} else {
					pr_err("error: attributes of connection %d are missing\n", i + 1);
					return NULL;
				}
			}
		}
		break;
	case JSON_ARRAY_BEGIN:
		if (lvl == 100) {
			ret = kzalloc(sizeof(piConnectionList) + element->length * sizeof(piConnection), GFP_KERNEL);
			ret->i16uNumEntries = element->length;
			for (i = 0; i < element->length; i++) {
				find_connections(element->u.array[i], devs, ent, &ret->conn[i], lvl + 1);
			}
		} else {
//			for (i = 0; i < element->length; i++)
//			{
//				ret = find_connections(element->u.array[i], devs, ent, conn, lvl + 1);
//			}
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
		pr_err("error: unhandled type %d\n", element->type);
		break;
	}
	return ret;
}

#ifdef DEBUG_CONFIG
static int print_tree_iter(json_val_t * element, int lvl)
{
	int i;
	char sLvl[] = "############";

	if (!element) {
		pr_info_config("error: no element in print tree\n");
		return -1;
	}

	sLvl[lvl] = 0;

	switch (element->type) {
	case JSON_OBJECT_BEGIN:
		pr_info_config("%s object begin (%d element)\n", sLvl, element->length);
		for (i = 0; i < element->length; i++) {
			pr_info_config("%s key: %s\n", sLvl, element->u.object[i]->key);
			print_tree_iter(element->u.object[i]->val, lvl + 1);
		}
		pr_info_config("%s object end\n", sLvl);
		break;
	case JSON_ARRAY_BEGIN:
		pr_info_config("%s array begin\n", sLvl);
		for (i = 0; i < element->length; i++) {
			print_tree_iter(element->u.array[i], lvl + 1);
		}
		pr_info_config("%s array end\n", sLvl);
		break;
	case JSON_FALSE:
	case JSON_TRUE:
	case JSON_NULL:
		pr_info_config("%s constant\n", sLvl);
		break;
	case JSON_INT:
		pr_info_config("%s integer: %s\n", sLvl, element->u.data);
		break;
	case JSON_STRING:
		pr_info_config("%s string: %s\n", sLvl, element->u.data);
		break;
	case JSON_FLOAT:
		pr_info_config("%s float: %s\n", sLvl, element->u.data);
		break;
	default:
		break;
	}
	return 0;
}
#endif

int piConfigParse(const char *filename, piDevices ** devs, piEntries ** ent, piCopylist ** cl,
		  piConnectionList ** connl)
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
	*connl = NULL;

	ret = do_tree(&config, filename, &root_structure);
	if (ret)
		return ret;

#ifdef DEBUG_CONFIG
	print_tree_iter(root_structure, 1);
#endif

	*devs = find_devices(root_structure, NULL, 1);
	if (*devs == NULL) {
		pr_err("find_devices returned NULL\n");
		return 3;
	}

	pr_info("found %d devices in configuration file\n", (*devs)->i16uNumDevices);

	cnt = 0;
	for (i = 0; i < (*devs)->i16uNumDevices; i++) {
		pr_info_config("device %d has %d entries, from %d. Offsets: Base=%3d"
			       //" In=%3d Out=%3d Conf=%3d"
			       "\n",
			       i, (*devs)->dev[i].i16uEntries, (*devs)->dev[i].i16uFirstEntry,
			       (*devs)->dev[i].i16uBaseOffset
			       //, (*devs)->dev[i].i16uInputOffset, (*devs)->dev[i].i16uOutputOffset, (*devs)->dev[i].i16uConfigOffset
		    );
		cnt += (*devs)->dev[i].i16uEntries;
	}
	pr_debug("%d entries in total\n", cnt);

	*ent = kmalloc(sizeof(piEntries) + cnt * sizeof(SEntryInfo), GFP_KERNEL);
	memset(*ent, 0, sizeof(piEntries) + cnt * sizeof(SEntryInfo));
	(*ent)->i16uNumEntries = cnt;
	cnt = 0;
	find_entries(root_structure, *ent, &cnt, 0, 0, 1);

	// copy the config value into the module driver
	piDIOComm_InitStart();
	piAIOComm_InitStart();
	revpi_mio_reset();
	revpi_ro_reset();

	for (i = 0; i < (*devs)->i16uNumDevices; i++) {
		pr_info_config("device %d typ %d has %d entries. Offsets: Base=%3d"
			       " In=%3d Out=%3d Conf=%3d"
			       "\n",
			       i, (*devs)->dev[i].i16uModuleType, (*devs)->dev[i].i16uEntries,
			       (*devs)->dev[i].i16uBaseOffset, (*devs)->dev[i].i16uInputOffset,
			       (*devs)->dev[i].i16uOutputOffset, (*devs)->dev[i].i16uConfigOffset);

		switch ((*devs)->dev[i].i16uModuleType) {
		case KUNBUS_FW_DESCR_TYP_PI_DIO_14:
		case KUNBUS_FW_DESCR_TYP_PI_DI_16:
		case KUNBUS_FW_DESCR_TYP_PI_DO_16:
			piDIOComm_Config((*devs)->dev[i].i8uAddress,
					 (*devs)->dev[i].i16uEntries,
					 &(*ent)->ent[(*devs)->dev[i].i16uFirstEntry]);
			break;
		case KUNBUS_FW_DESCR_TYP_PI_AIO:
			piAIOComm_Config((*devs)->dev[i].i8uAddress,
					 (*devs)->dev[i].i16uEntries,
					 &(*ent)->ent[(*devs)->dev[i].i16uFirstEntry]);
			break;
		case KUNBUS_FW_DESCR_TYP_PI_COMPACT:
			revpi_compact_config((*devs)->dev[i].i8uAddress,
					     (*devs)->dev[i].i16uEntries,
					     &(*ent)->ent[(*devs)->dev[i].i16uFirstEntry]);
			break;
		case KUNBUS_FW_DESCR_TYP_PI_MIO:
			revpi_mio_config((*devs)->dev[i].i8uAddress,
					 (*devs)->dev[i].i16uEntries,
					 &(*ent)->ent[(*devs)->dev[i].i16uFirstEntry]);
			break;
		case KUNBUS_FW_DESCR_TYP_PI_RO:
			revpi_ro_config((*devs)->dev[i].i8uAddress,
					(*devs)->dev[i].i16uEntries,
					&(*ent)->ent[(*devs)->dev[i].i16uFirstEntry]);
			break;
		}

	}

	// now correct the offsets with the base offset of the module
	d = 0;
	i = 0;
	exported_outputs = 0;
	idx[0] = idx[1] = idx[2] = idx[3] = 0;
	while (i < (*ent)->i16uNumEntries && d < (*devs)->i16uNumDevices) {
		if ((*ent)->ent[i].i8uAddress != (*devs)->dev[d].i8uAddress) {
			(*devs)->dev[d].i16uInputLength /= 8;
			(*devs)->dev[d].i16uOutputLength /= 8;
			(*devs)->dev[d].i16uConfigLength /= 8;

			pr_info_config("device %d typ %d adjusted. Offsets: Base=%3d\n"
				       "offset In=%3d Out=%3d Conf=%3d\n"
				       "length In=%3d Out=%3d Conf=%3d\n",
				       d, (*devs)->dev[d].i16uModuleType, (*devs)->dev[d].i16uBaseOffset,
				       (*devs)->dev[d].i16uInputOffset, (*devs)->dev[d].i16uOutputOffset,
				       (*devs)->dev[d].i16uConfigOffset, (*devs)->dev[d].i16uInputLength,
				       (*devs)->dev[d].i16uOutputLength, (*devs)->dev[d].i16uConfigLength);

			d++;	// goto next device
			idx[0] = idx[1] = idx[2] = idx[3] = 0;
		} else {
			uint8_t type = (*ent)->ent[i].i8uType;
			if (type == 0x82)
				exported_outputs++;

			type &= 0x7f;	// remove export flag

			(*ent)->ent[i].i16uOffset += (*devs)->dev[d].i16uBaseOffset;
			if (type == 1)	// Input
			{
				if (idx[0] == 0 || (*devs)->dev[d].i16uInputOffset > (*ent)->ent[i].i16uOffset) {
					idx[0]++;
					(*devs)->dev[d].i16uInputOffset = (*ent)->ent[i].i16uOffset;
				}
				(*devs)->dev[d].i16uInputLength += (*ent)->ent[i].i16uBitLength;
			} else if (type == 2)	// Output
			{
				if (idx[1] == 0 || (*devs)->dev[d].i16uOutputOffset > (*ent)->ent[i].i16uOffset) {
					idx[1]++;
					(*devs)->dev[d].i16uOutputOffset = (*ent)->ent[i].i16uOffset;
				}
				(*devs)->dev[d].i16uOutputLength += (*ent)->ent[i].i16uBitLength;
			} else if (type == 3)	// Memory
			{
				if (idx[2] == 0 || (*devs)->dev[d].i16uConfigOffset > (*ent)->ent[i].i16uOffset) {
					idx[2]++;
					(*devs)->dev[d].i16uConfigOffset = (*ent)->ent[i].i16uOffset;
				}
				(*devs)->dev[d].i16uConfigLength += (*ent)->ent[i].i16uBitLength;
			} else if (type == 4)	// Config
			{
				if (idx[0] == 0 || (*devs)->dev[d].i16uInputOffset > (*ent)->ent[i].i16uOffset) {
					idx[3]++;
					(*devs)->dev[d].i16uConfigOffset = (*ent)->ent[i].i16uOffset;
				}
				(*devs)->dev[d].i16uConfigLength += (*ent)->ent[i].i16uBitLength;
			}

			while ((*ent)->ent[i].i8uBitPos >= 8) {
				(*ent)->ent[i].i8uBitPos -= 8;
				(*ent)->ent[i].i16uOffset++;
			}

			pr_info_config("addr %2d  type %d  len %3d  offset %3d\n", (*ent)->ent[i].i8uAddress,
				       (*ent)->ent[i].i8uType, (*ent)->ent[i].i16uBitLength, (*ent)->ent[i].i16uOffset);
			i++;
		}
	}
	if (d < (*devs)->i16uNumDevices) {
		(*devs)->dev[d].i16uInputLength /= 8;
		(*devs)->dev[d].i16uOutputLength /= 8;
		(*devs)->dev[d].i16uConfigLength /= 8;

		pr_info_config("device %d typ %d adjusted. Offsets: Base=%3d\n"
			       "offset In=%3d Out=%3d Conf=%3d\n"
			       "length In=%3d Out=%3d Conf=%3d\n",
			       d, (*devs)->dev[d].i16uModuleType, (*devs)->dev[d].i16uBaseOffset,
			       (*devs)->dev[d].i16uInputOffset, (*devs)->dev[d].i16uOutputOffset,
			       (*devs)->dev[d].i16uConfigOffset, (*devs)->dev[d].i16uInputLength,
			       (*devs)->dev[d].i16uOutputLength, (*devs)->dev[d].i16uConfigLength);
	}

	*connl = find_connections(root_structure, *devs, *ent, NULL, 1);

#ifdef DEBUG_CONFIG
	for (i = 0; i < (*connl)->i16uNumEntries; i++) {
		pr_info_config("connection %2d: %d bits from %d/%d to %d/%d\n",
			       i, (*connl)->conn[i].i8uLength,
			       (*connl)->conn[i].i16uSrcAddr, (*connl)->conn[i].i8uSrcBit,
			       (*connl)->conn[i].i16uDestAddr, (*connl)->conn[i].i8uDestBit);
	}
#endif

	// Generate Copy List
	*cl = kmalloc(sizeof(piCopylist) + exported_outputs * sizeof(piCopyEntry), GFP_KERNEL);
	(*cl)->i16uNumEntries = exported_outputs;
	d = 0;
	for (i = 0; i < (*ent)->i16uNumEntries && d <= exported_outputs; i++) {
		if ((*ent)->ent[i].i8uType == 0x82) {
			if (d == exported_outputs) {
				pr_err("### internal error 1 ### %d %d  %d %d\n", i, (*ent)->i16uNumEntries, d,
				       exported_outputs);
				exported_outputs = 0;
			} else {
				(*cl)->ent[d].i16uAddr = (*ent)->ent[i].i16uOffset;
				(*cl)->ent[d].i8uBitMask =
				    (0xff >> (8 - (*ent)->ent[i].i16uBitLength)) << (*ent)->ent[i].i8uBitPos;
				(*cl)->ent[d].i16uLength = (*ent)->ent[i].i16uBitLength;
				d++;
			}
		}
	}

	pr_info_config("cl: %2d addr %2d  bit %02x  len %3d\n", i, (*cl)->ent[0].i16uAddr, (*cl)->ent[0].i8uBitMask,
		       (*cl)->ent[0].i16uLength);

	// prüfe ob die Einträge aufsteigend sortiert sind
	for (d = 1; d < exported_outputs; d++) {
		if (((*cl)->ent[d - 1].i16uAddr > (*cl)->ent[d].i16uAddr)
		    || (((*cl)->ent[d - 1].i16uAddr == (*cl)->ent[d].i16uAddr)
			&& ((*cl)->ent[d - 1].i8uBitMask & (*cl)->ent[d].i8uBitMask)
		    )
		    ) {
			pr_err("### internal error 2 ### %d\n", d);
			exported_outputs = 0;
			break;
		} else {
			pr_info_config("cl: %2d addr %2d  bit %02x  len %3d\n", i, (*cl)->ent[d].i16uAddr,
				       (*cl)->ent[d].i8uBitMask, (*cl)->ent[d].i16uLength);
		}
	}

	i = 0;
	for (d = 1; d < exported_outputs; d++) {
		if ((*cl)->ent[i].i16uAddr == (*cl)->ent[d].i16uAddr
		    && (*cl)->ent[i].i16uLength < 8 && (*cl)->ent[d].i16uLength < 8) {
			// fasse die beiden Einträge zusammen
			(*cl)->ent[i].i16uLength += (*cl)->ent[d].i16uLength;
			(*cl)->ent[i].i8uBitMask |= (*cl)->ent[d].i8uBitMask;
		} else if ((*cl)->ent[i].i16uLength >= 8
			   && (*cl)->ent[d].i16uLength >= 8
			   && (*cl)->ent[d].i16uAddr == (*cl)->ent[i].i16uAddr + (*cl)->ent[i].i16uLength / 8) {
			// fasse die beiden Einträge zusammen
			(*cl)->ent[i].i16uLength += (*cl)->ent[d].i16uLength;
		} else {
			// gehe zum nächsten Eintrag
			pr_debug("cl-comp: %2d addr %2d  bit %02x  len %3d\n", i, (*cl)->ent[i].i16uAddr,
				       (*cl)->ent[i].i8uBitMask, (*cl)->ent[i].i16uLength);

			i++;
			(*cl)->ent[i].i16uAddr = (*cl)->ent[d].i16uAddr;
			(*cl)->ent[i].i8uBitMask = (*cl)->ent[d].i8uBitMask;
			(*cl)->ent[i].i16uLength = (*cl)->ent[d].i16uLength;
		}
	}
	if (exported_outputs > 0) {
		pr_debug("cl-comp: %2d addr %2d  bit %02x  len %3d\n", i, (*cl)->ent[i].i16uAddr,
			       (*cl)->ent[i].i8uBitMask, (*cl)->ent[i].i16uLength);
		i++;
	}

	pr_info_config("copylist has %d entries\n", i);
	(*cl)->i16uNumEntries = i;

	free_tree(root_structure);

	return ret;
}

void revpi_set_defaults(unsigned char *mem, piEntries *entries)
{
	unsigned int offset;
	unsigned int type;
	SEntryInfo *ent;
	int i;

	for (i = 0; i < entries->i16uNumEntries; i++) {
		pr_info_aio("addr %2d  type %2x  len %3d  offset %3d+%d  default %x\n",
			    ent->i8uAddress, ent->i8uType, ent->i16uBitLength,
			    ent->i16uOffset, ent->i8uBitPos, ent->i32uDefault);

		ent = &entries->ent[i];
		type = ent->i8uType & ENTRY_INFO_TYPE_MASK;

		/* skip parameters that cant be changed by the user */
		if ((type != ENTRY_INFO_TYPE_OUTPUT) &&
		    (type != ENTRY_INFO_TYPE_MEMORY))
			continue;

		offset = ent->i16uOffset;

		if (ent->i16uBitLength == 1) {
			u8 mask;
			u8 val;
			u8 bit;

			bit = ent->i8uBitPos;

			offset += bit / 8;
			bit %= 8;

			if (offset > (KB_PI_LEN - 1)) {
				pr_err("invalid offset for configuration parameter %u\n",
				       offset);
				continue;
			}
			val = mem[offset];
			mask = (1 << bit);

			if (ent->i32uDefault != 0)
				val |= mask;
			else
				val &= ~mask;

			mem[offset] = val;
		} else if (ent->i16uBitLength == 8) {
			if (offset > (KB_PI_LEN - 1)) {
				pr_err("invalid offset for configuration parameter (%u)\n",
				       offset);
				continue;
			}
			mem[offset] = (u8) ent->i32uDefault;
		} else if (ent->i16uBitLength == 16) {
			u16 *valptr;

			if (offset > (KB_PI_LEN - 2)) {
				pr_err("invalid offset for configuration parameter (%u)\n",
				       offset);
				continue;
			}
			valptr = (u16 *) &mem[offset];
			*valptr = (u16) ent->i32uDefault;
		} else if (ent->i16uBitLength == 32) {
			u32 *valptr;

			if (offset > (KB_PI_LEN - 4)) {
				pr_err("invalid offset for configuration parameter (%u)\n",
				       offset);
				continue;
			}
			valptr = (u32 *) &mem[offset];
			*valptr = ent->i32uDefault;
		}
	}
}
