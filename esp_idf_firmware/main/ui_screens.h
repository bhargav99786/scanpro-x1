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
#include "ble_scanner.h"
#include <lvgl.h>
#include "network_mqtt.h"
#include "display_port.h"
#include "esp_es8311_port.h"
#include "gm65_scanner.h"
#include "pins_config.h"
#include "splash_logo.h"

#include "cJSON.h"

// Forward declarations
inline lv_obj_t *scr_connecting;
static void _load_scr_direct(lv_obj_t *scr, const char *title);
inline void update_tasks_ui();
static void update_task_detail_ui();
inline void update_inventory_ui();
inline void uiShowTaskError(const char* msg);

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
#define COLOR_WARNING   lv_color_hex(0xFF9800)   // Vibrant Amber/Orange for Medium
#define COLOR_DANGER_BG lv_color_hex(0x1A0010)   // Dark red bg
#define COLOR_GREEN     COLOR_NAVY_BLUE          // Replaced neon green with Navy Blue

// ---- Screens ----
inline lv_obj_t *scr_splash = NULL;
inline lv_timer_t *splash_timer = NULL;
inline lv_obj_t *global_status_bar_obj = NULL;
inline lv_obj_t *global_status_stripe_obj = NULL;
inline lv_obj_t *scr_login, *scr_home, *scr_scan, *scr_tasks, *scr_inventory, *scr_conn, *scr_settings, *scr_qty_adjust, *scr_ota;
inline lv_obj_t *settings_content_ptr = NULL; // global ref for resize/rebuild
inline lv_obj_t *settings_btn_row_ptr = NULL;  // global ref for settings bottom buttons
inline lv_obj_t *home_content_ptr = NULL;      // global ref for home screen resize
inline lv_obj_t *home_arc_ptr = NULL;          // arc widget on home screen
inline lv_obj_t *home_btn_container_ptr = NULL;// buttons container on home screen
inline lv_obj_t *sw_wrist_l_ptr = NULL;        // Left Wrist orientation switch
inline lv_obj_t *sw_wrist_r_ptr = NULL;        // Right Wrist orientation switch
inline lv_obj_t *sw_port_ptr = NULL;           // Handheld/Portrait orientation switch
inline int g_last_landscape_rot = 1;           // Remembers last landscape rotation (1 or 3)
inline int g_last_portrait_rot  = 1;           // Remembers last portrait rotation (1 or 3)
inline bool g_orient_cb_busy    = false;       // Re-entrancy guard for orientation callbacks

inline void update_orient_switches_state(uint8_t rot) {
  if (!sw_wrist_l_ptr || !sw_wrist_r_ptr || !sw_port_ptr) return;
  bool prev_busy = g_orient_cb_busy;
  g_orient_cb_busy = true;
  if (rot == 0) {
    lv_obj_clear_state(sw_wrist_l_ptr, LV_STATE_CHECKED);
    lv_obj_clear_state(sw_wrist_r_ptr, LV_STATE_CHECKED);
    lv_obj_add_state(sw_port_ptr, LV_STATE_CHECKED);
  } else if (rot == 3) {
    lv_obj_add_state(sw_wrist_l_ptr, LV_STATE_CHECKED);
    lv_obj_clear_state(sw_wrist_r_ptr, LV_STATE_CHECKED);
    lv_obj_clear_state(sw_port_ptr, LV_STATE_CHECKED);
  } else { // rot == 1 (default landscape)
    lv_obj_clear_state(sw_wrist_l_ptr, LV_STATE_CHECKED);
    lv_obj_add_state(sw_wrist_r_ptr, LV_STATE_CHECKED);
    lv_obj_clear_state(sw_port_ptr, LV_STATE_CHECKED);
  }
  g_orient_cb_busy = prev_busy;
}
inline lv_obj_t *label_qty_sku = NULL;
inline lv_obj_t *label_qty_name = NULL;
inline lv_obj_t *label_qty_val = NULL;
inline std::string current_scan_adjust_sku = "";
inline std::string selected_task_item_sku = "";
inline int current_scan_adjust_qty = 1;

// ---- Login state ----
inline bool      is_logged_in = false;
inline char      logged_in_user[32] = "";
inline char      logged_in_user_id[32] = "";
inline char      logged_in_user_role[32] = "";

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

inline void init_default_users() {
  global_user_count = 0;
  
  strncpy(global_users[0].id, "admin", sizeof(global_users[0].id) - 1);
  strncpy(global_users[0].name, "System Admin", sizeof(global_users[0].name) - 1);
  strncpy(global_users[0].role, "Supervisor", sizeof(global_users[0].role) - 1);
  strncpy(global_users[0].pin, "1234", sizeof(global_users[0].pin) - 1);
  global_user_count++;

  strncpy(global_users[1].id, "1", sizeof(global_users[1].id) - 1);
  strncpy(global_users[1].name, "Operator 1", sizeof(global_users[1].name) - 1);
  strncpy(global_users[1].role, "Warehouse Operator", sizeof(global_users[1].role) - 1);
  strncpy(global_users[1].pin, "1234", sizeof(global_users[1].pin) - 1);
  global_user_count++;

  strncpy(global_users[2].id, "2", sizeof(global_users[2].id) - 1);
  strncpy(global_users[2].name, "Operator 2", sizeof(global_users[2].name) - 1);
  strncpy(global_users[2].role, "Warehouse Operator", sizeof(global_users[2].role) - 1);
  strncpy(global_users[2].pin, "1234", sizeof(global_users[2].pin) - 1);
  global_user_count++;

  strncpy(global_users[3].id, "123", sizeof(global_users[3].id) - 1);
  strncpy(global_users[3].name, "Operator 1", sizeof(global_users[3].name) - 1);
  strncpy(global_users[3].role, "Warehouse Operator", sizeof(global_users[3].role) - 1);
  strncpy(global_users[3].pin, "123", sizeof(global_users[3].pin) - 1);
  global_user_count++;
}

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
  char assignee[32];
  char prio[16]; 
  char status[16];
  lv_color_t prio_color;
  int item_count;
  TaskItem items[MAX_ITEMS_PER_TASK];
};

inline TaskDef current_tasks[MAX_TASKS];
inline int current_task_count = 0;
inline lv_obj_t *tasks_content_ptr = NULL;
inline lv_obj_t *scan_content_ptr = NULL;
inline lv_obj_t *label_home_user = NULL;

inline TaskDef *active_task = NULL;
inline lv_obj_t *scr_task_detail = NULL;
inline lv_obj_t *task_detail_content = NULL;
inline lv_obj_t *task_error_label = NULL;

inline bool is_task_assigned_to_current_user(const TaskDef &t) {
  if (!is_logged_in) return false;
  // Supervisors and Admins can see and work on all tasks
  if (strcasecmp(logged_in_user_role, "supervisor") == 0 ||
      strcasecmp(logged_in_user_role, "admin") == 0 ||
      strcasecmp(logged_in_user, "system admin") == 0 ||
      strcasecmp(logged_in_user_id, "admin") == 0) {
    return true;
  }
  // Broadcast or unassigned tasks visible to everyone
  if (strlen(t.assignee) == 0 ||
      strcasecmp(t.assignee, "all") == 0 ||
      strcasecmp(t.assignee, "unassigned") == 0) {
    return true;
  }
  // Match by User ID
  if (strlen(logged_in_user_id) > 0 && strcasecmp(t.assignee, logged_in_user_id) == 0) {
    return true;
  }
  // Match by User Name
  if (strlen(logged_in_user) > 0 && strcasecmp(t.assignee, logged_in_user) == 0) {
    return true;
  }
  return false;
}

// ---- Inventory data ----
#define MAX_INVENTORY 50
struct InvItem { char name[32]; char sku[32]; char qty[16]; };
inline InvItem global_inventory[MAX_INVENTORY];
inline int global_inventory_count = 0;
inline lv_obj_t *inv_list = NULL;
inline lv_obj_t *inv_search_ta = NULL;

static std::string g_wifi_text_full = LV_SYMBOL_WIFI " WiFi: Off";

inline lv_obj_t *status_title_label;     // title text on top status bar
inline lv_obj_t *top_logout_btn;         // logout button on top bar
inline lv_obj_t *top_setup_btn;          // setup button on top bar
inline lv_obj_t *label_wifi_status;      // WiFi / BLE status indicator on top bar
inline lv_obj_t *label_battery_status;   // battery percentage indicator on status bar
inline lv_obj_t *label_last_scan_sku;
inline lv_obj_t *label_last_scan_flag;
inline lv_obj_t *label_scan_progress;
inline lv_obj_t *bar_scan_progress;
inline lv_obj_t *label_scan_count_home;
inline lv_obj_t *arc_scan_progress;
inline lv_obj_t *label_conn_wifi_detail; // IP / status shown in Conn screen
inline lv_obj_t *label_conn_ble_detail;  // BLE status shown in Conn screen
inline lv_obj_t *label_conn_ble_btn;     // BLE toggle button label
inline lv_obj_t *bar_ota_progress = NULL;
inline lv_obj_t *label_ota_progress = NULL;
inline lv_obj_t *label_ota_status = NULL;
inline lv_obj_t *light_switch_ptr = NULL;   // Settings: Lighting control switch
inline lv_obj_t *collim_switch_ptr = NULL;  // Settings: Collimation control switch
inline lv_obj_t *hw_taskbar_ptr = NULL;     // Floating hardware controls taskbar

// Pointers for dynamic Setup screen layout
inline lv_obj_t *conn_nav_rail_ptr = NULL;
inline lv_obj_t *conn_content_ptr = NULL;
inline lv_obj_t *conn_logout_lbl_ptr = NULL;
inline lv_obj_t *conn_bottom_row_ptr = NULL;
inline lv_obj_t *wcard_ref = NULL;
inline lv_obj_t *bcard_ref = NULL;
inline lv_obj_t *rcard_ref = NULL;
inline lv_obj_t *top_back_btn = NULL;
inline uint32_t scan_counter = 0;
inline uint32_t scan_target  = 120;

// Textarea handles (forward declared for global logout action)
inline lv_obj_t *ta_login_user = NULL;
inline lv_obj_t *ta_login_pass = NULL;
inline lv_obj_t *label_login_qr_status = NULL;

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
extern void devicePowerOff();
inline void uiSetWifiStatus(const std::string &text);
inline void uiSetBleConnected(bool connected);
inline lv_obj_t *floating_ptt_btn = NULL;
inline lv_obj_t *global_kb = NULL;
inline lv_obj_t *login_left_panel = NULL;
inline lv_obj_t *login_right_panel = NULL;
inline lv_obj_t *login_btn_submit = NULL;
inline lv_obj_t *login_eye_btn = NULL;

inline void publishDeviceStatus(bool is_online, const char *user) {
  network_publish_device_status(is_online, user);
}

inline void ui_update_tasks_from_json(const char *json_str) {
  if (!json_str) return;
  cJSON *root = cJSON_Parse(json_str);
  if (!root) return;

  if (cJSON_IsArray(root)) {
    int size = cJSON_GetArraySize(root);
    current_task_count = 0;

    for (int i = 0; i < size && i < MAX_TASKS; i++) {
      cJSON *item = cJSON_GetArrayItem(root, i);
      if (!cJSON_IsObject(item)) continue;

      TaskDef &t = current_tasks[current_task_count];
      memset(&t, 0, sizeof(TaskDef));

      cJSON *id = cJSON_GetObjectItem(item, "id");
      cJSON *name = cJSON_GetObjectItem(item, "name");
      cJSON *assignee = cJSON_GetObjectItem(item, "assignee");
      cJSON *prio = cJSON_GetObjectItem(item, "prio");
      cJSON *status = cJSON_GetObjectItem(item, "status");

      if (cJSON_IsString(id) && id->valuestring) strncpy(t.id, id->valuestring, sizeof(t.id) - 1);
      if (cJSON_IsString(name) && name->valuestring) strncpy(t.name, name->valuestring, sizeof(t.name) - 1);
      if (cJSON_IsString(assignee) && assignee->valuestring) strncpy(t.assignee, assignee->valuestring, sizeof(t.assignee) - 1);
      if (cJSON_IsString(prio) && prio->valuestring) strncpy(t.prio, prio->valuestring, sizeof(t.prio) - 1);
      if (cJSON_IsString(status) && status->valuestring) strncpy(t.status, status->valuestring, sizeof(t.status) - 1);

      if (strstr(t.prio, "High") || strstr(t.prio, "high") || strstr(t.prio, "HIGH")) {
        t.prio_color = COLOR_DANGER;
      } else if (strstr(t.prio, "Med") || strstr(t.prio, "med") || strstr(t.prio, "MED")) {
        t.prio_color = COLOR_WARNING;
      } else {
        t.prio_color = COLOR_CYAN;
      }

      cJSON *itemsArr = cJSON_GetObjectItem(item, "items");
      t.item_count = 0;
      if (cJSON_IsArray(itemsArr)) {
        int item_num = cJSON_GetArraySize(itemsArr);
        for (int j = 0; j < item_num && j < MAX_ITEMS_PER_TASK; j++) {
          cJSON *subItem = cJSON_GetArrayItem(itemsArr, j);
          if (!cJSON_IsObject(subItem)) continue;

          TaskItem &ti = t.items[t.item_count];
          memset(&ti, 0, sizeof(TaskItem));

          cJSON *sku = cJSON_GetObjectItem(subItem, "sku");
          cJSON *iname = cJSON_GetObjectItem(subItem, "name");
          cJSON *tqty = cJSON_GetObjectItem(subItem, "target_qty");
          cJSON *pqty = cJSON_GetObjectItem(subItem, "picked_qty");

          if (cJSON_IsString(sku) && sku->valuestring) strncpy(ti.sku, sku->valuestring, sizeof(ti.sku) - 1);
          if (cJSON_IsString(iname) && iname->valuestring) strncpy(ti.name, iname->valuestring, sizeof(ti.name) - 1);
          if (cJSON_IsNumber(tqty)) ti.target_qty = tqty->valueint;
          if (cJSON_IsNumber(pqty)) ti.picked_qty = pqty->valueint;

          t.item_count++;
        }
      }
      current_task_count++;
    }
    
    // Sort tasks by priority (High > Medium > Low) using robust strstr
    for (int i = 0; i < current_task_count - 1; i++) {
      for (int j = 0; j < current_task_count - i - 1; j++) {
        int score1 = 0, score2 = 0;
        if (strstr(current_tasks[j].prio, "High") || strstr(current_tasks[j].prio, "high") || strstr(current_tasks[j].prio, "HIGH")) score1 = 3;
        else if (strstr(current_tasks[j].prio, "Med") || strstr(current_tasks[j].prio, "med") || strstr(current_tasks[j].prio, "MED")) score1 = 2;
        else if (strstr(current_tasks[j].prio, "Low") || strstr(current_tasks[j].prio, "low") || strstr(current_tasks[j].prio, "LOW")) score1 = 1;
        
        if (strstr(current_tasks[j+1].prio, "High") || strstr(current_tasks[j+1].prio, "high") || strstr(current_tasks[j+1].prio, "HIGH")) score2 = 3;
        else if (strstr(current_tasks[j+1].prio, "Med") || strstr(current_tasks[j+1].prio, "med") || strstr(current_tasks[j+1].prio, "MED")) score2 = 2;
        else if (strstr(current_tasks[j+1].prio, "Low") || strstr(current_tasks[j+1].prio, "low") || strstr(current_tasks[j+1].prio, "LOW")) score2 = 1;
        
        if (score1 < score2) {
          TaskDef temp = current_tasks[j];
          current_tasks[j] = current_tasks[j+1];
          current_tasks[j+1] = temp;
        }
      }
    }
  }
  cJSON_Delete(root);

  vTaskDelay(pdMS_TO_TICKS(1)); // Yield CPU0 to prevent Task Watchdog during burst MQTT updates
  if (lvgl_port_lock(-1)) {
    update_tasks_ui();
    if (active_task != NULL) {
      update_task_detail_ui();
    }
    lvgl_port_unlock();
  }
}

inline void ui_update_inventory_from_json(const char *json_str) {
  if (!json_str) return;
  cJSON *root = cJSON_Parse(json_str);
  if (!root) return;

  if (cJSON_IsArray(root)) {
    int size = cJSON_GetArraySize(root);
    global_inventory_count = 0;
    for (int i = 0; i < size && i < MAX_INVENTORY; i++) {
      cJSON *item = cJSON_GetArrayItem(root, i);
      if (!cJSON_IsObject(item)) continue;

      InvItem &inv = global_inventory[global_inventory_count];
      memset(&inv, 0, sizeof(InvItem));

      cJSON *name = cJSON_GetObjectItem(item, "name");
      cJSON *sku = cJSON_GetObjectItem(item, "sku");
      cJSON *qty = cJSON_GetObjectItem(item, "qty");

      if (cJSON_IsString(name) && name->valuestring) strncpy(inv.name, name->valuestring, sizeof(inv.name) - 1);
      if (cJSON_IsString(sku) && sku->valuestring) strncpy(inv.sku, sku->valuestring, sizeof(inv.sku) - 1);
      if (cJSON_IsNumber(qty)) {
        snprintf(inv.qty, sizeof(inv.qty), "%d", qty->valueint);
      } else if (cJSON_IsString(qty) && qty->valuestring) {
        strncpy(inv.qty, qty->valuestring, sizeof(inv.qty) - 1);
      }
      global_inventory_count++;
    }
  }
  cJSON_Delete(root);

  vTaskDelay(pdMS_TO_TICKS(1)); // Yield CPU0 to prevent Task Watchdog during burst MQTT updates
  if (lvgl_port_lock(-1)) {
    update_inventory_ui();
    lvgl_port_unlock();
  }
}

