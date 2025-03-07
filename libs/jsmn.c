/*
 * MIT License
 *
 * Copyright (c) 2010 Serge Zaitsev
 *
 * [License text continues...]
 */

#include "libs/furi_utils.h"
#include <libs/jsmn.h>
#include <stdlib.h>
#include <string.h>

/**
 * Allocates a fresh unused token from the token pool.
 */
static jsmntok_t*
    jsmn_alloc_token(jsmn_parser* parser, jsmntok_t* tokens, const size_t num_tokens) {
    jsmntok_t* tok;

    if(parser->toknext >= num_tokens) {
        return NULL;
    }
    tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
#ifdef JSMN_PARENT_LINKS
    tok->parent = -1;
#endif
    return tok;
}

/**
 * Fills token type and boundaries.
 */
static void
    jsmn_fill_token(jsmntok_t* token, const jsmntype_t type, const int start, const int end) {
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

/**
 * Fills next available token with JSON primitive.
 */
static int jsmn_parse_primitive(
    jsmn_parser* parser,
    const char* js,
    const size_t len,
    jsmntok_t* tokens,
    const size_t num_tokens) {
    jsmntok_t* token;
    int start;

    start = parser->pos;

    for(; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
        switch(js[parser->pos]) {
#ifndef JSMN_STRICT
        /* In strict mode primitive must be followed by "," or "}" or "]" */
        case ':':
#endif
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case ',':
        case ']':
        case '}':
            goto found;
        default:
            /* to quiet a warning from gcc*/
            break;
        }
        if(js[parser->pos] < 32 || js[parser->pos] >= 127) {
            parser->pos = start;
            return JSMN_ERROR_INVAL;
        }
    }
#ifdef JSMN_STRICT
    /* In strict mode primitive must be followed by a comma/object/array */
    parser->pos = start;
    return JSMN_ERROR_PART;
#endif

found:
    if(tokens == NULL) {
        parser->pos--;
        return 0;
    }
    token = jsmn_alloc_token(parser, tokens, num_tokens);
    if(token == NULL) {
        parser->pos = start;
        return JSMN_ERROR_NOMEM;
    }
    jsmn_fill_token(token, JSMN_PRIMITIVE, start, parser->pos);
#ifdef JSMN_PARENT_LINKS
    token->parent = parser->toksuper;
#endif
    parser->pos--;
    return 0;
}

/**
 * Fills next token with JSON string.
 */
static int jsmn_parse_string(
    jsmn_parser* parser,
    const char* js,
    const size_t len,
    jsmntok_t* tokens,
    const size_t num_tokens) {
    jsmntok_t* token;

    int start = parser->pos;

    /* Skip starting quote */
    parser->pos++;

    for(; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
        char c = js[parser->pos];

        /* Quote: end of string */
        if(c == '\"') {
            if(tokens == NULL) {
                return 0;
            }
            token = jsmn_alloc_token(parser, tokens, num_tokens);
            if(token == NULL) {
                parser->pos = start;
                return JSMN_ERROR_NOMEM;
            }
            jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);
#ifdef JSMN_PARENT_LINKS
            token->parent = parser->toksuper;
#endif
            return 0;
        }

        /* Backslash: Quoted symbol expected */
        if(c == '\\' && parser->pos + 1 < len) {
            int i;
            parser->pos++;
            switch(js[parser->pos]) {
            /* Allowed escaped symbols */
            case '\"':
            case '/':
            case '\\':
            case 'b':
            case 'f':
            case 'r':
            case 'n':
            case 't':
                break;
            /* Allows escaped symbol \uXXXX */
            case 'u':
                parser->pos++;
                for(i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++) {
                    /* If it isn't a hex character we have an error */
                    if(!((js[parser->pos] >= 48 && js[parser->pos] <= 57) || /* 0-9 */
                         (js[parser->pos] >= 65 && js[parser->pos] <= 70) || /* A-F */
                         (js[parser->pos] >= 97 && js[parser->pos] <= 102))) { /* a-f */
                        parser->pos = start;
                        return JSMN_ERROR_INVAL;
                    }
                    parser->pos++;
                }
                parser->pos--;
                break;
            /* Unexpected symbol */
            default:
                parser->pos = start;
                return JSMN_ERROR_INVAL;
            }
        }
    }
    parser->pos = start;
    return JSMN_ERROR_PART;
}

