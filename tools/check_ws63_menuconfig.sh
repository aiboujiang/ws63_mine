#!/usr/bin/env bash
set -euo pipefail

# 编译前 menuconfig 体检脚本：
# 1) 校验 ws63-liteos-app 主配置文件是否存在；
# 2) 校验 final 分层与 SLE 核心桥接关键开关；
# 3) 阻止旧 sle 从机 demo 与 final 框架同时开启，避免双入口冲突。

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CFG_FILE="$ROOT_DIR/src/build/config/target_config/ws63/menuconfig/acore/ws63_liteos_app.config"

if [[ ! -f "$CFG_FILE" ]]; then
    echo "[menuconfig-check] 配置文件不存在: $CFG_FILE"
    echo "[menuconfig-check] 请先执行: cd src && python3 build.py -c ws63-liteos-app menuconfig"
    exit 1
fi

# 读取开关状态，默认按未设置处理。
final_layered="n"
sle_core="n"
old_sle_slave="n"

if grep -q '^CONFIG_MINE_SUPPORT_WS63_FINAL_LAYERED=y$' "$CFG_FILE"; then
    final_layered="y"
fi

if grep -q '^CONFIG_MINE_WS63_FINAL_SLE_SLAVE_CORE=y$' "$CFG_FILE"; then
    sle_core="y"
fi

if grep -q '^CONFIG_MINE_SUPPORT_SLE_UART_SLAVE_DEMO=y$' "$CFG_FILE"; then
    old_sle_slave="y"
fi

echo "[menuconfig-check] FINAL_LAYERED=$final_layered"
echo "[menuconfig-check] SLE_CORE=$sle_core"
echo "[menuconfig-check] OLD_SLE_SLAVE_DEMO=$old_sle_slave"

if [[ "$final_layered" != "y" ]]; then
    echo "[menuconfig-check] 错误: 未启用 CONFIG_MINE_SUPPORT_WS63_FINAL_LAYERED。"
    exit 2
fi

if [[ "$sle_core" != "y" ]]; then
    echo "[menuconfig-check] 错误: 未启用 CONFIG_MINE_WS63_FINAL_SLE_SLAVE_CORE。"
    exit 3
fi

if [[ "$old_sle_slave" == "y" ]]; then
    echo "[menuconfig-check] 错误: 旧从机 demo 与 final 同时开启，存在双入口冲突风险。"
    exit 4
fi

echo "[menuconfig-check] 通过: menuconfig 关键项正确。"
