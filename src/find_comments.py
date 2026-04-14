import sys, os, glob
for filepath in glob.glob("/home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/App/Task/*.c"):
    with open(filepath, 'rb') as f:
        lines = f.readlines()
    count = 0
    start = 0
    for i, _line in enumerate(lines):
        line_str = ''
        try:
            line_str = _line.decode('utf-8', errors='ignore').strip()
        except:
            pass
        if line_str.startswith("//"):
            if count == 0: start = i+1
            count += 1
        else:
            if count > 10:
                print(f"{filepath}: lines {start}-{i} ({count} lines)")
            count = 0
