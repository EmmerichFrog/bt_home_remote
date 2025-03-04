#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <libs/easy_flipper.h>

#define TAG                 "BT_HOME_REMOTE"
#define BT_APPS_DATA_FOLDER EXT_PATH("apps_data")
#define BT_SETTINGS_FOLDER  \
    BT_APPS_DATA_FOLDER "/" \
                        "bt_home_remote"
#define BT_CONF_FILE_NAME "conf.json"
#define BT_CONF_PATH      BT_SETTINGS_FOLDER "/" BT_CONF_FILE_NAME

#define INPUT_RESET      0xFF
#define DRAW_PERIOD      100U
#define RESET_KEY_PERIOD 200U

typedef enum {
    SubmenuIndexConfigure,
    SubmenuIndexBT,
    SubmenuIndexAbout,
} SubmenuIndex;

typedef enum {
    ViewSubmenu, // The menu when the app starts
    ViewTextInputDeviceName,
    ViewConfigure, // The configuration screen
    ViewBt,
    ViewSghz,
    ViewResp,
    ViewAbout,
} ViewEnum;

typedef enum {
    ConfigTextInputDeviceName,
    ConfigVariableItemBeaconPeriod,
    ConfigVariableItemBeaconDuration,
    ConfigVariableItemRandomizeMac,
} ConfigIndex;

typedef enum {
    EventIdBtRedrawScreen = 3, // Custom event to redraw the screen
    EventIdBtCheckBack = 23,
    EventIdForceBack = 29,
} EventId;

typedef enum {
    BEACON_INACTIVE,
    BEACON_BUSY,
} BeaconStatus;

typedef enum {
    PageFirst,
    PageSecond,
    PageLast,
} PageIndex;

typedef enum {
    ThreadCommStop = 0b00000001,
    ThreadCommUpdData = 0b00000010,
    ThreadCommSendCmd = 0b00000100,
    ThreadCommStopCmd = 0b00001000,
    ThreadCommSendCmdBt = 0b00010000,
} EventCommReq;

typedef struct App {
    ViewDispatcher* view_dispatcher; // Switches between our views
    Submenu* submenu; // The application menu
    VariableItemList* variable_item_list_config; // The configuration screen
    View* view_bt;
    uint8_t current_view;
    Widget* widget_about; // The about screen
    FuriMutex* config_mutex;

    uint32_t config_index;
    char* temp_buffer; // Temporary buffer for text input - common
    TextInput* text_input_device_name;
    char* temp_device_name; // Temporary buffer for text input
    size_t temp_device_name_size; // Size of temporary buffer
    VariableItem* device_name_item;
    VariableItem* beacon_period_item;
    VariableItem* beacon_duration_item;
    VariableItem* randomize_mac_enb_item;

    FuriTimer* timer_draw; // Timer for redrawing the screen
    FuriTimer* timer_reset_key;
    FuriThreadId comm_thread_id;
    FuriThread* comm_thread;
    FuriTimer* timer_comm_upd;
} App;

typedef struct {
    FuriMutex* worker_mutex;
    GapExtraBeaconConfig config;
    GapExtraBeaconConfig prev_config;
    uint8_t prev_data[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t prev_data_len;
    bool prev_active;
    bool prev_exists;
    uint8_t cnt;
    FuriString* mac_address_str;
    InputKey last_input;
    uint8_t status;
    FuriTimer* timer_reset_beacon;
    const char* default_device_name;
    char* device_name;
    size_t device_name_len;
    int8_t curr_page;
    uint16_t beacon_period;
    uint16_t beacon_duration;
    uint8_t beacon_period_idx;
    uint8_t beacon_duration_idx;
    bool randomize_mac_enb;
} BtBeacon;

void variable_item_setting_changed(VariableItem* item);
void conf_text_updated(void* context);
uint32_t navigation_configure_callback(void* context);
void submenu_callback(void* context, uint32_t index);
uint32_t navigation_exit_callback(void* context);
void setting_item_clicked(void* context, uint32_t index);
uint32_t navigation_submenu_callback(void* context);
void view_timer_key_reset_callback(void* context);
bool view_custom_event_callback(uint32_t event, void* context);
void comm_thread_timer_callback(void* context);
