#include "furi_utils.h"
#include <furi_hal.h>

/**
 * @brief       Buzz the vibration for n ms
*/
void futils_buzz_vibration(uint32_t ms) {
    furi_hal_vibro_on(true);
    furi_delay_ms(ms);
    furi_hal_vibro_on(false);
}

/**
 * @brief       Generate a random number bewteen min and max
 * @param       min  minimum value
 * @param       max  maximum value
 * @return      the random number
*/
uint32_t futils_random_limit(int32_t min, int32_t max) {
    uint32_t rnd = furi_hal_random_get() % (max + 1 - min) + min;
    return rnd;
}

static uint32_t rnd;
static size_t i = 0;
/**
 * @brief       Generate a random boolean value
 * @return      the random value
*/
bool futils_random_bool() {
    if(rnd == 0 || i > 31) {
        i = 0;
        rnd = furi_hal_random_get();
    } else {
        i++;
    }

    return rnd & (1 << i);
}

/**
 * @brief       Reverse an uint8_t array
 * @param       arr  pointer to the array to be reversed
 * @param       size  the array size
*/
void futils_reverse_array_uint8(uint8_t* arr, size_t size) {
    uint8_t temp[size];

    for(size_t i = 0; i < size; i++)
        temp[i] = arr[size - i - 1];

    for(size_t i = 0; i < size; i++)
        arr[i] = temp[i];
}

/**
 * @brief       Initialize a VariableItem
 * @param       item_list       pointer to the VariableItemList
 * @param       label           label for the setting
 * @param       curr_value_text text for the initial value
 * @param       values_count    number of values in the item
 * @param       curr_idx        current value index
 * @param       callback        function callback for value change, NULL if not used
 * @param       context         context for the callback, NULL if not used
 * @return      the pointer to the initialized VariableItem
*/
VariableItem* futils_variable_item_init(
    VariableItemList* item_list,
    const char* label,
    const char* curr_value_text,
    uint8_t values_count,
    uint8_t curr_idx,
    VariableItemChangeCallback callback,
    void* context) {
    VariableItem* item;

    item = variable_item_list_add(item_list, label, values_count, callback, context);
    if(curr_idx > 1) {
        variable_item_set_current_value_index(item, curr_idx);
    }
    variable_item_set_current_value_text(item, curr_value_text);
    return item;
}

/**
 * @brief       Draw an header to the canvas
 * @param       canvas          pointer to the Canvas
 * @param       title           text of the header
 * @param       curr_page       current page number
 * @param       y_pos           y position of the header
*/
void futils_draw_header(
    Canvas* canvas,
    const char* title,
    const int8_t curr_page,
    const int32_t y_pos) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, y_pos, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, y_pos + 1, 126, y_pos + 1);
    char page_num[6];
    snprintf(page_num, sizeof(page_num), "%i", curr_page);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 116, y_pos, page_num);
    canvas_set_font(canvas, FontSecondary);
}

