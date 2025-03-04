#include "app.h"
#include "src/alloc_free.h"
#include "src/utils.h"

const uint16_t beacon_period_values[4] = {20, 50, 75, 100};
const char* beacon_period_names[4] = {"20ms", "50ms", "75ms", "100ms"};
const uint16_t beacon_duration_values[4] = {1000, 2000, 5000, 10000};
const char* beacon_duration_names[4] = {"1s", "2s", "5s", "10s"};
const char* randomize_mac_names[2] = {"Off", "On"};

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *             indicate that we want to exit the application.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t navigation_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return ViewSubmenu to
 *             indicate that we want to navigate to the submenu.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t navigation_submenu_callback(void* context) {
    UNUSED(context);
    return ViewSubmenu;
}

/**
 * @brief      Callback for returning to configure screen.
 * @details    This function is called when user press back button.  We return ViewConfigure to
 *             indicate that we want to navigate to the configure screen.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t navigation_configure_callback(void* context) {
    UNUSED(context);
    return ViewConfigure;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - App object.
 * @param      index     The SubmenuIndex item that was clicked.
*/
void submenu_callback(void* context, uint32_t index) {
    App* app = (App*)context;
    switch(index) {
    case SubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewConfigure);
        break;
    case SubmenuIndexBT:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewBt);
        break;
    case SubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewAbout);
        break;
    default:
        break;
    }
}

/**
 * @brief      Function called when one of the VariableItem is updated.
 * @param      item  The pointer to the VariableItem object.
*/
void variable_item_setting_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    BtBeacon* bt_model = view_get_model(app->view_bt);

    uint8_t index = variable_item_list_get_selected_item_index(app->variable_item_list_config);
    FURI_LOG_I(TAG, "Index %u", index);
    switch(index) {
    case ConfigVariableItemBeaconPeriod:
        bt_model->beacon_period_idx = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(
            item, beacon_period_names[bt_model->beacon_period_idx]);
        bt_model->beacon_period = beacon_period_values[bt_model->beacon_period_idx];
        break;
    case ConfigVariableItemBeaconDuration:
        bt_model->beacon_duration_idx = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(
            item, beacon_duration_names[bt_model->beacon_duration_idx]);
        bt_model->beacon_duration = beacon_duration_values[bt_model->beacon_duration_idx];
        break;
    case ConfigVariableItemRandomizeMac:
        bt_model->randomize_mac_enb = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(
            item, randomize_mac_names[variable_item_get_current_value_index(item)]);
        break;

    default:
        FURI_LOG_E(TAG, "Unhandled index [%u] in variable_item_setting_changed.", index);
        return;
    }
    save_settings(app);
}

/**
 * @brief      Function called when one of the texts is updated.
 * @param      context  The context - App object.
*/
void conf_text_updated(void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);

    switch(app->config_index) {
    // Frame
    case ConfigTextInputDeviceName:
        char* p =
            memccpy(bt_model->device_name, app->temp_device_name, '\0', app->temp_device_name_size);
        if(!p) {
            bt_model->device_name[app->temp_device_name_size] = '\0';
            FURI_LOG_I(
                TAG,
                "[conf_text_updated]: Manually terminating string in [bt_model->device_name], check sizes");
        }
        variable_item_set_current_value_text(app->device_name_item, bt_model->device_name);
        break;
    default:
        FURI_LOG_E(TAG, "Unhandled index [%lu] in conf_text_updated.", app->config_index);
        return;
    }

    save_settings(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewConfigure);
}

/**
 * @brief      Callback when item in configuration screen is clicked.
 * @details    This function is called when user clicks OK on an item in the configuration screen.
 *             If the item clicked is a text field then we switch to the text input screen.
 * @param      context  The context - App object.
 * @param      index - The index of the item that was clicked.
*/
void setting_item_clicked(void* context, uint32_t index) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);
    size_t size;
    char* string;
    TextInput* text_input;
    uint8_t view_index;

    switch(index) {
    // Frame
    case ConfigTextInputDeviceName:
        app->temp_buffer = app->temp_device_name;
        size = app->temp_device_name_size;
        string = bt_model->device_name;
        text_input = app->text_input_device_name;
        text_input_set_minimum_length(text_input, 0);
        text_input_set_header_text(text_input, "Device Name:"),
            view_index = ViewTextInputDeviceName;
        break;
    default:
        // don't handle presses that are not explicitly defined in the enum
        return;
    }
    // Initialize temp_buffer with existing string
    char* p = memccpy(app->temp_buffer, string, '\0', size);
    if(!p) {
        app->temp_buffer[size] = '\0';
        FURI_LOG_I(
            TAG,
            "[settings_item_clicked]: Manually terminating string in [temp_buffer], check sizes");
    }
    // Configure the text input
    text_input_set_result_callback(
        text_input, conf_text_updated, app, app->temp_buffer, size, false);
    // Set the previous callback to return to Configure screen
    view_set_previous_callback(text_input_get_view(text_input), navigation_configure_callback);
    app->config_index = index;
    // Show text input dialog.
    view_dispatcher_switch_to_view(app->view_dispatcher, view_index);
}

/**
 * @brief      Callback to fire data update.
 * @details    This function is called when the timer ticks.
 * @param      context  The context - App object.
*/
void comm_thread_timer_callback(void* context) {
    App* app = (App*)context;
    furi_thread_flags_set(app->comm_thread_id, ThreadCommUpdData);
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - EventId value.
 * @param      context  The context - App object.
*/
bool view_custom_event_callback(uint32_t event, void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);

    switch(event) {
    // Redraw the screen, called by the timer callback every timer tick
    case EventIdBtRedrawScreen: {
        with_view_model(app->view_bt, BtBeacon * model, { UNUSED(model); }, true);
        return true;
    }
    case EventIdBtCheckBack:
        if(bt_model->status != BEACON_BUSY) {
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        }
        return true;
    case EventIdForceBack:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Callback of the timer_reset to update the btton pressed graphics.
 * @details    This function is called when the timer_reset ticks.
 * @param      context  The context - App object.
*/
void view_timer_key_reset_callback(void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);
    bt_model->last_input = INPUT_RESET;
}

/**
 * @brief      Main function for  application.
 * @details    This function is the entry point for the  application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t bt_home_remote_app(void* _p) {
    UNUSED(_p);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
