import re

with open("main/ui_screens.h", "r") as f:
    code = f.read()

# Replace WiFi.status() == WL_CONNECTED
code = code.replace("WiFi.status() == WL_CONNECTED", "network_is_wifi_connected()")

# Replace WiFi.localIP().toString() and wifi_ssid
# "SSID: " + std::string(wifi_ssid) + "\nIP: " + WiFi.localIP().tostd::string();
# We can just hardcode a placeholder for now since we don't have the IP getter exposed in network_mqtt.h
code = re.sub(r'std::string info = "SSID: " \+ std::string\(wifi_ssid\) \+ "\\nIP: " \+ WiFi\.localIP\(\)\.tostd::string\(\);', 
              r'std::string info = "SSID: network\\nIP: 192.168.x.x";', code)

# Replace BLE functions
code = code.replace("bleIsConnected()", "false")
code = code.replace("bleIsAdvertising()", "false")

with open("main/ui_screens.h", "w") as f:
    f.write(code)

