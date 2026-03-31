/**
 * @file ld2402.c
 * @brief LD2402 雷达模块驱动逻辑。
 *
 * 根据《HLK-LD2402用户手册 V1.08》:
 * - 波特率 115200
 * - 使能配置命令 (0x00FF), 结束配置命令 (0x00FE) 来确认通信正常。
 */
#include "ld2402.h"
#include "wk2114.h"
#include "ws63_final_osal.h"
#include "osal_debug.h"

// 使能配置命令: 帧头(4)+帧内长(2)+字(2)+值(2)+帧尾(4)
static const uint8_t g_ld2402_cmd_enable[] = {
    0xFD, 0xFC, 0xFB, 0xFA, 
    0x04, 0x00, 
    0xFF, 0x00, 
    0x01, 0x00, 
    0x04, 0x03, 0x02, 0x01
};

// 结束配置命令: 帧头(4)+帧内长(2)+字(2)+帧尾(4)
static const uint8_t g_ld2402_cmd_disable[] = {
    0xFD, 0xFC, 0xFB, 0xFA, 
    0x02, 0x00, 
    0xFE, 0x00, 
    0x04, 0x03, 0x02, 0x01
};

/**
 * @brief 初始化 ld2402 模块并校验通信。
 * 通过发送"使能配置命令"并读取响应来测试握手。
 */
errcode_t ld2402_init(uint8_t sub_port)
{
    uint8_t rx_buf[64] = {0};
    uint8_t len = 0;
    uint8_t retry = 3;
    errcode_t ret = ERRCODE_FAIL;

    osal_printk("[ld2402] init on port %u\r\n", sub_port);

    while (retry-- > 0) {
        // 清空接收缓存
        while (wk2114_subport_read(sub_port, rx_buf, sizeof(rx_buf)) > 0) {
            ws63_os_sleep_ms(2);
        }

        // 发送使能配置
        wk2114_subport_write(sub_port, g_ld2402_cmd_enable, sizeof(g_ld2402_cmd_enable));
        
        // 等待响应延迟
        ws63_os_sleep_ms(50);
        
        // 读取响应
        len = wk2114_subport_read(sub_port, rx_buf, sizeof(rx_buf));
        if (len >= 8 && rx_buf[0] == 0xFD && rx_buf[1] == 0xFC && rx_buf[2] == 0xFB && rx_buf[3] == 0xFA) {
            osal_printk("[ld2402] Enable Config ACK received, communication OK.\r\n");
            
            // 通信正常后发送结束配置，恢复工作模式
            wk2114_subport_write(sub_port, g_ld2402_cmd_disable, sizeof(g_ld2402_cmd_disable));
            ws63_os_sleep_ms(20);
            
            ret = ERRCODE_SUCC;
            break;
        }
        osal_printk("[ld2402] init retry...\r\n");
        ws63_os_sleep_ms(100);
    }

    if (ret != ERRCODE_SUCC) {
        osal_printk("[ld2402] init test failed.\r\n");
    }

    return ret;
}

/**
 * @brief 处理 ld2402 雷达上报数据
 */
void ld2402_process_data(uint8_t sub_port, const uint8_t *data, uint16_t len)
{
    (void)sub_port;
    if (len > 0) {
        osal_printk("LD2402 processing %u bytes.\r\n", (unsigned int)len);
        /* logic to parse ld2402 radar packet */
    }
}
