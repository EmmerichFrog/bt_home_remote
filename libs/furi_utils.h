#pragma once
#include <furi.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>

#define FURI_UTILS_TAG "FURI_UTILS"

uint32_t futils_random_limit(int32_t min, int32_t max);
bool futils_random_bool();
void futils_reverse_array_uint8(uint8_t* arr, size_t size);
void futils_buzz_vibration(uint32_t ms);
VariableItem* futils_variable_item_init(
    VariableItemList* item_list,
    const char* label,
    const char* curr_value_text,
    uint8_t values_count,
    uint8_t curr_idx,
    VariableItemChangeCallback callback,
    void* context);
void futils_draw_header(
    Canvas* canvas,
    const char* title,
    const int8_t curr_page,
    const int32_t y_pos);
void futils_text_box_format_msg(char* formatted_message, const char* message, TextBox* text_box);
