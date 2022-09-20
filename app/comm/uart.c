/*
 * @Description: 串口读写
 * @Author: zhengdgao
 * @Date: 2019-02-13 12:45:33
 * @LastEditTime: 2019-02-21 00:18:29
 * @LastEditors: Please set LastEditors
 */


#include "uart.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../NetIO/protocol.h"
#include "errcode.h"
#include "helpfunc.h"

static int sendByte(int fd, char c, unsigned int delay);
static int sendData(int fd, unsigned char *buf, int len, unsigned int charDelay);
static int setOptions(int fd, int baudrate, int databits, const char parity, const char stop);


/**
 * @brief  串口参数配置
 * @note
 * @param  fd: 设备文件
 * @param  baudrate: 波特率
 * @param  databits: 数据位
 * @param  parity: 校验位
 * @param  stop: 停止位
 * @retval 0：成功，否则错误代码
 */
static int setOptions(int fd, int baudrate, int databits, const char parity, const char stop)
{
    struct termios newtio;
    if (tcgetattr(fd, &newtio) != 0)
    {
        perror("tcgetattr() 3 failed");
        return -1;
    }

    speed_t _baud = 0;
    switch (baudrate)
    {
#ifdef B0
        case 0:
            _baud = B0;
            break;
#endif

#ifdef B50
        case 50:
            _baud = B50;
            break;
#endif
#ifdef B75
        case 75:
            _baud = B75;
            break;
#endif
#ifdef B110
        case 110:
            _baud = B110;
            break;
#endif
#ifdef B134
        case 134:
            _baud = B134;
            break;
#endif
#ifdef B150
        case 150:
            _baud = B150;
            break;
#endif
#ifdef B200
        case 200:
            _baud = B200;
            break;
#endif
#ifdef B300
        case 300:
            _baud = B300;
            break;
#endif
#ifdef B600
        case 600:
            _baud = B600;
            break;
#endif
#ifdef B1200
        case 1200:
            _baud = B1200;
            break;
#endif
#ifdef B1800
        case 1800:
            _baud = B1800;
            break;
#endif
#ifdef B2400
        case 2400:
            _baud = B2400;
            break;
#endif
#ifdef B4800
        case 4800:
            _baud = B4800;
            break;
#endif
#ifdef B7200
        case 7200:
            _baud = B7200;
            break;
#endif
#ifdef B9600
        case 9600:
            _baud = B9600;
            break;
#endif
#ifdef B14400
        case 14400:
            _baud = B14400;
            break;
#endif
#ifdef B19200
        case 19200:
            _baud = B19200;
            break;
#endif
#ifdef B28800
        case 28800:
            _baud = B28800;
            break;
#endif
#ifdef B38400
        case 38400:
            _baud = B38400;
            break;
#endif
#ifdef B57600
        case 57600:
            _baud = B57600;
            break;
#endif
#ifdef B76800
        case 76800:
            _baud = B76800;
            break;
#endif
#ifdef B115200
        case 115200:
            _baud = B115200;
            break;
#endif
#ifdef B128000
        case 128000:
            _baud = B128000;
            break;
#endif
#ifdef B230400
        case 230400:
            _baud = B230400;
            break;
#endif
#ifdef B460800
        case 460800:
            _baud = B460800;
            break;
#endif
#ifdef B576000
        case 576000:
            _baud = B576000;
            break;
#endif
#ifdef B921600
        case 921600:
            _baud = B921600;
            break;
#endif
        default:
            break;
    }
    cfsetospeed(&newtio, (speed_t)_baud);
    cfsetispeed(&newtio, (speed_t)_baud);

    /* We generate mark and space parity ourself. */
    // if (databits == 7 && (parity=="Mark" || parity == "Space"))
    {
        databits = 8;
    }
    switch (databits)
    {
        case 5:
            newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS5;
            break;
        case 6:
            newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS6;
            break;
        case 7:
            newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS7;
            break;
        case 8:
        default:
            newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS8;
            break;
    }
    newtio.c_cflag |= CLOCAL | CREAD;

    // parity, no parity
    newtio.c_cflag &= ~(PARENB | PARODD);
    //    if (parity == "Even")
    //    {
    //       newtio.c_cflag |= PARENB;
    //    }
    //    else if (parity== "Odd")
    //    {
    //       newtio.c_cflag |= (PARENB | PARODD);
    //    }

    // hardware handshake
    /*   if (hardwareHandshake)
          newtio.c_cflag |= CRTSCTS;
       else
          newtio.c_cflag &= ~CRTSCTS;*/
    newtio.c_cflag &= ~CRTSCTS;

    // stopbits
    if (stop == '2')
    {
        newtio.c_cflag |= CSTOPB;
    }
    else
    {
        newtio.c_cflag &= ~CSTOPB;
    }

    //   newtio.c_iflag=IGNPAR | IGNBRK;
    newtio.c_iflag = IGNBRK;
    //   newtio.c_iflag=IGNPAR;

    // software handshake
    //    if (softwareHandshake)
    //    {
    //       newtio.c_iflag |= IXON | IXOFF;
    //    }
    //    else
    {
        newtio.c_iflag &= ~(IXON | IXOFF | IXANY);
    }

    newtio.c_lflag = 0;
    newtio.c_oflag = 0;

    newtio.c_cc[VTIME] = 1;
    newtio.c_cc[VMIN]  = 255; /*60;*/

    //   tcflush(m_fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio) != 0)
    {
        perror("tcsetattr() 1 failed");
        return -1;
    }

    int mcs = 0;
    ioctl(fd, TIOCMGET, &mcs);
    mcs |= TIOCM_RTS;
    ioctl(fd, TIOCMSET, &mcs);

    if (tcgetattr(fd, &newtio) != 0)
    {
        perror("tcgetattr() 4 failed");
        return -1;
    }

    // hardware handshake
    //    if (hardwareHandshake)
    //    {
    //       newtio.c_cflag |= CRTSCTS;
    //    }
    //    else
    {
        newtio.c_cflag &= ~CRTSCTS;
    }
    /*  if (on)
         newtio.c_cflag |= CRTSCTS;
      else
         newtio.c_cflag &= ~CRTSCTS;*/
    if (tcsetattr(fd, TCSANOW, &newtio) != 0)
    {
        perror("tcsetattr() 2 failed");
        return -1;
    }

    return 1;
}

