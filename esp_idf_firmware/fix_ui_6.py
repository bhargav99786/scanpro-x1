import re

with open("main/ui_screens.h", "r") as f:
    code = f.read()

# Replace WiFi.disconnect and BLE
code = code.replace("WiFi.disconnect(true);", "// WiFi.disconnect(true);")
code = code.replace("bleStartAdvertising();", "// bleStartAdvertising();")
code = code.replace("bleStopAdvertising();", "// bleStopAdvertising();")
code = code.replace("wifiStartAsync();", "// wifiStartAsync();")

# Replace wifi_ssid and wifi_password
code = code.replace("wifi_ssid", '""')
code = code.replace("wifi_password", '""')

# Replace current_rotation
code = code.replace("if (current_rotation == 3)", "if (false)")

with open("main/ui_screens.h", "w") as f:
    f.write(code)

