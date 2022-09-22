/**
 * @file protocol.h
 * @brief MCU-蓝牙之间协议的结构体定义
 * @author
 * @version 0.1
 * @date 2022-09-21
 * @note
 * 1. 发送端发送数据后，要等待接收端应答，接收端根据协议需要决定何时应答。
 * 2. 应答数据要优先发送。
 * 4. retry次数暂定2次。
 *
 * @copyright Copyright (c) 2022
 */

#ifndef TBOX_PROTOCOL_H
#define TBOX_PROTOCOL_H

#include "comm.pb.h"
#include "type.h"
#include <ql_type.h>

#pragma pack(1)

/************************************* 数据包结构定义[start] ********************************************/
#define VK_PROTOCOL_HEAD_FIXED              0x55AA  /* 协议头起始字节,'##’ */

/* VIN码长度 */
#define VIN_LENGTH              17
/* 更新镜像头长度 */
#define UPGRADE_META_LENGTH     48

/**************************************
 *
 * 《MCU UART协议V1.16》数据包结构和定义
 *
***************************************/

/**
 * @brief 命令 / 应答共用消息头
 *
 */
typedef struct
{
    u16 u16Head;                    /* 起始符，固定为'0x55,0xAA' */
    u16 u16DataSize;                /* 数据单元的总字节数（小端）L=(L_high<<8)|L_low含CmdID和数据，不含数据校验 */
    u16 u16CheckSize;               /* 长度校验，-L_Low -L_High */
    u8  u8CmdID;                    /* 指令/响应码 */
} tVK_MSG_HEAD, *ptVK_MSG_HEAD;

/**
 * @brief 命令消息格式
 *
 */
typedef struct
{
    tVK_MSG_HEAD    tHead;          /* 命令头 */
    u8              *pData;         /* 命令内容 */
    u16             u16Checksum;    /* CRC_Low CRC_High，CRC16校验（小端），包含指令和数据使用CRC-16/MODBUS */
} tVK_CMD_MSG, *ptVK_CMD_MSG;

/**
 * @brief 应答消息格式
 *
 */
typedef struct
{
    tVK_MSG_HEAD    tHead;          /* 应答头 */
    u8              *pData;         /* 应答内容 */
    u16             u16Checksum;    /* CRC_Low CRC_High，CRC16校验（小端），包含指令和数据使用CRC-16/MODBUS */
} tVK_ACK_MSG, *ptVK_ACK_MSG;

/************************************* 数据包结构和定义[end] ********************************************/

/**
 * @brief 时间定义
 * 时间采用GMT+8时间
 */
typedef struct
{
    u8 u8Year;      /* 年，0-99 */
    u8 u8Month;     /* 月，1-12 */
    u8 u8Day;       /* 日，1-31 */
    u8 u8Hour;      /* 时，0-23 */
    u8 u8Minute;    /* 分，0-59 */
    u8 u8Second;    /* 秒，0-59 */
} tTIME;

/**
 * 1.MCU=>BT消息协议定义
 * 以下定义MCU发送给蓝牙模块的消息格式。
 *
 */
/**
 * @brief 1.1 更新通知（0x04）
 *
 */
typedef struct
{
    u8    u8UpgradeMeta[48];          /* 待更新镜像头（48字节） */
} tVKDATA_UPGRADE_NOTIFY;

/**
 * @brief 1.2 传输更新数据（0x03）
 *
 */
typedef struct
{
    u16     u16SerialNum;       /* 数据序列号 */
    u8      *pu8UpgradeData;    /* 更新数据，长度需要小于等于256字节限制，且为8字节的整数倍。 */
} tVKDATA_TRANS_UPGRADE_DATA;

/**
 * @brief 1.3 获取产线数据指令（0x05）
 *
 */
typedef struct
{
} tVKDATA_PRODUCTION_DATA;

/**
 * @brief 1.4 控车指令（0x08）
 *
 */
typedef struct
{
    /* TODO */
} tVKDATA_VEHICLE_CONTROL;

/**
 * @brief 1.5 定位报告（0x09）
 *
 */
typedef struct
{
    u8      u8OptRequest;   /* PEPS操作请求，0xD1 : 左前门PE  0xD2: 右前门PE  0xD3: 后备箱PE  0xD4: PS */
    u8      u8DoorStatus;   /* 车门状态，一个字节，每个bit代表一个车门状态分别为第1个bit左前门开关状态，第二个bit左前门门锁状态，第三个bit是否为PEPS四门PE操作，0表示闭锁/关门/四门PE操作，1表示开锁/开门/非四门PE操作 */
} tVKDATA_LOCATION_REPORT;

/**
 * @brief 1.6 写入VIN码（0x0A）
 *
 */
typedef struct
{
    u8      u8VIN[17];   /* 整车VIN码 */
} tVKDATA_INPUT_VIN;

/**
 * @brief 1.7 重启（0x0B）
 *
 */
typedef struct
{
} tVKDATA_REBOOT;

/**
 * @brief 1.8 获取蓝牙芯片工作状态（0x0D）
 *
 */
typedef struct
{
} tVKDATA_GET_BT_STATUS;

/**
 * @brief 1.9 上报车门状态（0x0F）
 *
 */
typedef struct
{
    u8      u8LockStatus : 1;       /* 车锁状态 */
    u8      u8ACCStatus : 1;        /* 车辆上电状态 */
    u8      u8FLWindow : 1;         /* 前左车窗 */
    u8      u8FRWindow : 1;         /* 前右车窗 */
    u8      u8RLWindow : 1;         /* 后左车窗 */
    u8      u8RRWindow : 1;         /* 后右车窗 */
    u8      u8Trunk : 1;            /* 后备箱状态 */
    u8      u8Reserve1 : 1;
    u8      u8RemoteDriveStatus;    /* 遥控驾驶可用状态， */
    u8      u8ResHintMsg;           /* 反馈提示语 */
    u8      u8EnableCall : 3;       /* 召唤使能 */
    u8      u8Reserve2 : 1;
    u8      u8TurnTo : 2;           /* 左右转向 */
    u8      u8Reserve3 : 1;
} tVKDATA_REPORT_VEHICLE_STATUS;

/**
 * @brief 1.A MCU休眠通知（0x10）
 *
 */
typedef struct
{
} tVKDATA_MCU_SLEPT;

/**
 * @brief 1.B 获取版本号（0x12）
 *
 */
typedef struct
{
    u8      u8BTModel;              /* 蓝牙模块，00 表示获取主模块版本信息， 01表示获取左门把手从模块版本信息，02表示获取右门把手模块版本信息， 03表示获取后备箱从模块版本信息 */
    u8      u8Copyright;            /* 版本信息，00 表示序列号，  01表示生产日期， 02表示硬件版本号， 03表示软件版本号， 04表示零件号， 05表示蓝牙MAC地址， 06表示生产商代码 */
} tVKDATA_GET_COPYRIGHT;

/**
 * @brief 1.C 无感控车（0x35）
 *
 */
typedef struct
{
    u8      u8ControlCmd;           /* 车控指令，00 表示迎宾， 01表示自动解锁，02表示自动闭锁 */
} tVKDATA_CONTROL_PS;

// 恢复默认对齐字节数
#pragma pack()

#endif
