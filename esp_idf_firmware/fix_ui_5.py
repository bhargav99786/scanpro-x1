import re

with open("main/ui_screens.h", "r") as f:
    code = f.read()

# Fix the hanging _load_scr_direct
code = code.replace('_load_scr_direct(scr_settings, "Settings"); }', '')

# Fix lv_font_montserrat_12 -> lv_font_montserrat_14
code = code.replace("lv_font_montserrat_12", "lv_font_montserrat_14")

# Add externs for currentTasks if not present
if "WarehouseTask currentTasks" not in code:
    externs = """
struct WarehouseTask {
    char id[16];
    char item_name[32];
    char sku[16];
    int req_qty;
    int picked_qty;
};
extern WarehouseTask currentTasks[10];
extern int current_tasks_count;
"""
    # Insert right after #include "display_port.h"
    code = code.replace('#include "display_port.h"', '#include "display_port.h"\n' + externs)

with open("main/ui_screens.h", "w") as f:
    f.write(code)