/**
 * Create JSON parser over an array of tokens
 */
void jsmn_init(jsmn_parser* parser) {
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
}

/**
 * Parse JSON string and fill tokens.
 */
int jsmn_parse(
    jsmn_parser* parser,
    const char* js,
    const size_t len,
    jsmntok_t* tokens,
    const unsigned int num_tokens) {
    int r;
    int i;
    jsmntok_t* token;
    int count = parser->toknext;

    for(; parser->pos < len && js[parser->pos] != '\0'; parser->pos++) {
        char c;
        jsmntype_t type;

        c = js[parser->pos];
        switch(c) {
        case '{':
        case '[':
            count++;
            if(tokens == NULL) {
                break;
            }
            token = jsmn_alloc_token(parser, tokens, num_tokens);
            if(token == NULL) {
                return JSMN_ERROR_NOMEM;
            }
            if(parser->toksuper != -1) {
                jsmntok_t* t = &tokens[parser->toksuper];
#ifdef JSMN_STRICT
                /* In strict mode an object or array can't become a key */
                if(t->type == JSMN_OBJECT) {
                    return JSMN_ERROR_INVAL;
                }
#endif
                t->size++;
#ifdef JSMN_PARENT_LINKS
                token->parent = parser->toksuper;
#endif
            }
            token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
            token->start = parser->pos;
            parser->toksuper = parser->toknext - 1;
            break;
        case '}':
        case ']':
            if(tokens == NULL) {
                break;
            }
            type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
#ifdef JSMN_PARENT_LINKS
            if(parser->toknext < 1) {
                return JSMN_ERROR_INVAL;
            }
            token = &tokens[parser->toknext - 1];
            for(;;) {
                if(token->start != -1 && token->end == -1) {
                    if(token->type != type) {
                        return JSMN_ERROR_INVAL;
                    }
                    token->end = parser->pos + 1;
                    parser->toksuper = token->parent;
                    break;
                }
                if(token->parent == -1) {
                    if(token->type != type || parser->toksuper == -1) {
                        return JSMN_ERROR_INVAL;
                    }
                    break;
                }
                token = &tokens[token->parent];
            }
#else
            for(i = parser->toknext - 1; i >= 0; i--) {
                token = &tokens[i];
                if(token->start != -1 && token->end == -1) {
                    if(token->type != type) {
                        return JSMN_ERROR_INVAL;
                    }
                    parser->toksuper = -1;
                    token->end = parser->pos + 1;
                    break;
                }
            }
            /* Error if unmatched closing bracket */
            if(i == -1) {
                return JSMN_ERROR_INVAL;
            }
            for(; i >= 0; i--) {
                token = &tokens[i];
                if(token->start != -1 && token->end == -1) {
                    parser->toksuper = i;
                    break;
                }
            }
#endif
            break;
        case '\"':
            r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
            if(r < 0) {
                return r;
            }
            count++;
            if(parser->toksuper != -1 && tokens != NULL) {
                tokens[parser->toksuper].size++;
            }
            break;
        case '\t':
        case '\r':
        case '\n':
        case ' ':
            break;
        case ':':
            parser->toksuper = parser->toknext - 1;
            break;
        case ',':
            if(tokens != NULL && parser->toksuper != -1 &&
               tokens[parser->toksuper].type != JSMN_ARRAY &&
               tokens[parser->toksuper].type != JSMN_OBJECT) {
#ifdef JSMN_PARENT_LINKS
                parser->toksuper = tokens[parser->toksuper].parent;
#else
                for(i = parser->toknext - 1; i >= 0; i--) {
                    if(tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT) {
                        if(tokens[i].start != -1 && tokens[i].end == -1) {
                            parser->toksuper = i;
                            break;
                        }
                    }
                }
#endif
            }
            break;
#ifdef JSMN_STRICT
        /* In strict mode primitives are: numbers and booleans */
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 't':
        case 'f':
        case 'n':
            /* And they must not be keys of the object */
            if(tokens != NULL && parser->toksuper != -1) {
                const jsmntok_t* t = &tokens[parser->toksuper];
                if(t->type == JSMN_OBJECT || (t->type == JSMN_STRING && t->size != 0)) {
                    return JSMN_ERROR_INVAL;
                }
            }
#else
        /* In non-strict mode every unquoted value is a primitive */
        default:
#endif
            r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
            if(r < 0) {
                return r;
            }
            count++;
            if(parser->toksuper != -1 && tokens != NULL) {
                tokens[parser->toksuper].size++;
            }
            break;

#ifdef JSMN_STRICT
        /* Unexpected char in strict mode */
        default:
            return JSMN_ERROR_INVAL;
#endif
        }
    }

    if(tokens != NULL) {
        for(i = parser->toknext - 1; i >= 0; i--) {
            /* Unmatched opened object or array */
            if(tokens[i].start != -1 && tokens[i].end == -1) {
                return JSMN_ERROR_PART;
            }
        }
    }

    return count;
}

