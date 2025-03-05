#pragma once
#include "app.h"

uint32_t futils_random_limit(int32_t min, int32_t max);
bool futils_random_bool();
void futils_reverse_array_uint8(uint8_t* arr, size_t size);
void futils_buzz_vibration(uint32_t ms);
void load_settings(App* app);
void save_settings(App* app);
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
void futils_extract_payload(
    const char* string,
    FuriString* dest,
    const char* beginning,
    const char* ending);
void futils_text_box_format_msg(char* formatted_message, const char* message, TextBox* text_box);