inline void ui_update_users_from_json(const char *json_str) {
  if (!json_str) return;
  cJSON *root = cJSON_Parse(json_str);
  if (!root) return;

  if (cJSON_IsArray(root)) {
    int size = cJSON_GetArraySize(root);
    global_user_count = 0;
    for (int i = 0; i < size && i < MAX_USERS; i++) {
      cJSON *item = cJSON_GetArrayItem(root, i);
      if (!cJSON_IsObject(item)) continue;

      UserDef &u = global_users[global_user_count];
      memset(&u, 0, sizeof(UserDef));

      cJSON *id = cJSON_GetObjectItem(item, "id");
      cJSON *name = cJSON_GetObjectItem(item, "name");
      cJSON *role = cJSON_GetObjectItem(item, "role");
      cJSON *pin = cJSON_GetObjectItem(item, "pin");

      if (cJSON_IsString(id) && id->valuestring) snprintf(u.id, sizeof(u.id), "%s", id->valuestring);
      if (cJSON_IsString(name) && name->valuestring) snprintf(u.name, sizeof(u.name), "%s", name->valuestring);
      if (cJSON_IsString(role) && role->valuestring) snprintf(u.role, sizeof(u.role), "%s", role->valuestring);
      if (cJSON_IsString(pin) && pin->valuestring) snprintf(u.pin, sizeof(u.pin), "%s", pin->valuestring);
      else if (cJSON_IsNumber(pin)) snprintf(u.pin, sizeof(u.pin), "%d", (int)pin->valueint);

      global_user_count++;
    }
  }
  cJSON_Delete(root);
  vTaskDelay(pdMS_TO_TICKS(1)); // Yield CPU0 to prevent Task Watchdog during burst MQTT updates
}

static void _logout_action_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    is_logged_in = false;
    publishDeviceStatus(false, "No Login");
    memset(logged_in_user, 0, sizeof(logged_in_user));
    memset(logged_in_user_id, 0, sizeof(logged_in_user_id));
    memset(logged_in_user_role, 0, sizeof(logged_in_user_role));
    current_task_count = 0;
    memset(current_tasks, 0, sizeof(current_tasks));
    active_task = NULL;
    update_tasks_ui();
    if (label_home_user) lv_label_set_text(label_home_user, "Not Logged In");
    if (ta_login_user) lv_textarea_set_text(ta_login_user, "");
    if (ta_login_pass) lv_textarea_set_text(ta_login_pass, "");
    // Reset QR status label back to idle prompt
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_BARS "  Scan QR badge to login");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_CYAN, 0);
    }
    if (scr_login) _load_scr_direct(scr_login, "");
  }
}

static void _power_off_action_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    devicePowerOff();
  }
}

extern volatile bool is_ptt_pressed;

// Toggle-based PTT: one tap activates mic streaming, another tap stops it.
static bool ptt_active_state = false;

static void _ptt_action_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED) return;

  lv_obj_t *btn = lv_event_get_target(e);
  lv_obj_t *lbl = lv_obj_get_child(btn, 0);

  ptt_active_state = !ptt_active_state;
  bool is_portrait = (lv_disp_get_hor_res(NULL) < 400);

  if (ptt_active_state) {
    // Active: mic streaming ON
    is_ptt_pressed = true;
    network_set_ptt(true);
    lv_obj_set_style_bg_color(btn, COLOR_DANGER, 0);     // Red = active
    lv_obj_set_style_shadow_color(btn, COLOR_DANGER, 0); // Red glow
    lv_label_set_text(lbl, is_portrait ? (LV_SYMBOL_AUDIO " LIVE") : (LV_SYMBOL_AUDIO "  TALKING"));
  } else {
    // Inactive: mic streaming OFF
    is_ptt_pressed = false;
    network_set_ptt(false);
    lv_obj_set_style_bg_color(btn, COLOR_SAFFRON, 0);    // Saffron = idle
    lv_obj_set_style_shadow_color(btn, COLOR_SAFFRON, 0);
    lv_label_set_text(lbl, is_portrait ? (LV_SYMBOL_AUDIO " TALK") : (LV_SYMBOL_AUDIO "  TAP TO TALK"));
  }
}