// Helper function to create a JSON object
char* jsmn(const char* key, const char* value) {
    int length = strlen(key) + strlen(value) + 8; // Calculate required length
    char* result = malloc(length * sizeof(char)); // Allocate memory
    if(result == NULL) {
        return NULL; // Handle memory allocation failure
    }
    snprintf(result, length, "{\"%s\":\"%s\"}", key, value);
    return result; // Caller is responsible for freeing this memory
}

// Helper function to compare JSON keys
int jsoneq(const char* json, jsmntok_t* tok, const char* s) {
    if(tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
       strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

// Return the value of the key in the JSON data -
// 26/02/2025 - updated by EmmeFrog
char* get_json_value(const char* restrict key, const char* restrict json_data, uint32_t max_tokens) {
    // Parse the JSON feed
    if(json_data != NULL) {
        jsmn_parser parser;
        jsmn_init(&parser);

        // Allocate tokens array on the heap
        jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * max_tokens);
        if(tokens == NULL) {
            FURI_LOG_E("JSMM.H", "Failed to allocate memory for JSON tokens.");
            return NULL;
        }

        int ret = jsmn_parse(&parser, json_data, strlen(json_data), tokens, max_tokens);
        if(ret < 0) {
            // Handle parsing errors
            FURI_LOG_E("JSMM.H", "Failed to parse JSON: %d", ret);
            free(tokens);
            return NULL;
        }

        // Ensure that the root element is an object
        if(ret < 1 || tokens[0].type != JSMN_OBJECT) {
            FURI_LOG_E("JSMM.H", "Root element is not an object.");
            free(tokens);
            return NULL;
        }

        // Loop through the tokens to find the key
        for(int i = 1; i < ret; i++) {
            if(jsoneq(json_data, &tokens[i], key) == 0) {
                // We found the key. Now, return the associated value.
                size_t length = tokens[i + 1].end - tokens[i + 1].start;
                char* value = malloc(length + 1);
                if(value == NULL) {
                    FURI_LOG_E("JSMM.H", "Failed to allocate memory for value.");
                    free(tokens);
                    return NULL;
                }
                futils_copy_str(
                    value, json_data + tokens[i + 1].start, length + 1, "get_json_value", value);
                free(tokens); // Free the token array
                return value; // Return the extracted value
            }
        }

        // Free the token array if key was not found
        free(tokens);
    } else {
        FURI_LOG_E("JSMM.H", "JSON data is NULL");
    }
    FURI_LOG_E("JSMM.H", "Failed to find the key in the JSON.");
    return NULL; // Return NULL if something goes wrong
}

// Revised get_json_array_value function
char* get_json_array_value(char* key, uint32_t index, char* json_data, uint32_t max_tokens) {
    // Retrieve the array string for the given key
    char* array_str = get_json_value(key, json_data, max_tokens);
    if(array_str == NULL) {
        FURI_LOG_E("JSMM.H", "Failed to get array for key: %s", key);
        return NULL;
    }

    // Initialize the JSON parser
    jsmn_parser parser;
    jsmn_init(&parser);

    // Allocate memory for JSON tokens
    jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * max_tokens);
    if(tokens == NULL) {
        FURI_LOG_E("JSMM.H", "Failed to allocate memory for JSON tokens.");
        free(array_str);
        return NULL;
    }

    // Parse the JSON array
    int ret = jsmn_parse(&parser, array_str, strlen(array_str), tokens, max_tokens);
    if(ret < 0) {
        FURI_LOG_E("JSMM.H", "Failed to parse JSON array: %d", ret);
        free(tokens);
        free(array_str);
        return NULL;
    }

    // Ensure the root element is an array
    if(ret < 1 || tokens[0].type != JSMN_ARRAY) {
        FURI_LOG_E("JSMM.H", "Value for key '%s' is not an array.", key);
        free(tokens);
        free(array_str);
        return NULL;
    }

    // Check if the index is within bounds
    if(index >= (uint32_t)tokens[0].size) {
        FURI_LOG_E(
            "JSMM.H",
            "Index %lu out of bounds for array with size %d.",
            (unsigned long)index,
            tokens[0].size);
        free(tokens);
        free(array_str);
        return NULL;
    }

    // Locate the token corresponding to the desired array element
    int current_token = 1; // Start after the array token
    for(uint32_t i = 0; i < index; i++) {
        if(tokens[current_token].type == JSMN_OBJECT) {
            // For objects, skip all key-value pairs
            current_token += 1 + 2 * tokens[current_token].size;
        } else if(tokens[current_token].type == JSMN_ARRAY) {
            // For nested arrays, skip all elements
            current_token += 1 + tokens[current_token].size;
        } else {
            // For primitive types, simply move to the next token
            current_token += 1;
        }

        // Safety check to prevent out-of-bounds
        if(current_token >= ret) {
            FURI_LOG_E("JSMM.H", "Unexpected end of tokens while traversing array.");
            free(tokens);
            free(array_str);
            return NULL;
        }
    }

    // Extract the array element
    jsmntok_t element = tokens[current_token];
    int length = element.end - element.start;
    char* value = malloc(length + 1);
    if(value == NULL) {
        FURI_LOG_E("JSMM.H", "Failed to allocate memory for array element.");
        free(tokens);
        free(array_str);
        return NULL;
    }

    // Copy the element value to a new string
    strncpy(value, array_str + element.start, length);
    value[length] = '\0'; // Null-terminate the string

    // Clean up
    free(tokens);
    free(array_str);

    return value;
}

