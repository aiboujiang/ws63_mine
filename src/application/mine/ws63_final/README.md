# WS63 Final Application

## 控制芯片替换
由于实际硬件中触控芯片为 VK36N16I，故将所有相关的 `TTP229` 驱动及 BSP 层配置全部替换为了 `VK36N16I`。

### 修改汇总：
- 文件名重命名：涉及所有的驱动 `Driver/ws63_vk36n16i.c/.h`、硬件 `BSP/ws63_final_bsp_vk36n16i.c`、任务 `App/Task/ws63_final_task_vk36n16i.c`。
- 代码内容中的 `TTP229`, `ttp229` 宏定义、函数名和引脚配置等一并完成替换。
- `CMakeLists.txt` 中的编译项同步更新。
