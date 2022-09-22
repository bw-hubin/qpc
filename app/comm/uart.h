/*
 * @Description: 串口读写
 * @Author: zhengdgao
 * @LastEditors: Please set LastEditors
 * @Date: 2019-02-20 23:32:10
 *
 * @LastEditTime: 2022-09-20
 * @LastEditors: hubin
 */

#ifndef UART_H
#define UART_H

#include <stdio.h>

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

int UART_sendData(int fd, char *buf, int bufSize);

#endif