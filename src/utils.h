#pragma once
#include "app.h"

#define DEFAULT_BEACON_PERIOD   20U
#define DEFAULT_BEACON_DURATION 1000U

int32_t random_limit(int32_t min, int32_t max);
void reverse_array_uint8(uint8_t* arr, size_t size);
void buzz_vibration(uint32_t ms);
void load_settings(App* app);
void save_settings(App* app);
VariableItem* variable_item_init(
    VariableItemList* item_list,
    const char* label,
    const char* curr_value_text,
    uint8_t values_count,
    uint8_t curr_idx,
    VariableItemChangeCallback callback,
    void* context);
void draw_header(Canvas* canvas, const char* title, const int8_t curr_page, const int32_t y_pos);
