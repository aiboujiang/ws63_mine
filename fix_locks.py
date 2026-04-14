import sys, os, glob, re

task_files = glob.glob("/home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/App/Task/*.c")

for fp in task_files:
    with open(fp, 'r', encoding='latin-1') as f:
        content = f.read()

    lock_match = re.search(r'static\s+unsigned\s+int\s+(ws63_[a-zA-Z0-9_]+_lock)\s*\(\s*void\s*\)', content)
    unlock_match = re.search(r'static\s+void\s+(ws63_[a-zA-Z0-9_]+_unlock)\s*\(\s*unsigned\s+int', content)
    
    if lock_match and unlock_match:
        lock_func = lock_match.group(1)
        unlock_func = unlock_match.group(1)
        
        # Replace occurrences FIRST (so definitions are replaced then wiped out... wait better to replace definitions first)
        content = re.sub(r'static\s+unsigned\s+int\s+' + lock_func + r'\s*\(\s*void\s*\)\s*\{[^}]*ws63_os_irq_lock\s*\(\s*\);\s*\}', '', content)
        content = re.sub(r'static\s+void\s+' + unlock_func + r'\s*\(\s*unsigned\s+int\s+[a-zA-Z0-9_]+\s*\)\s*\{[^}]*ws63_os_irq_unlock\s*\([^)]+\);\s*\}', '', content)
        
        # Replace occurrences
        content = re.sub(lock_func + r'\s*\(\s*\)', 'WS63_FINAL_IRQ_LOCK()', content)
        # Note the usage is like ws63_ttp229_unlock(irq_status) 
        content = re.sub(unlock_func + r'\s*\(\s*([^)]+)\s*\)', r'WS63_FINAL_IRQ_UNLOCK(\1)', content)
        
        with open(fp, 'w', encoding='latin-1') as f:
            f.write(content)
        print("Fixed " + fp)

