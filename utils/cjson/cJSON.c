/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include "cJSON.h"

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

/* Define our own boolean type */
#define true ((cJSON_bool)1)
#define false ((cJSON_bool)0)

/* define our own CJSON internal TRUE/FALSE */
#define CJSON_TRUE true
#define CJSON_FALSE false

/* define what would be more likely to store in the cJSON structure */
#define CJSON_TRUE_VALUE 1
#define CJSON_FALSE_VALUE 0

/* define how we will allocate memory */
static void *(*cJSON_malloc_fn)(size_t sz) = malloc;
static void (*cJSON_free_fn)(void *ptr) = free;

static unsigned char *cJSON_strdup(const unsigned char *string, const size_t length)
{
    unsigned char *copy = (unsigned char*)cJSON_malloc_fn(length + 1);
    if (copy == NULL)
    {
        return NULL;
    }
    if (string != NULL)
    {
        memcpy(copy, string, length);
    }
    copy[length] = '\0';
    return copy;
}

void cJSON_InitHooks(cJSON_Hooks* hooks)
{
    if (hooks == NULL)
    {
        return;
    }

    cJSON_malloc_fn = (hooks->malloc_fn != NULL) ? hooks->malloc_fn : malloc;
    cJSON_free_fn = (hooks->free_fn != NULL) ? hooks->free_fn : free;
}

/* Internal constructor. */
static cJSON *cJSON_New_Item(void)
{
    cJSON *node = (cJSON*)cJSON_malloc_fn(sizeof(cJSON));
    if (node)
    {
        memset(node, '\0', sizeof(cJSON));
    }

    return node;
}

/* Delete a cJSON structure. */
void cJSON_Delete(cJSON *item)
{
    cJSON *next = NULL;
    while (item != NULL)
    {
        next = item->next;
        if (!(item->type & cJSON_IsReference) && (item->child != NULL))
        {
            cJSON_Delete(item->child);
        }
        if (!(item->type & cJSON_IsReference) && (item->valuestring != NULL))
        {
            cJSON_free_fn(item->valuestring);
        }
        if (!(item->type & cJSON_StringIsConst) && (item->string != NULL))
        {
            cJSON_free_fn(item->string);
        }
        cJSON_free_fn(item);
        item = next;
    }
}

/* Parse the input text to generate a number, and populate the result into item. */
static const unsigned char *parse_number(cJSON *item, const unsigned char *num)
{
    double n = 0;
    int sign = 1;
    int scale = 0;
    int subscale = 0;
    int signsubscale = 1;

    if (*num == '-')
    {
        sign = -1;
        num++;
    }
    if (*num == '0')
    {
        num++;
    }
    if (*num >= '1' && *num <= '9')
    {
        do
        {
            n = (n * 10.0) + (*num++ - '0');
        } while (*num >= '0' && *num <= '9');
    }

    if (*num == '.' && num[1] >= '0' && num[1] <= '9')
    {
        num++;
        do
        {
            n = (n * 10.0) + (*num++ - '0');
            scale--;
        } while (*num >= '0' && *num <= '9');
    }

    if (*num == 'e' || *num == 'E')
    {
        num++;
        if (*num == '+')
        {
            num++;
        }
        else if (*num == '-')
        {
            signsubscale = -1;
            num++;
        }
        while (*num >= '0' && *num <= '9')
        {
            subscale = (subscale * 10) + (*num++ - '0');
        }
    }

    n = sign * n * pow(10.0, (double)(scale + subscale * signsubscale));

    item->valuedouble = n;
    item->valueint = (int)n;
    item->type = cJSON_Number;

    return num;
}

