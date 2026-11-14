/**
 * WiFi Configuration Screen Implementation
 */

#include "ui_wifi.h"
#include "ui_theme.h"
#include "ui_settings.h"
#include "ui_keyboard.h"
#include "buzzer.h"
#include "wifi_storage.h"
#include "wifi_time.h"

// ============================================
// Static UI elements
// ============================================
static lv_obj_t* screen_wifi = nullptr;
static lv_obj_t* lbl_ssid_val = nullptr;
static lv_obj_t* lbl_pass_val = nullptr;
static lv_obj_t* lbl_status = nullptr;
static lv_obj_t* lbl_ip = nullptr;
static lv_obj_t* btn_test = nullptr;
static lv_obj_t* spinner = nullptr;
static bool wifi_visible = false;

// Buffers for keyboard input
static char ssid_buffer[WIFI_SSID_MAX_LEN + 1] = {0};
static char pass_buffer[WIFI_PASSWORD_MAX_LEN + 1] = {0};

// Which field is being edited
static enum { EDIT_NONE, EDIT_SSID, EDIT_PASSWORD } edit_mode = EDIT_NONE;

// ============================================
// Forward declarations
// ============================================
static void on_back_click(lv_event_t* e);
static void on_ssid_click(lv_event_t* e);
static void on_pass_click(lv_event_t* e);
static void on_save_click(lv_event_t* e);
static void on_test_click(lv_event_t* e);

// ============================================
// Update status display
// ============================================
static void update_status_display() {
    if (!lbl_status || !lbl_ip) return;

    WiFiStatus status = wifi_time_get_status();
    const char* status_text = "";
    lv_color_t status_color = COLOR_GRAY;

    switch (status) {
        case WiFiStatus::DISCONNECTED:
            status_text = "Disconnesso";
            status_color = COLOR_WHITE;
            break;
        case WiFiStatus::CONNECTING:
            status_text = "Connessione...";
            status_color = COLOR_YELLOW;
            break;
        case WiFiStatus::CONNECTED:
            status_text = "Connesso";
            status_color = COLOR_GREEN;
            break;
        case WiFiStatus::FAILED:
            status_text = "Errore";
            status_color = COLOR_RED;
            break;
    }

    lv_label_set_text(lbl_status, status_text);
    lv_obj_set_style_text_color(lbl_status, status_color, 0);

    // Show IP if connected
    if (wifi_time_is_connected()) {
        lv_label_set_text(lbl_ip, wifi_time_get_ip());
        lv_obj_clear_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);
        if (spinner) lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    } else if (status == WiFiStatus::CONNECTING) {
        lv_label_set_text(lbl_ip, "");
        if (spinner) lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(lbl_ip, "");
        lv_obj_add_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);
        if (spinner) lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    }
}

