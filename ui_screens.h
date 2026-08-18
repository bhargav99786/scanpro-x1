/*
  ============================================================================
  ui_screens.h — FULL LVGL v8 UI matching the landscape tricolor mockup
  ----------------------------------------------------------------------------
  Screens: Home, Scan, Tasks, Inventory, Broadcast, Settings/Diagnostics
  Layout: left icon nav rail (matches the 480x320 landscape mockup), status
  bar with tricolor stripe underneath, tricolor theme throughout.

  This replaces the earlier simplified 2-screen ui_screens.h. Wire it into
  the same smart_barcode_scanner.ino / display_touch.h / scan_engine.h /
  network.h from the previous message — nothing else needs to change.
  ============================================================================
*/
#pragma once
#include <lvgl.h>
#include "network.h"
#include "ble_conn.h"
#include "display_touch.h"

// ---- Futuristic Dark Cyber Theme ----
#define COLOR_SAFFRON   lv_color_hex(0xFF9933)   // Accent Saffron
#define COLOR_NAVY_BLUE lv_color_hex(0x1D4ED8)   // Vibrant Navy Blue accent
#define COLOR_NAVY      lv_color_hex(0x0A0E2A)   // Deep space dark
#define COLOR_WHITE     lv_color_hex(0xE8F4FF)   // Cool white
#define COLOR_BG        lv_color_hex(0x070C1F)   // Near-black space bg
#define COLOR_CARD      lv_color_hex(0x0D1535)   // Dark card bg
#define COLOR_CARD_BRD  lv_color_hex(0x1A2952)   // Card border
#define COLOR_CYAN      lv_color_hex(0x00D4FF)   // Electric cyan accent
#define COLOR_MUTE      lv_color_hex(0x4A6080)   // Dim blue-grey text
#define COLOR_DANGER    lv_color_hex(0xFF3355)   // Neon red
#define COLOR_DANGER_BG lv_color_hex(0x1A0010)   // Dark red bg
#define COLOR_GREEN     COLOR_NAVY_BLUE          // Replaced neon green with Navy Blue

// ---- Screens ----
static lv_obj_t *scr_login, *scr_home, *scr_scan, *scr_tasks, *scr_inventory, *scr_conn, *scr_settings;

// ---- Login state ----
inline bool      is_logged_in = false;
inline char      logged_in_user[32] = "";

// ---- Users & Roles Sync ----
#define MAX_USERS 20
struct UserDef {
  char id[32];
  char name[32];
  char role[32];
  char pin[8];
};
inline UserDef global_users[MAX_USERS];
inline int global_user_count = 0;

// ---- Tasks data ----
#define MAX_TASKS 10
#define MAX_ITEMS_PER_TASK 5

struct TaskItem {
  char sku[32];
  char name[32];
  int target_qty;
  int picked_qty;
};

struct TaskDef { 
  char id[32];
  char name[32]; 
  char prio[16]; 
  lv_color_t prio_color;
  int item_count;
  TaskItem items[MAX_ITEMS_PER_TASK];
};

static TaskDef current_tasks[MAX_TASKS];
static int current_task_count = 0;
static lv_obj_t *tasks_content_ptr = NULL;

static TaskDef *active_task = NULL;
static lv_obj_t *scr_task_detail = NULL;
static lv_obj_t *task_detail_content = NULL;

// ---- Inventory data ----
#define MAX_INVENTORY 50
struct InvItem { char name[32]; char sku[32]; char qty[16]; };
inline InvItem global_inventory[MAX_INVENTORY];
inline int global_inventory_count = 0;
static lv_obj_t *inv_list = NULL;
static lv_obj_t *inv_search_ta = NULL;

static lv_obj_t *status_title_label;     // title text on top status bar
static lv_obj_t *label_wifi_status;      // WiFi / BLE status indicator on top bar
static lv_obj_t *label_battery_status;   // battery percentage indicator on status bar
static lv_obj_t *label_last_scan_sku;
static lv_obj_t *label_last_scan_flag;
static lv_obj_t *label_scan_progress;
static lv_obj_t *bar_scan_progress;
static lv_obj_t *label_scan_count_home;
static lv_obj_t *label_conn_wifi_detail; // IP / status shown in Conn screen
static lv_obj_t *label_conn_ble_detail;  // BLE status shown in Conn screen
static lv_obj_t *label_conn_ble_btn;     // BLE toggle button label

// Pointers for dynamic Setup screen layout
static lv_obj_t *conn_nav_rail_ptr = NULL;
static lv_obj_t *conn_content_ptr = NULL;
static lv_obj_t *conn_logout_lbl_ptr = NULL;
static lv_obj_t *conn_bottom_row_ptr = NULL;
static lv_obj_t *top_back_btn = NULL;
static uint32_t  scan_counter = 0;
static uint32_t  scan_target  = 120;

// Textarea handles (forward declared for global logout action)
static lv_obj_t *ta_login_user = NULL;
static lv_obj_t *ta_login_pass = NULL;
static lv_obj_t *label_login_qr_status = NULL;

// forward decls
static lv_obj_t* build_nav_rail(lv_obj_t *parent, int active_index);
static lv_obj_t* build_status_bar(lv_obj_t *parent, const char *screen_title);
static void nav_login_cb(lv_event_t *e);
static void nav_home_cb(lv_event_t *e);
static void nav_scan_cb(lv_event_t *e);
static void nav_tasks_cb(lv_event_t *e);
static void nav_inventory_cb(lv_event_t *e);
static void nav_conn_cb(lv_event_t *e);
static void nav_settings_cb(lv_event_t *e);
static void _ta_event_cb(lv_event_t *e);
static void task_row_event_cb(lv_event_t *e);
static void task_detail_back_cb(lv_event_t *e);
extern void devicePowerOff();
inline void uiSetWifiStatus(const String &text);

static void _logout_action_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    is_logged_in = false;
    publishDeviceStatus(false, "");
    memset(logged_in_user, 0, sizeof(logged_in_user));
    if (ta_login_user) lv_textarea_set_text(ta_login_user, "");
    if (ta_login_pass) lv_textarea_set_text(ta_login_pass, "");
    // Reset QR status label back to idle prompt
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_BARS "  Scan QR badge to login");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_CYAN, 0);
    }
    if (scr_login) lv_scr_load_anim(scr_login, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    if (status_title_label) lv_label_set_text(status_title_label, "");
  }
}

static void _power_off_action_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    devicePowerOff();
  }
}