/* Render the number nicely from the given item into a string. */
static unsigned char *print_number(const cJSON *item)
{
    unsigned char *str = NULL;
    double d = item->valuedouble;
    if (fabs(((double)item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN)
    {
        str = (unsigned char*)cJSON_malloc_fn(21);
        if (str != NULL)
        {
            sprintf((char*)str, "%d", item->valueint);
        }
    }
    else
    {
        str = (unsigned char*)cJSON_malloc_fn(64);
        if (str != NULL)
        {
            if (fabs(floor(d) - d) <= DBL_EPSILON)
            {
                sprintf((char*)str, "%.0f", d);
            }
            else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9)
            {
                sprintf((char*)str, "%e", d);
            }
            else
            {
                sprintf((char*)str, "%f", d);
            }
        }
    }

    return str;
}

/* Parse the input text into an unescaped cstring, and populate item. */
static const unsigned char *parse_string(cJSON *item, const unsigned char *str)
{
    const unsigned char *ptr = str + 1;
    unsigned char *ptr2;
    unsigned char *out;
    int len = 0;
    unsigned uc, uc2;
    if (*str != '"')
    {
        return NULL;
    }

    while (*ptr != '"' && *ptr != '\0')
    {
        if (*ptr++ == '\\')
        {
            ptr++;
        }
        len++;
    }

    out = (unsigned char*)cJSON_malloc_fn(len + 1);
    if (out == NULL)
    {
        return NULL;
    }

    ptr = str + 1;
    ptr2 = out;
    while (*ptr != '"' && *ptr != '\0')
    {
        if (*ptr != '\\')
        {
            *ptr2++ = *ptr++;
        }
        else
        {
            ptr++;
            switch (*ptr)
            {
                case 'b': *ptr2++ = '\b'; break;
                case 'f': *ptr2++ = '\f'; break;
                case 'n': *ptr2++ = '\n'; break;
                case 'r': *ptr2++ = '\r'; break;
                case 't': *ptr2++ = '\t'; break;
                case 'u':
                    sscanf((const char*)ptr + 1, "%4x", &uc);
                    ptr += 4;
                    if ((uc >= 0xD800) && (uc <= 0xDBFF))
                    {
                        if (ptr[1] != '\\' || ptr[2] != 'u')
                        {
                            cJSON_free_fn(out);
                            return NULL;
                        }
                        sscanf((const char*)ptr + 3, "%4x", &uc2);
                        ptr += 6;
                        uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
                    }
                    len = 4;
                    if (uc < 0x80)
                    {
                        *ptr2++ = (unsigned char)uc;
                    }
                    else if (uc < 0x800)
                    {
                        *ptr2++ = (unsigned char)((uc >> 6) | 0xC0);
                        *ptr2++ = (unsigned char)((uc & 0x3F) | 0x80);
                    }
                    else if (uc < 0x10000)
                    {
                        *ptr2++ = (unsigned char)((uc >> 12) | 0xE0);
                        *ptr2++ = (unsigned char)(((uc >> 6) & 0x3F) | 0x80);
                        *ptr2++ = (unsigned char)((uc & 0x3F) | 0x80);
                    }
                    else
                    {
                        *ptr2++ = (unsigned char)((uc >> 18) | 0xF0);
                        *ptr2++ = (unsigned char)(((uc >> 12) & 0x3F) | 0x80);
                        *ptr2++ = (unsigned char)(((uc >> 6) & 0x3F) | 0x80);
                        *ptr2++ = (unsigned char)((uc & 0x3F) | 0x80);
                    }
                    ptr++;
                    break;
                default:
                    *ptr2++ = *ptr;
                    break;
            }
            ptr++;
        }
    }
    *ptr2 = '\0';

    item->type = cJSON_String;
    item->valuestring = (char*)out;

    return ptr + 1;
}

/* Render the cstring provided to an escaped version that can be printed. */
static unsigned char *print_string_ptr(const unsigned char *str)
{
    const unsigned char *ptr;
    unsigned char *ptr2, *out;
    int len = 0;
    if (str == NULL)
    {
        out = (unsigned char*)cJSON_malloc_fn(3);
        if (out != NULL)
        {
            strcpy((char*)out, "\"\"");
        }
        return out;
    }
    ptr = str;
    while (*ptr)
    {
        if ((unsigned char)*ptr < 32 || *ptr == '"' || *ptr == '\\')
        {
            len++;
        }
        ptr++;
        len++;
    }

    out = (unsigned char*)cJSON_malloc_fn(len + 3);
    if (out == NULL)
    {
        return NULL;
    }

    ptr2 = out;
    *ptr2++ = '"';
    ptr = str;
    while (*ptr)
    {
        if ((unsigned char)*ptr > 31 && *ptr != '"' && *ptr != '\\')
        {
            *ptr2++ = *ptr++;
        }
        else
        {
            *ptr2++ = '\\';
            switch (*ptr)
            {
                case '\\': *ptr2++ = '\\'; break;
                case '"': *ptr2++ = '"'; break;
                case '\b': *ptr2++ = 'b'; break;
                case '\f': *ptr2++ = 'f'; break;
                case '\n': *ptr2++ = 'n'; break;
                case '\r': *ptr2++ = 'r'; break;
                case '\t': *ptr2++ = 't'; break;
                default:
                    sprintf((char*)ptr2, "u%04x", *ptr);
                    ptr2 += 5;
                    break;
            }
            ptr++;
        }
    }
    *ptr2++ = '"';
    *ptr2++ = '\0';

    return out;
}

static unsigned char *print_string(const cJSON *item)
{
    return print_string_ptr((const unsigned char*)item->valuestring);
}

/* Predeclare these prototypes. */
static const unsigned char *parse_value(cJSON *item, const unsigned char *value);
static unsigned char *print_value(const cJSON *item, int depth, int fmt);
static const unsigned char *parse_array(cJSON *item, const unsigned char *value);
static unsigned char *print_array(const cJSON *item, int depth, int fmt);
static const unsigned char *parse_object(cJSON *item, const unsigned char *value);
static unsigned char *print_object(const cJSON *item, int depth, int fmt);

/* Utility to skip whitespace. */
static const unsigned char *skip(const unsigned char *in)
{
    while (in && *in && (unsigned char)*in <= 32)
    {
        in++;
    }

    return in;
}

/* Parse an object - create a new root, and populate. */
cJSON *cJSON_ParseWithOpts(const char *value, const char **return_parse_end, int require_null_terminated)
{
    const unsigned char *end = NULL;
    cJSON *c = cJSON_New_Item();
    if (c == NULL)
    {
        return NULL;
    }

    end = parse_value(c, skip((const unsigned char*)value));
    if (end == NULL)
    {
        cJSON_Delete(c);
        return NULL;
    }

    if (require_null_terminated)
    {
        end = skip(end);
        if (*end)
        {
            cJSON_Delete(c);
            return NULL;
        }
    }
    if (return_parse_end)
    {
        *return_parse_end = (const char*)end;
    }

    return c;
}

cJSON *cJSON_Parse(const char *value)
{
    return cJSON_ParseWithOpts(value, NULL, 0);
}

cJSON *cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, int require_null_terminated)
{
    const unsigned char *end = NULL;
    cJSON *c = cJSON_New_Item();
    if (c == NULL)
    {
        return NULL;
    }

    end = parse_value(c, skip((const unsigned char*)value));
    if (end == NULL)
    {
        cJSON_Delete(c);
        return NULL;
    }

    if (require_null_terminated)
    {
        end = skip(end);
        if ((size_t)(end - (const unsigned char*)value) != buffer_length)
        {
            cJSON_Delete(c);
            return NULL;
        }
    }
    if (return_parse_end)
    {
        *return_parse_end = (const char*)end;
    }

    return c;
}

