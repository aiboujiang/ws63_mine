import sys, os, glob, re

task_files = glob.glob("/home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/App/Task/*.c")

for fp in task_files:
    with open(fp, 'rb') as f:
        content = f.read()

    lock_match = re.search(b'static\\s+unsigned\\s+int\\s+(ws63_[a-zA-Z0-9_]+_lock)\\s*\\(\\s*void\\s*\\)', content)
    unlock_match = re.search(b'static\\s+void\\s+(ws63_[a-zA-Z0-9_]+_unlock)\\s*\\(\\s*unsigned\\s+int', content)
    
    if lock_match and unlock_match:
        lock_func = lock_match.group(1)
        unlock_func = unlock_match.group(1)
        
        # Remove their definitions
        content = re.sub(b'static\\s+unsigned\\s+int\\s+' + lock_func + b'\\s*\\(\\s*void\\s*\\)\\s*\\{[^}]*ws63_os_irq_lock\\s*\\(\\s*\\);\\s*\\}', b'', content)
        content = re.sub(b'static\\s+void\\s+' + unlock_func + b'\\s*\\(\\s*unsigned\\s+int\\s+[a-zA-Z0-9_]+\\s*\\)\\s*\\{[^}]*ws63_os_irq_unlock\\s*\\([^)]+\\);\\s*\\}', b'', content)
        
        # Replace occurrences
        content = re.sub(lock_func + b'\\s*\\(\\s*\\)', b'WS63_FINAL_IRQ_LOCK()', content)
        content = re.sub(unlock_func + b'\\s*\\(\\s*([^)]+)\\s*\\)', b'WS63_FINAL_IRQ_UNLOCK(\\1)', content)
        
        with open(fp, 'wb') as f:
            f.write(content)
        print("Fixed " + fp)

