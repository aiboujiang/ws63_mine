#ifndef __LD2402_H__
#define __LD2402_H__

#include <stddef.h>

#include "LD2402_hal.h"

/* ==========================================
   1. 协议常量 (基于手册 V1.08)
   ========================================== */
#define LD2402_CMD_HEADER       0xFD, 0xFC, 0xFB, 0xFA
#define LD2402_CMD_TAIL         0x04, 0x03, 0x02, 0x01
#define LD2402_DATA_HEADER      0xF4, 0xF3, 0xF2, 0xF1
#define LD2402_DATA_TAIL        0xF8, 0xF7, 0xF6, 0xF5

#define LD2402_CMD_VERSION                  0x0000
#define LD2402_CMD_SET_PARAM                0x0007
#define LD2402_CMD_GET_PARAM                0x0008
#define LD2402_CMD_AUTO_THRESHOLD           0x0009
#define LD2402_CMD_AUTO_THRESHOLD_PROGRESS  0x000A
#define LD2402_CMD_READ_SN_CHAR             0x0011
#define LD2402_CMD_OUTPUT_MODE              0x0012
#define LD2402_CMD_AUTO_THRESHOLD_ALARM     0x0014
#define LD2402_CMD_READ_SN_HEX              0x0016
#define LD2402_CMD_AUTO_GAIN                0x00EE
#define LD2402_CMD_SAVE_PARAM               0x00FD
#define LD2402_CMD_END_CONFIG               0x00FE
#define LD2402_CMD_ENABLE_CONFIG            0x00FF

#define LD2402_PARAM_MAX_DIST       0x0001
#define LD2402_PARAM_DELAY_TIME     0x0004
#define LD2402_PARAM_POWER_INTER    0x0005
#define LD2402_PARAM_GATE_MOVE      0x0010
#define LD2402_PARAM_GATE_STATIC    0x0030
#define LD2402_PARAM_SAVE_FLAG      0x003F

#define LD2402_MODE_ENGINEERING     0x00000004
#define LD2402_MODE_NORMAL          0x00000064

#define LD2402_FRAME_BUFFER_SIZE    256
#define LD2402_CMD_TIMEOUT_MS       1000
#define LD2402_MAX_GATES            32

#define LD2402_ACK_CMD_OFFSET       0x0100
#define LD2402_ACK_STATUS_OK        0x0000

/* ==========================================
   2. 数据结构
   ========================================== */
typedef enum {
    LD2402_STATUS_NONE = 0,
    LD2402_STATUS_MOVE = 1,
    LD2402_STATUS_STATIC = 2
} LD2402_Status_t;

typedef struct {
    LD2402_Status_t status;
    uint16_t distance_cm;
    int32_t move_energy[LD2402_MAX_GATES];
} LD2402_DataFrame_t;

typedef struct {
    uint16_t cmd_id;
    uint16_t status;
    uint8_t *payload;
    uint16_t payload_len;
    bool is_valid;
} LD2402_Ack_t;

typedef struct {
    LD2402_HAL_t hal;

    uint8_t rx_buffer[LD2402_FRAME_BUFFER_SIZE];
    uint16_t rx_index;
    uint8_t state;
    uint16_t frame_len;
    uint8_t frame_type;

    volatile bool cmd_waiting;
    volatile uint16_t cmd_wait_expect_ack;
    volatile uint16_t cmd_wait_status;
    volatile bool cmd_done;

    volatile uint16_t cmd_last_ack_cmd;
    uint8_t cmd_last_payload[LD2402_FRAME_BUFFER_SIZE];
    volatile uint16_t cmd_last_payload_len;

    bool is_in_config_mode;

    void (*on_data_received)(LD2402_DataFrame_t *data);
    void (*on_log)(const char *fmt, ...);
} LD2402_Handle_t;

/* ==========================================
   3. API
   ========================================== */
void LD2402_Init(LD2402_Handle_t *handle, LD2402_HAL_t *hal);
void LD2402_InputByte(LD2402_Handle_t *handle, uint8_t byte);

int LD2402_SendCommand(LD2402_Handle_t *handle, uint16_t cmd, uint8_t *value, uint16_t v_len);

int LD2402_SetParam(LD2402_Handle_t *handle, uint16_t param_id, uint32_t value);
int LD2402_ReadParam(LD2402_Handle_t *handle, uint16_t param_id, uint32_t *value);

int LD2402_SetMaxDistance(LD2402_Handle_t *handle, float distance_m);
int LD2402_SetDisappearDelay(LD2402_Handle_t *handle, uint16_t seconds);
int LD2402_SetEngineeringMode(LD2402_Handle_t *handle);
int LD2402_SetNormalMode(LD2402_Handle_t *handle);
int LD2402_SaveParams(LD2402_Handle_t *handle);
int LD2402_AutoGainAdjust(LD2402_Handle_t *handle);

int LD2402_GetVersion(LD2402_Handle_t *handle, char *buf, uint16_t buf_len);
int LD2402_GetSN_Hex(LD2402_Handle_t *handle, uint8_t *buf, uint16_t buf_len);
int LD2402_GetSN_Char(LD2402_Handle_t *handle, char *buf, uint16_t buf_len);

void LD2402_EnterConfigMode(LD2402_Handle_t *handle);
void LD2402_ExitConfigMode(LD2402_Handle_t *handle);

#endif /* __LD2402_H__ */