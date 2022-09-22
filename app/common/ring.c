
#include "ring.h"

#include <assert.h>
#ifdef RING_MALLOC
#    include <malloc.h>
#endif
#include <stdio.h>
#include <string.h>

#ifdef RING_MALLOC
/**
 * @brief  创建一个环
 * @note   对于linux系统，为malloc动态创建;而对于MCU，为静态预分配。
 * @retval
 */
tRINGBUF *RING_Create(void)
{
    tRINGBUF *ptRing = NULL;
    ptRing           = (tRINGBUF *)malloc(sizeof(tRINGBUF));
    if (ptRing)
    {
        memset(ptRing->buf, 0, RING_SIZE);
        ptRing->rd         = 0;
        ptRing->wr         = 0;
        ptRing->nBytesLeft = 0;
    }

    return ptRing;
}
#endif

/**
 * @brief  环初始化
 * @note
 * @param  ptRing: 环
 * @retval None
 */
void RING_Init(tRINGBUF *ptRing)
{
    if (ptRing)
    {
        memset(ptRing->buf, 0, RING_SIZE);
        ptRing->rd         = 0;
        ptRing->wr         = 0;
        ptRing->nBytesLeft = 0;
    }
}

/**
 * @brief  环复位，重新初始化
 * @note
 * @param  ptRing: 环
 * @retval None
 */
void RING_Reset(tRINGBUF *ptRing)
{
    if (ptRing)
    {
        memset(ptRing->buf, 0, RING_SIZE);
        ptRing->rd         = 0;
        ptRing->wr         = 0;
        ptRing->nBytesLeft = 0;
    }
}

#ifdef RING_MALLOC
/**
 * @brief  环销毁
 * @note
 * @param  ptRing: 环
 * @retval None
 */
void RING_Destroy(tRINGBUF *ptRing)
{
    if (ptRing)
    {
        free(ptRing);
        ptRing = NULL;
    }
}
#endif

/**
 * @brief  附加一段数据到环
 * @note   不允许数据覆盖
 *
 * @param  *ptRing: 环
 * @param  *pData: 待插入数据
 * @param  u16DataLen: 待插入数据长度
 * @retval 0：成功，否则错误代码
 */
int RING_Attach_Data(tRINGBUF *ptRing, u8 *pData, u16 u16DataLen)
{
    assert(NULL != ptRing);
    assert(NULL != pData);
    if ((NULL == ptRing) || (NULL == pData))
    {
        return -1;
    }

    /**
     * 计算剩余空间是否够存放，不够则报错，不允许覆盖
     * 实际运行时不允许出现此种情况，否则会触发断言错误，程序退出
     */
    u16 nBytesFree = RING_GET_FREE_BYTES(ptRing->rd, ptRing->wr, RING_SIZE);
    if (nBytesFree < u16DataLen)
    {
        printf("[RING] too many data to insert. data_in : %d, free : %d \n", u16DataLen, nBytesFree);
        return -1;
    }

    /** 直接操作读写指针，不再维护剩余字节数，写入和读取及解析更灵活 */
    if (ptRing->rd == ptRing->wr)
    { /** 空环，直接写入 */
        /* 安全存入环缓区 */
        for (u16 i = 0; i < u16DataLen; i++)
        {
            *(ptRing->buf + ptRing->wr) = *pData++;
            ptRing->wr++;
            if (ptRing->wr >= RING_SIZE)
            {
                ptRing->wr = 0;
            }
        }
    }
    else
    { /** 循环写入 */
        // while(ptRing->rd != ptRing->wr) {
        for (u16 i = 0; i < u16DataLen; i++)
        {
            assert(ptRing->rd != ptRing->wr);
            *(ptRing->buf + ptRing->wr) = *pData++;
            ptRing->wr++;
            if (ptRing->wr >= RING_SIZE)
            {
                ptRing->wr = 0;
            }
        }
    }

    printf("[RING] data inserted. rd = %d, wr = %d \n", ptRing->rd, ptRing->wr);
    return 0;
}

#if 0
/**
 * @brief  从环中提取指定长度数据
 * @note
 * @param  *ptRing: 环
 * @param  *pBuf: 接收缓区
 * @param  u16BufSize:接收换取长度
 * @param  u16PollingBytes: 待提取字节数
 * @retval 返回实际提取字节数
 */
int RING_Poll_Data(tRINGBUF *ptRing, u8 *pBuf, u16 u16PollingBytes)
{
    assert(NULL != ptRing);
    assert(NULL != pBuf);
    if ((NULL == ptRing) || (NULL == pBuf)) {
        return -ERR_COMMON_ILLEGAL_PARAM;
    }

    u16 nBytes = (u16PollingBytes > ptRing->nBytesLeft) ? ptRing->nBytesLeft : u16PollingBytes;

    for(u16 i = 0; i < nBytes; i++) {
        *pBuf++ = *(ptRing->buf + ptRing->rd);
        ptRing->rd ++;
        if (ptRing->rd >= RING_SIZE) {
            ptRing->rd = 0;
        }
    }

    return nBytes;
}
#endif

/**
 * @brief  从环中提取完整帧，以’\n‘结束
 * @note
 * @param  *pRing: 环
 * @param  *pBuf: 接收缓区
 * @param  u16BufSize: 接收缓区最大长度
 * @retval 帧，字符串，’\0‘结尾
 */
int RING_Retrieve_Msg(tRINGBUF *pRing, u8 *pBuf, u16 u16BufSize)
{
    assert(NULL != pRing);
    if ((NULL == pRing) || (NULL == pBuf))
    {
        return -1;
    }

    u16 msg_len  = 0;
    u16 rd       = pRing->rd;
    int isFrame  = 0;
    u16 loops    = 0;
    int isPrefix = 1;

    /* 搜索帧尾标志'\n' */
    while (rd != pRing->wr)
    {
        loops++;

        /* 不允许从环中析出数据长度超过接收缓区长度 */
        if (loops >= u16BufSize)
        {
            // assert(0);
            // return -ERR_COMMON_RING_NO_ADEQUT_SPACE;
            /** unexpected data in the ring */
            break;
        }

        /* 找帧尾标志'\n' */
        *pBuf++ = pRing->buf[rd];
        if (pRing->buf[rd] == '\n')
        { /* 找到 */
            isFrame = 1;

            /* 指向下一个帧头位置 */
            rd++;
            if (rd >= RING_SIZE)
            {
                rd = 0;
            }

            break;
        }
        else
        {
            rd++;
            if (rd >= RING_SIZE)
            {
                rd = 0;
            }
        }
    }

    /** avoid invalid data flourd into ring */
    if (loops >= u16BufSize)
    {
        pRing->rd = rd;
        return 0;
    }

    /* 没有提取到完整帧 */
    if (0 == isFrame)
    {
        return 0;
    }

    /* 提取成功，更新读指针，返回实际提取数据长度 */
    pRing->rd = rd;

    // printf("[RING] retrieve msg OK.  rd = %d, wr = %d, \n", pRing->rd, pRing->wr);

    return loops;
}