// Revised get_json_array_values function with correct token skipping
char** get_json_array_values(char* key, char* json_data, uint32_t max_tokens, int* num_values) {
    // Retrieve the array string for the given key
    char* array_str = get_json_value(key, json_data, max_tokens);
    if(array_str == NULL) {
        FURI_LOG_E("JSMM.H", "Failed to get array for key: %s", key);
        return NULL;
    }

    // Initialize the JSON parser
    jsmn_parser parser;
    jsmn_init(&parser);

    // Allocate memory for JSON tokens
    jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * max_tokens); // Allocate on the heap
    if(tokens == NULL) {
        FURI_LOG_E("JSMM.H", "Failed to allocate memory for JSON tokens.");
        free(array_str);
        return NULL;
    }

    // Parse the JSON array
    int ret = jsmn_parse(&parser, array_str, strlen(array_str), tokens, max_tokens);
    if(ret < 0) {
        FURI_LOG_E("JSMM.H", "Failed to parse JSON array: %d", ret);
        free(tokens);
        free(array_str);
        return NULL;
    }

    // Ensure the root element is an array
    if(tokens[0].type != JSMN_ARRAY) {
        FURI_LOG_E("JSMM.H", "Value for key '%s' is not an array.", key);
        free(tokens);
        free(array_str);
        return NULL;
    }

    // Allocate memory for the array of values (maximum possible)
    int array_size = tokens[0].size;
    char** values = malloc(array_size * sizeof(char*));
    if(values == NULL) {
        FURI_LOG_E("JSMM.H", "Failed to allocate memory for array of values.");
        free(tokens);
        free(array_str);
        return NULL;
    }

    int actual_num_values = 0;

    // Traverse the array and extract all object values
    int current_token = 1; // Start after the array token
    for(int i = 0; i < array_size; i++) {
        if(current_token >= ret) {
            FURI_LOG_E("JSMM.H", "Unexpected end of tokens while traversing array.");
            break;
        }

        jsmntok_t element = tokens[current_token];

        if(element.type != JSMN_OBJECT) {
            FURI_LOG_E("JSMM.H", "Array element %d is not an object, skipping.", i);
            // Skip this element
            current_token += 1;
            continue;
        }

        int length = element.end - element.start;

        // Allocate a new string for the value and copy the data
        char* value = malloc(length + 1);
        if(value == NULL) {
            FURI_LOG_E("JSMM.H", "Failed to allocate memory for array element.");
            for(int j = 0; j < actual_num_values; j++) {
                free(values[j]);
            }
            free(values);
            free(tokens);
            free(array_str);
            return NULL;
        }

        strncpy(value, array_str + element.start, length);
        value[length] = '\0'; // Null-terminate the string

        values[actual_num_values] = value;
        actual_num_values++;

        // Skip all tokens related to this object to avoid misparsing
        current_token += 1 + (2 * element.size); // Each key-value pair consumes two tokens
    }

    *num_values = actual_num_values;

    // Reallocate the values array to actual_num_values if necessary
    if(actual_num_values < array_size) {
        char** reduced_values = realloc(values, actual_num_values * sizeof(char*));
        if(reduced_values != NULL) {
            values = reduced_values;
        }

        // Free the remaining values
        for(int i = actual_num_values; i < array_size; i++) {
            free(values[i]);
        }
    }

    // Clean up
    free(tokens);
    free(array_str);
    return values;
}

