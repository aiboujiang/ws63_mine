#!/usr/bin/env bash
set -euo pipefail

# 编译前 menuconfig 体检脚本：
# 1) 校验 ws63-liteos-app 主配置文件是否存在；
# 2) 根据 profile 检查关键开关是否开启；
# 3) 在 auto 模式下根据当前配置自动判定检查路径。
#
# 用法：
#   bash tools/check_ws63_menuconfig.sh                 # auto 检查
#   bash tools/check_ws63_menuconfig.sh ws63_final      # final 框架检查
#   bash tools/check_ws63_menuconfig.sh wk2114_zw101    # WK2114+ZW101 测试检查

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CFG_FILE="$ROOT_DIR/src/build/config/target_config/ws63/menuconfig/acore/ws63_liteos_app.config"
PROFILE="${1:-auto}"

if [[ ! -f "$CFG_FILE" ]]; then
    echo "[menuconfig-check] 配置文件不存在: $CFG_FILE"
    echo "[menuconfig-check] 请先执行: cd src && python3 build.py -c ws63-liteos-app menuconfig"
    exit 1
fi

is_enabled()
{
    local key="$1"
    grep -q "^${key}=y$" "$CFG_FILE"
}

final_layered="n"
sle_core="n"
old_sle_slave="n"
wk2114_ext="n"

if is_enabled "CONFIG_MINE_SUPPORT_WS63_FINAL_LAYERED"; then
    final_layered="y"
fi

if is_enabled "CONFIG_MINE_WS63_FINAL_SLE_SLAVE_CORE"; then
    sle_core="y"
fi

if is_enabled "CONFIG_MINE_SUPPORT_SLE_UART_SLAVE_DEMO"; then
    old_sle_slave="y"
fi

if is_enabled "CONFIG_MINE_SUPPORT_UART2_EXT_WK2114"; then
    wk2114_ext="y"
fi

if [[ "$PROFILE" == "auto" ]]; then
    if [[ "$wk2114_ext" == "y" ]]; then
        PROFILE="wk2114_zw101"
    elif [[ "$final_layered" == "y" ]]; then
        PROFILE="ws63_final"
    else
        echo "[menuconfig-check] 错误: auto 模式下未识别到可用 profile（既未开启 WK2114，也未开启 ws63_final）。"
        exit 2
    fi
fi

echo "[menuconfig-check] PROFILE=$PROFILE"
echo "[menuconfig-check] FINAL_LAYERED=$final_layered"
echo "[menuconfig-check] SLE_CORE=$sle_core"
echo "[menuconfig-check] WK2114_EXT=$wk2114_ext"
echo "[menuconfig-check] OLD_SLE_SLAVE_DEMO=$old_sle_slave"

case "$PROFILE" in
    ws63_final)
        if [[ "$final_layered" != "y" ]]; then
            echo "[menuconfig-check] 错误: 未启用 CONFIG_MINE_SUPPORT_WS63_FINAL_LAYERED。"
            exit 3
        fi

        if [[ "$sle_core" != "y" ]]; then
            echo "[menuconfig-check] 错误: 未启用 CONFIG_MINE_WS63_FINAL_SLE_SLAVE_CORE。"
            exit 4
        fi

        if [[ "$old_sle_slave" == "y" ]]; then
            echo "[menuconfig-check] 错误: 旧从机 demo 与 final 同时开启，存在双入口冲突风险。"
            exit 5
        fi
        ;;

    wk2114_zw101)
        if [[ "$wk2114_ext" != "y" ]]; then
            echo "[menuconfig-check] 错误: 未启用 CONFIG_MINE_SUPPORT_UART2_EXT_WK2114。"
            exit 6
        fi

        # wk2114 测试建议关闭 final 框架，避免多入口任务并行导致联调结果不稳定。
        if [[ "$final_layered" == "y" ]]; then
            echo "[menuconfig-check] 错误: WK2114_ZW101 测试模式下请关闭 CONFIG_MINE_SUPPORT_WS63_FINAL_LAYERED。"
            exit 7
        fi
        ;;

    *)
        echo "[menuconfig-check] 错误: 不支持的 profile '$PROFILE'。"
        echo "[menuconfig-check] 支持: auto | ws63_final | wk2114_zw101"
        exit 8
        ;;
esac

echo "[menuconfig-check] 通过: menuconfig 关键项正确。"
