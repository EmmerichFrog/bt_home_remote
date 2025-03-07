#include "app.h"
#include "alloc_free.h"
#include "bt.h"
#include "libs/furi_utils.h"

static const char* DEVICE_NAME_LABEL = "Device Name";
static const char* BEACON_PERIOD_LABEL = "Adv. Interval";
static const char* BEACON_DURATION_LABEL = "Beacon Duration";
static const char* RANDOMIZE_MAC_LABEL = "Randomize MAC";

extern const uint16_t beacon_period_values[4];
extern const char* beacon_period_names[4];
extern uint16_t beacon_duration_values[4];
extern char* beacon_duration_names[4];
extern const char* randomize_mac_names[2];

/**
 * @brief      Allocate the application.
 * @details    This function allocates the application resources.
 * @return     App object.
*/
App* app_alloc() {
    App* app = malloc(sizeof(App));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->config_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "BT Home Remote");
    submenu_add_item(app->submenu, "Config", SubmenuIndexConfigure, submenu_callback, app);
    submenu_add_item(app->submenu, "BT Home Remote", SubmenuIndexBT, submenu_callback, app);
    submenu_add_item(app->submenu, "About", SubmenuIndexAbout, submenu_callback, app);

    view_set_previous_callback(submenu_get_view(app->submenu), navigation_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, ViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);

    // BT Test
    app->view_bt = view_alloc();
    view_set_draw_callback(app->view_bt, bt_draw_callback);
    view_set_input_callback(app->view_bt, bt_input_callback);
    view_set_enter_callback(app->view_bt, bt_enter_callback);
    view_set_exit_callback(app->view_bt, bt_exit_callback);
    view_set_context(app->view_bt, app);
    view_set_custom_callback(app->view_bt, view_custom_event_callback);
    view_allocate_model(app->view_bt, ViewModelTypeLockFree, sizeof(BtBeacon));
    app->temp_device_name_size = MAX_NAME_LENGHT + 1;
    app->temp_device_name = malloc(app->temp_device_name_size + 1);
    app->text_input_device_name = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        ViewTextInputDeviceName,
        text_input_get_view(app->text_input_device_name));
    BtBeacon* bt_model = view_get_model(app->view_bt);
    bt_model->last_input = INPUT_RESET;
    bt_model->mac_address_str = furi_string_alloc();
    bt_model->cnt = 0;
    bt_model->worker_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    bt_model->config.adv_channel_map = GapAdvChannelMapAll;
    bt_model->config.adv_power_level = GapAdvPowerLevel_6dBm;
    bt_model->config.address_type = GapAddressTypePublic;
    bt_model->default_name_len = strlen(furi_hal_version_get_device_name_ptr());
    bt_model->default_device_name = furi_hal_version_get_device_name_ptr();
    bt_model->device_name = malloc(MAX_NAME_LENGHT + 1);
    futils_copy_str(
        bt_model->device_name,
        bt_model->default_device_name,
        bt_model->default_name_len + 1,
        "app_alloc",
        "bt_model->device_name");

    bt_model->device_name[bt_model->default_name_len] = '\0';
    bt_model->device_name_len = strlen(bt_model->default_device_name) + 1;
    bt_model->curr_page = PageFirst;
    FURI_LOG_I(
        BT_TAG, "Device Name: %s, Size: %u", bt_model->device_name, bt_model->device_name_len);
    const GapExtraBeaconConfig* prev_cfg_ptr = furi_hal_bt_extra_beacon_get_config();
    if(prev_cfg_ptr) {
        bt_model->prev_exists = true;
        memcpy(&bt_model->prev_config, prev_cfg_ptr, sizeof(bt_model->prev_config));
    } else {
        bt_model->prev_exists = false;
    }

    bt_model->prev_data_len = furi_hal_bt_extra_beacon_get_data(bt_model->prev_data);
    bt_model->prev_active = furi_hal_bt_extra_beacon_is_active();

    view_dispatcher_add_view(app->view_dispatcher, ViewBt, app->view_bt);

    load_settings(app);

    // Variable Items
    app->variable_item_list_config = variable_item_list_alloc();
    variable_item_list_reset(app->variable_item_list_config);
    variable_item_list_set_header(app->variable_item_list_config, "Configuration");

    // Device Name
    app->device_name_item = futils_variable_item_init(
        app->variable_item_list_config, DEVICE_NAME_LABEL, bt_model->device_name, 1, 0, NULL, NULL);
    // Beacon Period
    app->beacon_period_item = futils_variable_item_init(
        app->variable_item_list_config,
        BEACON_PERIOD_LABEL,
        beacon_period_names[bt_model->beacon_period_idx],
        COUNT_OF(beacon_period_names),
        bt_model->beacon_period_idx,
        variable_item_setting_changed,
        app);
    // Beacon Duration
    app->beacon_duration_item = futils_variable_item_init(
        app->variable_item_list_config,
        BEACON_DURATION_LABEL,
        beacon_duration_names[bt_model->beacon_duration_idx],
        COUNT_OF(beacon_duration_names),
        bt_model->beacon_duration_idx,
        variable_item_setting_changed,
        app);
    // Randomize MAC
    app->randomize_mac_enb_item = futils_variable_item_init(
        app->variable_item_list_config,
        RANDOMIZE_MAC_LABEL,
        randomize_mac_names[bt_model->randomize_mac_enb],
        COUNT_OF(randomize_mac_names),
        bt_model->randomize_mac_enb,
        variable_item_setting_changed,
        app);

    variable_item_list_set_enter_callback(
        app->variable_item_list_config, setting_item_clicked, app);
    view_set_previous_callback(
        variable_item_list_get_view(app->variable_item_list_config), navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        ViewConfigure,
        variable_item_list_get_view(app->variable_item_list_config));

    // About
    easy_flipper_set_widget(
        &app->widget_about, ViewAbout, NULL, navigation_submenu_callback, &app->view_dispatcher);
    widget_reset(app->widget_about);
    char about_text[] =
        "You need a device that\nunderstand the BT Home\n specification.\nThe app was tested on Home Assistant. \
        \nIf the bluetooth integration\nis enabled correctly, BT Home\ndevices should be \nautomatically found by HA\n(HA documentation:\nwww.home-assistant.io/integrations/bthome/). \
        \nAfter that, it can\nbe used as a normal BT Home\nbutton in Home Assistand\nAutomations. \
        \n\nIn the config page the device\nname can be customized.\nThe default beacon settings\nshould be fine, but depending\non the BT receiver they might\nneed to be adjusted. \
        \nTo Do: \
        \n  - allow for custom MAC,\n  right now a fixed MAC or\n  random MAC are available;\
        \n  - release on the Flipper App\n  Store";
    widget_add_text_scroll_element(app->widget_about, 0, 0, 128, 64, about_text);

    return app;
}