// ============================================================================
// Shared chrome: status bar (with tricolor stripe) + left nav rail
// ============================================================================
static void create_global_status_bar() {
  if (status_title_label) return;

  lv_obj_t *bar = lv_obj_create(lv_layer_top());
  lv_obj_set_size(bar, 480, 32);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x050A1A), 0);
  lv_obj_set_style_border_color(bar, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  status_title_label = lv_label_create(bar);
  lv_label_set_text(status_title_label, "Home");
  lv_obj_set_style_text_color(status_title_label, COLOR_CYAN, 0);
  lv_obj_set_style_text_font(status_title_label, &lv_font_montserrat_14, 0);
  lv_obj_align(status_title_label, LV_ALIGN_LEFT_MID, 6, 0);

  // Top Back Button (Only for pre-login setup)
  top_back_btn = lv_btn_create(bar);
  lv_obj_set_size(top_back_btn, 72, 24);
  lv_obj_align(top_back_btn, LV_ALIGN_LEFT_MID, 6, 0);
  lv_obj_set_style_bg_color(top_back_btn, COLOR_NAVY_BLUE, 0);
  lv_obj_set_style_radius(top_back_btn, 4, 0);
  lv_obj_set_style_pad_all(top_back_btn, 0, 0);
  lv_obj_t *tb_lbl = lv_label_create(top_back_btn);
  lv_label_set_text(tb_lbl, LV_SYMBOL_LEFT " BACK");
  lv_obj_set_style_text_color(tb_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(tb_lbl, &lv_font_montserrat_12, 0);
  lv_obj_center(tb_lbl);
  lv_obj_add_flag(top_back_btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(top_back_btn, [](lv_event_t *e) {
    lv_scr_load_anim(scr_login, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    if (status_title_label) {
      lv_obj_align(status_title_label, LV_ALIGN_LEFT_MID, 6, 0);
      lv_label_set_text(status_title_label, "");
    }
    lv_obj_add_flag(top_back_btn, LV_OBJ_FLAG_HIDDEN);
  }, LV_EVENT_CLICKED, NULL);

  // Instant top logout button (enlarged & clear)
  lv_obj_t *top_logout = lv_btn_create(bar);
  lv_obj_set_size(top_logout, 80, 24);
  lv_obj_align(top_logout, LV_ALIGN_RIGHT_MID, -185, 0);
  lv_obj_set_style_bg_color(top_logout, COLOR_DANGER, 0);
  lv_obj_set_style_radius(top_logout, 4, 0);
  lv_obj_set_style_pad_all(top_logout, 0, 0);
  lv_obj_t *tl_lbl = lv_label_create(top_logout);
  lv_label_set_text(tl_lbl, LV_SYMBOL_POWER " LOGOUT");
  lv_obj_set_style_text_color(tl_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(tl_lbl, &lv_font_montserrat_12, 0);
  lv_obj_center(tl_lbl);
  lv_obj_add_event_cb(top_logout, _logout_action_cb, LV_EVENT_CLICKED, NULL);

  // Top Setup Button (Wi-Fi/BLE)
  lv_obj_t *top_setup = lv_btn_create(bar);
  lv_obj_set_size(top_setup, 80, 24);
  lv_obj_align(top_setup, LV_ALIGN_RIGHT_MID, -275, 0);
  lv_obj_set_style_bg_color(top_setup, COLOR_NAVY_BLUE, 0);
  lv_obj_set_style_radius(top_setup, 4, 0);
  lv_obj_set_style_pad_all(top_setup, 0, 0);
  lv_obj_t *ts_lbl = lv_label_create(top_setup);
  lv_label_set_text(ts_lbl, LV_SYMBOL_WIFI " SETUP");
  lv_obj_set_style_text_color(ts_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(ts_lbl, &lv_font_montserrat_12, 0);
  lv_obj_center(ts_lbl);
  lv_obj_add_event_cb(top_setup, [](lv_event_t *e) {
    if (is_logged_in) {
      if (conn_nav_rail_ptr) lv_obj_clear_flag(conn_nav_rail_ptr, LV_OBJ_FLAG_HIDDEN);
      if (conn_bottom_row_ptr) lv_obj_clear_flag(conn_bottom_row_ptr, LV_OBJ_FLAG_HIDDEN);
      if (conn_content_ptr) {
        lv_obj_set_size(conn_content_ptr, 416, 288);
        lv_obj_set_pos(conn_content_ptr, 64, 32);
      }
      if (top_back_btn) lv_obj_add_flag(top_back_btn, LV_OBJ_FLAG_HIDDEN);
      if (status_title_label) lv_obj_align(status_title_label, LV_ALIGN_LEFT_MID, 6, 0);
    } else {
      if (conn_nav_rail_ptr) lv_obj_add_flag(conn_nav_rail_ptr, LV_OBJ_FLAG_HIDDEN);
      if (conn_bottom_row_ptr) lv_obj_add_flag(conn_bottom_row_ptr, LV_OBJ_FLAG_HIDDEN);
      if (conn_content_ptr) {
        lv_obj_set_size(conn_content_ptr, 480, 288); // Full width
        lv_obj_set_pos(conn_content_ptr, 0, 32);
      }
      if (top_back_btn) lv_obj_clear_flag(top_back_btn, LV_OBJ_FLAG_HIDDEN);
      if (status_title_label) lv_obj_align(status_title_label, LV_ALIGN_LEFT_MID, 84, 0);
    }
    lv_scr_load_anim(scr_conn, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    if (status_title_label) lv_label_set_text(status_title_label, is_logged_in ? "Connectivity" : "Pre-Login Setup");
  }, LV_EVENT_CLICKED, NULL);

  label_battery_status = lv_label_create(bar);
  lv_label_set_text(label_battery_status, LV_SYMBOL_BATTERY_FULL " 100%");
  lv_obj_set_style_text_color(label_battery_status, COLOR_NAVY_BLUE, 0);
  lv_obj_set_style_text_font(label_battery_status, &lv_font_montserrat_12, 0);
  lv_obj_align(label_battery_status, LV_ALIGN_RIGHT_MID, -6, 0);

  label_wifi_status = lv_label_create(bar);
  lv_label_set_text(label_wifi_status, LV_SYMBOL_WIFI " WiFi: Off");
  lv_obj_set_style_text_color(label_wifi_status, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(label_wifi_status, &lv_font_montserrat_12, 0);
  lv_obj_align(label_wifi_status, LV_ALIGN_RIGHT_MID, -85, 0);

  // Cyan accent stripe
  lv_obj_t *stripe = lv_obj_create(lv_layer_top());
  lv_obj_set_size(stripe, 480, 2);
  lv_obj_set_pos(stripe, 0, 32);
  lv_obj_set_style_border_width(stripe, 0, 0);
  lv_obj_set_style_radius(stripe, 0, 0);
  lv_obj_set_style_bg_color(stripe, COLOR_CYAN, 0);
  lv_obj_set_style_bg_grad_color(stripe, COLOR_SAFFRON, 0);
  lv_obj_set_style_bg_grad_dir(stripe, LV_GRAD_DIR_HOR, 0);
}

static lv_obj_t* build_status_bar(lv_obj_t *parent, const char *screen_title) {
  create_global_status_bar();
  if (status_title_label && screen_title) {
    lv_label_set_text(status_title_label, screen_title);
  }
  return NULL;
}

static lv_obj_t* build_nav_rail(lv_obj_t *parent, int active_index) {
  lv_obj_t *rail = lv_obj_create(parent);
  lv_obj_set_size(rail, 64, 320 - 32);
  lv_obj_set_pos(rail, 0, 32);
  lv_obj_set_style_bg_color(rail, lv_color_hex(0x050A1A), 0);
  lv_obj_set_style_border_color(rail, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(rail, 0, 0);
  lv_obj_set_style_border_side(rail, LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_border_width(rail, 1, 0);
  lv_obj_set_style_radius(rail, 0, 0);
  lv_obj_set_style_pad_all(rail, 0, 0);
  lv_obj_clear_flag(rail, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(rail, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(rail, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  const char *labels[5] = {"Home", "Scan", "Tasks", "Inv", "Setup"};
  lv_event_cb_t cbs[5] = {nav_home_cb, nav_scan_cb, nav_tasks_cb, nav_inventory_cb, nav_conn_cb};

  for (int i = 0; i < 5; i++) {
    lv_obj_t *btn = lv_btn_create(rail);
    lv_obj_set_size(btn, 56, 50);
    lv_obj_set_style_radius(btn, 8, 0);
    if (i == active_index) {
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x0D2545), 0);
      lv_obj_set_style_border_color(btn, COLOR_CYAN, 0);
      lv_obj_set_style_border_width(btn, 1, 0);
    } else {
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x050A1A), 0);
      lv_obj_set_style_border_width(btn, 0, 0);
    }
    lv_obj_add_event_cb(btn, cbs[i], LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, labels[i]);
    lv_obj_set_style_text_color(lbl, (i == active_index) ? COLOR_CYAN : COLOR_MUTE, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl);
  }
  return rail;
}

// content area helper: full width minus nav rail, below status bar
static lv_obj_t* build_content_area(lv_obj_t *parent) {
  lv_obj_t *content = lv_obj_create(parent);
  lv_obj_set_size(content, 480 - 64, 320 - 32);
  lv_obj_set_pos(content, 64, 32);
  lv_obj_set_style_bg_color(content, COLOR_BG, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_radius(content, 0, 0);
  lv_obj_set_style_pad_all(content, 8, 0);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  return content;
}

// ============================================================================
// A. HOME
// ============================================================================
static void build_home_screen() {
  scr_home = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_home, COLOR_BG, 0);
  build_status_bar(scr_home, "ScanPro X1");
  build_nav_rail(scr_home, 0);
  lv_obj_t *content = build_content_area(scr_home);

  lv_obj_t *hdr = lv_label_create(content);
  lv_label_set_text(hdr, LV_SYMBOL_HOME "  Warehouse Dashboard");
  lv_obj_set_style_text_color(hdr, COLOR_CYAN, 0);
  lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

  // 2x2 tile grid
  static lv_coord_t col_dsc[] = {150, 150, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_dsc[] = {70, 70, LV_GRID_TEMPLATE_LAST};
  lv_obj_t *grid = lv_obj_create(content);
  lv_obj_set_size(grid, 320, 150);
  lv_obj_align(grid, LV_ALIGN_TOP_LEFT, 0, 24);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

  struct TileDef { const char *label; lv_color_t accent; lv_event_cb_t cb; int col; int row; };
  TileDef tiles[4] = {
    {LV_SYMBOL_BARS "\nScan",       COLOR_CYAN,    nav_scan_cb,      0, 0},
    {LV_SYMBOL_LIST    "\nTasks",     COLOR_GREEN,   nav_tasks_cb,     1, 0},
    {LV_SYMBOL_LOOP    "\nInventory", COLOR_SAFFRON, nav_inventory_cb, 0, 1},
    {LV_SYMBOL_SETTINGS"\nSetup",    COLOR_CYAN,    nav_conn_cb,      1, 1},
  };
  for (int i = 0; i < 4; i++) {
    lv_obj_t *tile = lv_btn_create(grid);
    lv_obj_set_grid_cell(tile, LV_GRID_ALIGN_STRETCH, tiles[i].col, 1, LV_GRID_ALIGN_STRETCH, tiles[i].row, 1);
    lv_obj_set_style_bg_color(tile, COLOR_CARD, 0);
    lv_obj_set_style_border_color(tile, tiles[i].accent, 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_radius(tile, 6, 0);
    lv_obj_add_event_cb(tile, tiles[i].cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(tile);
    lv_label_set_text(lbl, tiles[i].label);
    lv_obj_set_style_text_color(lbl, tiles[i].accent, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_center(lbl);
  }

  label_scan_count_home = lv_label_create(content);
  lv_label_set_text(label_scan_count_home, "Scans this session: 0");
  lv_obj_set_style_text_color(label_scan_count_home, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(label_scan_count_home, &lv_font_montserrat_12, 0);
  lv_obj_align(label_scan_count_home, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

// ============================================================================
// B. SCAN
// ============================================================================
static void build_scan_screen() {
  scr_scan = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_scan, COLOR_BG, 0);
  build_status_bar(scr_scan, "Scan");
  build_nav_rail(scr_scan, 1);
  lv_obj_t *content = build_content_area(scr_scan);

  // Left: viewfinder placeholder
  lv_obj_t *vf = lv_obj_create(content);
  lv_obj_set_size(vf, 170, 200);
  lv_obj_align(vf, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(vf, lv_color_hex(0x12161C), 0);
  lv_obj_set_style_border_color(vf, COLOR_SAFFRON, 0);
  lv_obj_set_style_border_width(vf, 2, 0);
  lv_obj_set_style_radius(vf, 10, 0);
  lv_obj_t *vf_label = lv_label_create(vf);
  lv_label_set_text(vf_label, "Aim GM65\nat barcode");
  lv_obj_set_style_text_color(vf_label, COLOR_GREEN, 0);
  lv_obj_set_style_text_align(vf_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(vf_label);

  // Right: result card + progress
  lv_obj_t *card = lv_obj_create(content);
  lv_obj_set_size(card, 220, 100);
  lv_obj_align(card, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(0xE6F4E3), 0);
  lv_obj_set_style_border_color(card, COLOR_GREEN, 0);
  lv_obj_set_style_border_width(card, 2, 0);
  lv_obj_set_style_radius(card, 8, 0);

  label_last_scan_sku = lv_label_create(card);
  lv_label_set_text(label_last_scan_sku, "No scan yet");
  lv_obj_set_style_text_color(label_last_scan_sku, COLOR_NAVY, 0);
  lv_obj_align(label_last_scan_sku, LV_ALIGN_TOP_LEFT, 4, 4);
  lv_label_set_long_mode(label_last_scan_sku, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label_last_scan_sku, 210);

  label_last_scan_flag = lv_label_create(card);
  lv_label_set_text(label_last_scan_flag, "");
  lv_obj_set_style_text_color(label_last_scan_flag, COLOR_GREEN, 0);
  lv_obj_align(label_last_scan_flag, LV_ALIGN_BOTTOM_LEFT, 4, -4);

  label_scan_progress = lv_label_create(content);
  lv_label_set_text(label_scan_progress, "Progress: 0 / 0");
  lv_obj_set_style_text_color(label_scan_progress, COLOR_MUTE, 0);
  lv_obj_align(label_scan_progress, LV_ALIGN_TOP_RIGHT, 0, 108);

  bar_scan_progress = lv_bar_create(content);
  lv_obj_set_size(bar_scan_progress, 220, 10);
  lv_obj_align(bar_scan_progress, LV_ALIGN_TOP_RIGHT, 0, 130);
  lv_obj_set_style_bg_color(bar_scan_progress, COLOR_SAFFRON, LV_PART_INDICATOR);
  lv_bar_set_range(bar_scan_progress, 0, 100);
  lv_bar_set_value(bar_scan_progress, 0, LV_ANIM_OFF);
}

// ============================================================================
// C. TASKS
// ============================================================================
static void update_tasks_ui() {
  if (!tasks_content_ptr) return;
  lv_obj_clean(tasks_content_ptr); // Remove old rows
  
  if (current_task_count == 0) {
    lv_obj_t *empty = lv_label_create(tasks_content_ptr);
    lv_label_set_text(empty, "No active tasks assigned.");
    lv_obj_set_style_text_color(empty, COLOR_MUTE, 0);
    lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);
    return;
  }

  for (int i = 0; i < current_task_count; i++) {
    lv_obj_t *row = lv_obj_create(tasks_content_ptr);
    lv_obj_set_size(row, LV_PCT(100), 54);
    lv_obj_set_style_bg_color(row, COLOR_WHITE, 0);
    lv_obj_set_style_border_color(row, COLOR_CARD_BRD, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 2, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    
    // Make row clickable
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, task_row_event_cb, LV_EVENT_CLICKED, &current_tasks[i]);

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, current_tasks[i].name);
    lv_obj_set_style_text_color(name, COLOR_NAVY, 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 6, 2);

    lv_obj_t *sub = lv_label_create(row);
    char sub_text[64];
    snprintf(sub_text, sizeof(sub_text), "%d items", current_tasks[i].item_count);
    lv_label_set_text(sub, sub_text);
    lv_obj_set_style_text_color(sub, COLOR_MUTE, 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 6, -2);

    lv_obj_t *badge = lv_obj_create(row);
    lv_obj_set_size(badge, 44, 20);
    lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(badge, current_tasks[i].prio_color, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_radius(badge, 4, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *b_lbl = lv_label_create(badge);
    lv_label_set_text(b_lbl, current_tasks[i].prio);
    lv_obj_set_style_text_color(b_lbl, COLOR_WHITE, 0);
    lv_obj_set_style_text_font(b_lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(b_lbl);
  }
}

static void update_task_detail_ui() {
  if (!task_detail_content || !active_task) return;
  lv_obj_clean(task_detail_content);

  lv_obj_t *header = lv_label_create(task_detail_content);
  lv_label_set_text(header, active_task->name);
  lv_obj_set_style_text_color(header, COLOR_NAVY_BLUE, 0);
  lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
  lv_obj_set_style_pad_bottom(header, 10, 0);

  for (int i = 0; i < active_task->item_count; i++) {
    TaskItem &itm = active_task->items[i];
    
    lv_obj_t *row = lv_obj_create(task_detail_content);
    lv_obj_set_size(row, LV_PCT(100), 40);
    lv_obj_set_style_bg_color(row, COLOR_WHITE, 0);
    lv_obj_set_style_border_color(row, COLOR_CARD_BRD, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 4, 0);
    lv_obj_set_style_pad_all(row, 4, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    
    if (itm.picked_qty >= itm.target_qty) {
      lv_obj_set_style_bg_color(row, lv_color_hex(0xE8F5E9), 0); // light green
    }

    lv_obj_t *name = lv_label_create(row);
    lv_label_set_text(name, itm.name);
    lv_obj_set_style_text_color(name, COLOR_NAVY, 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t *qty = lv_label_create(row);
    char qty_text[32];
    snprintf(qty_text, sizeof(qty_text), "%d / %d", itm.picked_qty, itm.target_qty);
    lv_label_set_text(qty, qty_text);
    lv_obj_set_style_text_color(qty, COLOR_CYAN, 0);
    lv_obj_set_style_text_font(qty, &lv_font_montserrat_14, 0);
    lv_obj_align(qty, LV_ALIGN_RIGHT_MID, -4, 0);
  }
}

static void build_task_detail_screen() {
  if (scr_task_detail == NULL) {
    scr_task_detail = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_task_detail, COLOR_BG, 0);
    
    lv_obj_t *bar = lv_obj_create(scr_task_detail);
    lv_obj_set_size(bar, 480, 32);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x050A1A), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *back_btn = lv_btn_create(bar);
    lv_obj_set_size(back_btn, 72, 24);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 6, 0);
    lv_obj_set_style_bg_color(back_btn, COLOR_NAVY_BLUE, 0);
    lv_obj_t *tb_lbl = lv_label_create(back_btn);
    lv_label_set_text(tb_lbl, LV_SYMBOL_LEFT " BACK");
    lv_obj_center(tb_lbl);
    lv_obj_add_event_cb(back_btn, task_detail_back_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *title = lv_label_create(bar);
    lv_label_set_text(title, "Picking Task");
    lv_obj_set_style_text_color(title, COLOR_CYAN, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    task_detail_content = lv_obj_create(scr_task_detail);
    lv_obj_set_size(task_detail_content, 480, 320 - 32);
    lv_obj_set_pos(task_detail_content, 0, 32);
    lv_obj_set_style_bg_color(task_detail_content, COLOR_BG, 0);
    lv_obj_set_style_border_width(task_detail_content, 0, 0);
    lv_obj_set_flex_flow(task_detail_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(task_detail_content, 8, 0);
    lv_obj_set_style_pad_row(task_detail_content, 6, 0);
  }
  update_task_detail_ui();
}

static void task_row_event_cb(lv_event_t *e) {
  active_task = (TaskDef *)lv_event_get_user_data(e);
  build_task_detail_screen();
  lv_scr_load_anim(scr_task_detail, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
}

static void task_detail_back_cb(lv_event_t *e) {
  active_task = NULL;
  lv_scr_load_anim(scr_tasks, LV_SCR_LOAD_ANIM_FADE_ON, 150, 0, false);
}

static void build_tasks_screen() {
  scr_tasks = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_tasks, COLOR_BG, 0);
  build_status_bar(scr_tasks, "Tasks");
  build_nav_rail(scr_tasks, 2);
  tasks_content_ptr = build_content_area(scr_tasks);
  lv_obj_set_flex_flow(tasks_content_ptr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tasks_content_ptr, 6, 0);

  update_tasks_ui();
}

static void update_inventory_ui() {
  if (!inv_list) return;
  lv_obj_clean(inv_list);
  const char *query = inv_search_ta ? lv_textarea_get_text(inv_search_ta) : "";
  
  for (int i = 0; i < global_inventory_count; i++) {
    // Case-insensitive substring match on name or SKU
    String q = String(query); q.toLowerCase();
    String nm = String(global_inventory[i].name); nm.toLowerCase();
    String sk = String(global_inventory[i].sku); sk.toLowerCase();
    if (q.length() == 0 || nm.indexOf(q) >= 0 || sk.indexOf(q) >= 0) {
      char buf[80];
      snprintf(buf, sizeof(buf), "%s  |  %s  |  Qty %s",
               global_inventory[i].name, global_inventory[i].sku, global_inventory[i].qty);
      lv_list_add_btn(inv_list, LV_SYMBOL_FILE, buf);
    }
  }
}

// ============================================================================
// D. INVENTORY
// ============================================================================
static void build_inventory_screen() {
  scr_inventory = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_inventory, COLOR_BG, 0);
  build_status_bar(scr_inventory, "Inventory");
  build_nav_rail(scr_inventory, 3);
  lv_obj_t *content = build_content_area(scr_inventory);

  inv_search_ta = lv_textarea_create(content);
  lv_textarea_set_placeholder_text(inv_search_ta, "Search SKU or name");
  lv_textarea_set_one_line(inv_search_ta, true);
  lv_obj_set_size(inv_search_ta, LV_PCT(100), 36);
  lv_obj_align(inv_search_ta, LV_ALIGN_TOP_LEFT, 0, 0);
  
  // Live search event
  static auto _inv_search_cb = [](lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    update_inventory_ui();
  };
  lv_obj_add_event_cb(inv_search_ta, _inv_search_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(inv_search_ta, _ta_event_cb, LV_EVENT_ALL, NULL);

  inv_list = lv_list_create(content);
  lv_obj_set_size(inv_list, LV_PCT(100), 220 - 40);
  lv_obj_align(inv_list, LV_ALIGN_TOP_LEFT, 0, 42);
  lv_obj_set_style_bg_color(inv_list, COLOR_BG, 0);
  lv_obj_set_style_border_color(inv_list, COLOR_CARD_BRD, 0);
  lv_obj_set_style_radius(inv_list, 8, 0);

  update_inventory_ui();
}

// ============================================================================
// E. CONNECTIVITY (WiFi + BLE)
// ============================================================================
static lv_obj_t *ta_wifi_ssid;
static lv_obj_t *ta_wifi_pass;
static lv_obj_t *wcard_inputs = NULL;
static lv_obj_t *ble_active_panel = NULL;
static lv_obj_t *label_ble_large_status = NULL;
static lv_obj_t *wcard_ref = NULL;
static lv_obj_t *kb_shared = NULL;

static void _ta_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  if (!kb_shared) return;

  if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
    lv_keyboard_set_textarea(kb_shared, ta);
    lv_obj_clear_flag(kb_shared, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb_shared);
  } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY) {
    lv_obj_add_flag(kb_shared, LV_OBJ_FLAG_HIDDEN);
  }
}

static void _wifi_save_connect_btn_cb(lv_event_t *e) {
  const char *s = lv_textarea_get_text(ta_wifi_ssid);
  const char *p = lv_textarea_get_text(ta_wifi_pass);
  saveWifiCredentials(s, p);

  if (label_conn_wifi_detail)
    lv_label_set_text(label_conn_wifi_detail, "Connecting...");
  WiFi.disconnect(true);
  wifiConnect(8000);
}

static void _ble_toggle_btn_cb(lv_event_t *e) {
  if (bleIsAdvertising() || bleIsConnected()) {
    bleStopAdvertising();
    if (label_conn_ble_detail) lv_label_set_text(label_conn_ble_detail, "Off");
    if (label_conn_ble_btn)    lv_label_set_text(label_conn_ble_btn, "Start BLE");
  } else {
    bleStartAdvertising();
    if (label_conn_ble_detail) lv_label_set_text(label_conn_ble_detail, "Advertising...");
    if (label_conn_ble_btn)    lv_label_set_text(label_conn_ble_btn, "Stop BLE");
  }
}

static void _landscape_switch_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  bool is_right = lv_obj_has_state(sw, LV_STATE_CHECKED);
  saveScreenRotation(is_right ? 3 : 1);
}

static void _mode_switch_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  bool is_ble = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (is_ble) {
    WiFi.disconnect(true);
    bleStartAdvertising();
    if (wcard_inputs) lv_obj_add_flag(wcard_inputs, LV_OBJ_FLAG_HIDDEN);
    if (ble_active_panel) lv_obj_clear_flag(ble_active_panel, LV_OBJ_FLAG_HIDDEN);
    if (wcard_ref) lv_obj_set_style_border_color(wcard_ref, COLOR_GREEN, 0);
    if (label_conn_ble_detail) {
      lv_label_set_text(label_conn_ble_detail, "BLE Mode Active\nDevice: ScanPro-X1");
    }
    if (label_ble_large_status) {
      lv_label_set_text(label_ble_large_status, "Status: Advertising...\n\nDevice: ScanPro-X1\n\nLive Bluetooth scan streaming active.");
    }
    uiSetWifiStatus(LV_SYMBOL_BLUETOOTH " BLE: Adv...");
  } else {
    bleStopAdvertising();
    wifiStartAsync();
    if (wcard_inputs) lv_obj_clear_flag(wcard_inputs, LV_OBJ_FLAG_HIDDEN);
    if (ble_active_panel) lv_obj_add_flag(ble_active_panel, LV_OBJ_FLAG_HIDDEN);
    if (wcard_ref) lv_obj_set_style_border_color(wcard_ref, COLOR_SAFFRON, 0);
    if (label_conn_ble_detail) {
      lv_label_set_text(label_conn_ble_detail, "WiFi Mode Active\n(BLE Inactive)");
    }
    if (label_conn_wifi_detail) {
      lv_label_set_text(label_conn_wifi_detail, "Status: Connecting...\n(WiFi Mode)");
    }
    uiSetWifiStatus(LV_SYMBOL_WIFI " WiFi: Connecting...");
  }
}

static void build_conn_screen() {
  scr_conn = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_conn, COLOR_BG, 0);
  build_status_bar(scr_conn, "Connectivity");
  conn_nav_rail_ptr = build_nav_rail(scr_conn, 4);
  conn_content_ptr = build_content_area(scr_conn);
  lv_obj_t *content = conn_content_ptr;

  // ---- WiFi Card ----
  lv_obj_t *wcard = lv_obj_create(content);
  wcard_ref = wcard;
  lv_obj_set_size(wcard, 200, 240);
  lv_obj_align(wcard, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_bg_color(wcard, COLOR_WHITE, 0);
  lv_obj_set_style_border_color(wcard, COLOR_SAFFRON, 0);
  lv_obj_set_style_border_width(wcard, 2, 0);
  lv_obj_set_style_radius(wcard, 10, 0);
  lv_obj_set_style_pad_all(wcard, 6, 0);
  lv_obj_clear_flag(wcard, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *wt = lv_label_create(wcard);
  lv_label_set_text(wt, LV_SYMBOL_WIFI " WiFi Config");
  lv_obj_set_style_text_color(wt, COLOR_NAVY, 0);
  lv_obj_set_style_text_font(wt, &lv_font_montserrat_14, 0);
  lv_obj_align(wt, LV_ALIGN_TOP_LEFT, 0, 0);

  // Large BLE Active Panel (shown on left side during BLE Mode)
  ble_active_panel = lv_obj_create(wcard);
  lv_obj_set_size(ble_active_panel, 188, 200);
  lv_obj_align(ble_active_panel, LV_ALIGN_TOP_LEFT, 0, 22);
  lv_obj_set_style_bg_color(ble_active_panel, COLOR_WHITE, 0);
  lv_obj_set_style_border_color(ble_active_panel, COLOR_GREEN, 0);
  lv_obj_set_style_border_width(ble_active_panel, 1, 0);
  lv_obj_set_style_radius(ble_active_panel, 8, 0);
  lv_obj_set_style_pad_all(ble_active_panel, 6, 0);
  lv_obj_clear_flag(ble_active_panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *ble_banner_hdr = lv_label_create(ble_active_panel);
  lv_label_set_text(ble_banner_hdr, LV_SYMBOL_BLUETOOTH " BLE ACTIVE");
  lv_obj_set_style_text_color(ble_banner_hdr, COLOR_GREEN, 0);
  lv_obj_set_style_text_font(ble_banner_hdr, &lv_font_montserrat_14, 0);
  lv_obj_align(ble_banner_hdr, LV_ALIGN_TOP_MID, 0, 4);

  label_ble_large_status = lv_label_create(ble_active_panel);
  lv_label_set_text(label_ble_large_status, "Status: Advertising...\n\nDevice: ScanPro-X1\n\nLive Bluetooth scan streaming active.");
  lv_obj_set_style_text_color(label_ble_large_status, COLOR_NAVY, 0);
  lv_obj_set_style_text_font(label_ble_large_status, &lv_font_montserrat_12, 0);
  lv_label_set_long_mode(label_ble_large_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label_ble_large_status, 172);
  lv_obj_align(label_ble_large_status, LV_ALIGN_TOP_LEFT, 4, 34);

  lv_obj_add_flag(ble_active_panel, LV_OBJ_FLAG_HIDDEN); // hidden by default in WiFi mode

  // Input Fields Container Group
  wcard_inputs = lv_obj_create(wcard);
  lv_obj_set_size(wcard_inputs, 188, 200);
  lv_obj_align(wcard_inputs, LV_ALIGN_TOP_LEFT, 0, 22);
  lv_obj_set_style_bg_opa(wcard_inputs, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wcard_inputs, 0, 0);
  lv_obj_set_style_pad_all(wcard_inputs, 0, 0);
  lv_obj_clear_flag(wcard_inputs, LV_OBJ_FLAG_SCROLLABLE);

  // SSID Input Field
  ta_wifi_ssid = lv_textarea_create(wcard_inputs);
  lv_textarea_set_placeholder_text(ta_wifi_ssid, "WiFi SSID");
  lv_textarea_set_text(ta_wifi_ssid, wifi_ssid);
  lv_textarea_set_one_line(ta_wifi_ssid, true);
  lv_obj_set_size(ta_wifi_ssid, 184, 34);
  lv_obj_align(ta_wifi_ssid, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_text_font(ta_wifi_ssid, &lv_font_montserrat_12, 0);
  lv_obj_add_event_cb(ta_wifi_ssid, _ta_event_cb, LV_EVENT_ALL, NULL);

  // Password Input Field + Eye Toggle
  ta_wifi_pass = lv_textarea_create(wcard_inputs);
  lv_textarea_set_placeholder_text(ta_wifi_pass, "Password");
  lv_textarea_set_text(ta_wifi_pass, wifi_password);
  lv_textarea_set_password_mode(ta_wifi_pass, true);
  lv_textarea_set_one_line(ta_wifi_pass, true);
  lv_obj_set_size(ta_wifi_pass, 140, 34);
  lv_obj_align(ta_wifi_pass, LV_ALIGN_TOP_LEFT, 0, 38);
  lv_obj_set_style_text_font(ta_wifi_pass, &lv_font_montserrat_12, 0);
  lv_obj_add_event_cb(ta_wifi_pass, _ta_event_cb, LV_EVENT_ALL, NULL);

  lv_obj_t *btn_eye = lv_btn_create(wcard_inputs);
  lv_obj_set_size(btn_eye, 40, 34);
  lv_obj_align(btn_eye, LV_ALIGN_TOP_LEFT, 144, 38);
  lv_obj_set_style_bg_color(btn_eye, COLOR_BG, 0);
  lv_obj_set_style_border_color(btn_eye, COLOR_CARD_BRD, 0);
  lv_obj_set_style_border_width(btn_eye, 1, 0);
  lv_obj_set_style_radius(btn_eye, 6, 0);

  lv_obj_t *lbl_eye = lv_label_create(btn_eye);
  lv_label_set_text(lbl_eye, LV_SYMBOL_EYE_OPEN);
  lv_obj_set_style_text_color(lbl_eye, COLOR_CYAN, 0);
  lv_obj_center(lbl_eye);

  static auto _eye_btn_cb = [](lv_event_t *e) {
    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    bool pwd_mode = lv_textarea_get_password_mode(ta_wifi_pass);
    lv_textarea_set_password_mode(ta_wifi_pass, !pwd_mode);
    lv_label_set_text(lbl, !pwd_mode ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
  };
  lv_obj_add_event_cb(btn_eye, _eye_btn_cb, LV_EVENT_CLICKED, lbl_eye);

  // WiFi Status Detail Label
  label_conn_wifi_detail = lv_label_create(wcard_inputs);
  lv_label_set_text(label_conn_wifi_detail, "Not connected");
  lv_obj_set_style_text_color(label_conn_wifi_detail, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(label_conn_wifi_detail, &lv_font_montserrat_12, 0);
  lv_label_set_long_mode(label_conn_wifi_detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label_conn_wifi_detail, 184);
  lv_obj_align(label_conn_wifi_detail, LV_ALIGN_TOP_LEFT, 0, 78);

  // Save & Connect Button
  lv_obj_t *wbtn = lv_btn_create(wcard_inputs);
  lv_obj_set_size(wbtn, 184, 36);
  lv_obj_set_style_bg_color(wbtn, COLOR_SAFFRON, 0);
  lv_obj_set_style_radius(wbtn, 8, 0);
  lv_obj_align(wbtn, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_event_cb(wbtn, _wifi_save_connect_btn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *wbl = lv_label_create(wbtn);
  lv_label_set_text(wbl, "Save & Connect");
  lv_obj_set_style_text_color(wbl, COLOR_NAVY, 0);
  lv_obj_center(wbl);

  // Sync initial state if BLE mode was active at boot
  if (bleIsAdvertising() || bleIsConnected()) {
    lv_obj_add_flag(wcard_inputs, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ble_active_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_color(wcard, COLOR_GREEN, 0);
  }

  // ---- Network Mode Card (Exclusive WiFi vs BLE Switch) ----
  lv_obj_t *bcard = lv_obj_create(content);
  lv_obj_set_size(bcard, 195, 105);
  lv_obj_align(bcard, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(bcard, COLOR_WHITE, 0);
  lv_obj_set_style_border_color(bcard, COLOR_NAVY, 0);
  lv_obj_set_style_border_width(bcard, 2, 0);
  lv_obj_set_style_radius(bcard, 10, 0);
  lv_obj_set_style_pad_all(bcard, 6, 0);
  lv_obj_clear_flag(bcard, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *bt = lv_label_create(bcard);
  lv_label_set_text(bt, LV_SYMBOL_SETTINGS " Active Mode");
  lv_obj_set_style_text_color(bt, COLOR_NAVY, 0);
  lv_obj_set_style_text_font(bt, &lv_font_montserrat_14, 0);
  lv_obj_align(bt, LV_ALIGN_TOP_LEFT, 0, 0);

  label_conn_ble_detail = lv_label_create(bcard);
  lv_label_set_text(label_conn_ble_detail, "WiFi Mode Active");
  lv_obj_set_style_text_color(label_conn_ble_detail, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(label_conn_ble_detail, &lv_font_montserrat_12, 0);
  lv_obj_align(label_conn_ble_detail, LV_ALIGN_TOP_LEFT, 0, 22);

  // Exclusive Mode Switch: OFF = WiFi Mode, ON = BLE Mode
  lv_obj_t *sw_mode = lv_switch_create(bcard);
  lv_obj_set_size(sw_mode, 80, 36);
  lv_obj_align(sw_mode, LV_ALIGN_BOTTOM_MID, 0, 0);
  if (bleIsAdvertising() || bleIsConnected()) {
    lv_obj_add_state(sw_mode, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(sw_mode, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(sw_mode, _mode_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // ---- Landscape Orientation Card (Left / Right Landscape Switch) ----
  lv_obj_t *rcard = lv_obj_create(content);
  lv_obj_set_size(rcard, 195, 125);
  lv_obj_align(rcard, LV_ALIGN_TOP_RIGHT, 0, 115);
  lv_obj_set_style_bg_color(rcard, COLOR_WHITE, 0);
  lv_obj_set_style_border_color(rcard, COLOR_GREEN, 0);
  lv_obj_set_style_border_width(rcard, 2, 0);
  lv_obj_set_style_radius(rcard, 10, 0);
  lv_obj_set_style_pad_all(rcard, 6, 0);
  lv_obj_clear_flag(rcard, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *rt = lv_label_create(rcard);
  lv_label_set_text(rt, LV_SYMBOL_REFRESH " Orientation");
  lv_obj_set_style_text_color(rt, COLOR_NAVY, 0);
  lv_obj_set_style_text_font(rt, &lv_font_montserrat_14, 0);
  lv_obj_align(rt, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *rlbl = lv_label_create(rcard);
  lv_label_set_text(rlbl, "Left / Right Landscape");
  lv_obj_set_style_text_color(rlbl, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(rlbl, &lv_font_montserrat_12, 0);
  lv_obj_align(rlbl, LV_ALIGN_TOP_LEFT, 0, 22);

  // Toggle Switch: OFF = Left Landscape (rot 1), ON = Right Landscape (rot 3)
  lv_obj_t *sw = lv_switch_create(rcard);
  lv_obj_set_size(sw, 70, 36);
  lv_obj_align(sw, LV_ALIGN_BOTTOM_MID, 0, 4);
  if (current_rotation == 3) {
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(sw, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(sw, _landscape_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // ---- Action Buttons (Logout & Power Off) ----
  lv_obj_t *btn_row = lv_obj_create(content);
  conn_bottom_row_ptr = btn_row;
  lv_obj_set_size(btn_row, LV_PCT(100), 44);
  lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_row, 0, 0);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *conn_logout_btn = lv_btn_create(btn_row);
  lv_obj_set_size(conn_logout_btn, 195, 40);
  lv_obj_set_style_bg_color(conn_logout_btn, COLOR_DANGER, 0);
  lv_obj_set_style_radius(conn_logout_btn, 8, 0);
  conn_logout_lbl_ptr = lv_label_create(conn_logout_btn);
  lv_label_set_text(conn_logout_lbl_ptr, LV_SYMBOL_POWER " LOGOUT");
  lv_obj_set_style_text_color(conn_logout_lbl_ptr, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(conn_logout_lbl_ptr, &lv_font_montserrat_14, 0);
  lv_obj_center(conn_logout_lbl_ptr);
  lv_obj_add_event_cb(conn_logout_btn, _logout_action_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *conn_power_btn = lv_btn_create(btn_row);
  lv_obj_set_size(conn_power_btn, 195, 40);
  lv_obj_set_style_bg_color(conn_power_btn, lv_color_hex(0x880000), 0);
  lv_obj_set_style_border_color(conn_power_btn, COLOR_DANGER, 0);
  lv_obj_set_style_border_width(conn_power_btn, 1, 0);
  lv_obj_set_style_radius(conn_power_btn, 8, 0);
  lv_obj_t *conn_power_lbl = lv_label_create(conn_power_btn);
  lv_label_set_text(conn_power_lbl, LV_SYMBOL_POWER " POWER OFF");
  lv_obj_set_style_text_color(conn_power_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(conn_power_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(conn_power_lbl);
  lv_obj_add_event_cb(conn_power_btn, _power_off_action_cb, LV_EVENT_CLICKED, NULL);
}

// ============================================================================
// F. SETTINGS / DIAGNOSTICS
// ============================================================================
static void build_settings_screen() {
  scr_settings = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_settings, COLOR_BG, 0);
  build_status_bar(scr_settings, "Settings");
  build_nav_rail(scr_settings, -1);
  lv_obj_t *content = build_content_area(scr_settings);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(content, 4, 0);

  const char *rows[4] = {"WiFi status", "MQTT broker", "Battery / power", "Firmware version"};
  for (int i = 0; i < 4; i++) {
    lv_obj_t *row = lv_obj_create(content);
    lv_obj_set_size(row, LV_PCT(100), 36);
    lv_obj_set_style_bg_color(row, COLOR_WHITE, 0);
    lv_obj_set_style_border_color(row, COLOR_CARD_BRD, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, rows[i]);
    lv_obj_set_style_text_color(lbl, COLOR_NAVY, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 6, 0);
  }

  // Action Buttons Row (Logout & Power Off)
  lv_obj_t *s_btn_row = lv_obj_create(content);
  lv_obj_set_size(s_btn_row, LV_PCT(100), 44);
  lv_obj_set_style_bg_opa(s_btn_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(s_btn_row, 0, 0);
  lv_obj_set_style_pad_all(s_btn_row, 0, 0);
  lv_obj_clear_flag(s_btn_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(s_btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *logout_btn = lv_btn_create(s_btn_row);
  lv_obj_set_size(logout_btn, 195, 40);
  lv_obj_set_style_bg_color(logout_btn, COLOR_DANGER, 0);
  lv_obj_set_style_radius(logout_btn, 8, 0);
  lv_obj_t *logout_lbl = lv_label_create(logout_btn);
  lv_label_set_text(logout_lbl, LV_SYMBOL_POWER " LOGOUT");
  lv_obj_set_style_text_color(logout_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(logout_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(logout_lbl);
  lv_obj_add_event_cb(logout_btn, _logout_action_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *power_btn = lv_btn_create(s_btn_row);
  lv_obj_set_size(power_btn, 195, 40);
  lv_obj_set_style_bg_color(power_btn, lv_color_hex(0x880000), 0);
  lv_obj_set_style_border_color(power_btn, COLOR_DANGER, 0);
  lv_obj_set_style_border_width(power_btn, 1, 0);
  lv_obj_set_style_radius(power_btn, 8, 0);
  lv_obj_t *power_lbl = lv_label_create(power_btn);
  lv_label_set_text(power_lbl, LV_SYMBOL_POWER " POWER OFF");
  lv_obj_set_style_text_color(power_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(power_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(power_lbl);
  lv_obj_add_event_cb(power_btn, _power_off_action_cb, LV_EVENT_CLICKED, NULL);
}

// ============================================================================
// G. LOGIN SCREEN — Left ID/Pass + Right Permanent Number Board
// ============================================================================
static lv_obj_t *ta_active_login = NULL;
static lv_obj_t *label_login_err = NULL;

// Helper: styled futuristic textarea
static lv_obj_t* _make_cyber_ta(lv_obj_t *parent) {
  lv_obj_t *ta = lv_textarea_create(parent);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x070C1F), 0);
  lv_obj_set_style_border_color(ta, COLOR_CARD_BRD, 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_radius(ta, 6, 0);
  lv_obj_set_style_text_color(ta, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(ta, &lv_font_montserrat_12, 0);
  return ta;
}

static void _login_ta_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
    ta_active_login = ta;
    if (ta_login_user && ta_login_pass) {
      lv_obj_set_style_border_color(ta_login_user, (ta == ta_login_user) ? COLOR_CYAN : COLOR_CARD_BRD, 0);
      lv_obj_set_style_border_color(ta_login_pass, (ta == ta_login_pass) ? COLOR_CYAN : COLOR_CARD_BRD, 0);
    }
  }
}

static void _numpad_event_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  lv_obj_t *matrix = lv_event_get_target(e);
  uint16_t btn_id = lv_btnmatrix_get_selected_btn(matrix);
  if (btn_id == LV_BTNMATRIX_BTN_NONE) return;
  const char *txt = lv_btnmatrix_get_btn_text(matrix, btn_id);
  if (!txt || !ta_active_login) return;

  if (strcmp(txt, "Clr") == 0) {
    lv_textarea_set_text(ta_active_login, "");
  } else if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
    lv_textarea_del_char(ta_active_login);
  } else {
    lv_textarea_add_text(ta_active_login, txt);
  }
}

static void build_login_screen() {
  scr_login = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr_login, lv_color_hex(0x040812), 0);

  // ── LEFT PANEL: ID & Password Inputs ──────────────────────────────────────
  lv_obj_t *left_panel = lv_obj_create(scr_login);
  lv_obj_set_size(left_panel, 230, 260);
  lv_obj_align(left_panel, LV_ALIGN_TOP_LEFT, 8, 48);
  lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x080F25), 0);
  lv_obj_set_style_border_color(left_panel, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(left_panel, 1, 0);
  lv_obj_set_style_radius(left_panel, 10, 0);
  lv_obj_set_style_pad_all(left_panel, 10, 0);
  lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

  // User ID field
  lv_obj_t *ul = lv_label_create(left_panel);
  lv_label_set_text(ul, "USER ID");
  lv_obj_set_style_text_color(ul, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(ul, &lv_font_montserrat_12, 0);
  lv_obj_align(ul, LV_ALIGN_TOP_LEFT, 0, 0);

  ta_login_user = _make_cyber_ta(left_panel);
  lv_textarea_set_placeholder_text(ta_login_user, "User ID");
  lv_textarea_set_text(ta_login_user, "1");
  lv_textarea_set_one_line(ta_login_user, true);
  lv_obj_set_size(ta_login_user, 208, 36);
  lv_obj_align(ta_login_user, LV_ALIGN_TOP_LEFT, 0, 18);
  lv_obj_add_event_cb(ta_login_user, _login_ta_event_cb, LV_EVENT_ALL, NULL);
  ta_active_login = ta_login_user; // Default active field
  lv_obj_set_style_border_color(ta_login_user, COLOR_CYAN, 0);

  // Password field
  lv_obj_t *pl = lv_label_create(left_panel);
  lv_label_set_text(pl, "PASSWORD");
  lv_obj_set_style_text_color(pl, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(pl, &lv_font_montserrat_12, 0);
  lv_obj_align(pl, LV_ALIGN_TOP_LEFT, 0, 60);

  ta_login_pass = _make_cyber_ta(left_panel);
  lv_textarea_set_placeholder_text(ta_login_pass, "Password");
  lv_textarea_set_password_mode(ta_login_pass, true);
  lv_textarea_set_one_line(ta_login_pass, true);
  lv_obj_set_size(ta_login_pass, 166, 36);
  lv_obj_align(ta_login_pass, LV_ALIGN_TOP_LEFT, 0, 78);
  lv_obj_add_event_cb(ta_login_pass, _login_ta_event_cb, LV_EVENT_ALL, NULL);

  // Eye toggle button
  lv_obj_t *eye_btn = lv_btn_create(left_panel);
  lv_obj_set_size(eye_btn, 38, 36);
  lv_obj_align(eye_btn, LV_ALIGN_TOP_LEFT, 170, 78);
  lv_obj_set_style_bg_color(eye_btn, lv_color_hex(0x0D1535), 0);
  lv_obj_set_style_border_color(eye_btn, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(eye_btn, 1, 0);
  lv_obj_set_style_radius(eye_btn, 6, 0);
  lv_obj_set_style_shadow_width(eye_btn, 0, 0);
  lv_obj_t *eye_lbl = lv_label_create(eye_btn);
  lv_label_set_text(eye_lbl, LV_SYMBOL_EYE_OPEN);
  lv_obj_set_style_text_color(eye_lbl, COLOR_CYAN, 0);
  lv_obj_center(eye_lbl);
  lv_obj_add_event_cb(eye_btn, [](lv_event_t *e) {
    lv_obj_t *lbl = (lv_obj_t *)lv_event_get_user_data(e);
    bool pwd = lv_textarea_get_password_mode(ta_login_pass);
    lv_textarea_set_password_mode(ta_login_pass, !pwd);
    lv_label_set_text(lbl, !pwd ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
  }, LV_EVENT_CLICKED, eye_lbl);

  // Error label
  label_login_err = lv_label_create(left_panel);
  lv_label_set_text(label_login_err, "");
  lv_obj_set_style_text_color(label_login_err, COLOR_DANGER, 0);
  lv_obj_set_style_text_font(label_login_err, &lv_font_montserrat_12, 0);
  lv_obj_align(label_login_err, LV_ALIGN_TOP_LEFT, 0, 122);

  // ── QR Badge scan alternative ──────────────────────────────────────────

  lv_obj_t *qr_hint = lv_label_create(left_panel);
  lv_label_set_text(qr_hint, LV_SYMBOL_BARS "  Scan QR badge to login");
  lv_obj_set_style_text_color(qr_hint, COLOR_CYAN, 0);
  lv_obj_set_style_text_font(qr_hint, &lv_font_montserrat_12, 0);
  lv_label_set_long_mode(qr_hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(qr_hint, 208);
  lv_obj_align(qr_hint, LV_ALIGN_TOP_LEFT, 0, 158);
  label_login_qr_status = qr_hint; // Reuse this label for live feedback

  // Authenticate button
  lv_obj_t *btn_login = lv_btn_create(left_panel);
  lv_obj_set_size(btn_login, 208, 40);
  lv_obj_align(btn_login, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(btn_login, COLOR_SAFFRON, 0);
  lv_obj_set_style_radius(btn_login, 8, 0);
  lv_obj_set_style_shadow_width(btn_login, 0, 0);
  lv_obj_t *login_lbl = lv_label_create(btn_login);
  lv_label_set_text(login_lbl, LV_SYMBOL_OK "  AUTHENTICATE");
  lv_obj_set_style_text_color(login_lbl, lv_color_hex(0x040812), 0);
  lv_obj_set_style_text_font(login_lbl, &lv_font_montserrat_12, 0);
  lv_obj_center(login_lbl);
  lv_obj_add_event_cb(btn_login, [](lv_event_t *e) {
    const char *user = lv_textarea_get_text(ta_login_user);
    const char *pass = lv_textarea_get_text(ta_login_pass);
    if (strlen(user) == 0) {
      if (label_login_err) lv_label_set_text(label_login_err, "! User ID is required");
      return;
    }

    bool found = false;
    bool auth_ok = false;
    String matched_name = "";
    for (int i = 0; i < global_user_count; i++) {
      if (strcmp(global_users[i].id, user) == 0) {
        found = true;
        if (strcmp(global_users[i].pin, pass) == 0) {
          auth_ok = true;
          matched_name = global_users[i].name;
        }
        break;
      }
    }

    if (auth_ok) {
      is_logged_in = true;
      strncpy(logged_in_user, matched_name.c_str(), sizeof(logged_in_user)-1);
      logged_in_user[sizeof(logged_in_user)-1] = '\0';
      publishDeviceStatus(true, logged_in_user);
      lv_textarea_set_text(ta_login_user, "");
      lv_textarea_set_text(ta_login_pass, "");
      lv_textarea_set_password_mode(ta_login_pass, true);
      if (label_login_err) lv_label_set_text(label_login_err, "");
      lv_scr_load(scr_home);
      if (status_title_label) lv_label_set_text(status_title_label, "Home");
    } else {
      if (!found) {
        if (label_login_err) lv_label_set_text(label_login_err, "! User ID not found");
      } else {
        if (label_login_err) lv_label_set_text(label_login_err, "! Incorrect password");
      }
    }
  }, LV_EVENT_CLICKED, NULL);

  // ── RIGHT PANEL: Permanent Number Board (Keypad) ──────────────────────────
  lv_obj_t *right_panel = lv_obj_create(scr_login);
  lv_obj_set_size(right_panel, 226, 274);
  lv_obj_align(right_panel, LV_ALIGN_TOP_RIGHT, -8, 38);
  lv_obj_set_style_bg_color(right_panel, lv_color_hex(0x080F25), 0);
  lv_obj_set_style_border_color(right_panel, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(right_panel, 1, 0);
  lv_obj_set_style_radius(right_panel, 10, 0);
  lv_obj_set_style_pad_all(right_panel, 6, 0);
  lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *num_hdr = lv_label_create(right_panel);
  lv_label_set_text(num_hdr, "NUMBER BOARD");
  lv_obj_set_style_text_color(num_hdr, COLOR_CYAN, 0);
  lv_obj_set_style_text_font(num_hdr, &lv_font_montserrat_12, 0);
  lv_obj_align(num_hdr, LV_ALIGN_TOP_MID, 0, 2);

  static const char * numpad_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    "Clr", "0", LV_SYMBOL_BACKSPACE, ""
  };

  lv_obj_t *btnm = lv_btnmatrix_create(right_panel);
  lv_btnmatrix_set_map(btnm, numpad_map);
  lv_obj_set_size(btnm, 210, 240);
  lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(btnm, lv_color_hex(0x040812), 0);
  lv_obj_set_style_border_width(btnm, 0, 0);
  lv_obj_set_style_radius(btnm, 8, 0);

  // Button matrix styling: dark cyber buttons with cyan text
  lv_obj_set_style_bg_color(btnm, lv_color_hex(0x0D1535), LV_PART_ITEMS);
  lv_obj_set_style_text_color(btnm, COLOR_CYAN, LV_PART_ITEMS);
  lv_obj_set_style_text_font(btnm, &lv_font_montserrat_14, LV_PART_ITEMS);
  lv_obj_set_style_border_color(btnm, lv_color_hex(0x1A2952), LV_PART_ITEMS);
  lv_obj_set_style_border_width(btnm, 1, LV_PART_ITEMS);
  lv_obj_set_style_radius(btnm, 6, LV_PART_ITEMS);

  lv_obj_add_event_cb(btnm, _numpad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// ============================================================================
// Nav callbacks — direct 0ms load with no slide/animation
// ============================================================================
static void _load_scr_direct(lv_obj_t *target, const char *title) {
  if (!target) return;
  lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  if (status_title_label) lv_label_set_text(status_title_label, title ? title : "");
}

static void nav_login_cb(lv_event_t *e)      { _load_scr_direct(scr_login, ""); }
static void nav_home_cb(lv_event_t *e)       { if (!is_logged_in) { nav_login_cb(e); return; } _load_scr_direct(scr_home, "Home"); }
static void nav_scan_cb(lv_event_t *e)       { if (!is_logged_in) { nav_login_cb(e); return; } _load_scr_direct(scr_scan, "Scan"); }
static void nav_tasks_cb(lv_event_t *e)      { if (!is_logged_in) { nav_login_cb(e); return; } _load_scr_direct(scr_tasks, "Tasks"); }
static void nav_inventory_cb(lv_event_t *e)  { if (!is_logged_in) { nav_login_cb(e); return; } _load_scr_direct(scr_inventory, "Inventory"); }
static void nav_conn_cb(lv_event_t *e)       { if (!is_logged_in) { nav_login_cb(e); return; } _load_scr_direct(scr_conn, "Connectivity"); }
static void nav_settings_cb(lv_event_t *e)   { if (!is_logged_in) { nav_login_cb(e); return; } _load_scr_direct(scr_settings, "Settings"); }

// ============================================================================
// Public API (same function names as before, so smart_barcode_scanner.ino
// does not need to change)
// ============================================================================
inline void uiInit() {
  // Create the global persistent status bar FIRST so battery/wifi labels exist
  create_global_status_bar();

  kb_shared = lv_keyboard_create(lv_layer_top());
  lv_obj_set_size(kb_shared, 480, 130);
  lv_obj_align(kb_shared, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_add_flag(kb_shared, LV_OBJ_FLAG_HIDDEN);

  build_login_screen();
  build_home_screen();
  build_scan_screen();
  build_tasks_screen();
  build_inventory_screen();
  build_conn_screen();
  build_settings_screen();
  lv_scr_load(scr_login); // Start at login
}

// NOTE: each screen currently creates its own status-bar label, and this
// function only updates the one built last (Settings). For a quick fix that
// updates the CURRENT screen's status text regardless of which screen is
// active, call this every time right after lv_scr_load() switches screens,
// or (cleaner) refactor to a single persistent top-layer status bar shared
// across all screens using lv_layer_top(). Left as-is here to keep this
// pass focused and easy to read; flag it if you want the persistent-bar
// refactor next.
inline void uiSetWifiStatus(const String &text) {
  if (label_wifi_status) lv_label_set_text(label_wifi_status, text.c_str());
}

inline void uiSetBatteryLevel(uint8_t percent, bool isCharging = false) {
  if (!label_battery_status) return;
  if (percent > 100) percent = 100;

  const char *icon = LV_SYMBOL_BATTERY_FULL;
  if (isCharging) {
    icon = LV_SYMBOL_CHARGE;
  } else if (percent > 80) {
    icon = LV_SYMBOL_BATTERY_FULL;
  } else if (percent > 50) {
    icon = LV_SYMBOL_BATTERY_3;
  } else if (percent > 25) {
    icon = LV_SYMBOL_BATTERY_2;
  } else if (percent > 10) {
    icon = LV_SYMBOL_BATTERY_1;
  } else {
    icon = LV_SYMBOL_BATTERY_EMPTY;
  }

  char buf[20];
  snprintf(buf, sizeof(buf), "%s %d%%", icon, percent);
  lv_label_set_text(label_battery_status, buf);
}

inline void uiUpdateConnScreen() {
  if (lv_scr_act() != scr_conn) return;

  if (label_conn_wifi_detail) {
    if (WiFi.status() == WL_CONNECTED) {
      String info = "SSID: " + String(wifi_ssid) + "\nIP: " + WiFi.localIP().toString();
      lv_label_set_text(label_conn_wifi_detail, info.c_str());
    } else {
      lv_label_set_text(label_conn_wifi_detail, "Status: Disconnected\n(Tap Connect WiFi)");
    }
  }

  if (bleIsConnected()) {
    if (label_conn_ble_detail) lv_label_set_text(label_conn_ble_detail, "BLE Mode: Connected\nDevice: ScanPro-X1");
    if (label_ble_large_status) lv_label_set_text(label_ble_large_status, "Status: Connected!\n\nDevice: ScanPro-X1\n\nLive Bluetooth barcode streaming ready.");
  } else if (bleIsAdvertising()) {
    if (label_conn_ble_detail) lv_label_set_text(label_conn_ble_detail, "BLE Mode: Advertising...\nDevice: ScanPro-X1");
    if (label_ble_large_status) lv_label_set_text(label_ble_large_status, "Status: Advertising...\n\nDevice: ScanPro-X1\n\nReady for client connection.");
  } else {
    if (label_conn_ble_detail) lv_label_set_text(label_conn_ble_detail, "WiFi Mode Active\n(BLE Inactive)");
  }
}

inline void uiShowScanResult(const String &sku) {
  scan_counter++;
  if (label_last_scan_sku) {
    String txt = "SKU: " + sku;
    lv_label_set_text(label_last_scan_sku, txt.c_str());
  }
  if (label_last_scan_flag) {
    lv_label_set_text(label_last_scan_flag, "Matched - added to list");
  }
  if (label_scan_progress) {
    String txt = "Progress: " + String(scan_counter) + " / " + String(scan_target);
    lv_label_set_text(label_scan_progress, txt.c_str());
  }
  if (bar_scan_progress) {
    int pct = (scan_target > 0) ? (int)((scan_counter * 100) / scan_target) : 0;
    if (pct > 100) pct = 100;
    lv_bar_set_value(bar_scan_progress, pct, LV_ANIM_ON);
  }
  if (label_scan_count_home) {
    String txt = "Scans this session: " + String(scan_counter);
    lv_label_set_text(label_scan_count_home, txt.c_str());
  }
  lv_scr_load(scr_scan);
}

// ============================================================================
// QR Badge Login — called from loop() when a scan arrives and !is_logged_in
// QR format: "USER:<username>:<pin>"  e.g.  "USER:bhargav:1234"
// The embedded PIN is verified against DEFAULT_PASSWORD before granting access.
// ============================================================================
inline void uiLoginViaScan(const String &raw) {
  // ── 1. Validate prefix ────────────────────────────────────────────────────
  if (!raw.startsWith("USER:")) {
    Serial.printf("[login] QR rejected (no USER: prefix): %s\n", raw.c_str());
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_CLOSE "  Invalid badge not a user QR");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_DANGER, 0);
    }
    if (label_login_err) lv_label_set_text(label_login_err, "! Scan a valid USER QR badge");
    return;
  }

  // ── 2. Parse "USER:<username>:<pin>" ────────────────────────────────────
  String remainder = raw.substring(5); // strip "USER:"
  remainder.trim();
  int sep = remainder.indexOf(':');
  if (sep < 0) {
    // Missing PIN separator
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_CLOSE "  Bad QR format — need USER:id:pin");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_DANGER, 0);
    }
    if (label_login_err) lv_label_set_text(label_login_err, "! QR format: USER:<id>:<pin>");
    Serial.printf("[login] QR format error (missing PIN): %s\n", raw.c_str());
    return;
  }

  String username = remainder.substring(0, sep);
  String pin      = remainder.substring(sep + 1);
  username.trim();
  pin.trim();

  if (username.length() == 0) {
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_CLOSE "  QR has empty username");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_DANGER, 0);
    }
    return;
  }

  // ── 3. Verify PIN and User ──────────────────────────────────────────────
  bool found = false;
  bool auth_ok = false;
  String matched_name = username;
  for (int i = 0; i < global_user_count; i++) {
    if (String(global_users[i].id) == username) {
      found = true;
      if (String(global_users[i].pin) == pin) {
        auth_ok = true;
        matched_name = global_users[i].name;
      }
      break;
    }
  }

  if (!found || !auth_ok) {
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_CLOSE "  Invalid credentials or user not found");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_DANGER, 0);
    }
    if (label_login_err) lv_label_set_text(label_login_err, "! QR badge access denied");
    Serial.printf("[login] QR auth failed for user='%s'\n", username.c_str());
    return;
  }

  // ── 4. Login success ─────────────────────────────────────────────────────
  is_logged_in = true;
  strncpy(logged_in_user, matched_name.c_str(), sizeof(logged_in_user) - 1);
  logged_in_user[sizeof(logged_in_user) - 1] = '\0';
  publishDeviceStatus(true, logged_in_user);

  // Clear keyboard fields
  if (ta_login_user) lv_textarea_set_text(ta_login_user, "");
  if (ta_login_pass) {
    lv_textarea_set_text(ta_login_pass, "");
    lv_textarea_set_password_mode(ta_login_pass, true);
  }
  if (label_login_err) lv_label_set_text(label_login_err, "");

  // Brief welcome flash (visible for ~300 ms before screen switches)
  if (label_login_qr_status) {
    String welcome = String(LV_SYMBOL_OK) + "  Welcome, " + username + "!";
    lv_label_set_text(label_login_qr_status, welcome.c_str());
    lv_obj_set_style_text_color(label_login_qr_status, COLOR_GREEN, 0);
  }

  Serial.printf("[login] QR login OK — user='%s'\n", logged_in_user);

  lv_scr_load(scr_home);
  if (status_title_label) lv_label_set_text(status_title_label, "Home");
}