cJSON *cJSON_ParseWithLength(const char *value, size_t buffer_length)
{
    return cJSON_ParseWithLengthOpts(value, buffer_length, NULL, 0);
}

static const unsigned char *parse_value(cJSON *item, const unsigned char *value)
{
    if (value == NULL)
    {
        return NULL;
    }

    if (!strncmp((const char*)value, "null", 4))
    {
        item->type = cJSON_NULL;
        return value + 4;
    }
    if (!strncmp((const char*)value, "false", 5))
    {
        item->type = cJSON_False;
        return value + 5;
    }
    if (!strncmp((const char*)value, "true", 4))
    {
        item->type = cJSON_True;
        item->valueint = 1;
        return value + 4;
    }
    if (*value == '"')
    {
        return parse_string(item, value);
    }
    if (*value == '-' || (*value >= '0' && *value <= '9'))
    {
        return parse_number(item, value);
    }
    if (*value == '[')
    {
        return parse_array(item, value);
    }
    if (*value == '{')
    {
        return parse_object(item, value);
    }

    return NULL;
}

/* Render a value to text. */
static unsigned char *print_value(const cJSON *item, int depth, int fmt)
{
    unsigned char *out = NULL;
    if (item == NULL)
    {
        return NULL;
    }

    switch ((item->type) & 0xFF)
    {
        case cJSON_NULL: out = cJSON_strdup((unsigned char*)"null", 4); break;
        case cJSON_False: out = cJSON_strdup((unsigned char*)"false", 5); break;
        case cJSON_True: out = cJSON_strdup((unsigned char*)"true", 4); break;
        case cJSON_Number: out = print_number(item); break;
        case cJSON_String: out = print_string(item); break;
        case cJSON_Raw:
            if (item->valuestring) {
                out = cJSON_strdup((unsigned char*)item->valuestring, strlen(item->valuestring));
            }
            break;
        case cJSON_Array: out = print_array(item, depth, fmt); break;
        case cJSON_Object: out = print_object(item, depth, fmt); break;
        default: break;
    }

    return out;
}

/* Build an array from input text. */
static const unsigned char *parse_array(cJSON *item, const unsigned char *value)
{
    cJSON *child = NULL;
    if (*value != '[')
    {
        return NULL;
    }

    item->type = cJSON_Array;
    value = skip(value + 1);
    if (*value == ']')
    {
        return value + 1;
    }

    item->child = child = cJSON_New_Item();
    if (item->child == NULL)
    {
        return NULL;
    }
    value = skip(parse_value(child, skip(value)));
    if (value == NULL)
    {
        return NULL;
    }

    while (*value == ',')
    {
        cJSON *new_item = NULL;
        if (!(new_item = cJSON_New_Item()))
        {
            return NULL;
        }
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_value(child, skip(value + 1)));
        if (value == NULL)
        {
            return NULL;
        }
    }

    if (*value == ']')
    {
        return value + 1;
    }

    return NULL;
}

