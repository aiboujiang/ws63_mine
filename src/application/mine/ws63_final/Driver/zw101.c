/**
 * @file zw101.c
 * @brief ZW101 指纹模组驱动逻辑。
 *
 * 根据《指纹模组产品用户手册_V1.5.1》:
 * - 默认波特率通常为115200或其他(视配置而定)
 * - 握手指令 PS_HandShake (0x35)
 */
#include "zw101.h"
#include "wk2114.h"
#include "ws63_final_osal.h"
#include "osal_debug.h"

/* 清空接收缓存时的最大轮询次数，防止 RFCNT 读回异常导致死循环。 */
#define ZW101_DRAIN_MAX_ROUND 32U

// 握手指令: 包头(2)+地址(4)+标识(1)+长(2)+指令(1)+校验(2)
// EF 01 FF FF FF FF 01 00 03 35 00 39
static const uint8_t g_zw101_cmd_handshake[] = {
    0xEF, 0x01, 
    0xFF, 0xFF, 0xFF, 0xFF, 
    0x01, 
    0x00, 0x03, 
    0x35, 
    0x00, 0x39
};

/**
 * @brief 初始化 zw101 模块并校验通信。
 * 通过发送"握手指令(0x35)"并读取响应确认模组正常工作。
 */
errcode_t zw101_init(uint8_t sub_port)
{
    uint8_t rx_buf[64] = {0};
    uint8_t len = 0;
    uint8_t retry = 3;
    errcode_t ret = ERRCODE_FAIL;

    osal_printk("[zw101] init on port %u\r\n", sub_port);

    while (retry-- > 0) {
        uint8_t drain_round;

        // 清空接收缓存
        for (drain_round = 0U; drain_round < ZW101_DRAIN_MAX_ROUND; drain_round++) {
            if (wk2114_subport_read(sub_port, rx_buf, sizeof(rx_buf)) == 0U) {
                break;
            }
            ws63_os_sleep_ms(2);
        }

        if (drain_round >= ZW101_DRAIN_MAX_ROUND) {
            osal_printk("[zw101] drain rx hit limit, continue init.\r\n");
        }

        // 发送握手指令
        wk2114_subport_write(sub_port, g_zw101_cmd_handshake, sizeof(g_zw101_cmd_handshake));
        
        // 等待响应延迟
        ws63_os_sleep_ms(100);
        
        // 读取响应 (应答包包含 EF 01 FF FF FF FF 07 ... 等确认码代码)
        len = wk2114_subport_read(sub_port, rx_buf, sizeof(rx_buf));
        if (len >= 12 && rx_buf[0] == 0xEF && rx_buf[1] == 0x01) {
            osal_printk("[zw101] Handshake ACK received, communication OK.\r\n");
            
            // 确认确认码(确认字位置一般在第9字节，0表示成功)
            if (rx_buf[9] == 0x00) {
                osal_printk("[zw101] Module is ready.\r\n");
                ret = ERRCODE_SUCC;
                break;
            } else {
                osal_printk("[zw101] Module error code: 0x%02X\r\n", rx_buf[9]);
                // 可以视为成功通信但外设内部状态异常，或者判定为初始化失败。根据需求处理。
            }
        }
        osal_printk("[zw101] init retry...\r\n");
        ws63_os_sleep_ms(200);
    }

    if (ret != ERRCODE_SUCC) {
        osal_printk("[zw101] init test failed.\r\n");
    }

    return ret;
}

/**
 * @brief 处理 zw101 原始数据逻辑
 */
void zw101_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    (void)sub_port;
    /* 当前版本先保留接口，未解析载荷时需显式消除未使用参数告警。 */
    (void)data;
    if (len > 0) {
        osal_printk("ZW101 processing %u bytes.\r\n", (unsigned int)len);
        /* logic to parse zw101 fingerprint packet */
    }
}
