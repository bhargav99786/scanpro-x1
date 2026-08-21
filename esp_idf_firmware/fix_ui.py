import re

with open("main/ui_screens.h", "r") as f:
    code = f.read()

# Fix indexOf
code = code.replace(".indexOf", ".find")

# Fix trim (std::string doesn't have trim, we can just remove it or write a quick lambda)
code = re.sub(r'([a-zA-Z0-9_]+)\.trim\(\);', r'', code)

# Fix publishDeviceStatus -> network_set_ptt doesn't replace it, but maybe we can just comment it out
code = code.replace("publishDeviceStatus(true, logged_in_user);", "// network_publish_status(...)")

# Remove _ble_toggle_btn_cb completely
code = re.sub(r'static void _ble_toggle_btn_cb\(lv_event_t \*e\) \{.*?\}', '', code, flags=re.DOTALL)

# Remove nav_settings_cb completely
code = re.sub(r'static void nav_settings_cb\(lv_event_t \*e\)   \{.*?\}', '', code)
code = code.replace("nav_settings_cb", "NULL")

with open("main/ui_screens.h", "w") as f:
    f.write(code)