/* Render an array to text */
static unsigned char *print_array(const cJSON *item, int depth, int fmt)
{
    unsigned char **entries;
    unsigned char *out = NULL;
    unsigned char *ptr = NULL;
    int len = 5;
    int numentries = 0;
    cJSON *child = item->child;
    while (child)
    {
        numentries++;
        child = child->next;
    }

    if (numentries == 0)
    {
        out = (unsigned char*)cJSON_malloc_fn(3);
        if (out)
        {
            strcpy((char*)out, "[]");
        }
        return out;
    }

    entries = (unsigned char**)cJSON_malloc_fn(numentries * sizeof(unsigned char*));
    if (entries == NULL)
    {
        return NULL;
    }
    memset(entries, 0, numentries * sizeof(unsigned char*));

    child = item->child;
    numentries = 0;
    while (child)
    {
        entries[numentries++] = print_value(child, depth + 1, fmt);
        child = child->next;
    }

    for (int i = 0; i < numentries; i++)
    {
        if (entries[i])
        {
            len += strlen((char*)entries[i]) + 2 + (fmt ? 1 : 0);
        }
    }

    out = (unsigned char*)cJSON_malloc_fn(len);
    if (out == NULL)
    {
        for (int i = 0; i < numentries; i++)
        {
            cJSON_free_fn(entries[i]);
        }
        cJSON_free_fn(entries);
        return NULL;
    }

    *out = '[';
    ptr = out + 1;
    for (int i = 0; i < numentries; i++)
    {
        if (entries[i])
        {
            size_t item_len = strlen((char*)entries[i]);
            memcpy(ptr, entries[i], item_len);
            ptr += item_len;
            if (i != numentries - 1)
            {
                *ptr++ = ',';
            }
            if (fmt)
            {
                *ptr++ = ' ';
            }
            cJSON_free_fn(entries[i]);
        }
    }
    *ptr++ = ']';
    *ptr++ = '\0';

    cJSON_free_fn(entries);
    return out;
}

/* Build an object from the text. */
static const unsigned char *parse_object(cJSON *item, const unsigned char *value)
{
    cJSON *child = NULL;
    if (*value != '{')
    {
        return NULL;
    }

    item->type = cJSON_Object;
    value = skip(value + 1);
    if (*value == '}')
    {
        return value + 1;
    }

    item->child = child = cJSON_New_Item();
    if (item->child == NULL)
    {
        return NULL;
    }
    value = skip(parse_string(child, skip(value)));
    if (value == NULL)
    {
        return NULL;
    }
    child->string = child->valuestring;
    child->valuestring = NULL;
    if (*value != ':')
    {
        return NULL;
    }
    value = skip(parse_value(child, skip(value + 1)));
    if (value == NULL)
    {
        return NULL;
    }

    while (*value == ',')
    {
        cJSON *new_item = cJSON_New_Item();
        if (new_item == NULL)
        {
            return NULL;
        }
        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_string(child, skip(value + 1)));
        if (value == NULL)
        {
            return NULL;
        }
        child->string = child->valuestring;
        child->valuestring = NULL;
        if (*value != ':')
        {
            return NULL;
        }
        value = skip(parse_value(child, skip(value + 1)));
        if (value == NULL)
        {
            return NULL;
        }
    }

    if (*value == '}')
    {
        return value + 1;
    }

    return NULL;
}