/**
 * @brief  发送原始字节流，先将十六进制数据转换成字符再发送，发送完成后再发送一个“帧结束”字符‘\n’
 * @note
 * @param  fd: 串口设备文件
 * @param  buf: 发送数据，HEX
 * @param  len: 发送数据长度
 * @param  charDelay: 延迟
 * @retval 错误代码，0：成功
 */
int UART_sendDataAsString(int fd, unsigned char *buf, int len, unsigned int charDelay)
{
    char s[2];
    int  ret = -1;

    if (fd < 0)
        return -1;

    for (int i = 0; i < len; i++)
    {
        SingleByte2Char(buf[i], s);

        ret = sendData(fd, (unsigned char *)s, 2, 0);
        if (ret < 0)
        {
            return -1;
        }
    }

    // data format: string, must add '\n' after send data, as a indication of "end of line"
    ret = sendByte(fd, '\n', 0);
    if (ret < 0)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief  发送数据流，不对数据做任何转换
 * @note
 * @param  fd: 串口设备文件
 * @param  buf:
 * @param  len:
 * @param  charDelay:
 * @retval
 */
int sendData(int fd, unsigned char *buf, int len, unsigned int charDelay)
{
    int nBytes = write(fd, buf, len);
    if (nBytes < 1)
    {
        perror("write\n");
        return -1;
    }

    if (nBytes == len)
        return 0;

    return -1;
}


/**
 * @brief  发送单个字节数据
 * @note
 * @param  fd:
 * @param  c:
 * @param  delay:
 * @retval
 */
int sendByte(int fd, char c, unsigned int delay)
{
    if (fd == -1)
    {
        return -1;
    }

    int res = write(fd, &c, 1);
    if (res < 1)
    {
        perror("write\n");
        return -1;
    }

    return 0;
}


/**
 * @brief  读串口数据
 * @note
 * @param  fd: 设备文件
 * @param  buf: 接收缓区
 * @param  bufSize: 缓区长度
 * @retval 实际接收的字节数
 */
int UART_readData(int fd, char *buf, int bufSize)
{
    // int nBytesLeft = bufSize;
    int bytesRead = 0;

    if (!(fd > 0))
    {
        return -1;
    }

    // while(nBytesLeft > 0)
    {
        bytesRead = read(fd, buf + bytesRead, bufSize);

        if (bytesRead < 0)
        {
            perror("read: \n");
            return -1;
        }
        // if the device "disappeared", e.g. from USB, we get a read event for 0 bytes
        else if (bytesRead == 0)
        {
            printf("bytesRead==0 \n");
            UART_disconnectTTY(fd);
            return -1;
        }
        else
        {
            // nBytesLeft -= bytesRead;
            // if(buf[bufSize - nBytesLeft] == '\n')
            // {
            //    return bufSize - nBytesLeft -1;
            // }
            return bytesRead;
        }
    }

    return -1;
}


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
int UART_connectTTY(char *dev, int baudrate, int dataBits, char parity, char stop)
{
    int fd    = -1;
    int flags = 0;

    flags = O_RDWR | O_NOCTTY /*| O_NONBLOCK*/;

    fd = open(dev, flags);
    if (fd < 0)
    {
        perror("opening failed");
        return -ERR_CPU_COMM_OPEN;
    }

    // flushing is to be done after opening. This prevents first read and write to be spam'ish.
    tcflush(fd, TCIOFLUSH);

    int n = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, n & ~O_NDELAY);

    // if (tcgetattr(fd, &m_oldtio)!=0)
    // {
    //    perror("tcgetattr() 2 failed");
    //    return -1;
    // }

    int ret = setOptions(fd, baudrate, dataBits, parity, stop);
    if (ret < 0)
    {
        perror("setOptions");
        return -1;
    }

    return fd;
}

