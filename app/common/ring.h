/*
 * @Description: 环形缓区
 * @Author: zhengdgao
 * @LastEditors: Please set LastEditors
 * @Date: 2019-03-03 16:08:13
 * @LastEditTime: 2019-03-03 17:41:10
 */

#ifndef RING_H
#define RING_H

#include "type.h"
#include <ql_type.h>

// #define RING_MALLOC
#define RING_STATIC

#define RING_SIZE 50000

/* 环形缓区结构体定义 */
typedef struct _tRINGBUF
{
    u8  buf[RING_SIZE]; /* 缓区 */
    u16 rd;             /* 写指针 */
    u16 wr;             /* 读指针 */
    u16 nBytesLeft;     /* 有效数据长度 */

} tRINGBUF;


/* 计算环中当前可提取字节数 */
#define RING_GET_LEFT_BYTES(rd, wr, size) (((wr) >= (rd)) ? ((wr) - (rd)) : ((size) - (rd) + (wr)))

/** 计算环中当前空闲字节数 */
#define RING_GET_FREE_BYTES(rd, wr, size) ((size) - (RING_GET_LEFT_BYTES(rd, wr, size)))

#define RING_WRAP_AHEAD(ptr, ring_size) \
    do                                  \
    {                                   \
        (ptr)++;                        \
        if ((ptr) >= (ring_size))       \
        {                               \
            (ptr) = 0;                  \
        }                               \
    } while (0);


#ifdef RING_MALLOC
/**
 * @brief  创建一个环
 * @note   对于linux系统，为malloc动态创建;而对于MCU，为静态预分配。
 * @retval
 */
tRINGBUF *RING_Create(void);
#endif

/**
 * @brief  环初始化
 * @note
 * @param  ptRing: 环
 * @retval None
 */
void RING_Init(tRINGBUF *ptRing);

/**
 * @brief  还复位，重新初始化
 * @note
 * @param  ptRing: 环
 * @retval None
 */
void RING_Reset(tRINGBUF *ptRing);

#ifdef RING_MALLOC

/**
 * @brief  环销毁
 * @note
 * @param  ptRing: 环
 * @retval None
 */
void RING_Destroy(tRINGBUF *ptRing);

#endif // DEBUG

/**
 * @brief  附加一段数据到环
 * @note
 * @param  *ptRing: 环
 * @param  *pData: 待插入数据
 * @param  u16DataLen: 待插入数据长度
 * @retval 0：成功，否则错误代码
 */
int RING_Attach_Data(tRINGBUF *ptRing, u8 *pData, u16 u16DataLen);


/**
 * @brief  从环中提取指定长度数据
 * @note
 * @param  *ptRing: 环
 * @param  *pBuf: 接收缓区
 * @param  u16BufSize:接收换取长度
 * @param  u16PollingBytes: 待提取字节数
 * @retval 返回实际提取字节数
 */
int RING_Poll_Data(tRINGBUF *ptRing, u8 *pBuf, u16 u16PollingBytes);


/**
 * @brief  从环中提取完整帧，以’\0‘结束
 * @note
 * @param  *pRing: 环
 * @param  *pBuf: 接收缓区
 * @param  u16BufSize: 接收缓区最大长度
 * @retval 帧，字符串，’\0‘结尾
 */
int RING_Retrieve_Msg(tRINGBUF *pRing, u8 *pBuf, u16 u16BufSize);


#endif // !RING_H
