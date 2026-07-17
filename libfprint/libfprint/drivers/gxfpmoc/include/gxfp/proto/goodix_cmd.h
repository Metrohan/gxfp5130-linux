#pragma once

#define GXFP_CMD_SEND_NOP            0x01
#define GXFP_CMD_NOTIFY_POWER_STATE  0x0E
#define GXFP_CMD_RESET_DEVICE        0xA1
#define GXFP_CMD_READ_OTP            0xA6

#define GXFP_CMD_FDT_DOWN            0x32
#define GXFP_CMD_FDT_UP              0x34
#define GXFP_CMD_FDT_MODE            0x36
#define GXFP_CMD_FDT_STATUS          0xDA

#define GXFP_CMD_GET_IMAGE           0x20

#define GXFP_CMD_QUERY_MCU_STATE     0xAE
#define GXFP_CMD_TRIGGER_MCU_STATE   0xAF
#define GXFP_CMD_ACK                 0xB0
#define GXFP_CMD_UPLOAD_CONFIG_MCU   0x90

#define GXFP_CMD_REG_READ            0x82

#define GXFP_CMD_TLS_SERVER_INIT     0xD0
#define GXFP_CMD_D01                 0xD1
#define GXFP_CMD_SET_SLEEP_MODE      0xD2
#define GXFP_CMD_TLS_UNLOCK          0xD4
#define GXFP_CMD_TLS_UNLOCK_FORCE    0xD5

#define GXFP_CMD_PRODUCTION_WRITE_MCU 0xE0
#define GXFP_CMD_PRESET_PSK_READ      0xE4