/**
 * @brief  关闭串口设备
 * @note
 * @param  fd: 设备文件
 * @retval None
 */
void UART_disconnectTTY(int fd)
{
    if (fd != -1)
    {
        close(fd);
    }
}


/**
 * @brief  发送HEX数据，字符方式发送
 * @note
 * @param  fd: 设备文件
 * @param  *hexBuf: 数据缓区
 * @param  hexDataLen: 数据长度
 * @retval
 */
int UART_send_rawdata(int fd, unsigned char *hexBuf, int hexDataLen)
{
    if ((fd < 0) || (hexBuf == NULL) || (hexDataLen == 0))
        return -1;

    return UART_sendDataAsString(fd, hexBuf, hexDataLen, 0);
}


/**
 * @brief  发送定字节数据
 * @note
 * @param  fd: 设备文件号
 * @param  *vptr: 发送数据缓区
 * @param  n: 发送数据长度
 * @retval 剩余发送数据长度
 */
ssize_t safe_write(int fd, const void *vptr, size_t n)
{
    size_t      nleft;
    ssize_t     nwritten;
    const char *ptr;

    ptr   = vptr;
    nleft = n;

    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;
            else
                return -1;
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n);
}

/**
 * @brief  读指定字节数据
 * @note
 * @param  fd: 设备文件号
 * @param  *vptr: 数据接收缓区
 * @param  n: 读取长度
 * @retval 实际读取长度
 */