/* Render an object to text. */
static unsigned char *print_object(const cJSON *item, int depth, int fmt)
{
    unsigned char **entries = NULL;
    unsigned char **names = NULL;
    unsigned char *out = NULL;
    unsigned char *ptr = NULL;
    int len = 7;
    int numentries = 0;
    int i = 0;
    cJSON *child = item->child;

    while (child)
    {
        numentries++;
        child = child->next;
    }

    if (!numentries)
    {
        out = (unsigned char*)cJSON_malloc_fn(3);
        if (out)
        {
            strcpy((char*)out, "{}");
        }
        return out;
    }

    entries = (unsigned char**)cJSON_malloc_fn(numentries * sizeof(unsigned char*));
    if (entries == NULL)
    {
        return NULL;
    }
    names = (unsigned char**)cJSON_malloc_fn(numentries * sizeof(unsigned char*));
    if (names == NULL)
    {
        cJSON_free_fn(entries);
        return NULL;
    }
    memset(entries, 0, sizeof(unsigned char*) * numentries);
    memset(names, 0, sizeof(unsigned char*) * numentries);

    child = item->child;
    numentries = 0;
    while (child)
    {
        names[numentries] = print_string_ptr((unsigned char*)child->string);
        entries[numentries] = print_value(child, depth + 1, fmt);
        numentries++;
        child = child->next;
    }

    for (i = 0; i < numentries; i++)
    {
        if (entries[i])
        {
            len += strlen((char*)entries[i]) + 2 + (fmt ? 2 + depth : 0);
        }
        if (names[i])
        {
            len += strlen((char*)names[i]) + 2 + (fmt ? 2 + depth : 0);
        }
    }

    out = (unsigned char*)cJSON_malloc_fn(len);
    if (out == NULL)
    {
        for (i = 0; i < numentries; i++)
        {
            cJSON_free_fn(names[i]);
            cJSON_free_fn(entries[i]);
        }
        cJSON_free_fn(names);
        cJSON_free_fn(entries);
        return NULL;
    }

    *out = '{';
    ptr = out + 1;
    if (fmt)
    {
        *ptr++ = '\n';
    }

    for (i = 0; i < numentries; i++)
    {
        if (fmt)
        {
            for (int j = 0; j < depth + 1; j++)
            {
                *ptr++ = '\t';
            }
        }
        if (names[i])
        {
            size_t name_len = strlen((char*)names[i]);
            memcpy(ptr, names[i], name_len);
            ptr += name_len;
        }
        *ptr++ = ':';
        if (fmt)
        {
            *ptr++ = '\t';
        }
        if (entries[i])
        {
            size_t entry_len = strlen((char*)entries[i]);
            memcpy(ptr, entries[i], entry_len);
            ptr += entry_len;
        }
        if (i != numentries - 1)
        {
            *ptr++ = ',';
        }
        if (fmt)
        {
            *ptr++ = '\n';
        }
        cJSON_free_fn(names[i]);
        cJSON_free_fn(entries[i]);
    }

    cJSON_free_fn(names);
    cJSON_free_fn(entries);

    if (fmt)
    {
        for (i = 0; i < depth; i++)
        {
            *ptr++ = '\t';
        }
    }
    *ptr++ = '}';
    *ptr++ = '\0';

    return out;
}

int cJSON_GetArraySize(const cJSON *array)
{
    cJSON *child = array ? array->child : NULL;
    int size = 0;
    while (child)
    {
        size++;
        child = child->next;
    }
    return size;
}

cJSON *cJSON_GetArrayItem(const cJSON *array, int index)
{
    cJSON *child = array ? array->child : NULL;
    while (child && index > 0)
    {
        index--;
        child = child->next;
    }
    return child;
}

cJSON *cJSON_GetObjectItem(const cJSON * const object, const char * const string)
{
    return cJSON_GetObjectItemCaseSensitive(object, string);
}

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string)
{
    cJSON *child = object ? object->child : NULL;
    while (child && child->string && (strcmp(child->string, string) != 0))
    {
        child = child->next;
    }
    return child;
}

cJSON_bool cJSON_HasObjectItem(const cJSON *object, const char *string)
{
    return cJSON_GetObjectItemCaseSensitive(object, string) ? true : false;
}

const char *cJSON_GetErrorPtr(void)
{
    return NULL;
}

cJSON_bool cJSON_IsInvalid(const cJSON * const item) { return (item == NULL) ? true : ((item->type & 0xFF) == cJSON_Invalid) ? true : false; }
cJSON_bool cJSON_IsFalse(const cJSON * const item) { return (item == NULL) ? false : ((item->type & 0xFF) == cJSON_False) ? true : false; }
cJSON_bool cJSON_IsTrue(const cJSON * const item) { return (item == NULL) ? false : ((item->type & 0xFF) == cJSON_True) ? true : false; }
cJSON_bool cJSON_IsBool(const cJSON * const item) { return cJSON_IsFalse(item) || cJSON_IsTrue(item); }
cJSON_bool cJSON_IsNull(const cJSON * const item) { return (item == NULL) ? false : ((item->type & 0xFF) == cJSON_NULL) ? true : false; }
cJSON_bool cJSON_IsNumber(const cJSON * const item) { return (item == NULL) ? false : ((item->type & 0xFF) == cJSON_Number) ? true : false; }
cJSON_bool cJSON_IsString(const cJSON * const item) { return (item == NULL) ? false : ((item->type & 0xFF) == cJSON_String) ? true : false; }
cJSON_bool cJSON_IsArray(const cJSON * const item) { return (item == NULL) ? false : ((item->type & 0xFF) == cJSON_Array) ? true : false; }
cJSON_bool cJSON_IsObject(const cJSON * const item) { return (item == NULL) ? false : ((item->type & 0xFF) == cJSON_Object) ? true : false; }
cJSON_bool cJSON_IsRaw(const cJSON * const item) { return (item == NULL) ? false : ((item->type & 0xFF) == cJSON_Raw) ? true : false; }

