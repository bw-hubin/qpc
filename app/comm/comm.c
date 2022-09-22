/**
 * @file comm.c
 * @author
 * @brief 蓝牙与MCU串口通信
 * @version 0.1
 * @date 2022-09-20
 * @note
 *
 * @copyright Copyright (c) 2022
 *
 */


#include "comm.h"

#include <assert.h>
#include <model/bluetooth.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <ql_type.h>

#include "comm.pb.h"
#include "qpc.h"
#include "uart.h"

/** 是否使用设备板上串口，定义则用 */
// #define BOARD_COMM

/**
 * @brief  发送命令数据，给蓝牙
 * @note
 * @param  fd: 蓝牙串口设备文件号
 * @param  cmd: 命令
 * @param  target: 目标
 * @param  ack_Flag: 应答标志
 * @param  pData: 命令参数数据
 * @param  datalen: 命令参数数据长度
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Cmd(int fd, u8 session_id, u8 cmd, u8 target, u8 ack_Flag, u8 *pData, u16 datalen)
{
    /* 定义蓝牙命令消息 */
    tVK_CMD_MSG msg;

    /* 初始化 msg */
    /* TODO */

    return COMM_send_msg(fd, &msg);
}

/**
 * @brief  发送更新通知指令，MCU->BT
 * @note
 * @param  pUpgradeMeta         [in]        待更新镜像头
 * @param  u8Len                [in]        镜像头长度，默认为48
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Upgrade_Meta_Cmd(u8 *pUpgradeMeta, u8 u8Len)
{
    /* TODO */
    /*
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
    assert(pe != NULL);

    if (pe)
    {
        pe->task_id   = 0xFF;
        pe->data_type = eDATAType_CMD_PARAM;
        pe->cmd_id    = PROTOCOL_CMD_GB_VEHICLE_LOGIN;
        pe->target    = eECUType_TBOX;
        pe->ack_flag  = 0xFE;
        pe->data_len  = sizeof(tSYSTIME);
        pe->data      = (u8 *)ptTime;

        QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
    }
    */

    return 0;
}

/**
 * @brief  发送命令接收确认应答数据包，MCU->BT
 * @note
 * @param  session_id: SESSION号
 * @param  u8Cmd: u8Cmd: 命令字
 * @param  ack_flag: 应答标志
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Ack_Echo(u16 session_id, u8 u8Cmd, u8 ack_flag)
{
    /* CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
    assert(pe != NULL);

    if (pe)
    {
        pe->task_id   = session_id;
        pe->data_type = eDATAType_ECHO;
        pe->cmd_id    = u8Cmd;
        pe->target    = eECUType_TBOX;
        pe->ack_flag  = ack_flag;
        pe->data_len  = 0;
        pe->data      = NULL;

        QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
    } */

    return 0;
}

/**
 * @brief  串口发送数据统一入口
 * @note
 * @param  fd: 串口设备文件号
 * @param  type: 数据类型，必须为预定义的几种类型之一
 * @param  session_id: SESSION号
 * @param  cmd: 命令
 * @param  target: 目标地址
 * @param  ack_Flag: 应答标志
 * @param  pData: 数据内容，不同类型消息的数据内容结构不一样
 * @param  datalen: 数据长度
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Data(int fd, u8 data_type, u8 session_id, u8 cmd, u8 target, u8 ack_Flag, u8 *pData, u16 datalen)
{
    tVK_CMD_MSG msg;

    /* 保存会话上下文标识 */
    msg.tHead.u8CmdID = cmd;

    switch (data_type)
    {
        default:
            break;
    }

    return COMM_send_msg(fd, &msg);
}

/**
 * @brief  打开通信串口
 * @note
 * @retval 打开的串口句柄，负数为失败
 */
int COMM_open_dev()
{
#ifndef BOARD_COMM
    char dev[] = "/dev/ttyUSB0";
    int  fd;

    printf(" opening  %s \n", dev);
    fd = UART_connectTTY(dev, 115200, 8, 'N', '1');
    if (fd < 0)
        printf("COMM open error. \n");

    return fd;
#else

    /* 打开4G模块串口 */
    int fd = Serialcomm_Open();
    return fd;
#endif
}

/**
 * @brief  读数据
 * @note
 * @param  fd: 串口设备文件号
 * @param  *pBuf: 接收缓区
 * @param  u16DataLen: 接收缓区长度
 * @retval 实际接收字节数
 */
int COMM_poll_msg(int fd, u8 *pBuf, u16 u16BufSize)
{
    if ((fd < 0) || (NULL == pBuf))
    {
        return -1;
    }

#ifndef BOARD_COMM
    /* 调试机串口读取 */
    int nread = read(fd, pBuf, u16BufSize);
    return nread;
#else
    /* 板上串口读取 */
    int ret = Serialcomm_ReadData(fd, pBuf, u16BufSize);

    return ret;
#endif
}

/**
 * @brief  解析环内数据，由于状态机的polling和parse分时转换，因此无需考虑同步
 * @note   注意是字符格式，要转换成HEX数据
 * @param  *pRing: 环指针
 * @retval 0：成功，否则错误代码
 */
int COMM_handle_msg_Ring(tRINGBUF *pRing)
{
    assert(NULL != pRing);
    if (NULL == pRing)
    {
        return -1;
    }

    static u8 msgData[COMM_MSG_SIZE];

    /* 从环提取帧 */
    int len = RING_Retrieve_Msg(pRing, msgData, COMM_MSG_SIZE);
    if (len <= 0)
    {
        printf("[BT] retrieving msg, ret = %d \n", len);
        return len; /* error or empty */
    }

    tVK_CMD_MSG tMsg;
    /* 赋值给tMsg */
    /* TODO */

    return COMM_Parse_Msg(&tMsg);
}

/**
 * @brief  解包并处理
 * @note   可能的几类消息：1）应答消息，对MCU侧命令的应答
 *                        2）主动上报消息，蓝牙侧周期性上报消息
 * @param  *pMsg:
 * @retval
 */
int COMM_Parse_Msg(tVK_CMD_MSG *pMsg)
{
    assert(NULL != pMsg);
    if (NULL == pMsg)
    {
        return -1;
    }

    /* 单帧应答包或命令包 */
    /* TODO */

    return 0;
}

/**
 * @brief  发送数据
 * @note
 * @param  fd: 串口设备文件
 * @param  *pMsg: 待发送消息
 * @retval 0：成功，否则失败
 */
int COMM_send_msg(int fd, tVK_CMD_MSG *pMsg)
{
    assert((fd > 0) && (NULL != pMsg));

    if ((fd < 0) || (NULL == pMsg))
    {
        return -1;
    }

    /* 直接发送数据 */
#ifndef BOARD_COMM
    int ret = UART_sendData(fd, pMsg, sizeof(tVK_CMD_MSG));
    return ret;
#else
    /* 板上串口发送 */
    PrintHex_v2(1, LOG_TAG, "send data: ", buf, u16WrittenBytes);

    int ret = Serialcomm_SendDataAsString(fd, buf, u16WrittenBytes);
    return ret;
#endif

}

/**
 * @brief  关闭通信串口
 * @note
 * @param  fd: 串口设备文件
 * @retval None
 */
void COMM_close_dev(int fd)
{
#ifndef BOARD_COMM
    if (fd > 0)
    {
        close(fd);
    }

#else
    Serialcomm_Close(fd);
#endif
}