// EmmeFrog helper functions
/**
 * @brief      Clean the json and copy to a string
 * @param      json  FuriJson*
 * @return     True on success
*/
bool furi_json_to_text(FuriJson* json) {
    size_t size = strlen(furi_string_get_cstr(json->_json)) + 1;
    json->to_text = realloc(json->to_text, size);
    futils_copy_str(
        json->to_text,
        furi_string_get_cstr(json->_json),
        size,
        "furi_json_to_text",
        "json->to_text");

    char buffer[size];
    futils_copy_str(buffer, json->to_text, size, "furi_json_to_text", "buffer");
    // Removes the %s
    int32_t ret = snprintf(json->to_text, size, buffer, "");
    bool success = (ret > 0 && ret < (int32_t)size) ? true : false;
    return success;
}

/**
 * @brief      Allocate the FuriJson*
 * @return     FuriJson*
*/
FuriJson* furi_json_alloc() {
    FuriJson* json = malloc(sizeof(FuriJson));
    json->_json = furi_string_alloc();
    furi_string_set_str(json->_json, "{%s\n}");
    size_t size = strlen(furi_string_get_cstr(json->_json)) + 1;
    json->to_text = malloc(size);
    futils_copy_str(
        json->to_text, furi_string_get_cstr(json->_json), size, "furi_json_alloc", "json->to_text");
    furi_json_to_text(json);
    return json;
}