// ============================================================================
// Shared chrome: status bar (with tricolor stripe) + left nav rail
// ============================================================================
static void create_global_status_bar() {
  if (status_title_label) return;

  lv_obj_clear_flag(lv_layer_top(), LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(lv_layer_top(), LV_OBJ_FLAG_CLICKABLE);

  global_status_bar_obj = lv_obj_create(lv_layer_top());
  lv_obj_t *bar = global_status_bar_obj;
  lv_obj_set_size(bar, LV_PCT(100), 32);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x050A1A), 0);
  lv_obj_set_style_border_color(bar, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);

  status_title_label = lv_label_create(bar);
  lv_label_set_text(status_title_label, "Home");
  lv_obj_set_style_text_color(status_title_label, COLOR_CYAN, 0);
  lv_obj_set_style_text_font(status_title_label, &lv_font_montserrat_14, 0);
  lv_obj_align(status_title_label, LV_ALIGN_LEFT_MID, 6, 0);

  // Top Back Button (Only for pre-login setup)
  top_back_btn = lv_btn_create(bar);
  lv_obj_set_size(top_back_btn, 28, 24);
  lv_obj_align(top_back_btn, LV_ALIGN_LEFT_MID, 4, 0);
  lv_obj_set_style_bg_color(top_back_btn, COLOR_NAVY_BLUE, 0);
  lv_obj_set_style_radius(top_back_btn, 4, 0);
  lv_obj_set_style_pad_all(top_back_btn, 0, 0);
  lv_obj_t *tb_lbl = lv_label_create(top_back_btn);
  lv_label_set_text(tb_lbl, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(tb_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(tb_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(tb_lbl);
  lv_obj_add_flag(top_back_btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(top_back_btn, [](lv_event_t *e) {
    _load_scr_direct(scr_login, "");
  }, LV_EVENT_CLICKED, NULL);

  // Instant top logout button
  top_logout_btn = lv_btn_create(bar);
  lv_obj_set_size(top_logout_btn, 80, 24);
  lv_obj_align(top_logout_btn, LV_ALIGN_CENTER, -40, 0);
  lv_obj_set_style_bg_color(top_logout_btn, COLOR_DANGER, 0);
  lv_obj_set_style_radius(top_logout_btn, 4, 0);
  lv_obj_set_style_pad_all(top_logout_btn, 0, 0);
  lv_obj_t *tl_lbl = lv_label_create(top_logout_btn);
  lv_label_set_text(tl_lbl, LV_SYMBOL_POWER " LOGOUT");
  lv_obj_set_style_text_color(tl_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(tl_lbl, &lv_font_montserrat_12, 0);
  lv_obj_center(tl_lbl);
  lv_obj_add_event_cb(top_logout_btn, _logout_action_cb, LV_EVENT_CLICKED, NULL);

  // Top Setup Button (Wi-Fi/BLE)
  top_setup_btn = lv_btn_create(bar);
  lv_obj_set_size(top_setup_btn, 80, 24);
  lv_obj_align(top_setup_btn, LV_ALIGN_CENTER, -40, 0);
  lv_obj_set_style_bg_color(top_setup_btn, COLOR_NAVY_BLUE, 0);
  lv_obj_set_style_radius(top_setup_btn, 4, 0);
  lv_obj_set_style_pad_all(top_setup_btn, 0, 0);
  lv_obj_t *ts_lbl = lv_label_create(top_setup_btn);
  lv_label_set_text(ts_lbl, LV_SYMBOL_WIFI " SETUP");
  lv_obj_set_style_text_color(ts_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(ts_lbl, &lv_font_montserrat_12, 0);
  lv_obj_center(ts_lbl);
  lv_obj_add_event_cb(top_setup_btn, [](lv_event_t *e) {
    _load_scr_direct(scr_conn, is_logged_in ? "Network" : "Setup");
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
  lv_obj_align(label_wifi_status, LV_ALIGN_RIGHT_MID, -150, 0);

  // Floating Push-To-Talk Button in Bottom-Right Corner
  floating_ptt_btn = lv_btn_create(lv_layer_top());
  lv_obj_set_size(floating_ptt_btn, 130, 42);
  lv_obj_align(floating_ptt_btn, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
  lv_obj_set_style_bg_color(floating_ptt_btn, COLOR_SAFFRON, 0);
  lv_obj_set_style_radius(floating_ptt_btn, 21, 0); // Modern floating pill button
  lv_obj_set_style_shadow_width(floating_ptt_btn, 10, 0);
  lv_obj_set_style_shadow_color(floating_ptt_btn, COLOR_SAFFRON, 0);
  lv_obj_set_style_pad_all(floating_ptt_btn, 0, 0);

  lv_obj_t *fp_lbl = lv_label_create(floating_ptt_btn);
  lv_label_set_text(fp_lbl, LV_SYMBOL_AUDIO "  TAP TO TALK");
  lv_obj_set_style_text_color(fp_lbl, lv_color_hex(0x040812), 0); // High contrast dark text
  lv_obj_set_style_text_font(fp_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(fp_lbl);
  lv_obj_clear_flag(fp_lbl, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_add_event_cb(floating_ptt_btn, _ptt_action_cb, LV_EVENT_CLICKED, NULL);
  if (!is_logged_in) lv_obj_add_flag(floating_ptt_btn, LV_OBJ_FLAG_HIDDEN);

  // Cyan accent stripe
  global_status_stripe_obj = lv_obj_create(lv_layer_top());
  lv_obj_t *stripe = global_status_stripe_obj;
  lv_obj_set_size(stripe, LV_PCT(100), 2);
  lv_obj_set_pos(stripe, 0, 32);
  lv_obj_set_style_border_width(stripe, 0, 0);
  lv_obj_set_style_radius(stripe, 0, 0);
  lv_obj_set_style_bg_color(stripe, COLOR_CYAN, 0);
  lv_obj_set_style_bg_grad_color(stripe, COLOR_SAFFRON, 0);
  lv_obj_set_style_bg_grad_dir(stripe, LV_GRAD_DIR_HOR, 0);
  lv_obj_clear_flag(stripe, LV_OBJ_FLAG_CLICKABLE);


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
  lv_obj_set_size(rail, 64, lv_disp_get_ver_res(NULL) - 32);
  lv_obj_set_pos(rail, 0, 32);
  lv_obj_set_style_bg_color(rail, lv_color_hex(0x050A1A), 0);
  lv_obj_set_style_border_color(rail, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(rail, 0, 0);
  lv_obj_set_style_border_side(rail, LV_BORDER_SIDE_RIGHT, 0);
  lv_obj_set_style_border_width(rail, 1, 0);
  lv_obj_set_style_radius(rail, 0, 0);
  lv_obj_set_style_pad_all(rail, 0, 0);
  lv_obj_clear_flag(rail, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(rail, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_clear_flag(rail, LV_OBJ_FLAG_SCROLL_MOMENTUM);

  const int NAV_H  = 44;

  const char *labels[5] = {"Home", "Tasks", "Inv", "Setup", "Settings"};
  const char *icons[5]  = {LV_SYMBOL_HOME, LV_SYMBOL_LIST, LV_SYMBOL_DIRECTORY, LV_SYMBOL_WIFI, LV_SYMBOL_SETTINGS};
  lv_event_cb_t cbs[5]  = {nav_home_cb, nav_tasks_cb, nav_inventory_cb, nav_conn_cb, nav_settings_cb};

  for (int i = 0; i < 5; i++) {
    lv_obj_t *btn = lv_btn_create(rail);
    lv_obj_set_size(btn, 64, NAV_H);
    lv_obj_set_pos(btn, 0, i * NAV_H);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    if (i == active_index) {
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x0D2545), 0);
      lv_obj_set_style_border_color(btn, COLOR_CYAN, 0);
      lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_LEFT, 0);
      lv_obj_set_style_border_width(btn, 3, 0);
    } else {
      lv_obj_set_style_bg_color(btn, lv_color_hex(0x050A1A), 0);
    }
    lv_obj_add_event_cb(btn, cbs[i], LV_EVENT_CLICKED, NULL);

    lv_obj_t *ico = lv_label_create(btn);
    lv_label_set_text(ico, icons[i]);
    lv_obj_set_style_text_color(ico, (i == active_index) ? COLOR_CYAN : COLOR_MUTE, 0);
    lv_obj_set_style_text_font(ico, &lv_font_montserrat_16, 0);
    lv_obj_align(ico, LV_ALIGN_CENTER, 0, 0);
  }

  return rail;
}

static void _global_screen_resize_cb(lv_event_t *e) {
    lv_obj_t *scr = lv_event_get_target(e);
    lv_coord_t w = lv_disp_get_hor_res(NULL);
    lv_coord_t h = lv_disp_get_ver_res(NULL);

    if (top_logout_btn && lv_obj_get_child_cnt(top_logout_btn) > 0) {
        lv_obj_t *lbl = lv_obj_get_child(top_logout_btn, 0);
        if (w < 400) {
            lv_obj_set_size(top_logout_btn, 32, 24);
            lv_obj_align(top_logout_btn, LV_ALIGN_CENTER, -30, 0);
            lv_label_set_text(lbl, LV_SYMBOL_POWER);
        } else {
            lv_obj_set_size(top_logout_btn, 80, 24);
            lv_obj_align(top_logout_btn, LV_ALIGN_CENTER, -40, 0);
            lv_label_set_text(lbl, LV_SYMBOL_POWER " LOGOUT");
        }
    }
    if (top_setup_btn && lv_obj_get_child_cnt(top_setup_btn) > 0) {
        lv_obj_t *lbl = lv_obj_get_child(top_setup_btn, 0);
        if (w < 400) {
            lv_obj_set_size(top_setup_btn, 32, 24);
            lv_obj_align(top_setup_btn, LV_ALIGN_CENTER, -30, 0);
            lv_label_set_text(lbl, LV_SYMBOL_WIFI);
        } else {
            lv_obj_set_size(top_setup_btn, 80, 24);
            lv_obj_align(top_setup_btn, LV_ALIGN_CENTER, -40, 0);
            lv_label_set_text(lbl, LV_SYMBOL_WIFI " SETUP");
        }
    }
    if (label_wifi_status) {
        lv_obj_align(label_wifi_status, LV_ALIGN_RIGHT_MID, w < 400 ? -100 : -150, 0);
    }
    if (floating_ptt_btn && lv_obj_get_child_cnt(floating_ptt_btn) > 0) {
        lv_obj_t *lbl = lv_obj_get_child(floating_ptt_btn, 0);
        if (w < 400) {
            lv_obj_set_size(floating_ptt_btn, 74, 42);
            lv_obj_set_style_radius(floating_ptt_btn, 12, 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
            lv_label_set_text(lbl, ptt_active_state ? (LV_SYMBOL_AUDIO " LIVE") : (LV_SYMBOL_AUDIO " TALK"));
        } else {
            lv_obj_set_size(floating_ptt_btn, 120, 42);
            lv_obj_set_style_radius(floating_ptt_btn, 21, 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_label_set_text(lbl, ptt_active_state ? (LV_SYMBOL_AUDIO "  TALKING") : (LV_SYMBOL_AUDIO "  TAP TO TALK"));
        }
    }

    for(uint32_t i=0; i<lv_obj_get_child_cnt(scr); i++) {
        lv_obj_t *child = lv_obj_get_child(scr, i);
        if (lv_obj_get_y(child) == 0 && lv_obj_get_height(child) <= 64) {
            lv_obj_set_width(child, w); 
        } 
        else if (lv_obj_get_x(child) == 0 && lv_obj_get_width(child) == 64 && lv_obj_get_y(child) >= 32) {
            lv_obj_set_height(child, h - lv_obj_get_y(child)); 
        } 
        else if (lv_obj_get_x(child) == 64 && lv_obj_get_y(child) == 32) {
            lv_obj_set_size(child, w - 64, h - 32); 
            lv_obj_set_style_pad_bottom(child, 72, 0);
        }
        else if (lv_obj_get_x(child) == 0 && lv_obj_get_y(child) == 32) {
            lv_obj_set_size(child, w, h - 32);
            lv_obj_set_style_pad_bottom(child, 120, 0);
        }
    }
    
    // Update Wi-Fi label format based on new width
    uiSetWifiStatus(g_wifi_text_full);

    // Login screen layout resizing
    if (scr == scr_login && login_left_panel && login_right_panel) {
        if (w < 400) {
            // Portrait mode: stack vertically
            lv_obj_set_size(login_right_panel, 300, 210);
            lv_obj_align(login_right_panel, LV_ALIGN_TOP_MID, 0, 48);

            lv_obj_set_size(login_left_panel, 300, 210);
            lv_obj_align(login_left_panel, LV_ALIGN_TOP_MID, 0, 260); // 48 + 210 + small gap
            
            // Adjust inner fields to fill width in portrait
            if (ta_login_user) {
                lv_obj_set_width(ta_login_user, 280);
            }
            if (ta_login_pass) {
                lv_obj_set_width(ta_login_pass, 230);
            }
            if (login_eye_btn) {
                lv_obj_align(login_eye_btn, LV_ALIGN_TOP_LEFT, 240, 72);
            }
            if (login_btn_submit) {
                lv_obj_set_width(login_btn_submit, 280);
            }
        } else {
            // Landscape mode: side by side
            lv_obj_set_size(login_left_panel, 226, 210);
            lv_obj_align(login_left_panel, LV_ALIGN_TOP_LEFT, 8, 48);
            
            lv_obj_set_size(login_right_panel, 230, 210);
            lv_obj_align(login_right_panel, LV_ALIGN_TOP_RIGHT, -8, 48);
            
            if (ta_login_user) {
                lv_obj_set_width(ta_login_user, 208);
            }
            if (ta_login_pass) {
                lv_obj_set_width(ta_login_pass, 166);
            }
            if (login_eye_btn) {
                lv_obj_align(login_eye_btn, LV_ALIGN_TOP_LEFT, 170, 72);
            }
            if (login_btn_submit) {
                lv_obj_set_width(login_btn_submit, 208);
            }
        }
    }

    // For the home screen: reposition arc and buttons for portrait/landscape
    if (scr == scr_home && home_content_ptr && home_arc_ptr && home_btn_container_ptr) {
        if (w < 400) {
            // Portrait: arc near top, buttons stacked directly below
            lv_obj_align(home_arc_ptr, LV_ALIGN_TOP_MID, 0, 10);
            lv_obj_align(home_btn_container_ptr, LV_ALIGN_TOP_MID, 0, 155);
            lv_obj_set_flex_flow(home_btn_container_ptr, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(home_btn_container_ptr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            for (uint32_t k = 0; k < lv_obj_get_child_cnt(home_btn_container_ptr); k++) {
                lv_obj_t *btn = lv_obj_get_child(home_btn_container_ptr, k);
                lv_obj_set_size(btn, 180, 48); // nice wide buttons in portrait
            }
        } else {
            // Landscape: original positions side-by-side
            lv_obj_align(home_arc_ptr, LV_ALIGN_TOP_MID, 0, 25);
            lv_obj_align(home_btn_container_ptr, LV_ALIGN_TOP_MID, 0, 165);
            lv_obj_set_flex_flow(home_btn_container_ptr, LV_FLEX_FLOW_ROW_WRAP);
            lv_obj_set_flex_align(home_btn_container_ptr, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            for (uint32_t k = 0; k < lv_obj_get_child_cnt(home_btn_container_ptr); k++) {
                lv_obj_t *btn = lv_obj_get_child(home_btn_container_ptr, k);
                lv_obj_set_size(btn, 130, 48);
            }
        }
        lv_obj_invalidate(home_content_ptr);
    }

    // For the settings screen: refresh flex layout and fix portrait clipping
    if (scr == scr_settings && settings_content_ptr) {
        lv_coord_t content_w = w - 64;
        lv_coord_t row_h = 56;
        lv_coord_t content_h = h - 32 - row_h;
        // Set both dimensions so the flex container knows its bounds
        lv_obj_set_size(settings_content_ptr, content_w, content_h);
        lv_obj_set_pos(settings_content_ptr, 64, 32);

        if (w < 400) {
            // Portrait: allow scrolling so all items are reachable
            lv_obj_set_scroll_dir(settings_content_ptr, LV_DIR_VER);
            lv_obj_set_flex_align(settings_content_ptr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_row(settings_content_ptr, 6, 0);
            lv_obj_set_style_pad_bottom(settings_content_ptr, 8, 0);
        } else {
            lv_obj_set_scroll_dir(settings_content_ptr, LV_DIR_VER);
            lv_obj_set_flex_align(settings_content_ptr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
            lv_obj_set_style_pad_row(settings_content_ptr, 4, 0);
            lv_obj_set_style_pad_bottom(settings_content_ptr, 8, 0);
        }

        // Force every child row to fill the full width
        for (uint32_t k = 0; k < lv_obj_get_child_cnt(settings_content_ptr); k++) {
            lv_obj_t *row = lv_obj_get_child(settings_content_ptr, k);
            lv_obj_set_width(row, LV_PCT(100));
        }

        if (settings_btn_row_ptr) {
            lv_obj_set_size(settings_btn_row_ptr, content_w, row_h);
            lv_obj_align(settings_btn_row_ptr, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
            if (lv_obj_get_child_cnt(settings_btn_row_ptr) >= 2) {
                lv_obj_t *logout_btn = lv_obj_get_child(settings_btn_row_ptr, 0);
                lv_obj_t *power_btn  = lv_obj_get_child(settings_btn_row_ptr, 1);
                lv_obj_add_flag(logout_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
                lv_obj_add_flag(power_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);

                if (w < 400) {
                    // Portrait mode: 3 buttons side-by-side (Logout 74px, Power 74px, PTT 74px)
                    lv_obj_set_size(logout_btn, 74, 42);
                    lv_obj_set_size(power_btn, 74, 42);
                    lv_obj_set_style_radius(logout_btn, 12, 0);
                    lv_obj_set_style_radius(power_btn, 12, 0);
                    lv_obj_align(logout_btn, LV_ALIGN_LEFT_MID, 6, 0);
                    lv_obj_align(power_btn, LV_ALIGN_LEFT_MID, 88, 0);
                    if (lv_obj_get_child_cnt(logout_btn) > 0) {
                        lv_obj_t *btn_lbl = lv_obj_get_child(logout_btn, 0);
                        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0);
                        lv_label_set_text(btn_lbl, LV_SYMBOL_POWER " OUT");
                    }
                    if (lv_obj_get_child_cnt(power_btn) > 0) {
                        lv_obj_t *btn_lbl = lv_obj_get_child(power_btn, 0);
                        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0);
                        lv_label_set_text(btn_lbl, LV_SYMBOL_POWER " OFF");
                    }
                } else {
                    // Landscape mode: 120px buttons
                    lv_obj_set_size(logout_btn, 120, 42);
                    lv_obj_set_size(power_btn, 120, 42);
                    lv_obj_set_style_radius(logout_btn, 12, 0);
                    lv_obj_set_style_radius(power_btn, 12, 0);
                    lv_obj_align(logout_btn, LV_ALIGN_LEFT_MID, 12, 0);
                    lv_obj_align(power_btn, LV_ALIGN_LEFT_MID, 144, 0);
                    if (lv_obj_get_child_cnt(logout_btn) > 0) {
                        lv_obj_t *btn_lbl = lv_obj_get_child(logout_btn, 0);
                        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
                        lv_label_set_text(btn_lbl, LV_SYMBOL_POWER " LOGOUT");
                    }
                    if (lv_obj_get_child_cnt(power_btn) > 0) {
                        lv_obj_t *btn_lbl = lv_obj_get_child(power_btn, 0);
                        lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
                        lv_label_set_text(btn_lbl, LV_SYMBOL_POWER " POWER");
                    }
                }
            }
        }
        lv_obj_invalidate(settings_content_ptr);
    }

    // For the tasks screen: refresh flex layout of task cards to match new width
    if (scr == scr_tasks && tasks_content_ptr) {
        lv_obj_set_width(tasks_content_ptr, w - 64);
        lv_obj_invalidate(tasks_content_ptr);
        // Force each task card to fill full width of the resized container
        for (uint32_t k = 0; k < lv_obj_get_child_cnt(tasks_content_ptr); k++) {
            lv_obj_t *card = lv_obj_get_child(tasks_content_ptr, k);
            lv_obj_set_width(card, LV_PCT(100));
        }
    }

    if (scr == scr_conn && wcard_ref && bcard_ref && rcard_ref && conn_bottom_row_ptr) {
        if (w < 400) {
            lv_obj_set_size(conn_bottom_row_ptr, w - 64, 44);
            lv_obj_align(wcard_ref, LV_ALIGN_TOP_MID, 0, 0);
            lv_obj_align(bcard_ref, LV_ALIGN_TOP_MID, 0, 225);
            lv_obj_align(rcard_ref, LV_ALIGN_TOP_MID, 0, 330);
            lv_obj_align(conn_bottom_row_ptr, LV_ALIGN_BOTTOM_RIGHT, 0, -12);

            if (lv_obj_get_child_cnt(conn_bottom_row_ptr) >= 2) {
                lv_obj_t *logout_btn = lv_obj_get_child(conn_bottom_row_ptr, 0);
                lv_obj_t *power_btn = lv_obj_get_child(conn_bottom_row_ptr, 1);
                lv_obj_add_flag(logout_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
                lv_obj_add_flag(power_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
                
                // All buttons match size of tap to talk button (74 x 42)
                lv_obj_set_size(logout_btn, 74, 42);
                lv_obj_set_size(power_btn, 74, 42);
                lv_obj_set_style_radius(logout_btn, 12, 0);
                lv_obj_set_style_radius(power_btn, 12, 0);
                
                // Align side by side with tap to talk without overlapping
                // Left margin: 6, logout: 74 (6..80), gap: 8, power: 74 (88..162), gap: 8, ptt: 74 (170..244)
                lv_obj_align(logout_btn, LV_ALIGN_LEFT_MID, 6, 0);
                lv_obj_align(power_btn, LV_ALIGN_LEFT_MID, 88, 0);
                
                if (lv_obj_get_child_cnt(logout_btn) > 0) {
                    lv_obj_t *btn_lbl = lv_obj_get_child(logout_btn, 0);
                    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0);
                    lv_label_set_text(btn_lbl, LV_SYMBOL_POWER " OUT");
                }
                if (lv_obj_get_child_cnt(power_btn) > 0) {
                    lv_obj_t *btn_lbl = lv_obj_get_child(power_btn, 0);
                    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_12, 0);
                    lv_label_set_text(btn_lbl, LV_SYMBOL_POWER " OFF");
                }
            }
        } else {
            lv_obj_set_size(conn_bottom_row_ptr, w - 64, 44);
            lv_obj_align(wcard_ref, LV_ALIGN_TOP_LEFT, 0, 0);
            lv_obj_align(bcard_ref, LV_ALIGN_TOP_RIGHT, 0, 0);
            lv_obj_align(rcard_ref, LV_ALIGN_TOP_RIGHT, 0, 100);
            lv_obj_align(conn_bottom_row_ptr, LV_ALIGN_BOTTOM_RIGHT, 0, -12);

            if (lv_obj_get_child_cnt(conn_bottom_row_ptr) >= 2) {
                lv_obj_t *logout_btn = lv_obj_get_child(conn_bottom_row_ptr, 0);
                lv_obj_t *power_btn = lv_obj_get_child(conn_bottom_row_ptr, 1);
                lv_obj_add_flag(logout_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
                lv_obj_add_flag(power_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
                
                // Same size as tap to talk button in landscape (120 x 42)
                lv_obj_set_size(logout_btn, 120, 42);
                lv_obj_set_size(power_btn, 120, 42);
                lv_obj_set_style_radius(logout_btn, 12, 0);
                lv_obj_set_style_radius(power_btn, 12, 0);

                lv_obj_align(logout_btn, LV_ALIGN_LEFT_MID, 12, 0);
                lv_obj_align(power_btn, LV_ALIGN_LEFT_MID, 144, 0);

                if (lv_obj_get_child_cnt(logout_btn) > 0) {
                    lv_obj_t *btn_lbl = lv_obj_get_child(logout_btn, 0);
                    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
                    lv_label_set_text(btn_lbl, LV_SYMBOL_POWER " LOGOUT");
                }
                if (lv_obj_get_child_cnt(power_btn) > 0) {
                    lv_obj_t *btn_lbl = lv_obj_get_child(power_btn, 0);
                    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, 0);
                    lv_label_set_text(btn_lbl, LV_SYMBOL_POWER " POWER");
                }
            }
        }
    }
    
    lv_disp_rot_t cur_rot = lv_disp_get_rotation(NULL);
    uint8_t rot_num = (cur_rot == LV_DISP_ROT_NONE) ? 0 : ((cur_rot == LV_DISP_ROT_270) ? 3 : 1);
    update_orient_switches_state(rot_num);
}

// content area helper: full width minus nav rail, below status bar

static lv_obj_t* build_content_area(lv_obj_t *parent) {
  lv_obj_t *content = lv_obj_create(parent);
  lv_obj_set_size(content, lv_disp_get_hor_res(NULL) - 64, lv_disp_get_ver_res(NULL) - 32);
  lv_obj_set_pos(content, 64, 32);
  lv_obj_set_style_bg_color(content, COLOR_BG, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_radius(content, 0, 0);
  lv_obj_set_style_pad_all(content, 8, 0);
  lv_obj_set_style_pad_bottom(content, lv_disp_get_hor_res(NULL) < 400 ? 64 : 8, 0);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_add_event_cb(parent, _global_screen_resize_cb, LV_EVENT_SIZE_CHANGED, NULL);
  return content;
}

// ============================================================================
// A. HOME
// ============================================================================
static void build_home_screen() {
  scr_home = lv_obj_create(NULL);
  lv_obj_clear_flag(scr_home, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr_home, COLOR_BG, 0);
  build_status_bar(scr_home, "ScanPro X1");
  build_nav_rail(scr_home, 0);
  home_content_ptr = build_content_area(scr_home);
  lv_obj_t *content = home_content_ptr;

  lv_obj_t *hdr = lv_label_create(content);
  lv_label_set_text(hdr, LV_SYMBOL_HOME "  Command Center");
  lv_obj_set_style_text_color(hdr, COLOR_CYAN, 0);
  lv_obj_set_style_text_font(hdr, &lv_font_montserrat_14, 0);
  lv_obj_align(hdr, LV_ALIGN_TOP_LEFT, 0, 0);

  label_home_user = lv_label_create(content);
  lv_label_set_text(label_home_user, is_logged_in ? logged_in_user : "Not Logged In");
  lv_obj_set_style_text_color(label_home_user, COLOR_SAFFRON, 0);
  lv_obj_set_style_text_font(label_home_user, &lv_font_montserrat_12, 0);
  lv_obj_align(label_home_user, LV_ALIGN_TOP_RIGHT, 0, 2);

  // Large centered arc
  arc_scan_progress = lv_arc_create(content);
  home_arc_ptr = arc_scan_progress;
  lv_obj_set_size(arc_scan_progress, 130, 130);
  lv_arc_set_rotation(arc_scan_progress, 270);
  lv_arc_set_bg_angles(arc_scan_progress, 0, 360);
  lv_arc_set_range(arc_scan_progress, 0, 100);
  lv_obj_remove_style(arc_scan_progress, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(arc_scan_progress, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(arc_scan_progress, lv_color_hex(0x1A2952), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_scan_progress, COLOR_CYAN, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc_scan_progress, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_scan_progress, 8, LV_PART_INDICATOR);
  lv_obj_align(arc_scan_progress, LV_ALIGN_TOP_MID, 0, 25);
  lv_arc_set_value(arc_scan_progress, 0);

  // Big number in the center of arc
  label_scan_count_home = lv_label_create(arc_scan_progress);
  lv_label_set_text(label_scan_count_home, "0");
  lv_obj_set_style_text_color(label_scan_count_home, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(label_scan_count_home, &lv_font_montserrat_16, 0); 
  
  lv_obj_t *subtitle = lv_label_create(arc_scan_progress);
  lv_label_set_text(subtitle, "SCANS");
  lv_obj_set_style_text_color(subtitle, COLOR_CYAN, 0);
  lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
  
  lv_obj_align(label_scan_count_home, LV_ALIGN_CENTER, 0, -8);
  lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 12);

  // Wrap buttons in a flex container to keep them completely inside screen bounds
  lv_obj_t *btn_container = lv_obj_create(content);
  home_btn_container_ptr = btn_container;
  lv_obj_set_size(btn_container, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_align(btn_container, LV_ALIGN_TOP_MID, 0, 165);
  lv_obj_set_style_bg_opa(btn_container, 0, 0);
  lv_obj_set_style_border_width(btn_container, 0, 0);
  lv_obj_set_style_pad_all(btn_container, 0, 0);
  lv_obj_set_style_pad_row(btn_container, 10, 0);
  lv_obj_set_style_pad_column(btn_container, 10, 0);
  lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *btn_tasks = lv_btn_create(btn_container);
  lv_obj_set_size(btn_tasks, 130, 48);
  lv_obj_set_style_bg_color(btn_tasks, COLOR_CARD, 0);
  lv_obj_set_style_border_color(btn_tasks, COLOR_SAFFRON, 0);
  lv_obj_set_style_border_width(btn_tasks, 2, 0);
  lv_obj_set_style_radius(btn_tasks, 12, 0);
  lv_obj_add_event_cb(btn_tasks, nav_tasks_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *lbl_tasks = lv_label_create(btn_tasks);
  lv_label_set_text(lbl_tasks, LV_SYMBOL_LIST " Tasks");
  lv_obj_set_style_text_color(lbl_tasks, COLOR_SAFFRON, 0);
  lv_obj_set_style_text_font(lbl_tasks, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_tasks);

  lv_obj_t *btn_inv = lv_btn_create(btn_container);
  lv_obj_set_size(btn_inv, 130, 48);
  lv_obj_set_style_bg_color(btn_inv, COLOR_CARD, 0);
  lv_obj_set_style_border_color(btn_inv, COLOR_GREEN, 0);
  lv_obj_set_style_border_width(btn_inv, 2, 0);
  lv_obj_set_style_radius(btn_inv, 12, 0);
  lv_obj_add_event_cb(btn_inv, nav_inventory_cb, LV_EVENT_CLICKED, NULL);
  
  lv_obj_t *lbl_inv = lv_label_create(btn_inv);
  lv_label_set_text(lbl_inv, LV_SYMBOL_LOOP " Inv.");
  lv_obj_set_style_text_color(lbl_inv, COLOR_GREEN, 0);
  lv_obj_set_style_text_font(lbl_inv, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_inv);
}

// ============================================================================
// B. SCAN (Shifted into Tasks tab)
// ============================================================================
static void build_scan_screen() {
  scr_scan = scr_tasks;
  scan_content_ptr = tasks_content_ptr;
}

// ============================================================================
// C. TASKS
// ============================================================================
inline void update_tasks_ui() {
  if (!tasks_content_ptr) return;
  lv_obj_clean(tasks_content_ptr); // Remove old rows

  // Only display tasks when connected to the server
  if (!network_is_server_connected()) {
    lv_obj_t *empty = lv_label_create(tasks_content_ptr);
    lv_label_set_text(empty, "Offline - Server disconnected");
    lv_obj_set_style_text_color(empty, COLOR_MUTE, 0);
    lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);
    return;
  }

  // 1. Live Barcode Scan Info Card at top of Tasks screen
  if (scan_counter > 0) {
    lv_obj_t *scan_card = lv_obj_create(tasks_content_ptr);
    lv_obj_set_size(scan_card, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(scan_card, COLOR_CARD, 0);
    lv_obj_set_style_border_color(scan_card, COLOR_CYAN, 0);
    lv_obj_set_style_border_width(scan_card, 1, 0);
    lv_obj_set_style_radius(scan_card, 6, 0);
    lv_obj_set_style_pad_all(scan_card, 4, 0);
    lv_obj_clear_flag(scan_card, LV_OBJ_FLAG_SCROLLABLE);

    label_last_scan_sku = lv_label_create(scan_card);
    lv_label_set_text(label_last_scan_sku, LV_SYMBOL_BARS " Barcode Scanned");
    lv_obj_set_style_text_color(label_last_scan_sku, COLOR_CYAN, 0);
    lv_obj_set_style_text_font(label_last_scan_sku, &lv_font_montserrat_12, 0);
    lv_obj_align(label_last_scan_sku, LV_ALIGN_LEFT_MID, 4, 0);

    label_scan_progress = lv_label_create(scan_card);
    char prog_buf[32];
    snprintf(prog_buf, sizeof(prog_buf), "Total Scans: %lu", (unsigned long)scan_counter);
    lv_label_set_text(label_scan_progress, prog_buf);
    lv_obj_set_style_text_color(label_scan_progress, COLOR_MUTE, 0);
    lv_obj_set_style_text_font(label_scan_progress, &lv_font_montserrat_12, 0);
    lv_obj_align(label_scan_progress, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  // Count tasks visible to the currently logged-in user
  int visible_count = 0;
  for (int i = 0; i < current_task_count; i++) {
    if (is_task_assigned_to_current_user(current_tasks[i])) {
      visible_count++;
    }
  }

  // 2. Task items list (Urgent tasks highlighted at top)
  if (visible_count == 0) {
    lv_obj_t *empty = lv_label_create(tasks_content_ptr);
    if (!is_logged_in) {
      lv_label_set_text(empty, "Please log in to view tasks.");
    } else {
      lv_label_set_text(empty, "No active tasks assigned to this user.");
    }
    lv_obj_set_style_text_color(empty, COLOR_MUTE, 0);
    lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);
  } else {
    for (int i = 0; i < current_task_count; i++) {
      if (!is_task_assigned_to_current_user(current_tasks[i])) continue;

      lv_obj_t *row = lv_obj_create(tasks_content_ptr);
      lv_obj_set_size(row, LV_PCT(100), 54);

      bool is_urgent = (strstr(current_tasks[i].prio, "High") || strstr(current_tasks[i].prio, "high") || strstr(current_tasks[i].prio, "HIGH"));

      if (strcmp(current_tasks[i].status, "complete") == 0) {
        lv_obj_set_style_bg_color(row, lv_color_hex(0xE8F5E9), 0); // Light green bg
        lv_obj_set_style_border_color(row, COLOR_CARD_BRD, 0);
      } else if (is_urgent) {
        lv_obj_set_style_bg_color(row, lv_color_hex(0xFFEAEA), 0); // Light red tint for urgent
        lv_obj_set_style_border_color(row, COLOR_DANGER, 0);
      } else {
        lv_obj_set_style_bg_color(row, COLOR_WHITE, 0);
        lv_obj_set_style_border_color(row, COLOR_CARD_BRD, 0);
      }

      lv_obj_set_style_border_width(row, 1, 0);
      lv_obj_set_style_radius(row, 6, 0);
      lv_obj_set_style_pad_all(row, 2, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      
      // Make row clickable
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(row, task_row_event_cb, LV_EVENT_CLICKED, &current_tasks[i]);

      lv_obj_t *name = lv_label_create(row);
      lv_label_set_text(name, current_tasks[i].name);
      lv_obj_set_style_text_color(name, (is_urgent && strcmp(current_tasks[i].status, "complete") != 0) ? COLOR_DANGER : COLOR_NAVY, 0);
      lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
      lv_obj_align(name, LV_ALIGN_TOP_LEFT, 6, 2);

      if (strcmp(current_tasks[i].status, "complete") == 0) {
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
      }

      lv_obj_t *sub = lv_label_create(row);
      char sub_text[64];
      if (is_urgent && strcmp(current_tasks[i].status, "complete") != 0) {
        snprintf(sub_text, sizeof(sub_text), "%d items - URGENT", current_tasks[i].item_count);
      } else {
        snprintf(sub_text, sizeof(sub_text), "%d items", current_tasks[i].item_count);
      }
      lv_label_set_text(sub, sub_text);
      lv_obj_set_style_text_color(sub, (is_urgent && strcmp(current_tasks[i].status, "complete") != 0) ? COLOR_DANGER : COLOR_MUTE, 0);
      lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
      lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, 6, -2);
      
      lv_obj_t *badge = lv_obj_create(row);
      lv_obj_set_size(badge, 65, 20);
      lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -6, 0);
      lv_obj_set_style_border_width(badge, 0, 0);
      lv_obj_set_style_radius(badge, 4, 0);
      lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t *b_lbl = lv_label_create(badge);
      if (strcmp(current_tasks[i].status, "complete") == 0) {
        lv_label_set_text(b_lbl, "Complete");
        lv_obj_set_style_bg_color(badge, lv_color_hex(0x00D650), 0); // Green
      } else {
        lv_label_set_text(b_lbl, current_tasks[i].prio);
        lv_obj_set_style_bg_color(badge, current_tasks[i].prio_color, 0);
      }
      
      lv_obj_set_style_text_color(b_lbl, COLOR_WHITE, 0);
      lv_obj_set_style_text_font(b_lbl, &lv_font_montserrat_12, 0);
      lv_obj_center(b_lbl);
    }
  }

  if (scan_content_ptr && scan_content_ptr != tasks_content_ptr) {
    lv_obj_clean(scan_content_ptr);
  }
}

// Digit adjust callback data (3 digits x MAX_ITEMS_PER_TASK)
struct DigitData { TaskItem *itm; int pos; }; // pos: 2=hundreds, 1=tens, 0=units
static DigitData s_digit_data[MAX_ITEMS_PER_TASK * 3];

static void digit_inc_cb(lv_event_t *e) {
  DigitData *d = (DigitData *)lv_event_get_user_data(e);
  if (!d || !d->itm || !active_task) return;
  int mult = 1;
  for (int x = 0; x < d->pos; x++) mult *= 10;
  int cur_digit = (d->itm->picked_qty / mult) % 10;
  if (cur_digit < 9) {
    d->itm->picked_qty += mult;
    beepSuccess();
    update_task_detail_ui(); // just refresh, no auto-complete
  }
}

static void digit_dec_cb(lv_event_t *e) {
  DigitData *d = (DigitData *)lv_event_get_user_data(e);
  if (!d || !d->itm || !active_task) return;
  int mult = 1;
  for (int x = 0; x < d->pos; x++) mult *= 10;
  int cur_digit = (d->itm->picked_qty / mult) % 10;
  if (cur_digit > 0) {
    d->itm->picked_qty -= mult;
    update_task_detail_ui();
  }
}

inline void complete_and_deduct_task(TaskDef *task) {
  if (!task) return;

  // 1. Deduct picked quantities from local inventory in real time
  for (int i = 0; i < task->item_count; i++) {
    int deduct = (task->items[i].picked_qty > 0) ? task->items[i].picked_qty : task->items[i].target_qty;
    if (deduct <= 0) continue;

    for (int j = 0; j < global_inventory_count; j++) {
      if (strcasecmp(global_inventory[j].sku, task->items[i].sku) == 0) {
        int cur_qty = atoi(global_inventory[j].qty);
        int rem_qty = cur_qty - deduct;
        if (rem_qty < 0) rem_qty = 0;
        snprintf(global_inventory[j].qty, sizeof(global_inventory[j].qty), "%d", rem_qty);
        break;
      }
    }
  }

  // Refresh Inventory screen UI immediately
  update_inventory_ui();

  // 2. Build items JSON payload to inform server & MQTT broker of exact deductions
  cJSON *arr = cJSON_CreateArray();
  if (arr) {
    for (int i = 0; i < task->item_count; i++) {
      cJSON *obj = cJSON_CreateObject();
      if (obj) {
        cJSON_AddStringToObject(obj, "sku", task->items[i].sku);
        cJSON_AddStringToObject(obj, "name", task->items[i].name);
        cJSON_AddNumberToObject(obj, "target_qty", task->items[i].target_qty);
        int p_qty = (task->items[i].picked_qty > 0) ? task->items[i].picked_qty : task->items[i].target_qty;
        cJSON_AddNumberToObject(obj, "picked_qty", p_qty);
        cJSON_AddItemToArray(arr, obj);
      }
    }
    char *items_json = cJSON_PrintUnformatted(arr);
    network_publish_task_complete_payload(task->id, items_json);
    if (items_json) free(items_json);
    cJSON_Delete(arr);
  } else {
    network_publish_task_complete(task->id);
  }

  // 3. Mark task as complete and reset UI state
  strncpy(task->status, "complete", sizeof(task->status) - 1);
  selected_task_item_sku = "";
  active_task = NULL;
}

// Called when user taps CONFIRM on an expanded task item row
static void confirm_task_item_cb(lv_event_t *e) {
  if (!active_task) return;
  beepSuccess();
  selected_task_item_sku = ""; // collapse the row

  // Check if all items are now fully picked
  bool all_done = true;
  for (int i = 0; i < active_task->item_count; i++) {
    if (active_task->items[i].picked_qty < active_task->items[i].target_qty) {
      all_done = false;
      break;
    }
  }
  if (all_done) {
    beepStartup();
    complete_and_deduct_task(active_task);
    _load_scr_direct(scr_tasks, "Tasks");
    update_tasks_ui();
  } else {
    update_task_detail_ui();
  }
}

[[maybe_unused]] static void inline_task_item_dec_cb(lv_event_t *e) {
  TaskItem *itm = (TaskItem *)lv_event_get_user_data(e);
  if (!itm || !active_task) return;
  if (itm->picked_qty > 0) { itm->picked_qty--; update_task_detail_ui(); }
}

static void update_task_detail_ui() {
  if (!task_detail_content || !active_task) return;
  lv_obj_clean(task_detail_content);

  // Large easily clickable back button at the top of the detail view
  lv_obj_t *back_btn = lv_btn_create(task_detail_content);
  lv_obj_set_size(back_btn, LV_PCT(100), 45);
  lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x2A2D3D), 0);
  lv_obj_set_style_radius(back_btn, 6, 0);
  lv_obj_add_event_cb(back_btn, [](lv_event_t *e) {
      active_task = NULL;
      _load_scr_direct(scr_tasks, "Tasks");
  }, LV_EVENT_CLICKED, NULL);

  lv_obj_t *back_lbl = lv_label_create(back_btn);
  lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " BACK TO TASKS");
  lv_obj_set_style_text_color(back_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(back_lbl);

  lv_obj_t *header = lv_label_create(task_detail_content);
  lv_label_set_text(header, active_task->name);
  lv_obj_set_style_text_color(header, COLOR_NAVY_BLUE, 0);
  lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
  lv_obj_set_style_pad_top(header, 8, 0);
  lv_obj_set_style_pad_bottom(header, 8, 0);

  for (int i = 0; i < active_task->item_count; i++) {
    TaskItem &itm = active_task->items[i];

    bool is_selected = (selected_task_item_sku == itm.sku);
    bool is_completed = (itm.picked_qty >= itm.target_qty);

    lv_obj_t *row = lv_obj_create(task_detail_content);
    lv_obj_set_style_bg_color(row, COLOR_CARD, 0);
    lv_obj_set_style_border_color(row, is_selected ? COLOR_CYAN : (is_completed ? lv_color_hex(0x22CC55) : COLOR_CARD_BRD), 0);
    lv_obj_set_style_border_width(row, is_selected ? 2 : 1, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    if (is_completed && !is_selected) {
      lv_obj_set_style_bg_color(row, lv_color_hex(0x0A2A14), 0); // dark green done
    }

    if (is_selected) {
      // ── EXPANDED ROW: 3-digit counter (ones, tens, hundreds) appears ONLY for scanned item ──
      lv_obj_set_size(row, LV_PCT(100), 175);

      // Item name top-left
      lv_obj_t *name = lv_label_create(row);
      lv_label_set_text(name, itm.name);
      lv_obj_set_style_text_color(name, COLOR_WHITE, 0);
      lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
      lv_obj_align(name, LV_ALIGN_TOP_LEFT, 4, 2);

      // SKU below name
      lv_obj_t *sku_lbl = lv_label_create(row);
      char sku_text[48];
      snprintf(sku_text, sizeof(sku_text), "SKU: %s", itm.sku);
      lv_label_set_text(sku_lbl, sku_text);
      lv_obj_set_style_text_color(sku_lbl, COLOR_MUTE, 0);
      lv_obj_set_style_text_font(sku_lbl, &lv_font_montserrat_14, 0);
      lv_obj_align(sku_lbl, LV_ALIGN_TOP_LEFT, 4, 24);

      // Progress label below SKU
      lv_obj_t *prog = lv_label_create(row);
      char prog_text[32];
      snprintf(prog_text, sizeof(prog_text), "Picked: %d / %d", itm.picked_qty, itm.target_qty);
      lv_label_set_text(prog, prog_text);
      lv_obj_set_style_text_color(prog, COLOR_SAFFRON, 0);
      lv_obj_set_style_text_font(prog, &lv_font_montserrat_14, 0);
      lv_obj_align(prog, LV_ALIGN_TOP_LEFT, 4, 46);

      // ── Three digit columns on the right (ones, tens, hundreds) ──
      int col_offsets[3] = { -4, -56, -108 }; // right-to-left: ones, tens, hundreds
      int col_positions[3] = { 0, 1, 2 };     // ones=0, tens=1, hundreds=2
      const char *col_titles[3] = { "ONES", "TENS", "100s" };

      for (int d = 0; d < 3; d++) {
        int pos = col_positions[d];    // digit place value
        int mult = 1;
        for (int x = 0; x < pos; x++) mult *= 10;
        int digit_val = (itm.picked_qty / mult) % 10;

        // Store callback data in static array slot: item_index*3 + digit
        int slot = i * 3 + d;
        s_digit_data[slot].itm = &itm;
        s_digit_data[slot].pos = pos;

        // Digit column container
        lv_obj_t *col = lv_obj_create(row);
        lv_obj_set_size(col, 48, 92);
        lv_obj_align(col, LV_ALIGN_TOP_RIGHT, col_offsets[d], 4);
        lv_obj_set_style_bg_color(col, lv_color_hex(0x050A1A), 0);
        lv_obj_set_style_border_color(col, COLOR_NAVY_BLUE, 0);
        lv_obj_set_style_border_width(col, 1, 0);
        lv_obj_set_style_radius(col, 6, 0);
        lv_obj_set_style_pad_all(col, 0, 0);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

        // [+] button
        lv_obj_t *bp = lv_btn_create(col);
        lv_obj_set_size(bp, 48, 26);
        lv_obj_align(bp, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(bp, COLOR_NAVY_BLUE, 0);
        lv_obj_set_style_radius(bp, 4, 0);
        lv_obj_set_style_pad_all(bp, 0, 0);
        lv_obj_add_event_cb(bp, digit_inc_cb, LV_EVENT_CLICKED, &s_digit_data[slot]);
        lv_obj_t *lp = lv_label_create(bp);
        lv_label_set_text(lp, LV_SYMBOL_PLUS);
        lv_obj_set_style_text_color(lp, COLOR_WHITE, 0);
        lv_obj_set_style_text_font(lp, &lv_font_montserrat_14, 0);
        lv_obj_center(lp);

        // Big digit box
        lv_obj_t *dbox = lv_obj_create(col);
        lv_obj_set_size(dbox, 48, 34);
        lv_obj_align(dbox, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(dbox, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(dbox, COLOR_CYAN, 0);
        lv_obj_set_style_border_width(dbox, 1, 0);
        lv_obj_set_style_radius(dbox, 4, 0);
        lv_obj_set_style_pad_all(dbox, 0, 0);
        lv_obj_clear_flag(dbox, LV_OBJ_FLAG_SCROLLABLE);
        char dstr[4]; snprintf(dstr, sizeof(dstr), "%d", digit_val);
        lv_obj_t *dlbl = lv_label_create(dbox);
        lv_label_set_text(dlbl, dstr);
        lv_obj_set_style_text_color(dlbl, COLOR_CYAN, 0);
        lv_obj_set_style_text_font(dlbl, &lv_font_montserrat_16, 0);
        lv_obj_center(dlbl);

        // [-] button
        lv_obj_t *bm = lv_btn_create(col);
        lv_obj_set_size(bm, 48, 26);
        lv_obj_align(bm, LV_ALIGN_BOTTOM_MID, 0, -2);
        lv_obj_set_style_bg_color(bm, COLOR_MUTE, 0);
        lv_obj_set_style_radius(bm, 4, 0);
        lv_obj_set_style_pad_all(bm, 0, 0);
        lv_obj_add_event_cb(bm, digit_dec_cb, LV_EVENT_CLICKED, &s_digit_data[slot]);
        lv_obj_t *lm = lv_label_create(bm);
        lv_label_set_text(lm, LV_SYMBOL_MINUS);
        lv_obj_set_style_text_color(lm, COLOR_WHITE, 0);
        lv_obj_set_style_text_font(lm, &lv_font_montserrat_14, 0);
        lv_obj_center(lm);

        // Column label (ONES, TENS, 100s)
        lv_obj_t *clbl = lv_label_create(row);
        lv_label_set_text(clbl, col_titles[d]);
        lv_obj_set_style_text_color(clbl, COLOR_CYAN, 0);
        lv_obj_set_style_text_font(clbl, &lv_font_montserrat_12, 0);
        lv_obj_align(clbl, LV_ALIGN_TOP_RIGHT, col_offsets[d] - 6, 100);
      }

      // ── CONFIRM button — moved to bottom-left ──
      lv_obj_t *btn_confirm = lv_btn_create(row);
      lv_obj_set_size(btn_confirm, 155, 32);
      lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_LEFT, 4, -6);
      lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0x00AA44), 0);
      lv_obj_set_style_radius(btn_confirm, 6, 0);
      lv_obj_set_style_pad_all(btn_confirm, 0, 0);
      lv_obj_add_event_cb(btn_confirm, confirm_task_item_cb, LV_EVENT_CLICKED, &itm);
      lv_obj_t *lbl_confirm = lv_label_create(btn_confirm);
      lv_label_set_text(lbl_confirm, LV_SYMBOL_OK "  CONFIRM");
      lv_obj_set_style_text_color(lbl_confirm, COLOR_WHITE, 0);
      lv_obj_set_style_text_font(lbl_confirm, &lv_font_montserrat_14, 0);
      lv_obj_center(lbl_confirm);
    } else {
      // ── COMPACT ROW: Item waiting to be scanned ──
      lv_obj_set_size(row, LV_PCT(100), 72);

      // Item name top-left
      lv_obj_t *name = lv_label_create(row);
      lv_label_set_text(name, itm.name);
      lv_obj_set_style_text_color(name, COLOR_WHITE, 0);
      lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
      lv_obj_align(name, LV_ALIGN_TOP_LEFT, 4, 4);

      // SKU below name
      lv_obj_t *sku_lbl = lv_label_create(row);
      char sku_text[48];
      snprintf(sku_text, sizeof(sku_text), "SKU: %s", itm.sku);
      lv_label_set_text(sku_lbl, sku_text);
      lv_obj_set_style_text_color(sku_lbl, COLOR_MUTE, 0);
      lv_obj_set_style_text_font(sku_lbl, &lv_font_montserrat_14, 0);
      lv_obj_align(sku_lbl, LV_ALIGN_TOP_LEFT, 4, 26);

      // Progress label below SKU
      lv_obj_t *prog = lv_label_create(row);
      char prog_text[32];
      snprintf(prog_text, sizeof(prog_text), "Picked: %d / %d", itm.picked_qty, itm.target_qty);
      lv_label_set_text(prog, prog_text);
      lv_obj_set_style_text_color(prog, is_completed ? lv_color_hex(0x22CC55) : COLOR_SAFFRON, 0);
      lv_obj_set_style_text_font(prog, &lv_font_montserrat_14, 0);
      lv_obj_align(prog, LV_ALIGN_TOP_LEFT, 4, 46);

      // Right-side badge
      if (is_completed) {
        lv_obj_t *done_badge = lv_obj_create(row);
        lv_obj_set_size(done_badge, 110, 32);
        lv_obj_align(done_badge, LV_ALIGN_RIGHT_MID, -6, 0);
        lv_obj_set_style_bg_color(done_badge, lv_color_hex(0x0A3A1A), 0);
        lv_obj_set_style_border_color(done_badge, lv_color_hex(0x22CC55), 0);
        lv_obj_set_style_border_width(done_badge, 1, 0);
        lv_obj_set_style_radius(done_badge, 6, 0);
        lv_obj_set_style_pad_all(done_badge, 0, 0);
        lv_obj_clear_flag(done_badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *done_lbl = lv_label_create(done_badge);
        lv_label_set_text(done_lbl, LV_SYMBOL_OK " DONE");
        lv_obj_set_style_text_color(done_lbl, lv_color_hex(0x22CC55), 0);
        lv_obj_set_style_text_font(done_lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(done_lbl);
      } else {
        lv_obj_t *scan_badge = lv_obj_create(row);
        lv_obj_set_size(scan_badge, 126, 32);
        lv_obj_align(scan_badge, LV_ALIGN_RIGHT_MID, -6, 0);
        lv_obj_set_style_bg_color(scan_badge, lv_color_hex(0x08152E), 0);
        lv_obj_set_style_border_color(scan_badge, COLOR_CYAN, 0);
        lv_obj_set_style_border_width(scan_badge, 1, 0);
        lv_obj_set_style_radius(scan_badge, 6, 0);
        lv_obj_set_style_pad_all(scan_badge, 0, 0);
        lv_obj_clear_flag(scan_badge, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *scan_lbl = lv_label_create(scan_badge);
        lv_label_set_text(scan_lbl, LV_SYMBOL_CHARGE " SCAN ITEM");
        lv_obj_set_style_text_color(scan_lbl, COLOR_CYAN, 0);
        lv_obj_set_style_text_font(scan_lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(scan_lbl);
      }

      // Tap event callback to prompt scanning
      lv_obj_add_event_cb(row, [](lv_event_t *e) {
        uiShowTaskError("Scan item barcode to pick!");
      }, LV_EVENT_CLICKED, NULL);
    }
  }
}

static void build_task_detail_screen() {
  if (scr_task_detail == NULL) {
    scr_task_detail = lv_obj_create(NULL);
    lv_obj_clear_flag(scr_task_detail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr_task_detail, COLOR_BG, 0);
    lv_obj_add_event_cb(scr_task_detail, _global_screen_resize_cb, LV_EVENT_SIZE_CHANGED, NULL);

    task_detail_content = lv_obj_create(scr_task_detail);
    lv_obj_set_size(task_detail_content, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL) - 32);
    lv_obj_set_pos(task_detail_content, 0, 32);
    lv_obj_set_style_bg_color(task_detail_content, COLOR_BG, 0);
    lv_obj_set_style_border_width(task_detail_content, 0, 0);
    lv_obj_set_flex_flow(task_detail_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(task_detail_content, 8, 0);
    lv_obj_set_style_pad_bottom(task_detail_content, 72, 0);
    lv_obj_set_scroll_dir(task_detail_content, LV_DIR_VER);
    lv_obj_set_style_pad_row(task_detail_content, 6, 0);

    task_error_label = lv_label_create(scr_task_detail);
    lv_obj_set_style_bg_color(task_error_label, lv_color_hex(0xFF0000), 0); // Pure red
    lv_obj_set_style_text_color(task_error_label, COLOR_WHITE, 0);
    lv_obj_set_style_pad_all(task_error_label, 10, 0);
    lv_obj_set_style_radius(task_error_label, 6, 0);
    lv_obj_set_style_text_font(task_error_label, &lv_font_montserrat_16, 0);
    lv_label_set_text(task_error_label, "Invalid Item!");
    lv_obj_align(task_error_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_flag(task_error_label, LV_OBJ_FLAG_HIDDEN);
  }
  update_task_detail_ui();
}

inline void uiShowTaskError(const char* msg) {
  if (!task_error_label) return;
  lv_label_set_text(task_error_label, msg);
  lv_obj_clear_flag(task_error_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_opa(task_error_label, 255, 0);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, task_error_label);
  lv_anim_set_time(&a, 300);
  lv_anim_set_delay(&a, 1200);
  lv_anim_set_values(&a, 255, 0);
  lv_anim_set_exec_cb(&a, [](void *obj, int32_t v) {
      lv_obj_set_style_opa((lv_obj_t*)obj, v, 0);
  });
  lv_anim_set_ready_cb(&a, [](lv_anim_t *a) {
      lv_obj_add_flag((lv_obj_t*)a->var, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_style_opa((lv_obj_t*)a->var, 255, 0);
  });
  lv_anim_start(&a);
}

static void task_row_event_cb(lv_event_t *e) {
  TaskDef *task = (TaskDef *)lv_event_get_user_data(e);
  if (!task) return;
  if (strcmp(task->status, "complete") == 0) {
    beepError();
    uiShowTaskError("Task already completed!");
    return; // Never open completed task for picking!
  }
  active_task = task;
  selected_task_item_sku = ""; // Clean state: no item selected initially
  build_task_detail_screen();
  _load_scr_direct(scr_task_detail, "Picking Task");
}

inline bool uiTryStartTask(const std::string &sku, bool &is_item_scan) {
  ESP_LOGI("UI", "uiTryStartTask called with sku: '%s'", sku.c_str());
  std::string sku_lower = sku;
  for (auto &c : sku_lower) c = tolower(c);

  bool was_already_completed = false;

  for (int i = 0; i < current_task_count; i++) {
    if (!is_task_assigned_to_current_user(current_tasks[i])) continue;

    // NEVER match or re-open completed tasks!
    if (strcmp(current_tasks[i].status, "complete") == 0) {
      std::string task_id_lower = current_tasks[i].id;
      for (auto &c : task_id_lower) c = tolower(c);
      if (sku_lower == task_id_lower || 
          sku_lower.find(task_id_lower) != std::string::npos || 
          task_id_lower.find(sku_lower) != std::string::npos) {
        was_already_completed = true;
      }
      for (int j = 0; j < current_tasks[i].item_count; j++) {
        if (sku == current_tasks[i].items[j].sku) {
          was_already_completed = true;
        }
      }
      continue;
    }

    std::string task_id_lower = current_tasks[i].id;
    for (auto &c : task_id_lower) c = tolower(c);

    // 1. Check if the scanned code is the Task ID itself
    if (sku_lower == task_id_lower || 
        sku_lower.find(task_id_lower) != std::string::npos || 
        task_id_lower.find(sku_lower) != std::string::npos) {
      active_task = &current_tasks[i];
      selected_task_item_sku = ""; // Task opened via Task ID, no item selected yet
      build_task_detail_screen();
      _load_scr_direct(scr_task_detail, "Picking Task");
      update_task_detail_ui(); 
      is_item_scan = false;
      return true;
    }

    // 2. Check if the scanned code is a Product SKU that belongs to this task
    for (int j = 0; j < current_tasks[i].item_count; j++) {
      if (sku == current_tasks[i].items[j].sku) {
        // If this specific item is already fully picked, skip it
        if (current_tasks[i].items[j].picked_qty >= current_tasks[i].items[j].target_qty) {
          was_already_completed = true;
          continue;
        }
        active_task = &current_tasks[i];
        selected_task_item_sku = sku; // Product SKU scanned directly! Expand this item!
        build_task_detail_screen();
        _load_scr_direct(scr_task_detail, "Picking Task");
        update_task_detail_ui();
        is_item_scan = true;
        return true;
      }
    }
  }

  if (was_already_completed) {
    uiShowTaskError("Task / Item already completed!");
  }
  return false;
}

static void build_tasks_screen() {
  scr_tasks = lv_obj_create(NULL);
  lv_obj_clear_flag(scr_tasks, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr_tasks, COLOR_BG, 0);
  build_status_bar(scr_tasks, "Tasks");
  build_nav_rail(scr_tasks, 1);
  tasks_content_ptr = build_content_area(scr_tasks);
  lv_obj_set_flex_flow(tasks_content_ptr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(tasks_content_ptr, 6, 0);

  update_tasks_ui();
}

// ============================================================================
// B2. QUANTITY ADJUSTMENT SCREEN (+/- Tab after scanning)
// ============================================================================
static void qty_minus_cb(lv_event_t *e) {
  if (current_scan_adjust_qty > 1) {
    current_scan_adjust_qty--;
    if (label_qty_val) {
      lv_label_set_text(label_qty_val, std::to_string(current_scan_adjust_qty).c_str());
    }
  }
}

static void qty_plus_cb(lv_event_t *e) {
  current_scan_adjust_qty++;
  if (label_qty_val) {
    lv_label_set_text(label_qty_val, std::to_string(current_scan_adjust_qty).c_str());
  }
}

static void qty_cancel_cb(lv_event_t *e) {
  _load_scr_direct(scr_tasks, "Tasks");
  update_tasks_ui();
}

static void qty_confirm_cb(lv_event_t *e) {
  if (current_scan_adjust_sku.empty()) {
    _load_scr_direct(scr_tasks, "Tasks");
    update_tasks_ui();
    return;
  }

  // 1. Publish scan via MQTT
  network_publish_scan(current_scan_adjust_sku);

  // 2. Update matching task items with adjusted quantity
  bool item_matched = false;
  if (active_task) {
    for (int i = 0; i < active_task->item_count; i++) {
      if (std::string(active_task->items[i].sku) == current_scan_adjust_sku) {
        active_task->items[i].picked_qty += current_scan_adjust_qty;
        item_matched = true;
        break;
      }
    }
  }
  if (!item_matched) {
    for (int t = 0; t < current_task_count; t++) {
      for (int i = 0; i < current_tasks[t].item_count; i++) {
        if (std::string(current_tasks[t].items[i].sku) == current_scan_adjust_sku) {
          current_tasks[t].items[i].picked_qty += current_scan_adjust_qty;
          item_matched = true;
          break;
        }
      }
      if (item_matched) break;
    }
  }

  // 3. Update total scan stats
  scan_counter += current_scan_adjust_qty;
  if (label_scan_count_home) {
    std::string txt = std::to_string(scan_counter);
    lv_label_set_text(label_scan_count_home, txt.c_str());
  }
  if (arc_scan_progress) {
    lv_arc_set_value(arc_scan_progress, (scan_counter * 5) % 100);
  }

  // 4. Load tasks view
  _load_scr_direct(scr_tasks, "Tasks");
  update_tasks_ui();
}

static void build_qty_adjust_screen() {
  scr_qty_adjust = lv_obj_create(NULL);
  lv_obj_clear_flag(scr_qty_adjust, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr_qty_adjust, COLOR_BG, 0);
  build_status_bar(scr_qty_adjust, "Quantity Adjust");

  lv_obj_t *content = build_content_area(scr_qty_adjust);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(content, 12, 0);
  lv_obj_set_style_pad_row(content, 10, 0);

  // Header Card: Scanned Item Details
  lv_obj_t *card = lv_obj_create(content);
  lv_obj_set_size(card, LV_PCT(100), 75);
  lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
  lv_obj_set_style_border_color(card, COLOR_CARD_BRD, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 8, 0);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  label_qty_name = lv_label_create(card);
  lv_label_set_text(label_qty_name, "Scanned Item");
  lv_obj_set_style_text_color(label_qty_name, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(label_qty_name, &lv_font_montserrat_16, 0);
  lv_obj_align(label_qty_name, LV_ALIGN_TOP_LEFT, 4, 2);

  label_qty_sku = lv_label_create(card);
  lv_label_set_text(label_qty_sku, "SKU: ---");
  lv_obj_set_style_text_color(label_qty_sku, COLOR_CYAN, 0);
  lv_obj_set_style_text_font(label_qty_sku, &lv_font_montserrat_14, 0);
  lv_obj_align(label_qty_sku, LV_ALIGN_BOTTOM_LEFT, 4, -2);

  // Quantity Controls Box (- Qty +)
  lv_obj_t *qty_ctrl_box = lv_obj_create(content);
  lv_obj_set_size(qty_ctrl_box, LV_PCT(100), 95);
  lv_obj_set_style_bg_color(qty_ctrl_box, COLOR_CARD, 0);
  lv_obj_set_style_border_color(qty_ctrl_box, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(qty_ctrl_box, 1, 0);
  lv_obj_set_style_radius(qty_ctrl_box, 8, 0);
  lv_obj_clear_flag(qty_ctrl_box, LV_OBJ_FLAG_SCROLLABLE);

  // Minus button (-)
  lv_obj_t *btn_minus = lv_btn_create(qty_ctrl_box);
  lv_obj_set_size(btn_minus, 80, 60);
  lv_obj_align(btn_minus, LV_ALIGN_LEFT_MID, 16, 0);
  lv_obj_set_style_bg_color(btn_minus, lv_color_hex(0x8B0000), 0);
  lv_obj_set_style_radius(btn_minus, 8, 0);
  lv_obj_add_event_cb(btn_minus, qty_minus_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_m = lv_label_create(btn_minus);
  lv_label_set_text(lbl_m, LV_SYMBOL_MINUS);
  lv_obj_set_style_text_font(lbl_m, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_m);

  // Quantity Value Display
  label_qty_val = lv_label_create(qty_ctrl_box);
  lv_label_set_text(label_qty_val, "1");
  lv_obj_set_style_text_color(label_qty_val, COLOR_SAFFRON, 0);
  lv_obj_set_style_text_font(label_qty_val, &lv_font_montserrat_16, 0);
  lv_obj_align(label_qty_val, LV_ALIGN_CENTER, 0, 0);

  // Plus button (+)
  lv_obj_t *btn_plus = lv_btn_create(qty_ctrl_box);
  lv_obj_set_size(btn_plus, 80, 60);
  lv_obj_align(btn_plus, LV_ALIGN_RIGHT_MID, -16, 0);
  lv_obj_set_style_bg_color(btn_plus, lv_color_hex(0x006400), 0);
  lv_obj_set_style_radius(btn_plus, 8, 0);
  lv_obj_add_event_cb(btn_plus, qty_plus_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_p = lv_label_create(btn_plus);
  lv_label_set_text(lbl_p, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_font(lbl_p, &lv_font_montserrat_16, 0);
  lv_obj_center(lbl_p);

  // Bottom Action Row (Cancel / Confirm)
  lv_obj_t *btn_row = lv_obj_create(content);
  lv_obj_set_size(btn_row, LV_PCT(100), 55);
  lv_obj_set_style_bg_color(btn_row, COLOR_BG, 0);
  lv_obj_set_style_border_width(btn_row, 0, 0);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *btn_cancel = lv_btn_create(btn_row);
  lv_obj_set_size(btn_cancel, 150, 45);
  lv_obj_align(btn_cancel, LV_ALIGN_LEFT_MID, 5, 0);
  lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x3A3D4D), 0);
  lv_obj_add_event_cb(btn_cancel, qty_cancel_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_c = lv_label_create(btn_cancel);
  lv_label_set_text(lbl_c, "CANCEL");
  lv_obj_set_style_text_font(lbl_c, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_c);

  lv_obj_t *btn_confirm = lv_btn_create(btn_row);
  lv_obj_set_size(btn_confirm, 240, 45);
  lv_obj_align(btn_confirm, LV_ALIGN_RIGHT_MID, -5, 0);
  lv_obj_set_style_bg_color(btn_confirm, COLOR_NAVY_BLUE, 0);
  lv_obj_add_event_cb(btn_confirm, qty_confirm_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_ok = lv_label_create(btn_confirm);
  lv_label_set_text(lbl_ok, LV_SYMBOL_OK " CONFIRM QTY");
  lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_ok);
}

inline void update_inventory_ui() {
  if (!inv_list) return;
  lv_obj_clean(inv_list);
  
  // Only display inventory items when connected to the server
  if (!network_is_server_connected()) {
    return;
  }
  
  const char *query = inv_search_ta ? lv_textarea_get_text(inv_search_ta) : "";
  
  for (int i = 0; i < global_inventory_count; i++) {
    // Case-insensitive substring match on name or SKU
    std::string q = std::string(query); for (auto &c : q) c = tolower(c);
    std::string nm = std::string(global_inventory[i].name); for (auto &c : nm) c = tolower(c);
    std::string sk = std::string(global_inventory[i].sku); for (auto &c : sk) c = tolower(c);
    if (q.length() == 0 || nm.find(q) != std::string::npos || sk.find(q) != std::string::npos) {
      char buf[128];
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
  lv_obj_clear_flag(scr_inventory, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr_inventory, COLOR_BG, 0);
  build_status_bar(scr_inventory, "Inventory");
  build_nav_rail(scr_inventory, 2);
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
inline lv_obj_t *ta_wifi_ssid;
inline lv_obj_t *ta_wifi_pass;
inline lv_obj_t *wcard_inputs = NULL;
inline lv_obj_t *ble_active_panel = NULL;
inline lv_obj_t *label_ble_large_status = NULL;
static void _kb_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    if (global_kb) {
      lv_obj_add_flag(global_kb, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void _ta_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);

  if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
    if (!global_kb) {
      global_kb = lv_keyboard_create(lv_layer_top());
      lv_obj_set_size(global_kb, LV_PCT(100), lv_disp_get_ver_res(NULL) / 2);
      lv_obj_align(global_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
      lv_obj_add_event_cb(global_kb, _kb_event_cb, LV_EVENT_ALL, NULL);
    }
    lv_keyboard_set_textarea(global_kb, ta);
    lv_obj_clear_flag(global_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(global_kb);
  } else if (code == LV_EVENT_DEFOCUSED) {
    if (global_kb) {
      lv_obj_add_flag(global_kb, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void _wifi_save_connect_btn_cb(lv_event_t *e) {
  (void)e;
  if (ta_wifi_ssid && ta_wifi_pass) {
    const char *s = lv_textarea_get_text(ta_wifi_ssid);
    const char *p = lv_textarea_get_text(ta_wifi_pass);
    if (s && strlen(s) > 0) {
      if (label_conn_wifi_detail) {
        lv_label_set_text(label_conn_wifi_detail, "Connecting...");
        lv_obj_set_style_text_color(label_conn_wifi_detail, COLOR_SAFFRON, 0);
      }
      uiSetWifiStatus(LV_SYMBOL_WIFI " Connecting...");
      network_save_and_connect_wifi(s, p ? p : "");
    }
  }
}



static void _mode_switch_cb(lv_event_t *e) {
  lv_obj_t *sw = lv_event_get_target(e);
  bool is_ble = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (is_ble) {
    // Start BLE advertising — device becomes visible to phones as "ScanPro-X1-BLE"
    bleStartAdvertising();
    if (wcard_inputs) lv_obj_add_flag(wcard_inputs, LV_OBJ_FLAG_HIDDEN);
    if (ble_active_panel) lv_obj_clear_flag(ble_active_panel, LV_OBJ_FLAG_HIDDEN);
    if (wcard_ref) lv_obj_set_style_border_color(wcard_ref, COLOR_GREEN, 0);
    if (label_conn_ble_detail) {
      char buf[64];
      snprintf(buf, sizeof(buf), "BLE Mode Active\nDevice: %s", bleGetDeviceName());
      lv_label_set_text(label_conn_ble_detail, buf);
    }
    if (label_ble_large_status) {
      char buf[128];
      snprintf(buf, sizeof(buf), "Status: Advertising...\n\nDevice: %s\n\nVisible to nearby phones via Bluetooth.", bleGetDeviceName());
      lv_label_set_text(label_ble_large_status, buf);
    }
    uiSetWifiStatus(LV_SYMBOL_BLUETOOTH " BLE: Adv...");
  } else {
    // Stop BLE advertising — device is hidden from phones
    bleStopAdvertising();
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
  lv_obj_clear_flag(scr_conn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr_conn, COLOR_BG, 0);
  build_status_bar(scr_conn, "Connectivity");
  conn_nav_rail_ptr = build_nav_rail(scr_conn, 3);
  conn_content_ptr = build_content_area(scr_conn);
  lv_obj_t *content = conn_content_ptr;

  // ---- WiFi Card ----
  lv_obj_t *wcard = lv_obj_create(content);
  wcard_ref = wcard;
  lv_obj_set_size(wcard, 200, 215);
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
  char buf_ble_init[128];
  snprintf(buf_ble_init, sizeof(buf_ble_init), "Status: Advertising...\n\nDevice: %s\n\nLive Bluetooth scan streaming active.", bleGetDeviceName());
  lv_label_set_text(label_ble_large_status, buf_ble_init);
  lv_obj_set_style_text_color(label_ble_large_status, COLOR_NAVY, 0);
  lv_obj_set_style_text_font(label_ble_large_status, &lv_font_montserrat_12, 0);
  lv_label_set_long_mode(label_ble_large_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label_ble_large_status, 172);
  lv_obj_align(label_ble_large_status, LV_ALIGN_TOP_LEFT, 4, 34);

  lv_obj_add_flag(ble_active_panel, LV_OBJ_FLAG_HIDDEN); // hidden by default in WiFi mode

  // Input Fields Container Group
  wcard_inputs = lv_obj_create(wcard);
  lv_obj_set_size(wcard_inputs, 188, 180);
  lv_obj_align(wcard_inputs, LV_ALIGN_TOP_LEFT, 0, 22);
  lv_obj_set_style_bg_opa(wcard_inputs, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wcard_inputs, 0, 0);
  lv_obj_set_style_pad_all(wcard_inputs, 0, 0);
  lv_obj_clear_flag(wcard_inputs, LV_OBJ_FLAG_SCROLLABLE);

  char s_ssid[64] = {0};
  char s_pass[64] = {0};
  bool has_saved = network_get_saved_wifi(s_ssid, sizeof(s_ssid), s_pass, sizeof(s_pass));

  // SSID Input Field
  ta_wifi_ssid = lv_textarea_create(wcard_inputs);
  lv_textarea_set_placeholder_text(ta_wifi_ssid, "WiFi SSID");
  lv_textarea_set_text(ta_wifi_ssid, has_saved ? s_ssid : "waveshare");
  lv_textarea_set_one_line(ta_wifi_ssid, true);
  lv_obj_set_size(ta_wifi_ssid, 184, 34);
  lv_obj_align(ta_wifi_ssid, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_text_font(ta_wifi_ssid, &lv_font_montserrat_12, 0);
  lv_obj_add_event_cb(ta_wifi_ssid, _ta_event_cb, LV_EVENT_ALL, NULL);

  // Password Input Field + Eye Toggle
  ta_wifi_pass = lv_textarea_create(wcard_inputs);
  lv_textarea_set_placeholder_text(ta_wifi_pass, "Password");
  lv_textarea_set_text(ta_wifi_pass, has_saved ? s_pass : "12345678");
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
  if (false || false) {
    lv_obj_add_flag(wcard_inputs, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ble_active_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_color(wcard, COLOR_GREEN, 0);
  }

  // ---- Network Mode Card (Exclusive WiFi vs BLE Switch) ----
  lv_obj_t *bcard = lv_obj_create(content);
  bcard_ref = bcard;
  lv_obj_set_size(bcard, 195, 95);
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
  if (false || false) {
    lv_obj_add_state(sw_mode, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(sw_mode, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(sw_mode, _mode_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // ---- Landscape Orientation Card (Left / Right Landscape Switch) ----
  lv_obj_t *rcard = lv_obj_create(content);
  rcard_ref = rcard;
  lv_obj_set_size(rcard, 195, 110);
  lv_obj_align(rcard, LV_ALIGN_TOP_RIGHT, 0, 100);
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

  // ── WRIST L SWITCH (rot 3) ──────────────────────────────────────────────
  sw_wrist_l_ptr = lv_switch_create(rcard);
  lv_obj_set_size(sw_wrist_l_ptr, 50, 28);
  lv_obj_align(sw_wrist_l_ptr, LV_ALIGN_BOTTOM_LEFT, 0, 4);
  lv_obj_clear_state(sw_wrist_l_ptr, LV_STATE_CHECKED);

  // ── WRIST R SWITCH (rot 1) ──────────────────────────────────────────────
  sw_wrist_r_ptr = lv_switch_create(rcard);
  lv_obj_set_size(sw_wrist_r_ptr, 50, 28);
  lv_obj_align(sw_wrist_r_ptr, LV_ALIGN_BOTTOM_MID, 0, 4);
  lv_obj_add_state(sw_wrist_r_ptr, LV_STATE_CHECKED); // Default is Right Landscape (rot 1)

  // ── PORT SWITCH (rot 0) ──────────────────────────────────────────────────
  sw_port_ptr = lv_switch_create(rcard);
  lv_obj_set_size(sw_port_ptr, 50, 28);
  lv_obj_align(sw_port_ptr, LV_ALIGN_BOTTOM_RIGHT, 0, 4);
  lv_obj_clear_state(sw_port_ptr, LV_STATE_CHECKED);

  // ── LABELS ───────────────────────────────────────────────────────────────
  lv_obj_t *lbl_wrist_l = lv_label_create(rcard);
  lv_label_set_text(lbl_wrist_l, "Wrist L");
  lv_obj_set_style_text_color(lbl_wrist_l, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(lbl_wrist_l, &lv_font_montserrat_12, 0);
  lv_obj_align_to(lbl_wrist_l, sw_wrist_l_ptr, LV_ALIGN_OUT_TOP_MID, 0, -5);

  lv_obj_t *lbl_wrist_r = lv_label_create(rcard);
  lv_label_set_text(lbl_wrist_r, "Wrist R");
  lv_obj_set_style_text_color(lbl_wrist_r, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(lbl_wrist_r, &lv_font_montserrat_12, 0);
  lv_obj_align_to(lbl_wrist_r, sw_wrist_r_ptr, LV_ALIGN_OUT_TOP_MID, 0, -5);

  lv_obj_t *lbl_port = lv_label_create(rcard);
  lv_label_set_text(lbl_port, "Hand");
  lv_obj_set_style_text_color(lbl_port, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(lbl_port, &lv_font_montserrat_12, 0);
  lv_obj_align_to(lbl_port, sw_port_ptr, LV_ALIGN_OUT_TOP_MID, 0, -5);

  // ── CALLBACKS ────────────────────────────────────────────────────────────
  static auto _sw_wrist_l_cb = [](lv_event_t *e) {
    if (g_orient_cb_busy) return;
    g_last_landscape_rot = 3;
    update_orient_switches_state(3);
    display_port_set_rotation_locked(3); // Landscape Inverted
  };
  lv_obj_add_event_cb(sw_wrist_l_ptr, _sw_wrist_l_cb, LV_EVENT_VALUE_CHANGED, NULL);

  static auto _sw_wrist_r_cb = [](lv_event_t *e) {
    if (g_orient_cb_busy) return;
    g_last_landscape_rot = 1;
    update_orient_switches_state(1);
    display_port_set_rotation_locked(1); // Landscape
  };
  lv_obj_add_event_cb(sw_wrist_r_ptr, _sw_wrist_r_cb, LV_EVENT_VALUE_CHANGED, NULL);

  static auto _sw_port_cb = [](lv_event_t *e) {
    if (g_orient_cb_busy) return;
    update_orient_switches_state(0);
    display_port_set_rotation_locked(0); // Portrait
  };
  lv_obj_add_event_cb(sw_port_ptr, _sw_port_cb, LV_EVENT_VALUE_CHANGED, NULL);

  // ---- Action Buttons (Logout & Power Off) ----
  lv_obj_t *btn_row = lv_obj_create(scr_conn);
  conn_bottom_row_ptr = btn_row;
  lv_obj_set_size(btn_row, LV_PCT(100), 44);
  lv_obj_align(btn_row, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_row, 0, 0);
  lv_obj_set_style_pad_all(btn_row, 0, 0);
  lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *conn_logout_btn = lv_btn_create(btn_row);
  lv_obj_set_size(conn_logout_btn, 120, 42);
  lv_obj_align(conn_logout_btn, LV_ALIGN_LEFT_MID, 12, 0);
  lv_obj_set_style_bg_color(conn_logout_btn, COLOR_DANGER, 0);
  lv_obj_set_style_radius(conn_logout_btn, 12, 0);
  conn_logout_lbl_ptr = lv_label_create(conn_logout_btn);
  lv_label_set_text(conn_logout_lbl_ptr, LV_SYMBOL_POWER " LOGOUT");
  lv_obj_set_style_text_color(conn_logout_lbl_ptr, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(conn_logout_lbl_ptr, &lv_font_montserrat_14, 0);
  lv_obj_center(conn_logout_lbl_ptr);
  lv_obj_add_event_cb(conn_logout_btn, _logout_action_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *conn_power_btn = lv_btn_create(btn_row);
  lv_obj_set_size(conn_power_btn, 120, 42);
  lv_obj_align(conn_power_btn, LV_ALIGN_LEFT_MID, 144, 0);
  lv_obj_set_style_bg_color(conn_power_btn, lv_color_hex(0x880000), 0);
  lv_obj_set_style_border_color(conn_power_btn, COLOR_DANGER, 0);
  lv_obj_set_style_border_width(conn_power_btn, 1, 0);
  lv_obj_set_style_radius(conn_power_btn, 12, 0);
  lv_obj_t *conn_power_lbl = lv_label_create(conn_power_btn);
  lv_label_set_text(conn_power_lbl, LV_SYMBOL_POWER " POWER");
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
  lv_obj_clear_flag(scr_settings, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr_settings, COLOR_BG, 0);
  build_status_bar(scr_settings, "Settings");
  build_nav_rail(scr_settings, 4);
  settings_content_ptr = build_content_area(scr_settings);
  lv_obj_t *content = settings_content_ptr;
  // Keep scrollable so portrait mode can reach the buttons at the bottom
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(content, 6, 0);

  // Settings Title
  lv_obj_t *title = lv_label_create(content);
  lv_label_set_text(title, "Scanner Hardware Settings  [" FIRMWARE_VERSION "]");
  lv_obj_set_style_text_color(title, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_pad_bottom(title, 2, 0);

  // Helper macro for creating setting row
  auto make_row = [&](const char *label_text, lv_color_t color, int type) {
      lv_obj_t *row = lv_obj_create(content);
      lv_obj_set_size(row, LV_PCT(100), 32);
      lv_obj_set_style_bg_color(row, COLOR_CARD, 0);
      lv_obj_set_style_border_color(row, color, 0);
      lv_obj_set_style_border_width(row, 1, 0);
      lv_obj_set_style_radius(row, 6, 0);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t *lbl = lv_label_create(row);
      lv_label_set_text(lbl, label_text);
      lv_obj_set_style_text_color(lbl, color, 0);
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
      lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);

      lv_obj_t *sw = lv_switch_create(row);
      lv_obj_set_size(sw, 40, 20);
      lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -10, 0);
      lv_obj_set_style_bg_color(sw, color, (lv_style_selector_t)((int)LV_PART_INDICATOR | (int)LV_STATE_CHECKED));
      return sw;
  };

  lv_obj_t *sw_light = make_row("Light", COLOR_CYAN, 0);
  light_switch_ptr = sw_light;
  if (gm65_light_percent > 0) lv_obj_add_state(sw_light, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw_light, [](lv_event_t *e) {
      bool is_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
      gm65_set_lighting(is_on ? 100 : 0);
  }, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *sw_laser = make_row("Laser", COLOR_SAFFRON, 1);
  collim_switch_ptr = sw_laser;
  if (gm65_laser_percent > 0) lv_obj_add_state(sw_laser, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw_laser, [](lv_event_t *e) {
      bool is_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
      gm65_set_collimation(is_on ? 100 : 0);
  }, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *sw_1d = make_row("1D Scan", COLOR_NAVY_BLUE, 2);
  if (gm65_1d_enabled) lv_obj_add_state(sw_1d, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw_1d, [](lv_event_t *e) {
      bool is_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
      gm65_set_1d(is_on);
  }, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *sw_2d = make_row("2D Scan", COLOR_NAVY_BLUE, 3);
  if (gm65_2d_enabled) lv_obj_add_state(sw_2d, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw_2d, [](lv_event_t *e) {
      bool is_on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
      gm65_set_2d(is_on);
  }, LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *btn_reset = lv_btn_create(content);
  lv_obj_set_size(btn_reset, LV_PCT(100), 32);
  lv_obj_set_style_bg_color(btn_reset, COLOR_DANGER, 0);
  lv_obj_set_style_radius(btn_reset, 6, 0);
  lv_obj_add_event_cb(btn_reset, [](lv_event_t *e) {
      gm65_factory_reset();
  }, LV_EVENT_CLICKED, NULL);
  lv_obj_t *lbl_reset = lv_label_create(btn_reset);
  lv_label_set_text(lbl_reset, "Factory Reset Scanner");
  lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_14, 0);
  lv_obj_center(lbl_reset);

  // Small visual gap before action buttons
  lv_obj_t *spacer = lv_obj_create(content);
  lv_obj_set_size(spacer, LV_PCT(100), 6);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(spacer, 0, 0);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

  // Action Buttons Row — LOGOUT at LEFT, POWER OFF at RIGHT (pinned to bottom of scr_settings)
  lv_coord_t disp_w       = lv_disp_get_hor_res(NULL);
  lv_coord_t disp_h       = lv_disp_get_ver_res(NULL);
  bool       is_portrait  = (disp_w < 400);
  lv_coord_t row_h        = 56;
  lv_coord_t btn_h        = 42;
  lv_coord_t btn_w        = is_portrait ? 74 : 120;

  // Reduce content area height so it doesn't overlap the pinned bottom button row
  lv_obj_set_size(content, disp_w - 64, disp_h - 32 - row_h);

  lv_obj_t *s_btn_row = lv_obj_create(scr_settings);
  settings_btn_row_ptr = s_btn_row;
  lv_obj_set_size(s_btn_row, disp_w - 64, row_h);
  lv_obj_align(s_btn_row, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(s_btn_row, COLOR_BG, 0);
  lv_obj_set_style_border_color(s_btn_row, COLOR_CARD_BRD, 0);
  lv_obj_set_style_border_side(s_btn_row, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(s_btn_row, 1, 0);
  lv_obj_set_style_pad_all(s_btn_row, 0, 0);
  lv_obj_clear_flag(s_btn_row, LV_OBJ_FLAG_SCROLLABLE);

  // ── LOGOUT button — pinned to the LEFT ──────────────────────────────────
  lv_obj_t *logout_btn = lv_btn_create(s_btn_row);
  lv_obj_add_flag(logout_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(logout_btn, btn_w, btn_h);
  lv_obj_align(logout_btn, LV_ALIGN_LEFT_MID, is_portrait ? 6 : 12, 0);
  lv_obj_set_style_bg_color(logout_btn, COLOR_DANGER, 0);
  lv_obj_set_style_radius(logout_btn, 12, 0);
  lv_obj_t *logout_lbl = lv_label_create(logout_btn);
  lv_label_set_text(logout_lbl, is_portrait ? (LV_SYMBOL_POWER " OUT") : (LV_SYMBOL_POWER " LOGOUT"));
  lv_obj_set_style_text_color(logout_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(logout_lbl, is_portrait ? &lv_font_montserrat_12 : &lv_font_montserrat_14, 0);
  lv_obj_center(logout_lbl);
  lv_obj_add_event_cb(logout_btn, _logout_action_cb, LV_EVENT_CLICKED, NULL);

  // ── POWER OFF button — pinned alongside LOGOUT ──────────────────────────
  lv_obj_t *power_btn = lv_btn_create(s_btn_row);
  lv_obj_add_flag(power_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(power_btn, btn_w, btn_h);
  lv_obj_align(power_btn, LV_ALIGN_LEFT_MID, is_portrait ? 88 : 144, 0);
  lv_obj_set_style_bg_color(power_btn, lv_color_hex(0x880000), 0);
  lv_obj_set_style_border_color(power_btn, COLOR_DANGER, 0);
  lv_obj_set_style_border_width(power_btn, 1, 0);
  lv_obj_set_style_radius(power_btn, 12, 0);
  lv_obj_t *power_lbl = lv_label_create(power_btn);
  lv_label_set_text(power_lbl, is_portrait ? (LV_SYMBOL_POWER " OFF") : (LV_SYMBOL_POWER " POWER"));
  lv_obj_set_style_text_color(power_lbl, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(power_lbl, is_portrait ? &lv_font_montserrat_12 : &lv_font_montserrat_14, 0);
  lv_obj_center(power_lbl);
  lv_obj_add_event_cb(power_btn, _power_off_action_cb, LV_EVENT_CLICKED, NULL);
}

// ============================================================================
// G. LOGIN SCREEN — Left ID/Pass + Right Permanent Number Board
// ============================================================================
inline lv_obj_t *ta_active_login = NULL;
inline lv_obj_t *label_login_err = NULL;

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

static void splash_finish(void) {
  if (!scr_splash) return;
  if (!scr_login) return;
  if (splash_timer) {
    lv_timer_del(splash_timer);
    splash_timer = NULL;
  }
  if (global_status_bar_obj) lv_obj_clear_flag(global_status_bar_obj, LV_OBJ_FLAG_HIDDEN);
  if (global_status_stripe_obj) lv_obj_clear_flag(global_status_stripe_obj, LV_OBJ_FLAG_HIDDEN);
  _load_scr_direct(scr_login, "ScanPro X1");
  lv_event_send(scr_login, LV_EVENT_SIZE_CHANGED, NULL);
  lv_obj_del_async(scr_splash);
  scr_splash = NULL;
}

static void splash_timer_cb(lv_timer_t *timer) {
  splash_finish();
}

static void splash_click_cb(lv_event_t *e) {
  splash_finish();
}

inline void uiShowSplash() {
  if (scr_splash) return;
  scr_splash = lv_obj_create(NULL);
  lv_obj_clear_flag(scr_splash, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(scr_splash, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(scr_splash, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(scr_splash, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(scr_splash, 0, 0);
  lv_obj_set_style_outline_width(scr_splash, 0, 0);
  lv_obj_set_style_pad_all(scr_splash, 0, 0);
  lv_obj_set_style_radius(scr_splash, 0, 0);

  lv_obj_t *img = lv_img_create(scr_splash);
  lv_img_set_src(img, &splash_logo_img);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);

  // Tap anywhere on splash to skip directly to login
  lv_obj_add_event_cb(scr_splash, splash_click_cb, LV_EVENT_CLICKED, NULL);
  lv_scr_load(scr_splash);
}

static void build_splash_screen() {
  uiShowSplash();
}

static void build_login_screen() {
  scr_login = lv_obj_create(NULL);
  lv_obj_clear_flag(scr_login, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(scr_login, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(scr_login, lv_color_hex(0x040812), 0);
  lv_obj_set_style_bg_opa(scr_login, LV_OPA_COVER, 0);
  lv_obj_add_event_cb(scr_login, _global_screen_resize_cb, LV_EVENT_SIZE_CHANGED, NULL);

  // ── LEFT PANEL: Permanent Number Board (Keypad) ──────────────────────────
  lv_obj_t *left_panel = lv_obj_create(scr_login);
  login_left_panel = left_panel;
  lv_obj_set_size(left_panel, 226, 210);
  lv_obj_align(left_panel, LV_ALIGN_TOP_LEFT, 8, 48);
  lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x080F25), 0);
  lv_obj_set_style_bg_opa(left_panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(left_panel, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(left_panel, 1, 0);
  lv_obj_set_style_radius(left_panel, 10, 0);
  lv_obj_set_style_pad_all(left_panel, 6, 0);
  lv_obj_clear_flag(left_panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *num_hdr = lv_label_create(left_panel);
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

  lv_obj_t *btnm = lv_btnmatrix_create(left_panel);
  lv_btnmatrix_set_map(btnm, numpad_map);
  lv_obj_set_size(btnm, 210, 180);
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

  // ── RIGHT PANEL: ID & Password Inputs ─────────────────────────────────────
  lv_obj_t *right_panel = lv_obj_create(scr_login);
  login_right_panel = right_panel;
  lv_obj_set_size(right_panel, 230, 210);
  lv_obj_align(right_panel, LV_ALIGN_TOP_RIGHT, -8, 48);
  lv_obj_set_style_bg_color(right_panel, lv_color_hex(0x080F25), 0);
  lv_obj_set_style_bg_opa(right_panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(right_panel, COLOR_CYAN, 0);
  lv_obj_set_style_border_width(right_panel, 1, 0);
  lv_obj_set_style_radius(right_panel, 10, 0);
  lv_obj_set_style_pad_all(right_panel, 10, 0);
  lv_obj_clear_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

  // User ID field
  lv_obj_t *ul = lv_label_create(right_panel);
  lv_label_set_text(ul, "USER ID");
  lv_obj_set_style_text_color(ul, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(ul, &lv_font_montserrat_12, 0);
  lv_obj_align(ul, LV_ALIGN_TOP_LEFT, 0, 0);

  ta_login_user = _make_cyber_ta(right_panel);
  lv_textarea_set_placeholder_text(ta_login_user, "User ID");
  lv_textarea_set_text(ta_login_user, "1");
  lv_textarea_set_one_line(ta_login_user, true);
  lv_obj_set_size(ta_login_user, 208, 36);
  lv_obj_align(ta_login_user, LV_ALIGN_TOP_LEFT, 0, 16);
  lv_obj_add_event_cb(ta_login_user, _login_ta_event_cb, LV_EVENT_ALL, NULL);
  ta_active_login = ta_login_user;
  lv_obj_set_style_border_color(ta_login_user, COLOR_CYAN, 0);

  // Password field
  lv_obj_t *pl = lv_label_create(right_panel);
  lv_label_set_text(pl, "PASSWORD");
  lv_obj_set_style_text_color(pl, COLOR_MUTE, 0);
  lv_obj_set_style_text_font(pl, &lv_font_montserrat_12, 0);
  lv_obj_align(pl, LV_ALIGN_TOP_LEFT, 0, 56);

  ta_login_pass = _make_cyber_ta(right_panel);
  lv_textarea_set_placeholder_text(ta_login_pass, "Password");
  lv_textarea_set_password_mode(ta_login_pass, true);
  lv_textarea_set_one_line(ta_login_pass, true);
  lv_obj_set_size(ta_login_pass, 166, 36);
  lv_obj_align(ta_login_pass, LV_ALIGN_TOP_LEFT, 0, 72);
  lv_obj_add_event_cb(ta_login_pass, _login_ta_event_cb, LV_EVENT_ALL, NULL);

  // Eye toggle button
  lv_obj_t *eye_btn = lv_btn_create(right_panel);
  login_eye_btn = eye_btn;
  lv_obj_set_size(eye_btn, 38, 36);
  lv_obj_align(eye_btn, LV_ALIGN_TOP_LEFT, 170, 72);
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
  label_login_err = lv_label_create(right_panel);
  lv_label_set_text(label_login_err, "");
  lv_obj_set_style_text_color(label_login_err, COLOR_DANGER, 0);
  lv_obj_set_style_text_font(label_login_err, &lv_font_montserrat_12, 0);
  lv_obj_align(label_login_err, LV_ALIGN_TOP_LEFT, 0, 114);

  // QR Badge scan alternative
  lv_obj_t *qr_hint = lv_label_create(right_panel);
  lv_label_set_text(qr_hint, LV_SYMBOL_BARS "  Scan QR badge to login");
  lv_obj_set_style_text_color(qr_hint, COLOR_CYAN, 0);
  lv_obj_set_style_text_font(qr_hint, &lv_font_montserrat_12, 0);
  lv_label_set_long_mode(qr_hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(qr_hint, 208);
  lv_obj_align(qr_hint, LV_ALIGN_TOP_LEFT, 0, 134);
  label_login_qr_status = qr_hint;

  // Authenticate button
  lv_obj_t *btn_login = lv_btn_create(right_panel);
  login_btn_submit = btn_login;
  lv_obj_set_size(btn_login, 208, 36);
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
    ESP_LOGI("LOGIN", "Attempt: user='%s' pass='%s' pass_len=%d global_user_count=%d", user, pass, (int)strlen(pass), global_user_count);
    if (strlen(user) == 0) {
      if (label_login_err) lv_label_set_text(label_login_err, "! User ID is required");
      return;
    }
    bool found = false;
    bool auth_ok = false;
    std::string matched_id = "";
    std::string matched_name = "";
    std::string matched_role = "Warehouse Operator";
    
    // 1. Check registered users in local/server database
    for (int i = 0; i < global_user_count; i++) {
      ESP_LOGI("LOGIN", "  [%d] id='%s' pin='%s' pin_len=%d", i, global_users[i].id, global_users[i].pin, (int)strlen(global_users[i].pin));
      if (strcmp(global_users[i].id, user) == 0 || strcasecmp(global_users[i].name, user) == 0) {
        found = true;
        matched_id = global_users[i].id;
        matched_name = global_users[i].name;
        matched_role = global_users[i].role;
        ESP_LOGI("LOGIN", "  Found user '%s', stored_pin='%s', entered_pass='%s', match=%d", global_users[i].id, global_users[i].pin, pass, strcmp(global_users[i].pin, pass) == 0);
        if (strcmp(global_users[i].pin, pass) == 0) { 
          auth_ok = true; 
        }
        break;
      }
    }

    if (!found) {
      if (label_login_err) lv_label_set_text(label_login_err, "! User ID not registered");
      return;
    }

    if (!auth_ok) {
      if (label_login_err) lv_label_set_text(label_login_err, "! Incorrect PIN");
      return;
    }

    // Authenticated successfully for registered user
    is_logged_in = true;
    current_task_count = 0;
    memset(current_tasks, 0, sizeof(current_tasks));
    active_task = NULL;
    update_tasks_ui();

    strncpy(logged_in_user_id, matched_id.c_str(), sizeof(logged_in_user_id) - 1);
    logged_in_user_id[sizeof(logged_in_user_id) - 1] = '\0';
    strncpy(logged_in_user, matched_name.c_str(), sizeof(logged_in_user) - 1);
    logged_in_user[sizeof(logged_in_user) - 1] = '\0';
    strncpy(logged_in_user_role, matched_role.c_str(), sizeof(logged_in_user_role) - 1);
    logged_in_user_role[sizeof(logged_in_user_role) - 1] = '\0';

    if (label_home_user) {
      lv_label_set_text(label_home_user, logged_in_user);
    }

    publishDeviceStatus(true, logged_in_user_id);

    lv_textarea_set_text(ta_login_user, "");
    lv_textarea_set_text(ta_login_pass, "");
    lv_textarea_set_password_mode(ta_login_pass, true);
    if (label_login_err) lv_label_set_text(label_login_err, "");
    _load_scr_direct(scr_home, "Home");
  }, LV_EVENT_CLICKED, NULL);

  // Force initial layout alignment based on current screen rotation
  lv_event_send(scr_login, LV_EVENT_SIZE_CHANGED, NULL);
}

// ============================================================================
// Nav callbacks — direct 0ms load with no slide/animation
// ============================================================================
static void _load_scr_direct(lv_obj_t *target, const char *title) {
  if (!target) return;
  if (global_kb) {
    lv_obj_add_flag(global_kb, LV_OBJ_FLAG_HIDDEN);
  }
  if (floating_ptt_btn) {
    if (target == scr_login || !is_logged_in) {
      lv_obj_add_flag(floating_ptt_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(floating_ptt_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  lv_obj_invalidate(target);
  if (status_title_label) lv_label_set_text(status_title_label, title ? title : "");
  if (top_back_btn) {
    if (target == scr_conn && !is_logged_in) {
      lv_obj_clear_flag(top_back_btn, LV_OBJ_FLAG_HIDDEN);
      if (status_title_label) lv_obj_align(status_title_label, LV_ALIGN_LEFT_MID, 38, 0);
    } else {
      lv_obj_add_flag(top_back_btn, LV_OBJ_FLAG_HIDDEN);
      if (status_title_label) lv_obj_align(status_title_label, LV_ALIGN_LEFT_MID, 6, 0);
    }
  }

  if (top_setup_btn) {
    if (target == scr_conn || is_logged_in) {
      lv_obj_add_flag(top_setup_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(top_setup_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (top_logout_btn) {
    if (!is_logged_in) {
      lv_obj_add_flag(top_logout_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_clear_flag(top_logout_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }


  if (target == scr_conn) {
    if (is_logged_in) {
      if (conn_nav_rail_ptr) lv_obj_clear_flag(conn_nav_rail_ptr, LV_OBJ_FLAG_HIDDEN);
      if (conn_bottom_row_ptr) lv_obj_clear_flag(conn_bottom_row_ptr, LV_OBJ_FLAG_HIDDEN);
      if (conn_content_ptr) {
        lv_obj_set_size(conn_content_ptr, lv_disp_get_hor_res(NULL) - 64, lv_disp_get_ver_res(NULL) - 32);
        lv_obj_set_pos(conn_content_ptr, 64, 32);
      }
      if (hw_taskbar_ptr) lv_obj_clear_flag(hw_taskbar_ptr, LV_OBJ_FLAG_HIDDEN);
    } else {
      if (conn_nav_rail_ptr) lv_obj_add_flag(conn_nav_rail_ptr, LV_OBJ_FLAG_HIDDEN);
      if (conn_bottom_row_ptr) lv_obj_add_flag(conn_bottom_row_ptr, LV_OBJ_FLAG_HIDDEN);
      if (conn_content_ptr) {
        lv_obj_set_size(conn_content_ptr, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL) - 32);
        lv_obj_set_pos(conn_content_ptr, 0, 32);
      }
      if (hw_taskbar_ptr) lv_obj_add_flag(hw_taskbar_ptr, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void nav_login_cb(lv_event_t *e)      { _load_scr_direct(scr_login, ""); }
static void nav_home_cb(lv_event_t *e)       { if (!is_logged_in) { nav_login_cb(e); return; } _load_scr_direct(scr_home, "Home"); }
[[maybe_unused]] static void nav_scan_cb(lv_event_t *e)       { if (!is_logged_in) { nav_login_cb(e); return; } update_tasks_ui(); _load_scr_direct(scr_tasks, "Tasks"); }
static void nav_tasks_cb(lv_event_t *e)      { if (!is_logged_in) { nav_login_cb(e); return; } update_tasks_ui(); _load_scr_direct(scr_tasks, "Tasks"); }
static void nav_inventory_cb(lv_event_t *e)  { if (!is_logged_in) { nav_login_cb(e); return; } update_inventory_ui(); _load_scr_direct(scr_inventory, "Inventory"); }
static void nav_conn_cb(lv_event_t *e)       { if (!is_logged_in) { nav_login_cb(e); return; } _load_scr_direct(scr_conn, "Network"); }
static void nav_settings_cb(lv_event_t *e)   { if (!is_logged_in) { nav_login_cb(e); return; } _load_scr_direct(scr_settings, "Settings"); }
 

static void build_ota_screen() {
  scr_ota = lv_obj_create(NULL);
  lv_obj_clear_flag(scr_ota, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(scr_ota, COLOR_BG, 0);

  lv_obj_t *spinner = lv_spinner_create(scr_ota, 1000, 60);
  lv_obj_set_size(spinner, 60, 60);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -40);

  label_ota_status = lv_label_create(scr_ota);
  lv_label_set_text(label_ota_status, "Downloading Update...\nDo Not Turn Off!");
  lv_obj_set_style_text_align(label_ota_status, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(label_ota_status, COLOR_WHITE, 0);
  lv_obj_set_style_text_font(label_ota_status, &lv_font_montserrat_16, 0);
  lv_obj_align_to(label_ota_status, spinner, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

  bar_ota_progress = lv_bar_create(scr_ota);
  lv_obj_set_size(bar_ota_progress, 200, 20);
  lv_obj_align_to(bar_ota_progress, label_ota_status, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
  lv_bar_set_range(bar_ota_progress, 0, 100);
  lv_bar_set_value(bar_ota_progress, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar_ota_progress, COLOR_CYAN, LV_PART_INDICATOR);

  label_ota_progress = lv_label_create(scr_ota);
  lv_label_set_text(label_ota_progress, "0%");
  lv_obj_set_style_text_color(label_ota_progress, COLOR_CYAN, 0);
  lv_obj_align_to(label_ota_progress, bar_ota_progress, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
}

inline void ui_update_ota_progress(int percent) {
  if (bar_ota_progress) lv_bar_set_value(bar_ota_progress, percent, LV_ANIM_ON);
  if (label_ota_progress) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%d%%", percent);
      lv_label_set_text(label_ota_progress, buf);
  }
}

inline void ui_update_ota_status(const char *status_text) {
  if (label_ota_status && status_text) {
      lv_label_set_text(label_ota_status, status_text);
  }
}

// ============================================================================
// Public API (same function names as before, so smart_barcode_scanner.ino
// does not need to change)
// ============================================================================
inline void uiInit() {
  init_default_users();
  // Create the global persistent status bar FIRST so battery/wifi labels exist
  create_global_status_bar();

  // If splash screen is currently active, keep status bar and stripe hidden
  if (scr_splash) {
    if (global_status_bar_obj) lv_obj_add_flag(global_status_bar_obj, LV_OBJ_FLAG_HIDDEN);
    if (global_status_stripe_obj) lv_obj_add_flag(global_status_stripe_obj, LV_OBJ_FLAG_HIDDEN);
  }

  build_login_screen();
  build_home_screen();
  build_tasks_screen();
  build_scan_screen();
  build_inventory_screen();
  build_conn_screen();
  build_settings_screen();
  build_qty_adjust_screen();
  build_ota_screen();

  if (scr_splash) {
    if (!splash_timer) {
      splash_timer = lv_timer_create(splash_timer_cb, 2000, NULL);
      if (splash_timer) {
        lv_timer_set_repeat_count(splash_timer, 1);
      }
    }
  } else {
    _load_scr_direct(scr_login, "ScanPro X1");
  }
}

// Call this AFTER display rotation is applied and LVGL has had time to process it.
// This triggers a layout recalculation so all screens match the boot orientation.
inline void uiApplyBootOrientation() {
  lv_event_send(scr_login, LV_EVENT_SIZE_CHANGED, NULL);
  lv_event_send(scr_home, LV_EVENT_SIZE_CHANGED, NULL);
  lv_event_send(scr_tasks, LV_EVENT_SIZE_CHANGED, NULL);
  lv_event_send(scr_inventory, LV_EVENT_SIZE_CHANGED, NULL);
  lv_event_send(scr_conn, LV_EVENT_SIZE_CHANGED, NULL);
  lv_event_send(scr_settings, LV_EVENT_SIZE_CHANGED, NULL);

  // Re-populate dynamic screens so their child widgets are built at
  // the correct (post-rotation) size from the start.
  update_tasks_ui();

  // Rebuild settings rows so they fill the correct portrait width
  if (settings_content_ptr) {
    for (uint32_t k = 0; k < lv_obj_get_child_cnt(settings_content_ptr); k++) {
      lv_obj_t *row = lv_obj_get_child(settings_content_ptr, k);
      lv_obj_set_width(row, LV_PCT(100));
    }
    lv_obj_invalidate(settings_content_ptr);
  }

  // Reposition home screen arc and buttons for boot orientation
  if (home_content_ptr && home_arc_ptr && home_btn_container_ptr) {
    lv_coord_t w = lv_disp_get_hor_res(NULL);
    if (w < 400) {
      lv_obj_align(home_arc_ptr, LV_ALIGN_TOP_MID, 0, 10);
      lv_obj_align(home_btn_container_ptr, LV_ALIGN_TOP_MID, 0, 155);
      lv_obj_set_flex_flow(home_btn_container_ptr, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_flex_align(home_btn_container_ptr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      for (uint32_t k = 0; k < lv_obj_get_child_cnt(home_btn_container_ptr); k++) {
        lv_obj_t *btn = lv_obj_get_child(home_btn_container_ptr, k);
        lv_obj_set_size(btn, 180, 48); // nice wide buttons in portrait
      }
    } else {
      lv_obj_align(home_arc_ptr, LV_ALIGN_TOP_MID, 0, 25);
      lv_obj_align(home_btn_container_ptr, LV_ALIGN_TOP_MID, 0, 165);
      lv_obj_set_flex_flow(home_btn_container_ptr, LV_FLEX_FLOW_ROW_WRAP);
      lv_obj_set_flex_align(home_btn_container_ptr, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
      for (uint32_t k = 0; k < lv_obj_get_child_cnt(home_btn_container_ptr); k++) {
        lv_obj_t *btn = lv_obj_get_child(home_btn_container_ptr, k);
        lv_obj_set_size(btn, 130, 48);
      }
    }
    lv_obj_invalidate(home_content_ptr);
  }

  lv_disp_rot_t cur_rot = lv_disp_get_rotation(NULL);
  uint8_t rot_num = (cur_rot == LV_DISP_ROT_NONE) ? 0 : ((cur_rot == LV_DISP_ROT_270) ? 3 : 1);
  update_orient_switches_state(rot_num);
}

inline void ui_show_ota_screen() {
  _load_scr_direct(scr_ota, "OTA Update");
}

// NOTE: each screen currently creates its own status-bar label, and this
// function only updates the one built last (Settings). For a quick fix that
// updates the CURRENT screen's status text regardless of which screen is
// active, call this every time right after lv_scr_load() switches screens,
// or (cleaner) refactor to a single persistent top-layer status bar shared
// across all screens using lv_layer_top(). Left as-is here to keep this
// pass focused and easy to read; flag it if you want the persistent-bar
inline void uiSetBleConnected(bool connected) {
  if (connected) {
    uiSetWifiStatus(LV_SYMBOL_BLUETOOTH " BLE: Connected");
    if (label_ble_large_status) {
      char buf[128];
      snprintf(buf, sizeof(buf), "Status: Connected\n\nDevice: %s\n\nSuccessfully paired with phone.", bleGetDeviceName());
      lv_label_set_text(label_ble_large_status, buf);
    }
    if (label_conn_ble_detail) {
      lv_label_set_text(label_conn_ble_detail, "BLE Mode Active\nStatus: Connected");
    }
  } else {
    uiSetWifiStatus(LV_SYMBOL_BLUETOOTH " BLE: Adv...");
    if (label_ble_large_status) {
      char buf[128];
      snprintf(buf, sizeof(buf), "Status: Advertising...\n\nDevice: %s\n\nVisible to nearby phones via Bluetooth.", bleGetDeviceName());
      lv_label_set_text(label_ble_large_status, buf);
    }
    if (label_conn_ble_detail) {
      char buf[64];
      snprintf(buf, sizeof(buf), "BLE Mode Active\nDevice: %s", bleGetDeviceName());
      lv_label_set_text(label_conn_ble_detail, buf);
    }
  }
}

inline void uiSetWifiStatus(const std::string &text) {
  g_wifi_text_full = text;
  if (label_wifi_status) {
    if (lv_disp_get_hor_res(NULL) < 400) {
      // Extract the first symbol (UTF-8) for portrait mode
      std::string sym = "";
      if (text.length() >= 3) {
          sym = text.substr(0, 3); // usually LV_SYMBOL_* is 3 bytes
      }
      
      if (text.find("Disconn") != std::string::npos || text.find("Off") != std::string::npos) {
          sym += " Off";
      } else if (text.find("Connect") != std::string::npos || text.find("Adv") != std::string::npos) {
          sym += " Conn";
      } else {
          sym += " On";
      }
      
      lv_label_set_text(label_wifi_status, sym.c_str());
      lv_obj_align(label_wifi_status, LV_ALIGN_RIGHT_MID, -120, 0);
    } else {
      lv_label_set_text(label_wifi_status, text.c_str());
      lv_obj_align(label_wifi_status, LV_ALIGN_RIGHT_MID, -85, 0);
    }
    
    // Set color based on status text
    if (text.find("Disconn") != std::string::npos || text.find("Off") != std::string::npos) {
        lv_obj_set_style_text_color(label_wifi_status, COLOR_DANGER, 0);
    } else if (text.find("Connect") != std::string::npos || text.find("Adv") != std::string::npos) {
        lv_obj_set_style_text_color(label_wifi_status, COLOR_SAFFRON, 0);
    } else {
        lv_obj_set_style_text_color(label_wifi_status, COLOR_CYAN, 0);
    }
  }
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
    if (network_is_wifi_connected()) {
      std::string info = "SSID: network\nIP: 192.168.x.x";
      lv_label_set_text(label_conn_wifi_detail, info.c_str());
    } else {
      lv_label_set_text(label_conn_wifi_detail, "Status: Disconnected\n(Tap Connect WiFi)");
    }
  }

  if (false) {
    if (label_conn_ble_detail) lv_label_set_text(label_conn_ble_detail, "BLE Mode: Connected\nDevice: ScanPro-X1-BLE");
    if (label_ble_large_status) lv_label_set_text(label_ble_large_status, "Status: Connected!\n\nDevice: ScanPro-X1-BLE\n\nLive Bluetooth barcode streaming ready.");
  } else if (false) {
    if (label_conn_ble_detail) lv_label_set_text(label_conn_ble_detail, "BLE Mode: Advertising...\nDevice: ScanPro-X1-BLE");
    if (label_ble_large_status) lv_label_set_text(label_ble_large_status, "Status: Advertising...\n\nDevice: ScanPro-X1-BLE\n\nReady for client connection.");
  } else {
    if (label_conn_ble_detail) lv_label_set_text(label_conn_ble_detail, "WiFi Mode Active\n(BLE Inactive)");
  }
}

inline void uiShowScanResult(const std::string &sku) {
  current_scan_adjust_sku = sku;
  current_scan_adjust_qty = 1;

  std::string prod_name = "Scanned Item";
  for (int i = 0; i < global_inventory_count; i++) {
    if (std::string(global_inventory[i].sku) == sku) {
      prod_name = global_inventory[i].name;
      break;
    }
  }
  if (prod_name == "Scanned Item") {
    for (int t = 0; t < current_task_count; t++) {
      if (!is_task_assigned_to_current_user(current_tasks[t])) continue;
      for (int i = 0; i < current_tasks[t].item_count; i++) {
        if (std::string(current_tasks[t].items[i].sku) == sku) {
          prod_name = current_tasks[t].items[i].name;
          break;
        }
      }
    }
  }

  if (label_qty_name) lv_label_set_text(label_qty_name, prod_name.c_str());
  if (label_qty_sku) {
    std::string txt = "SKU: " + sku;
    lv_label_set_text(label_qty_sku, txt.c_str());
  }
  if (label_qty_val) lv_label_set_text(label_qty_val, "1");

  _load_scr_direct(scr_qty_adjust, "Quantity Adjust");
}

// ============================================================================
// QR Badge Login — called from loop() when a scan arrives and !is_logged_in
// QR format: "USER:<username>:<pin>"  e.g.  "USER:bhargav:1234"
// The embedded PIN is verified against DEFAULT_PASSWORD before granting access.
// ============================================================================
inline void uiLoginViaScan(const std::string &raw) {
  // ── 1. Validate prefix ────────────────────────────────────────────────────
  if (raw.find("USER:") != 0) {
    ESP_LOGI("UI", "[login] QR rejected (no USER: prefix): %s\n", raw.c_str());
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_CLOSE "  Invalid badge not a user QR");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_DANGER, 0);
    }
    if (label_login_err) lv_label_set_text(label_login_err, "! Scan a valid USER QR badge");
    return;
  }

  // ── 2. Parse "USER:<username>:<pin>" ────────────────────────────────────
  std::string remainder = raw.substr(5); // strip "USER:"
  
  int sep = remainder.find(':');
  if (sep < 0) {
    // Missing PIN separator
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_CLOSE "  Bad QR format — need USER:id:pin");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_DANGER, 0);
    }
    if (label_login_err) lv_label_set_text(label_login_err, "! QR format: USER:<id>:<pin>");
    ESP_LOGI("UI", "[login] QR format error (missing PIN): %s\n", raw.c_str());
    return;
  }

  std::string username = remainder.substr(0, sep);
  std::string pin      = remainder.substr(sep + 1);
  
  

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
  std::string matched_id = username;
  std::string matched_name = username;
  std::string matched_role = "Warehouse Operator";

  for (int i = 0; i < global_user_count; i++) {
    if (std::string(global_users[i].id) == username || strcasecmp(global_users[i].name, username.c_str()) == 0) {
      found = true;
      matched_id = global_users[i].id;
      matched_name = global_users[i].name;
      matched_role = global_users[i].role;
      if (std::string(global_users[i].pin) == pin) {
        auth_ok = true;
      }
      break;
    }
  }

  if (!found) {
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_CLOSE "  User ID not registered");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_DANGER, 0);
    }
    if (label_login_err) lv_label_set_text(label_login_err, "! User ID not registered");
    ESP_LOGI("UI", "[login] QR auth rejected — unregistered user='%s'\n", username.c_str());
    return;
  }

  if (!auth_ok) {
    if (label_login_qr_status) {
      lv_label_set_text(label_login_qr_status, LV_SYMBOL_CLOSE "  Incorrect PIN for user");
      lv_obj_set_style_text_color(label_login_qr_status, COLOR_DANGER, 0);
    }
    if (label_login_err) lv_label_set_text(label_login_err, "! Incorrect PIN");
    ESP_LOGI("UI", "[login] QR auth rejected — wrong PIN for user='%s'\n", username.c_str());
    return;
  }

  // ── 4. Login success ─────────────────────────────────────────────────────
  is_logged_in = true;
  current_task_count = 0;
  memset(current_tasks, 0, sizeof(current_tasks));
  active_task = NULL;
  update_tasks_ui();

  strncpy(logged_in_user_id, matched_id.c_str(), sizeof(logged_in_user_id) - 1);
  logged_in_user_id[sizeof(logged_in_user_id) - 1] = '\0';
  strncpy(logged_in_user, matched_name.c_str(), sizeof(logged_in_user) - 1);
  logged_in_user[sizeof(logged_in_user) - 1] = '\0';
  strncpy(logged_in_user_role, matched_role.c_str(), sizeof(logged_in_user_role) - 1);
  logged_in_user_role[sizeof(logged_in_user_role) - 1] = '\0';

  if (label_home_user) {
    lv_label_set_text(label_home_user, logged_in_user);
  }

  publishDeviceStatus(true, logged_in_user_id);

  // Clear keyboard fields
  if (ta_login_user) lv_textarea_set_text(ta_login_user, "");
  if (ta_login_pass) {
    lv_textarea_set_text(ta_login_pass, "");
    lv_textarea_set_password_mode(ta_login_pass, true);
  }
  if (label_login_err) lv_label_set_text(label_login_err, "");

  // Brief welcome flash (visible for ~300 ms before screen switches)
  if (label_login_qr_status) {
    std::string welcome = std::string(LV_SYMBOL_OK) + "  Welcome, " + username + "!";
    lv_label_set_text(label_login_qr_status, welcome.c_str());
    lv_obj_set_style_text_color(label_login_qr_status, COLOR_GREEN, 0);
  }

  ESP_LOGI("UI", "[login] QR login OK — user='%s' (id='%s')\n", logged_in_user, logged_in_user_id);

  _load_scr_direct(scr_home, "Home");
}
