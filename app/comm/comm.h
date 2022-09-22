/*
 * @Description: 蓝牙COMM通信，消息封装和解析
 * @Author: hubin
 * @LastEditors:
 * @Date: 2022-09-22
 * @LastEditTime:
 */

#ifndef COMM_H
#define COMM_H

#include "../common/ring.h"
#include "comm.pb.h"
#include "type.h"
#include "protocol.h"

/* 串口数据I/O方式，标准方式为ENCODE，DECODE内部阻塞方式读写;STRING方式为将数据转换成字符 */
#define COMM_PROTO_ENDECODE 0
#define COMM_PROTO_STRING   1
//#define COMM_IOTYPE             COMM_PROTO_ENDECODE
#define COMM_IOTYPE         COMM_PROTO_STRING

#define COMM_MSG_SIZE       8000

/**
 * @brief  发送命令数据，给蓝牙
 * @note
 * @param  fd: 串口设备文件号
 * @param  cmd: 命令
 * @param  target: 目标
 * @param  ack_Flag: 应答标志
 * @param  pData: 命令参数数据
 * @param  datalen: 命令参数数据长度
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Cmd(int fd, u8 session_id, u8 cmd, u8 target, u8 ack_Flag, u8 *pData, u16 datalen);

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
int COMM_Send_Data(int fd, u8 data_type, u8 session_id, u8 cmd, u8 target, u8 ack_Flag, u8 *pData, u16 datalen);

/**
 * @brief  发送更新通知指令，MCU->BT
 * @note
 * @param  pUpgradeMeta         [in]        待更新镜像头
 * @param  u8Len                [in]        镜像头长度，默认为48
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Upgrade_Meta_Cmd(u8 *pUpgradeMeta, u8 u8Len);

/**
 * @brief  发送数据
 * @note
 * @param  fd: 串口设备文件
 * @param  *pMsg: 待发送消息
 * @retval 0：成功，否则失败
 */
int COMM_send_msg(int fd, tVK_CMD_MSG *pMsg);

/**
 * @brief  打开通信串口
 * @note
 * @retval 打开的串口句柄，负数为失败
 */
int COMM_open_dev();

/**
 * @brief  读数据
 * @note
 * @param  fd: 串口设备文件号
 * @param  *pBuf: 接收缓区
 * @param  u16DataLen: 接收缓区长度
 * @retval 实际接收字节数
 */
int COMM_poll_msg(int fd, u8 *pBuf, u16 u16BufSize);

/**
 * @brief  解析环内数据，由于状态机的polling和parse分时转换，因此无需考虑同步
 * @note   注意是字符格式，要转换成HEX数据
 * @param  *pRing: 环指针
 * @retval 0：成功，否则错误代码
 */
int COMM_handle_msg_Ring(tRINGBUF *pRing);


#endif