/**
 * @brief      Free the FuriJson*
 * @param      json  FuriJson*
*/
void furi_json_free(FuriJson* json) {
    furi_string_free(json->_json);
    free(json->to_text);
    free(json);
}

/**
 * @brief      Add Key:Value pair to the json
 * @param      json  FuriJson*
 * @param      key   const char*
 * @param      value const char*
 * @return     True on success
*/
bool furi_json_add_entry_s(FuriJson* json, const char* key, const char* value) {
    bool success = false;
    char json_entry_fmt[18];
    // If == 0 we don't need the comma
    if(json->entries == 0) {
        const char json_entry_fmt_init[] = "\n\t\"%s\": \"%s\"%s";
        futils_copy_str(
            json_entry_fmt,
            json_entry_fmt_init,
            sizeof(json_entry_fmt_init),
            "furi_json_add_entry_s",
            "json_entry_fmt");
    } else {
        const char json_entry_fmt_init[] = ",\n\t\"%s\": \"%s\"%s";
        futils_copy_str(
            json_entry_fmt,
            json_entry_fmt_init,
            sizeof(json_entry_fmt_init),
            "furi_json_add_entry_s",
            "json_entry_fmt");
    }

    size_t tot_size = sizeof(json_entry_fmt) + strlen(key) + strlen(value) + 1;
    char buffer[tot_size];
    int32_t ret = snprintf(buffer, tot_size, json_entry_fmt, key, value, "%s");
    if(ret > 0 && ret < (int32_t)tot_size) {
        size_t size = strlen(furi_string_get_cstr(json->_json)) + 1;
        char buffer2[size];
        futils_copy_str(
            buffer2, furi_string_get_cstr(json->_json), size, "furi_json_add_entry_s", "buffer2");
        furi_string_printf(json->_json, buffer2, buffer);
        json->entries++;
        success = furi_json_to_text(json);
    } else {
        FURI_LOG_E(
            FURI_JSON_TAG,
            "[furi_json_add_entry_s] Key: [%s] Value: [%s] - failed to encode string.",
            key,
            value);
    }
    return success;
}

/**
 * @brief      Add Key:Value pair to the json
 * @param      json  FuriJson*
 * @param      key   const char*
 * @param      value uint32_t
 * @return     True on success
*/
bool furi_json_add_entry_u(FuriJson* json, const char* key, uint32_t value) {
    bool success = false;
    char json_entry_fmt[18];
    // If == 0 we don't need the comma
    if(json->entries == 0) {
        const char json_entry_fmt_init[] = "\n\t\"%s\": \"%lu\"%s";
        futils_copy_str(
            json_entry_fmt,
            json_entry_fmt_init,
            sizeof(json_entry_fmt_init),
            "furi_json_add_entry_u",
            "json_entry_fmt");
    } else {
        const char json_entry_fmt_init[] = ",\n\t\"%s\": \"%lu\"%s";
        futils_copy_str(
            json_entry_fmt,
            json_entry_fmt_init,
            sizeof(json_entry_fmt_init),
            "furi_json_add_entry_u",
            "json_entry_fmt");
    }

    size_t tot_size = sizeof(json_entry_fmt) + strlen(key) + snprintf(NULL, 0, "%lu", value) + 1;
    char buffer[tot_size];
    int32_t ret = snprintf(buffer, tot_size, json_entry_fmt, key, value, "%s");
    if(ret > 0 && ret < (int32_t)tot_size) {
        size_t size = strlen(furi_string_get_cstr(json->_json)) + 1;
        char buffer2[size];
        futils_copy_str(
            buffer2, furi_string_get_cstr(json->_json), size, "furi_json_add_entry_u", "buffer2");

        furi_string_printf(json->_json, buffer2, buffer);

        json->entries++;
        success = furi_json_to_text(json);
    } else {
        FURI_LOG_E(
            FURI_JSON_TAG,
            "[furi_json_add_entry_u] Key: [%s] Value: [%lu] - failed to encode string.",
            key,
            value);
    }
    return success;
}
