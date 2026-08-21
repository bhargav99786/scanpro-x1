import re

with open("main/ui_screens.h", "r") as f:
    code = f.read()

# Replace String with std::string
code = code.replace("String", "std::string")
code = code.replace("std::string(", "std::string(")
code = code.replace(".c_str()", ".c_str()")
code = code.replace(".length()", ".length()")

# Replace Serial.println and Serial.printf
code = re.sub(r'Serial\.println\((.*?)\);', r'ESP_LOGI("UI", "%s", \1);', code)
code = re.sub(r'Serial\.printf\((.*?)\);', r'ESP_LOGI("UI", \1);', code)

# Remove Arduino headers
code = code.replace('#include <Arduino.h>', '#include <string>\n#include "esp_log.h"')
code = code.replace('#include "pins_config.h"', '#include "pins_config.h"\n#include "display_port.h"')

# Replace millis()
code = code.replace('millis()', '(xTaskGetTickCount() * portTICK_PERIOD_MS)')

# Replace WiFi.localIP().toString()
code = code.replace('WiFi.localIP().toString()', 'std::string("192.168.x.x")')
code = code.replace('WiFi.macAddress()', 'std::string("XX:XX:XX:XX:XX:XX")')

# Replace delay()
code = code.replace('delay(', 'vTaskDelay(pdMS_TO_TICKS(')

with open("main/ui_screens.h", "w") as f:
    f.write(code)