cJSON *cJSON_CreateNull(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_NULL; return item; }
cJSON *cJSON_CreateTrue(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_True; return item; }
cJSON *cJSON_CreateFalse(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_False; return item; }
cJSON *cJSON_CreateBool(cJSON_bool boolean) { cJSON *item = cJSON_New_Item(); if (item) item->type = boolean ? cJSON_True : cJSON_False; return item; }
cJSON *cJSON_CreateNumber(double num) { cJSON *item = cJSON_New_Item(); if (item) { item->type = cJSON_Number; item->valuedouble = num; item->valueint = (int)num; } return item; }
cJSON *cJSON_CreateString(const char *string) { cJSON *item = cJSON_New_Item(); if (item) { item->type = cJSON_String; item->valuestring = (char*)cJSON_strdup((const unsigned char*)string, strlen(string)); } return item; }
cJSON *cJSON_CreateRaw(const char *raw) { cJSON *item = cJSON_New_Item(); if (item) { item->type = cJSON_Raw; item->valuestring = (char*)cJSON_strdup((const unsigned char*)raw, strlen(raw)); } return item; }
cJSON *cJSON_CreateArray(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Array; return item; }
cJSON *cJSON_CreateObject(void) { cJSON *item = cJSON_New_Item(); if (item) item->type = cJSON_Object; return item; }

cJSON *cJSON_CreateIntArray(const int *numbers, int count) { cJSON *n = NULL; cJSON *a = cJSON_CreateArray(); for (int i = 0; a && i < count; i++) { n = cJSON_CreateNumber(numbers[i]); if (!n) { cJSON_Delete(a); return NULL; } cJSON_AddItemToArray(a, n); } return a; }
cJSON *cJSON_CreateFloatArray(const float *numbers, int count) { cJSON *n = NULL; cJSON *a = cJSON_CreateArray(); for (int i = 0; a && i < count; i++) { n = cJSON_CreateNumber(numbers[i]); if (!n) { cJSON_Delete(a); return NULL; } cJSON_AddItemToArray(a, n); } return a; }
cJSON *cJSON_CreateDoubleArray(const double *numbers, int count) { cJSON *n = NULL; cJSON *a = cJSON_CreateArray(); for (int i = 0; a && i < count; i++) { n = cJSON_CreateNumber(numbers[i]); if (!n) { cJSON_Delete(a); return NULL; } cJSON_AddItemToArray(a, n); } return a; }
cJSON *cJSON_CreateStringArray(const char *const *strings, int count) { cJSON *n = NULL; cJSON *a = cJSON_CreateArray(); for (int i = 0; a && i < count; i++) { n = cJSON_CreateString(strings[i]); if (!n) { cJSON_Delete(a); return NULL; } cJSON_AddItemToArray(a, n); } return a; }

cJSON_bool cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
    cJSON *child = NULL;
    if (item == NULL)
    {
        return false;
    }
    child = array->child;
    if (child == NULL)
    {
        array->child = item;
    }
    else
    {
        while (child->next)
        {
            child = child->next;
        }
        child->next = item;
        item->prev = child;
    }
    return true;
}

cJSON_bool cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
    if (item == NULL)
    {
        return false;
    }
    if (item->string)
    {
        cJSON_free_fn(item->string);
    }
    item->string = (char*)cJSON_strdup((const unsigned char*)string, strlen(string));
    return cJSON_AddItemToArray(object, item);
}

cJSON_bool cJSON_AddItemToObjectCS(cJSON *object, const char *string, cJSON *item)
{
    if (item == NULL)
    {
        return false;
    }
    if (!(item->type & cJSON_StringIsConst) && item->string)
    {
        cJSON_free_fn(item->string);
    }
    item->string = (char*)string;
    item->type |= cJSON_StringIsConst;
    return cJSON_AddItemToArray(object, item);
}

cJSON_bool cJSON_AddItemReferenceToArray(cJSON *array, cJSON *item)
{
    if (item == NULL)
    {
        return false;
    }
    item->type |= cJSON_IsReference;
    return cJSON_AddItemToArray(array, item);
}

cJSON_bool cJSON_AddItemReferenceToObject(cJSON *object, const char *string, cJSON *item)
{
    if (item == NULL)
    {
        return false;
    }
    item->type |= cJSON_IsReference;
    return cJSON_AddItemToObject(object, string, item);
}

cJSON *cJSON_DetachItemViaPointer(cJSON *parent, cJSON * const item)
{
    if (item == NULL)
    {
        return NULL;
    }
    if (item->prev)
    {
        item->prev->next = item->next;
    }
    if (item->next)
    {
        item->next->prev = item->prev;
    }
    if (item == parent->child)
    {
        parent->child = item->next;
    }
    item->prev = item->next = NULL;
    return item;
}

