import os

filepath = "/home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/README.md"
with open(filepath, 'rb') as f:
    raw = f.read()

content = raw.decode('utf-8', errors='replace')
new_content = content.replace("TTP229", "VK36N16I")
new_content = new_content.replace("ttp229", "vk36n16i")
new_content = new_content.replace("Ttp229", "Vk36n16i")

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(new_content)
print("Updated README.md")
