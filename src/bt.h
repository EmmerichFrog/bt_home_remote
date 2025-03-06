#pragma once
#include "app.h"
#include <furi.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#define BT_TAG "BT"

typedef enum {
    BTHomeShortPress = 0x01,
    BTHomeLongPress = 0x04,
} BTHomeEventType;

void bt_enter_callback(void* context);
void bt_exit_callback(void* context);
void bt_draw_callback(Canvas* canvas, void* model);
bool bt_input_callback(InputEvent* event, void* context);
void timer_beacon_reset_callback(void* context);
bool make_packet(BtBeacon* bt_model, uint8_t* _size, uint8_t** _packet);
void randomize_mac(uint8_t address[EXTRA_BEACON_MAC_ADDR_SIZE]);
void pretty_print_mac(FuriString* mac_str, uint8_t address[EXTRA_BEACON_MAC_ADDR_SIZE]);
int32_t bt_comm_worker(void* context);
