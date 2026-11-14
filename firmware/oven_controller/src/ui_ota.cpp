/**
 * OTA Update Screen Implementation
 */

#include "ui_ota.h"
#include "ui_theme.h"
#include "ui_settings.h"
#include "buzzer.h"
#include "wifi_time.h"
#include "ota_server.h"
#include "oven_state.h"
#include "ssr_control.h"

// ============================================
// Static UI elements
// ============================================
static lv_obj_t* screen_ota = nullptr;
static lv_obj_t* lbl_status = nullptr;
static lv_obj_t* lbl_ip = nullptr;
static lv_obj_t* lbl_instruction = nullptr;
static lv_obj_t* bar_progress = nullptr;
static lv_obj_t* lbl_progress = nullptr;
static lv_obj_t* spinner = nullptr;
static lv_obj_t* btn_cancel = nullptr;
static bool ota_visible = false;
static unsigned long reboot_time = 0;

// ============================================
// Forward declarations
// ============================================
static void on_cancel_click(lv_event_t* e);
static void update_display();

// ============================================
// Show OTA screen
// ============================================
void ui_ota_show() {
    ota_visible = true;
    reboot_time = 0;

    // Force standby if oven is running
    if (g_state.power_on && g_state.mode != OvenMode::STANDBY) {
        g_state.power_on = false;
        oven_state_go_standby();
        ssr_control_all_off();
    }

    // Create screen if not exists
    if (!screen_ota) {
        screen_ota = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_ota, COLOR_BG, 0);
        lv_obj_clear_flag(screen_ota, LV_OBJ_FLAG_SCROLLABLE);

        // Header bar
        lv_obj_t* header = lv_obj_create(screen_ota);
        lv_obj_set_size(header, SCREEN_WIDTH, 50);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, COLOR_PANEL_BG, 0);
        lv_obj_set_style_radius(header, 0, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // Title
        lv_obj_t* lbl_title = lv_label_create(header);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl_title, COLOR_WHITE, 0);
        lv_label_set_text(lbl_title, LV_SYMBOL_DOWNLOAD " Aggiornamento");
        lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

        // ==========================================
        // Main content area
        // ==========================================
        lv_obj_t* content = lv_obj_create(screen_ota);
        lv_obj_set_size(content, SCREEN_WIDTH - 20, 320);
        lv_obj_set_pos(content, 10, 60);
        lv_obj_set_style_bg_color(content, COLOR_PANEL_BG, 0);
        lv_obj_set_style_border_color(content, COLOR_DARK_GRAY, 0);
        lv_obj_set_style_border_width(content, 1, 0);
        lv_obj_set_style_radius(content, 12, 0);
        lv_obj_set_style_pad_all(content, 15, 0);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

        // Status label (big)
        lbl_status = lv_label_create(content);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl_status, COLOR_YELLOW, 0);
        lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lbl_status, SCREEN_WIDTH - 60);
        lv_obj_align(lbl_status, LV_ALIGN_TOP_MID, 0, 10);

        // Spinner (shown during connecting)
        spinner = lv_spinner_create(content, 1000, 60);
        lv_obj_set_size(spinner, 60, 60);
        lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -30);

        // IP address label (big, prominent)
        lbl_ip = lv_label_create(content);
        lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(lbl_ip, COLOR_GREEN, 0);
        lv_obj_set_style_text_align(lbl_ip, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lbl_ip, SCREEN_WIDTH - 60);
        lv_obj_align(lbl_ip, LV_ALIGN_CENTER, 0, -20);
        lv_obj_add_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);

        // Instruction label
        lbl_instruction = lv_label_create(content);
        lv_obj_set_style_text_font(lbl_instruction, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_instruction, COLOR_WHITE, 0);
        lv_obj_set_style_text_align(lbl_instruction, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lbl_instruction, SCREEN_WIDTH - 60);
        lv_label_set_text(lbl_instruction, "Apri questo indirizzo nel browser\nper caricare il firmware");
        lv_obj_align(lbl_instruction, LV_ALIGN_CENTER, 0, 40);
        lv_obj_add_flag(lbl_instruction, LV_OBJ_FLAG_HIDDEN);

        // Progress bar
        bar_progress = lv_bar_create(content);
        lv_obj_set_size(bar_progress, SCREEN_WIDTH - 80, 30);
        lv_obj_align(bar_progress, LV_ALIGN_CENTER, 0, 100);
        lv_bar_set_range(bar_progress, 0, 100);
        lv_obj_set_style_bg_color(bar_progress, COLOR_DARK_GRAY, LV_PART_MAIN);
        lv_obj_set_style_bg_color(bar_progress, COLOR_GREEN, LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar_progress, 5, 0);
        lv_obj_set_style_radius(bar_progress, 5, LV_PART_INDICATOR);
        lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);

        // Progress label
        lbl_progress = lv_label_create(content);
        lv_obj_set_style_text_font(lbl_progress, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl_progress, COLOR_WHITE, 0);
        lv_obj_align(lbl_progress, LV_ALIGN_CENTER, 0, 140);
        lv_obj_add_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);

        // Cancel button
        btn_cancel = lv_btn_create(screen_ota);
        lv_obj_set_size(btn_cancel, SCREEN_WIDTH - 40, 50);
        lv_obj_set_pos(btn_cancel, 20, 400);
        lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x8B0000), 0);
        lv_obj_set_style_radius(btn_cancel, 8, 0);
        lv_obj_add_event_cb(btn_cancel, on_cancel_click, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_cancel = lv_label_create(btn_cancel);
        lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_cancel, COLOR_WHITE, 0);
        lv_label_set_text(lbl_cancel, LV_SYMBOL_CLOSE " Annulla");
        lv_obj_center(lbl_cancel);
    }

    // Reset UI state
    lv_label_set_text(lbl_status, "Connessione WiFi...");
    lv_obj_set_style_text_color(lbl_status, COLOR_YELLOW, 0);
    lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_instruction, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_cancel, LV_OBJ_FLAG_HIDDEN);

    // Enable keep-connected mode and reconnect
    wifi_time_keep_connected(true);
    wifi_time_reconnect();

    // Load screen
    lv_scr_load(screen_ota);
}

