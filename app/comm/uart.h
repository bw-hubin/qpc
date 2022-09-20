/*
 * @Description: 串口读写
 * @Author: zhengdgao
 * @LastEditors: Please set LastEditors
 * @Date: 2019-02-20 23:32:10
 * @LastEditTime: 2019-02-21 00:18:46
 */

#ifndef UART_H
#define UART_H

#include "type.h"


/**
 * @brief  打开串口设备
 * @note
 * @param  dev: 串口设备描述符
 * @param  baudrate: 波特率
 * @param  dataBits: 数据位
 * @param  parity: 校验位
 * @param  stop: 停止位
 * @retval 0：成功，否则错误代码
 */
int UART_connectTTY(char *dev, int baudrate, int dataBits, char parity, char stop);

/**
 * @brief  关闭串口设备
 * @note
 * @param  fd: 设备文件
 * @retval None
 */
void UART_disconnectTTY(int fd);


/**
 * @brief  读串口数据
 * @note
 * @param  fd: 设备文件
 * @param  buf: 接收缓区
 * @param  bufSize: 缓区长度
 * @retval 实际接收的字节数
 */
int UART_readData(int fd, char *buf, int bufSize);


/**
 * @brief  发送HEX数据，字符方式发送
 * @note
 * @param  fd: 设备文件
 * @param  *hexBuf: 数据缓区
 * @param  hexDataLen: 数据长度
 * @retval
 */
int UART_send_rawdata(int fd, unsigned char *hexBuf, int hexDataLen);


/**
 * @brief  发送原始字节流，先将十六进制数据转换成字符再发送，发送完成后再发送一个“帧结束”字符‘\n’
 * @note
 * @param  fd: 串口设备文件
 * @param  buf: 发送数据，HEX
 * @param  len: 发送数据长度
 * @param  charDelay: 延迟
 * @retval 错误代码，0：成功
 */
int UART_sendDataAsString(int fd, unsigned char *buf, int len, unsigned int charDelay);


/**
 * @brief  发送定字节数据
 * @note
 * @param  fd: 设备文件号
 * @param  *vptr: 发送数据缓区
 * @param  n: 发送数据长度
 * @retval 剩余发送数据长度
 */
ssize_t safe_write(int fd, const void *vptr, size_t n);


/**
 * @brief  读指定字节数据
 * @note
 * @param  fd: 设备文件号
 * @param  *vptr: 数据接收缓区
 * @param  n: 读取长度
 * @retval 实际读取长度
 */
ssize_t safe_read(int fd, void *vptr, size_t n);


#endif