ssize_t safe_read(int fd, void *vptr, size_t n)
{
    size_t  nleft;
    ssize_t nread;
    char   *ptr;

    ptr   = vptr;
    nleft = n;

    while (nleft > 0)
    {
        if ((nread = read(fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR) //被信号中断
                nread = 0;
            else
                return -1;
        }
        else if (nread == 0)
            break;
        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft);
}


#if 0

/**
 * Hex2String, String2Hex test case 
 */
int main() 
{
   char strbuf[] = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F";
   // char strbuf[] = "102030405060708090";
   unsigned char rbuf[1024];

   printf("[======== String2Hex test ===========] \n");
   printf("string = %s \n", strbuf);

   char s[2];
   unsigned char a = 0x80;
   SingleByte2Char(a, s);
   printf("[%c %c] \n", s[0], s[1]);

   int ret = String2Hex(strbuf, strlen(strbuf), rbuf, 1024);
   if(ret < 0)
   {
      printf("error decode string 2 hex \n");
      return -1;
   }

   printf("HEX: \n");
   for(int i = 0; i < ret; i++) 
   {
      printf("%2x \n", rbuf[i]);
   }
   printf(" \n");

   printf("[======== Hex2String test ===========] \n");
   char string[1024];
   ret = Hex2String(rbuf, ret, string, 1024);
   if(ret < 0)
   {
      printf("error Hex2String \n");
      return -1;
   }
   string[ret] = '\0';

   printf("[hex2string], string = %s \n", string);
   
   printf(" \n");

   printf("[======== String2Hex test ===========] \n");
   printf("string = %s \n", string);

   ret = String2Hex(string, strlen(string), rbuf, 1024);
   if(ret < 0)
   {
      printf("error decode string 2 hex \n");
      return -1;
   }

   printf("HEX: \n");
   for(int i = 0; i < ret; i++) 
   {
      printf("%2x \n", rbuf[i]);
   }
   printf(" \n");

   return 0;
}

#endif

#if 0
int main()
{
   char dev[] = "/dev/ttyUSB0";
   int baund = 115200;
   int fd = -1;
   // struct termios m_oldtio;

   // char wbuf[] = "sxxxxxxxxxxxxxxxDDCCCCCCCCxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxfggg\n";
   // char wbuf[] = "1234567890123456789012345678901234567890123456789012345678901234567890";
   // char strBuf[] = "000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F20";
   // char strBuf[] = "1234567890123456789012345678901234567890123456789012345678901234567890";
   char strBuf[] = "111111111011111111101111111110111111111011111111101111111110111111111011111111101111111110111111111011111111101111111110111111111011111111101111111110";
   
   unsigned char hexBuf[1024];
   char recvString[2048];
   unsigned char recvHexBuf[1024];

   unsigned int loops = 0;

   fd = connectTTY(dev, baund, 8, 'N', '1');
   if(fd < 0) 
   {
      return -1;
   }

   while(1)
   {
      printf("[%d] \n", loops++);

      int hexDataLen = String2Hex(strBuf, strlen(strBuf), hexBuf, 1024);
      if(hexDataLen < 0)
         return -1;

      printf("[send msg (HEX)]: \n");

      for(int i = 0; i < hexDataLen; i ++)
      {
         printf("%2x \n", hexBuf[i]);
      }

      printf(" \n");

      int ret = sendDataAsString(fd, hexBuf, hexDataLen, 0);
      if(ret < 0) {
         return -1;
      }
      
      //data format: string, must add '\n' after send data, as a indication of "end of line" 
      // if (-1 == sendByte('\n', 0))
      // {
      //     return -1;
      // }

      int readBytes = readData(fd, recvString + readBytes, 2048);
      if(readBytes > 0)
      {
         // recvBuf[ret] = '\0';
         printf("[recv msg] len = %d : \n", readBytes);

         for(int i = 0; i < readBytes; i ++)
         {
            printf("%c \n", recvString[i]);
         }

         printf(" \n");

         if(recvString[readBytes-1] == '\n')
         {
            printf(" OK, get a complete msg. \n");            
         
            int recvBytes = String2Hex(recvString, readBytes - 1, recvHexBuf, 1024);

            printf("[Hex2String]: \n");
            
            for(int i = 0; i < recvBytes; i ++)
            {
               printf("%2x \n", recvHexBuf[i]);
            }

            printf(" \n");
         }
      }

      // sleep(1);
   }

}
#endif
