with open("main/ui_screens.h", "r") as f:
    code = f.read()

code = code.replace(".startsWith", ".starts_with")
code = code.replace(".substring", ".substr")
# wait, if starts_with doesn't exist in older C++ (ESP-IDF uses gnu++2b in v5.4 so it might exist, but just in case)
code = code.replace(".starts_with(\"USER:\")", ".find(\"USER:\") == 0")

with open("main/ui_screens.h", "w") as f:
    f.write(code)

