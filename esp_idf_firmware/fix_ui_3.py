import re

with open("main/ui_screens.h", "r") as f:
    code = f.read()

# Replace std::string(var) with std::to_string(var) if var is scan_counter or scan_target
code = code.replace("std::string(scan_counter)", "std::to_string(scan_counter)")
code = code.replace("std::string(scan_target)", "std::to_string(scan_target)")

# In case there are others like std::string(int)
code = re.sub(r'std::string\(([0-9]+)\)', r'std::to_string(\1)', code)

with open("main/ui_screens.h", "w") as f:
    f.write(code)