// ============================================
// Show WiFi screen
// ============================================
void ui_wifi_show() {
    wifi_visible = true;

    // Load current credentials into buffers
    if (wifi_storage_has_credentials()) {
        wifi_storage_get_ssid(ssid_buffer, sizeof(ssid_buffer));
        wifi_storage_get_password(pass_buffer, sizeof(pass_buffer));
    } else {
        ssid_buffer[0] = '\0';
        pass_buffer[0] = '\0';
    }

    // Create screen if not exists
    if (!screen_wifi) {
        screen_wifi = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_wifi, COLOR_BG, 0);
        lv_obj_clear_flag(screen_wifi, LV_OBJ_FLAG_SCROLLABLE);

        // Header bar
        lv_obj_t* header = lv_obj_create(screen_wifi);
        lv_obj_set_size(header, SCREEN_WIDTH, 50);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, COLOR_PANEL_BG, 0);
        lv_obj_set_style_radius(header, 0, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // Back button
        lv_obj_t* btn_back = lv_btn_create(header);
        lv_obj_set_size(btn_back, 50, 40);
        lv_obj_set_pos(btn_back, 5, 5);
        lv_obj_set_style_bg_color(btn_back, COLOR_BUTTON_BG, 0);
        lv_obj_set_style_radius(btn_back, 8, 0);
        lv_obj_add_event_cb(btn_back, on_back_click, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_back = lv_label_create(btn_back);
        lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_back, COLOR_WHITE, 0);
        lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
        lv_obj_center(lbl_back);

        // Title
        lv_obj_t* lbl_title = lv_label_create(header);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl_title, COLOR_WHITE, 0);
        lv_label_set_text(lbl_title, "WiFi");
        lv_obj_align(lbl_title, LV_ALIGN_CENTER, 10, 0);

        // WiFi icon
        lv_obj_t* wifi_icon = lv_label_create(header);
        lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(wifi_icon, COLOR_WHITE, 0);
        lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_align(wifi_icon, LV_ALIGN_RIGHT_MID, -15, 0);

        // ==========================================
        // Credentials section
        // ==========================================
        lv_obj_t* section_cred = lv_obj_create(screen_wifi);
        lv_obj_set_size(section_cred, SCREEN_WIDTH - 20, 180);
        lv_obj_set_pos(section_cred, 10, 60);
        lv_obj_set_style_bg_color(section_cred, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_color(section_cred, COLOR_DARK_GRAY, 0);
        lv_obj_set_style_border_width(section_cred, 1, 0);
        lv_obj_set_style_radius(section_cred, 12, 0);
        lv_obj_set_style_pad_all(section_cred, 12, 0);
        lv_obj_clear_flag(section_cred, LV_OBJ_FLAG_SCROLLABLE);

        // Section title
        lv_obj_t* lbl_cred_title = lv_label_create(section_cred);
        lv_obj_set_style_text_font(lbl_cred_title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_cred_title, COLOR_ORANGE, 0);
        lv_label_set_text(lbl_cred_title, "RETE");
        lv_obj_set_pos(lbl_cred_title, 0, 0);

        // SSID row
        lv_obj_t* btn_ssid = lv_btn_create(section_cred);
        lv_obj_set_size(btn_ssid, SCREEN_WIDTH - 50, 50);
        lv_obj_set_pos(btn_ssid, 0, 30);
        lv_obj_set_style_bg_color(btn_ssid, COLOR_BUTTON_BG, 0);
        lv_obj_set_style_radius(btn_ssid, 8, 0);
        lv_obj_add_event_cb(btn_ssid, on_ssid_click, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_ssid = lv_label_create(btn_ssid);
        lv_obj_set_style_text_font(lbl_ssid, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_ssid, COLOR_WHITE, 0);
        lv_label_set_text(lbl_ssid, "SSID");
        lv_obj_align(lbl_ssid, LV_ALIGN_LEFT_MID, 5, -10);

        lbl_ssid_val = lv_label_create(btn_ssid);
        lv_obj_set_style_text_font(lbl_ssid_val, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_ssid_val, COLOR_WHITE, 0);
        lv_obj_set_width(lbl_ssid_val, SCREEN_WIDTH - 100);
        lv_label_set_long_mode(lbl_ssid_val, LV_LABEL_LONG_CLIP);
        lv_obj_align(lbl_ssid_val, LV_ALIGN_LEFT_MID, 5, 10);

        // Password row
        lv_obj_t* btn_pass = lv_btn_create(section_cred);
        lv_obj_set_size(btn_pass, SCREEN_WIDTH - 50, 50);
        lv_obj_set_pos(btn_pass, 0, 90);
        lv_obj_set_style_bg_color(btn_pass, COLOR_BUTTON_BG, 0);
        lv_obj_set_style_radius(btn_pass, 8, 0);
        lv_obj_add_event_cb(btn_pass, on_pass_click, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_pass = lv_label_create(btn_pass);
        lv_obj_set_style_text_font(lbl_pass, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_pass, COLOR_WHITE, 0);
        lv_label_set_text(lbl_pass, "Password");
        lv_obj_align(lbl_pass, LV_ALIGN_LEFT_MID, 5, -10);

        lbl_pass_val = lv_label_create(btn_pass);
        lv_obj_set_style_text_font(lbl_pass_val, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_pass_val, COLOR_WHITE, 0);
        lv_obj_align(lbl_pass_val, LV_ALIGN_LEFT_MID, 5, 10);

        // ==========================================
        // Status section
        // ==========================================
        lv_obj_t* section_status = lv_obj_create(screen_wifi);
        lv_obj_set_size(section_status, SCREEN_WIDTH - 20, 100);
        lv_obj_set_pos(section_status, 10, 250);
        lv_obj_set_style_bg_color(section_status, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_color(section_status, COLOR_DARK_GRAY, 0);
        lv_obj_set_style_border_width(section_status, 1, 0);
        lv_obj_set_style_radius(section_status, 12, 0);
        lv_obj_set_style_pad_all(section_status, 12, 0);
        lv_obj_clear_flag(section_status, LV_OBJ_FLAG_SCROLLABLE);

        // Section title
        lv_obj_t* lbl_stat_title = lv_label_create(section_status);
        lv_obj_set_style_text_font(lbl_stat_title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_stat_title, COLOR_ORANGE, 0);
        lv_label_set_text(lbl_stat_title, "STATO");
        lv_obj_set_pos(lbl_stat_title, 0, 0);

        // Status label
        lbl_status = lv_label_create(section_status);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_20, 0);
        lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 5);

        // IP label
        lbl_ip = lv_label_create(section_status);
        lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_ip, COLOR_WHITE, 0);
        lv_obj_align(lbl_ip, LV_ALIGN_CENTER, 0, 30);

        // Spinner for connecting
        spinner = lv_spinner_create(section_status, 1000, 60);
        lv_obj_set_size(spinner, 30, 30);
        lv_obj_align(spinner, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

        // ==========================================
        // Buttons
        // ==========================================
        // Test button
        btn_test = lv_btn_create(screen_wifi);
        lv_obj_set_size(btn_test, 140, 50);
        lv_obj_set_pos(btn_test, 10, 360);
        lv_obj_set_style_bg_color(btn_test, lv_color_hex(0x2980b9), 0);
        lv_obj_set_style_radius(btn_test, 8, 0);
        lv_obj_add_event_cb(btn_test, on_test_click, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_test = lv_label_create(btn_test);
        lv_obj_set_style_text_font(lbl_test, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_test, COLOR_WHITE, 0);
        lv_label_set_text(lbl_test, LV_SYMBOL_REFRESH " Test");
        lv_obj_center(lbl_test);

        // Save button
        lv_obj_t* btn_save = lv_btn_create(screen_wifi);
        lv_obj_set_size(btn_save, 140, 50);
        lv_obj_set_pos(btn_save, 165, 360);
        lv_obj_set_style_bg_color(btn_save, COLOR_GREEN, 0);
        lv_obj_set_style_radius(btn_save, 8, 0);
        lv_obj_add_event_cb(btn_save, on_save_click, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_save = lv_label_create(btn_save);
        lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_save, COLOR_BLACK, 0);
        lv_label_set_text(lbl_save, LV_SYMBOL_SAVE " Salva");
        lv_obj_center(lbl_save);
    }

    // Update UI with current values
    lv_label_set_text(lbl_ssid_val, strlen(ssid_buffer) > 0 ? ssid_buffer : "(non configurato)");

    // Show password as asterisks
    if (strlen(pass_buffer) > 0) {
        char masked[17];
        int len = strlen(pass_buffer);
        if (len > 16) len = 16;
        for (int i = 0; i < len; i++) masked[i] = '*';
        masked[len] = '\0';
        lv_label_set_text(lbl_pass_val, masked);
    } else {
        lv_label_set_text(lbl_pass_val, "(non configurata)");
    }

    update_status_display();

    // Load screen
    lv_scr_load(screen_wifi);
}

// ============================================
// Hide WiFi screen
// ============================================
void ui_wifi_hide() {
    wifi_visible = false;
    ui_settings_show();
}

// ============================================
// Check visibility
// ============================================
bool ui_wifi_is_visible() {
    return wifi_visible;
}

// ============================================
// Update (call in loop)
// ============================================
void ui_wifi_update() {
    if (!wifi_visible) return;

    // Check if keyboard closed and process result
    if (edit_mode != EDIT_NONE && !ui_keyboard_is_visible()) {
        // Keyboard was closed, update display
        if (edit_mode == EDIT_SSID) {
            lv_label_set_text(lbl_ssid_val, strlen(ssid_buffer) > 0 ? ssid_buffer : "(non configurato)");
        } else if (edit_mode == EDIT_PASSWORD) {
            if (strlen(pass_buffer) > 0) {
                char masked[17];
                int len = strlen(pass_buffer);
                if (len > 16) len = 16;
                for (int i = 0; i < len; i++) masked[i] = '*';
                masked[len] = '\0';
                lv_label_set_text(lbl_pass_val, masked);
            } else {
                lv_label_set_text(lbl_pass_val, "(non configurata)");
            }
        }
        edit_mode = EDIT_NONE;
    }

    // Update status
    update_status_display();
}

// ============================================
// Event callbacks
// ============================================
static void on_back_click(lv_event_t* e) {
    buzzer_beep();
    ui_wifi_hide();
}

static void on_ssid_click(lv_event_t* e) {
    buzzer_beep();
    edit_mode = EDIT_SSID;
    ui_keyboard_show(ssid_buffer, "Nome Rete (SSID)", WIFI_SSID_MAX_LEN);
}

static void on_pass_click(lv_event_t* e) {
    buzzer_beep();
    edit_mode = EDIT_PASSWORD;
    ui_keyboard_show(pass_buffer, "Password", WIFI_PASSWORD_MAX_LEN);
}

static void on_save_click(lv_event_t* e) {
    buzzer_beep();

    if (strlen(ssid_buffer) == 0) {
        // Show error - SSID required
        lv_label_set_text(lbl_status, "SSID richiesto!");
        lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
        return;
    }

    // Save credentials
    wifi_storage_save(ssid_buffer, pass_buffer);

    // Show confirmation
    lv_label_set_text(lbl_status, "Salvato!");
    lv_obj_set_style_text_color(lbl_status, COLOR_GREEN, 0);

    Serial.printf("WiFi UI: Salvate credenziali per '%s'\n", ssid_buffer);
}

static void on_test_click(lv_event_t* e) {
    buzzer_beep();

    if (strlen(ssid_buffer) == 0) {
        lv_label_set_text(lbl_status, "SSID richiesto!");
        lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
        return;
    }

    // Save credentials first (so wifi_time uses them)
    wifi_storage_save(ssid_buffer, pass_buffer);

    // Start connection test
    wifi_time_reconnect();

    lv_label_set_text(lbl_status, "Connessione...");
    lv_obj_set_style_text_color(lbl_status, COLOR_YELLOW, 0);
}
