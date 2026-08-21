import re

with open("main/ui_screens.h", "r") as f:
    code = f.read()

# Fix the broken variables
code = code.replace('ta_""', 'ta_wifi_ssid')
code = code.replace('ta_password', 'ta_wifi_password') # Wait, did it replace wifi_password in ta_wifi_password?
code = code.replace('ta_wifi_pass', 'ta_wifi_pass') # Wait, the variable was ta_wifi_pass!

# Let's see what the original code for ta_wifi_pass was.
# Actually I replaced "wifi_password" with '""'.
# And "wifi_ssid" with '""'.

code = code.replace('lv_textarea_set_text(ta_wifi_ssid, "");', 'lv_textarea_set_text(ta_wifi_ssid, "waveshare");')
code = code.replace('lv_textarea_set_text(ta_wifi_pass, "");', 'lv_textarea_set_text(ta_wifi_pass, "12345678");')

with open("main/ui_screens.h", "w") as f:
    f.write(code)

