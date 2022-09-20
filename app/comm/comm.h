/*
 * @Description: COMM通信，消息封装和解析
 * @Author: zhengdgao
 * @LastEditors: Please set LastEditors
 * @Date: 2019-02-22 23:10:18
 * @LastEditTime: 2022-09-14 17:45:48
 */

#ifndef COMM_H
#define COMM_H

#include "../common/ring.h"
#include "comm.pb.h"
#include "helpfunc.h"
#include "type.h"


/* 串口数据I/O方式，标准方式为ENCODE，DECODE内部阻塞方式读写;STRING方式为将数据转换成字符 */
#define COMM_PROTO_ENDECODE 0
#define COMM_PROTO_STRING   1
//#define COMM_IOTYPE             COMM_PROTO_ENDECODE
#define COMM_IOTYPE         COMM_PROTO_STRING


#define COMM_MSG_SIZE       8000
#define OTA_FILE_BLOCK_SIZE 1000

// int submit_cmd(u8 cmd, u8 target, u8 *pParamData, u16 u16ParamDataLen);


/**
 * @brief  发送命令数据，给串口
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
 * @brief  发送文件信息
 * @note
 * @param  fd: 串口设备文件号
 * @param  cmd: 命令
 * @param  target: 目标
 * @param  ack_Flag: 应答标志
 * @param  file_name: 文件名称
 * @param  file_size: 文件大小
 * @param  block_size: 约定块大小
 * @param  *version_hw: 硬件版本号
 * @param  version_sw: 软件版本号
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_FileInfo(int fd, u8 session_id, u8 cmd, u8 target, u8 ack_Flag, char *file_name, u32 file_size, u16 block_size, char *version_hw, char *version_sw);

/**
 * @brief  发送升级文件元信息
 * @note
 * @param  *pFileInfo: 升级文件元信息结构
 * @retval
 */
int COMM_Send_MCUFileInfo(DATA_FileInfo *pFileInfo);

/**
 * @brief  发送分块数据
 * @note
 * @param  fd: 设备文件号
 * @param  cmd: 命令字
 * @param  target: 目标
 * @param  ack_Flag: 应答标志
 * @param  block_amount: 分块总数
 * @param  block_id: 当前块序号
 * @param  pData: 当前块数据
 * @param  data_len: 当前块数据长度
 * @retval 0：成功，否则错误代码
 */
int COMM_send_BlockData(int fd, u8 session_id, u8 cmd, u8 target, u8 ack_Flag, u16 block_amount, u16 block_id, u8 *pData, u16 data_len);

/**
 * @brief  发送升级数据块，CPU->MCU
 * @note
 * @param  *pBlockData: 块数据结构体
 * @retval 0=成功，否则错误代码
 */
int COMM_send_MCUBlockData(DATA_Block *pBlockData);

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
 * @brief  发送注册数据请求，CPU->MCU，MCU应答包应为实际注册包
 * @note
 * @param  *ptTime: 系统时间
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_RegistorRequest(tSYSTIME *ptTime);


/**
 * @brief  发送命令接收确认应答数据包，MCU->CPU
 * @note
 * @param  session_id: SESSION号
 * @param  u8Cmd: 命令字
 * @param  ack_flag: 应答标志
 * @param  has_errcode: 是否有错误代码
 * @param  errcode: 错误代码
 * @param  has_blockid: 是否有块序号
 * @param  blockid: 块序号
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Echo(u8 session_id, u8 u8Cmd, u8 ack_flag, u8 has_errcode, u8 errcode, u8 has_blockid, u8 blockid);


/**
 * @brief  发送数据
 * @note
 * @param  fd: 串口设备文件
 * @param  *pMsg: 待发送消息
 * @retval 0：成功，否则失败
 */
int COMM_send_msg(int fd, COMM_Msg *pMsg);

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
 * @brief  提取串口数据，proto标准方式
 * @note
 * @param  fd: 串口设备句柄
 * @retval 0：成功，否则失败
 */
int COMM_poll_msg_proto(int fd);


/**
 * @brief  读取串口数据，非阻塞
 * @note
 * @param  fd: 串口设备文件
 * @param  *pBuf: 接收缓区
 * @param  u16BufSize: 接收缓区大小
 * @param  u16ReadBytes: 待读取字节数
 * @retval 实际读取字节数，或错误代码
 */
int COMM_poll_msg_Ring(int fd, tRINGBUF *pRing);

/**
 * @brief  解析环内数据，由于状态机的polling和parse分时转换，因此无需考虑同步
 * @note   注意是字符格式，要转换成HEX数据
 * @param  *pRing: 环指针
 * @retval 0：成功，否则错误代码
 */
int COMM_handle_msg_Ring(tRINGBUF *pRing);


/**
 * @brief  发送串口自检指令
 * @note
 * @param  fd: fd: 串口设备文件号
 * @retval
 */
int COMM_send_test_msg(int fd);


#endif
