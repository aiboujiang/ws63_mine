import sys, os, glob, re

task_files = glob.glob("/home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/App/Task/*.c")

for fp in task_files:
    with open(fp, 'rb') as f:
        content = f.read()

    # Remove orphan Doxygen comments for locks
    content = re.sub(b'/\\*\\*\\s*\\* @brief [^\r\n]*状态锁.*?\\*/\\s*', b'', content)
    
    with open(fp, 'wb') as f:
        f.write(content)