// ============================================
// Hide OTA screen
// ============================================
void ui_ota_hide() {
    ota_visible = false;

    // Stop OTA server
    ota_server_stop();

    // Disable keep-connected mode
    wifi_time_keep_connected(false);
    wifi_time_disconnect();

    ui_settings_show();
}

// ============================================
// Check visibility
// ============================================
bool ui_ota_is_visible() {
    return ota_visible;
}

// ============================================
// Update display
// ============================================
static void update_display() {
    OtaStatus ota_status = ota_server_get_status();

    // Check if WiFi connected
    if (!wifi_time_is_connected()) {
        WiFiStatus wifi_status = wifi_time_get_status();

        if (wifi_status == WiFiStatus::CONNECTING) {
            lv_label_set_text(lbl_status, "Connessione WiFi...");
            lv_obj_set_style_text_color(lbl_status, COLOR_YELLOW, 0);
            lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        } else if (wifi_status == WiFiStatus::FAILED) {
            lv_label_set_text(lbl_status, "WiFi non disponibile");
            lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    // WiFi connected - start OTA server if not running
    if (!ota_server_is_running()) {
        ota_server_start();
    }

    // Update based on OTA status
    switch (ota_status) {
        case OtaStatus::IDLE:
        case OtaStatus::WAITING:
            lv_label_set_text(lbl_status, "Pronto per upload");
            lv_obj_set_style_text_color(lbl_status, COLOR_GREEN, 0);
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

            // Show IP address
            char ip_buf[32];
            snprintf(ip_buf, sizeof(ip_buf), "http://%s", wifi_time_get_ip());
            lv_label_set_text(lbl_ip, ip_buf);
            lv_obj_clear_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lbl_instruction, LV_OBJ_FLAG_HIDDEN);

            // Hide progress
            lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            break;

        case OtaStatus::UPLOADING:
            lv_label_set_text(lbl_status, "Upload in corso...");
            lv_obj_set_style_text_color(lbl_status, COLOR_ORANGE, 0);
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_instruction, LV_OBJ_FLAG_HIDDEN);

            // Show progress
            lv_obj_clear_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(bar_progress, ota_server_get_progress(), LV_ANIM_ON);

            char prog_buf[16];
            snprintf(prog_buf, sizeof(prog_buf), "%d%%", ota_server_get_progress());
            lv_label_set_text(lbl_progress, prog_buf);

            // Hide cancel during upload
            lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_HIDDEN);
            break;

        case OtaStatus::SUCCESS:
            lv_label_set_text(lbl_status, "Completato!");
            lv_obj_set_style_text_color(lbl_status, COLOR_GREEN, 0);
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

            lv_bar_set_value(bar_progress, 100, LV_ANIM_OFF);
            lv_label_set_text(lbl_progress, "Riavvio in corso...");

            lv_obj_add_flag(lbl_ip, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_instruction, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_HIDDEN);

            // Schedule reboot
            if (reboot_time == 0) {
                reboot_time = millis() + 2000;  // 2 seconds
                buzzer_confirm();
            }
            break;

        case OtaStatus::ERROR:
            lv_label_set_text(lbl_status, "Errore upload!");
            lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

            lv_label_set_text(lbl_progress, ota_server_get_error());
            lv_obj_clear_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(lbl_progress, COLOR_RED, 0);

            // Show cancel button again
            lv_obj_clear_flag(btn_cancel, LV_OBJ_FLAG_HIDDEN);

            // Reset after showing error
            static unsigned long error_shown = 0;
            if (error_shown == 0) {
                error_shown = millis();
            } else if (millis() - error_shown > 5000) {
                ota_server_reset_status();
                error_shown = 0;
            }
            break;
    }
}

// ============================================
// Update (call in loop)
// ============================================
void ui_ota_update() {
    if (!ota_visible) return;

    // Handle OTA server
    ota_server_update();

    // Update display
    update_display();

    // Handle reboot
    if (reboot_time > 0 && millis() >= reboot_time) {
        ota_server_reboot();
    }
}

// ============================================
// Event callbacks
// ============================================
static void on_cancel_click(lv_event_t* e) {
    buzzer_beep();
    ui_ota_hide();
}