cJSON *cJSON_DetachItemFromArray(cJSON *array, int which)
{
    cJSON *c = array ? array->child : NULL;
    while (c && which > 0)
    {
        c = c->next;
        which--;
    }
    if (!c)
    {
        return NULL;
    }
    return cJSON_DetachItemViaPointer(array, c);
}

void cJSON_DeleteItemFromArray(cJSON *array, int which)
{
    cJSON_Delete(cJSON_DetachItemFromArray(array, which));
}

cJSON *cJSON_DetachItemFromObject(cJSON *object, const char *string)
{
    return cJSON_DetachItemFromObjectCaseSensitive(object, string);
}

cJSON *cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
    cJSON *c = object ? object->child : NULL;
    while (c && c->string && (strcmp(c->string, string) != 0))
    {
        c = c->next;
    }
    if (!c)
    {
        return NULL;
    }
    return cJSON_DetachItemViaPointer(object, c);
}

void cJSON_DeleteItemFromObject(cJSON *object, const char *string)
{
    cJSON_Delete(cJSON_DetachItemFromObject(object, string));
}

void cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
    cJSON_Delete(cJSON_DetachItemFromObjectCaseSensitive(object, string));
}

cJSON_bool cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem)
{
    cJSON *c = array->child;
    while (c && which > 0)
    {
        c = c->next;
        which--;
    }
    if (!c)
    {
        return cJSON_AddItemToArray(array, newitem);
    }
    newitem->next = c;
    newitem->prev = c->prev;
    c->prev = newitem;
    if (c == array->child)
    {
        array->child = newitem;
    }
    else
    {
        newitem->prev->next = newitem;
    }
    return true;
}

cJSON_bool cJSON_ReplaceItemViaPointer(cJSON *parent, cJSON * const item, cJSON *replacement)
{
    if (replacement == NULL || item == NULL)
    {
        return false;
    }

    replacement->next = item->next;
    replacement->prev = item->prev;

    if (replacement->next)
    {
        replacement->next->prev = replacement;
    }
    if (item == parent->child)
    {
        parent->child = replacement;
    }
    else
    {
        replacement->prev->next = replacement;
    }

    item->next = item->prev = NULL;
    cJSON_Delete(item);
    return true;
}

cJSON_bool cJSON_ReplaceItemInArray(cJSON *array, int which, cJSON *newitem)
{
    cJSON *c = array->child;
    while (c && which > 0)
    {
        c = c->next;
        which--;
    }
    if (!c)
    {
        return false;
    }
    return cJSON_ReplaceItemViaPointer(array, c, newitem);
}

cJSON_bool cJSON_ReplaceItemInObject(cJSON *object, const char *string, cJSON *newitem)
{
    return cJSON_ReplaceItemInObjectCaseSensitive(object, string, newitem);
}

cJSON_bool cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem)
{
    cJSON *c = object->child;
    while (c && c->string && (strcmp(c->string, string) != 0))
    {
        c = c->next;
    }
    if (!c)
    {
        return false;
    }

    newitem->string = (char*)cJSON_strdup((const unsigned char*)string, strlen(string));
    cJSON_ReplaceItemViaPointer(object, c, newitem);
    return true;
}

cJSON *cJSON_Duplicate(const cJSON *item, cJSON_bool recurse)
{
    cJSON *newitem = NULL;
    cJSON *child = NULL;
    cJSON *next = NULL;
    cJSON *newchild = NULL;

    if (item == NULL)
    {
        return NULL;
    }

    newitem = cJSON_New_Item();
    if (newitem == NULL)
    {
        return NULL;
    }

    newitem->type = item->type & (~cJSON_IsReference);
    newitem->valueint = item->valueint;
    newitem->valuedouble = item->valuedouble;

    if (item->valuestring)
    {
        newitem->valuestring = (char*)cJSON_strdup((unsigned char*)item->valuestring, strlen(item->valuestring));
        if (newitem->valuestring == NULL)
        {
            cJSON_Delete(newitem);
            return NULL;
        }
    }

    if (item->string)
    {
        newitem->string = (char*)cJSON_strdup((unsigned char*)item->string, strlen(item->string));
        if (newitem->string == NULL)
        {
            cJSON_Delete(newitem);
            return NULL;
        }
    }

    if (!recurse)
    {
        return newitem;
    }

    child = item->child;
    while (child)
    {
        newchild = cJSON_Duplicate(child, true);
        if (newchild == NULL)
        {
            cJSON_Delete(newitem);
            return NULL;
        }
        if (next != NULL)
        {
            next->next = newchild;
            newchild->prev = next;
            next = newchild;
        }
        else
        {
            newitem->child = newchild;
            newchild->prev = NULL;
            next = newchild;
        }
        child = child->next;
    }

    return newitem;
}

