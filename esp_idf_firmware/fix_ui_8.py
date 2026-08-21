import re

with open("main/ui_screens.h", "r") as f:
    code = f.read()

# Fix saveWifiCredentials and wifiConnect
code = code.replace("saveWifiCredentials(s, p);", "// saveWifiCredentials(s, p);")
code = code.replace("wifiConnect(8000);", "// wifiConnect(8000);")

# Fix saveScreenRotation
code = code.replace("saveScreenRotation(is_right ? 3 : 1);", "// saveScreenRotation(is_right ? 3 : 1);")
code = code.replace("saveScreenRotation(0);", "// saveScreenRotation(0);")

# Fix the unqualified-id before 'else' at line 757
# This happened because my previous script removed the 'if (bleIsAdvertising())' logic completely!
# Let's just fix the whole _wifi_save_connect_btn_cb function using regex.
def replace_cb(match):
    return "static void _wifi_save_connect_btn_cb(lv_event_t *e) {\n    _load_scr_direct(scr_connecting, \"Connecting...\");\n}"
code = re.sub(r'static void _wifi_save_connect_btn_cb\(lv_event_t \*e\) \{.*?\}', replace_cb, code, flags=re.DOTALL)

with open("main/ui_screens.h", "w") as f:
    f.write(code)
