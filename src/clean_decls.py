import sys, os, glob, re

task_files = glob.glob("/home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/App/Task/*.c")

for fp in task_files:
    with open(fp, 'r', encoding='latin-1') as f:
        content = f.read()

    # Find the forward declarations that became `static unsigned int WS63_FINAL_IRQ_LOCK();`
    # Wait, the macro WS63_FINAL_IRQ_LOCK() doesn't have args. So `static unsigned int ws63_os_irq_lock();`
    content = re.sub(r'static\s+unsigned\s+int\s+ws63_os_irq_lock\s*\(\s*void\s*\)\s*;', '', content)
    content = re.sub(r'static\s+void\s+ws63_os_irq_unlock\s*\(\s*unsigned\s+int[^\)]*\)\s*;', '', content)
    
    # Also I need to make sure the original unused function `ws63_zw101_lock` wasn't left unreplaced for some reason.
    # Ah! `ws63_zw101_lock` had the name `ws63_zw101_lock` because my regex lock_func was only capturing ONE lock function! 
    # What if a file has multiple lock functions? e.g. `ws63_zw101_lock` and something else?
    # sensor_bridge.c might have zw101 and radar locks.

    with open(fp, 'w', encoding='latin-1') as f:
        f.write(content)