void cJSON_Minify(char *json)
{
    char *into = json;
    while (*json)
    {
        if (*json == ' ' || *json == '\t' || *json == '\r' || *json == '\n')
        {
            json++;
        }
        else if (*json == '/' && json[1] == '/')
        {
            while (*json && *json != '\n')
            {
                json++;
            }
        }
        else if (*json == '/' && json[1] == '*')
        {
            while (*json && !(*json == '*' && json[1] == '/'))
            {
                json++;
            }
            json += 2;
        }
        else if (*json == '"')
        {
            *into++ = *json++;
            while (*json && *json != '"')
            {
                if (*json == '\\')
                {
                    *into++ = *json++;
                }
                *into++ = *json++;
            }
            *into++ = *json++;
        }
        else
        {
            *into++ = *json++;
        }
    }

    *into = '\0';
}

cJSON *cJSON_AddObjectToObject(cJSON *object, const char *name)
{
    cJSON *item = cJSON_CreateObject();
    if (cJSON_AddItemToObject(object, name, item))
    {
        return item;
    }
    cJSON_Delete(item);
    return NULL;
}

cJSON *cJSON_AddArrayToObject(cJSON *object, const char *name)
{
    cJSON *item = cJSON_CreateArray();
    if (cJSON_AddItemToObject(object, name, item))
    {
        return item;
    }
    cJSON_Delete(item);
    return NULL;
}

cJSON *cJSON_CreateStringReference(const char *string)
{
    cJSON *item = cJSON_New_Item();
    if (item)
    {
        item->type = cJSON_String | cJSON_IsReference;
        item->valuestring = (char*)string;
    }
    return item;
}

cJSON *cJSON_CreateObjectReference(const cJSON *child)
{
    cJSON *item = cJSON_New_Item();
    if (item)
    {
        item->type = cJSON_Object | cJSON_IsReference;
        item->child = (cJSON*)child;
    }
    return item;
}

cJSON *cJSON_CreateArrayReference(const cJSON *child)
{
    cJSON *item = cJSON_New_Item();
    if (item)
    {
        item->type = cJSON_Array | cJSON_IsReference;
        item->child = (cJSON*)child;
    }
    return item;
}

cJSON *cJSON_CreateIntArrayReference(const int *numbers, int count)
{
    cJSON *item = cJSON_New_Item();
    if (item)
    {
        item->type = cJSON_Array | cJSON_IsReference;
        item->child = cJSON_CreateIntArray(numbers, count)->child;
    }
    return item;
}

cJSON *cJSON_CreateFloatArrayReference(const float *numbers, int count)
{
    cJSON *item = cJSON_New_Item();
    if (item)
    {
        item->type = cJSON_Array | cJSON_IsReference;
        item->child = cJSON_CreateFloatArray(numbers, count)->child;
    }
    return item;
}

cJSON *cJSON_CreateDoubleArrayReference(const double *numbers, int count)
{
    cJSON *item = cJSON_New_Item();
    if (item)
    {
        item->type = cJSON_Array | cJSON_IsReference;
        item->child = cJSON_CreateDoubleArray(numbers, count)->child;
    }
    return item;
}

cJSON *cJSON_CreateStringArrayReference(const char *const *strings, int count)
{
    cJSON *item = cJSON_New_Item();
    if (item)
    {
        item->type = cJSON_Array | cJSON_IsReference;
        item->child = cJSON_CreateStringArray(strings, count)->child;
    }
    return item;
}

void *cJSON_malloc(size_t size) { return cJSON_malloc_fn(size); }
void cJSON_free(void *object) { cJSON_free_fn(object); }

char *cJSON_Print(const cJSON *item) { return (char*)print_value(item, 0, 1); }
char *cJSON_PrintUnformatted(const cJSON *item) { return (char*)print_value(item, 0, 0); }
char *cJSON_PrintBuffered(const cJSON *item, int prebuffer, int fmt)
{
    unsigned char *printbuf = NULL;
    if (prebuffer < 1) prebuffer = 256;
    printbuf = (unsigned char*)cJSON_malloc_fn(prebuffer);
    if (!printbuf) return NULL;
    memset(printbuf, 0, prebuffer);
    return (char*)print_value(item, 0, fmt);
}

int cJSON_PrintPreallocated(cJSON *item, char *buf, const int len, const int fmt)
{
    char *printed = NULL;
    if (!buf || !len) return false;
    printed = (char*)print_value(item, 0, fmt);
    if (!printed) return false;
    if ((int)strlen(printed) >= len)
    {
        cJSON_free_fn(printed);
        return false;
    }
    strcpy(buf, printed);
    cJSON_free_fn(printed);
    return true;
}
