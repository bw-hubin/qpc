/* Simple binding of nanopb streams to TCP sockets.
 */

#include "common.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "type.h"
#include "uart.h"


/**
 * @brief  写设备回调函数
 * @note
 * @param  *stream: 数据流
 * @param  *buf: 数据缓区
 * @param  count: 写数据长度
 * @retval
 */
static bool write_callback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
#ifdef TCP
    int fd = (intptr_t)stream->state;

    int result = send(fd, buf, count, 0);
    printf("[ write_callback ] count = %d | buf = \n", (int)count);
    for (int i = 0; i < count; i++)
        printf(" %2x \n", buf[i]);
    printf("  |  result = %d \n", result);
    sleep(1);
    return result == count;
#endif

#ifdef UART
    int fd     = (intptr_t)stream->state;
    int nBytes = safe_write(fd, buf, count);

    return nBytes == count;
#endif
}

/**
 * @brief  从设备读回调函数
 * @note
 * @param  *stream: 数据流
 * @param  *buf: 数据缓区
 * @param  count: 读取的数据字节数
 * @retval
 */
static bool read_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
#ifdef TCP
    int fd = (intptr_t)stream->state;
    int result;


    result = recv(fd, buf, count, MSG_WAITALL);

    printf("[ read_callback ] : result = %d, count = %d, \n", result, (int)count);
    if (result == 0)
    {
        stream->bytes_left = 0; /* EOF */
        printf(" ====[EOF]==== \n");
    }
    else
    {
        printf("  | buf =  \n");
        for (int i = 0; i < result; i++)
            printf(" %2x \n", buf[i]);
        printf(" \n");
    }

    sleep(1);
    return result == count;
#endif

#ifdef UART
    int fd         = (intptr_t)stream->state;
    int nBytesRead = safe_read(fd, buf, count);

    return nBytesRead == count;

#endif
}

/**
 * @brief  创建设备输出流
 * @note
 * @param  fd: 设备文件
 * @retval 设备流对象
 */
pb_ostream_t pb_ostream_from_socket(int fd)
{
    pb_ostream_t stream = {&write_callback, (void *)(intptr_t)fd, SIZE_MAX, 0};
    return stream;
}

/**
 * @brief  创建设备输入流
 * @note
 * @param  fd: 设备文件
 * @retval 设备流对象
 */
pb_istream_t pb_istream_from_socket(int fd)
{
    pb_istream_t stream = {&read_callback, (void *)(intptr_t)fd, SIZE_MAX};
    return stream;
}