/**
 * @brief      Free the  application.
 * @details    This function frees the  application resources.
 * @param      app  The  application object.
*/
void app_free(App* app) {
    BtBeacon* bt_model = view_get_model(app->view_bt);

    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_check(furi_hal_bt_extra_beacon_stop());
    }

    if(bt_model->prev_exists) {
        furi_check(furi_hal_bt_extra_beacon_set_config(&bt_model->prev_config));
        furi_check(
            furi_hal_bt_extra_beacon_set_data(bt_model->prev_data, bt_model->prev_data_len));
    }

    if(bt_model->prev_active) {
        furi_check(furi_hal_bt_extra_beacon_start());
    }

    furi_mutex_free(app->config_mutex);
    furi_mutex_free(bt_model->worker_mutex);

    furi_string_free(bt_model->mac_address_str);
    free(bt_model->device_name);

    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInputDeviceName);
    text_input_free(app->text_input_device_name);
    free(app->temp_device_name);

    view_dispatcher_remove_view(app->view_dispatcher, ViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewConfigure);
    variable_item_list_free(app->variable_item_list_config);
    view_dispatcher_remove_view(app->view_dispatcher, ViewBt);
    view_free(app->view_bt);
    view_dispatcher_remove_view(app->view_dispatcher, ViewAbout);
    widget_free(app->widget_about);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}
