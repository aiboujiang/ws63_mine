import sys, os, glob, re

task_files = glob.glob("/home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/App/Task/*.c")

for fp in task_files:
    with open(fp, 'r') as f:
        content = f.read()

    # Replace specific private lock functions with nothing, and their usages
    # They look like: static unsigned int ws63_xxx_lock(void) { return ws63_os_irq_lock(); }
    # and static void ws63_xxx_unlock(unsigned int irq_status) { ws63_os_irq_unlock(irq_status); }
    content = re.sub(r'static\s+unsigned\s+int\s+ws63_[a-zA-Z0-9_]+_lock\s*\(\s*void\s*\)\s*\{[^}]*ws63_os_irq_lock\s*\(\s*\);\s*\}', '', content)
    content = re.sub(r'static\s+void\s+ws63_[a-zA-Z0-9_]+_unlock\s*\(\s*unsigned\s+int\s+[a-zA-Z0-9_]+\s*\)\s*\{[^}]*ws63_os_irq_unlock\s*\([^)]+\);\s*\}', '', content)
    
    # Replace usages
    content = re.sub(r'ws63_[a-zA-Z0-9_]+_lock\s*\(\s*\)', 'WS63_FINAL_IRQ_LOCK()', content)
    content = re.sub(r'ws63_[a-zA-Z0-9_]+_unlock\s*\(\s*([^)]+)\s*\)', r'WS63_FINAL_IRQ_UNLOCK(\1)', content)
    
    # Refactor osal_printk( "[xxx] fmt"...) with WS63_LOG("xxx", fmt)
    # Be careful not to replace multi-line strings easily without parsing, but simple cases are fine.
    
    # Replace boilerplate task creation with ws63_task_create_with_queue
    # This might be too complex for a regex, so maybe just replace locks first.

    with open(fp, 'w') as f:
        f.write(content)