/**
 * @brief      Formats the text box json string.
 * @param      formatted_message    char* holding the formatted string, needs to be freed if not used anymore
 * @param      message              The string to format
 * @param      text_box             Pointer to the TextBox object
*/
void futils_text_box_format_msg(char* formatted_message, const char* message, TextBox* text_box) {
    if(text_box == NULL) {
        FURI_LOG_E(FURI_UTILS_TAG, "Invalid pointer to TextBox");
        return;
    }

    if(message == NULL) {
        FURI_LOG_E(FURI_UTILS_TAG, "Invalid pointer to message");
        return;
    }

    text_box_reset(text_box);

    uint32_t message_length = strlen(message); // Length of the message
    if(message_length > 0) {
        uint32_t i = 0; // Index tracker
        uint32_t formatted_index = 0; // Tracker for where we are in the formatted message
        if(formatted_message) {
            free(formatted_message);
            formatted_message = NULL;
        }

        formatted_message = (char*)malloc((message_length * 5));
        if(!formatted_message) {
            FURI_LOG_E(FURI_UTILS_TAG, "Failed to allocate formatted_message buffer");
            return;
        }

        while(i < message_length) {
            uint32_t max_line_length = 31; // Maximum characters per line
            uint32_t remaining_length = message_length - i; // Remaining characters
            uint32_t line_length = (remaining_length < max_line_length) ? remaining_length :
                                                                          max_line_length;

            // Check for newline character within the current segment
            uint32_t newline_pos = i;
            bool found_newline = false;
            for(; newline_pos < i + line_length && newline_pos < message_length; newline_pos++) {
                if(message[newline_pos] == '\n') {
                    found_newline = true;
                    break;
                }
            }

            if(found_newline) {
                // If newline found, set line_length up to the newline
                line_length = newline_pos - i;
            }

            // Temporary buffer to hold the current line
            char line[32];
            strncpy(line, message + i, line_length);
            line[line_length] = '\0';

            // Move the index forward by the determined line_length
            if(!found_newline) {
                i += line_length;

                // Skip any spaces at the beginning of the next line
                while(i < message_length && message[i] == ' ') {
                    i++;
                }
            }
            uint8_t indent_level = 0;
            // Manually copy the fixed line into the formatted_message buffer, adding newlines where necessary
            for(uint32_t j = 0; j < line_length; j++) {
                switch(line[j]) {
                case '{':
                    formatted_message[formatted_index++] = line[j];
                    formatted_message[formatted_index++] = '\n';
                    indent_level++;
                    for(size_t k = 0; k < indent_level; k++) {
                        formatted_message[formatted_index++] = ' ';
                    }

                    break;
                case ',':
                    formatted_message[formatted_index++] = line[j];
                    formatted_message[formatted_index++] = '\n';
                    for(size_t k = 0; k < indent_level; k++) {
                        formatted_message[formatted_index++] = ' ';
                    }
                    break;
                case '}':
                    formatted_message[formatted_index++] = '\n';
                    for(size_t k = 1; k < indent_level; k++) {
                        formatted_message[formatted_index++] = ' ';
                    }
                    indent_level--;
                    formatted_message[formatted_index++] = line[j];
                    break;
                case '\"':
                    break;
                default:
                    formatted_message[formatted_index++] = line[j];
                    break;
                }
            }
        }

        // Null-terminate the formatted_message
        formatted_message[formatted_index] = '\0';

        // Add the formatted message to the text box
        text_box_set_text(text_box, formatted_message);
        text_box_set_focus(text_box, TextBoxFocusStart);
    } else {
        text_box_set_text(text_box, "No data in payload");
    }
}

#if MEMCCPY
void futils_copy_str(
    char* dest,
    const char* src,
    size_t size,
    const char* dbg_func,
    const char* dbg_name) {
    char* p = memccpy(dest, src, '\0', size);
    if(!p) {
        dest[size - 1] = '\0';
        if(dbg_func && dbg_name) {
            FURI_LOG_I(
                FURI_UTILS_TAG,
                "[%s]: Manually terminating string in [%s], check sizes. Curr. value: %s",
                dbg_func,
                dbg_name,
                dest);
        } else {
            FURI_LOG_I(
                FURI_UTILS_TAG,
                "Manually terminating string in check sizes. Curr. value: %s",
                dest);
        }
    }
}
#else
void futils_copy_str(
    char* dest,
    const char* src,
    size_t size,
    const char* dbg_func,
    const char* dbg_name) {
    int32_t ret = snprintf(dest, size, "%s", src);
    if(ret > (int32_t)size) {
        if(dbg_func && dbg_name) {
            FURI_LOG_I(
                FURI_UTILS_TAG,
                "[%s]: Possibly truncated string in [%s], check sizes. Curr. value: %s",
                dbg_func,
                dbg_name,
                dest);
        } else {
            FURI_LOG_I(
                FURI_UTILS_TAG,
                "Possibly truncated  string in check sizes. Curr. value: %s",
                dest);
        }
    } else if(ret < 0) {
        if(dbg_func && dbg_name) {
            FURI_LOG_I(
                FURI_UTILS_TAG,
                "[%s]: Failed to encode string [%s], check parameters.",
                dbg_func,
                dbg_name);
        } else {
            FURI_LOG_I(FURI_UTILS_TAG, "Failed to encode string, check parameters.");
        }
    }
}
#endif
