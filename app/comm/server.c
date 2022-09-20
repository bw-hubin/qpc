/* This is a simple TCP server that listens on port 1234 and provides lists
 * of files to clients, using a protocol defined in file_server.proto.
 *
 * It directly deserializes and serializes messages from network, minimizing
 * memory use.
 *
 * For flexibility, this example is implemented using posix api.
 * In a real embedded system you would typically use some other kind of
 * a communication and filesystem layer.
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "comm.pb.h"
#include "common.h"

extern int connectTTY(char *dev, int baudrate, int dataBits, char parity, char stop);

#if 0
/* This callback function will be called once during the encoding.
 * It will write out any number of FileInfo entries, without consuming unnecessary memory.
 * This is accomplished by fetching the filenames one at a time and encoding them
 * immediately.
 */
bool listdir_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    static int listdir_callback_loops = 0;

    DIR *dir = (DIR*) *arg;
    struct dirent *file;
    FileInfo fileinfo = {};

    printf("[server] listdir_callback(...)  \n");
    
    while ((file = readdir(dir)) != NULL)
    {
        fileinfo.inode = file->d_ino;

        strncpy(fileinfo.name, file->d_name, sizeof(fileinfo.name));
        fileinfo.name[sizeof(fileinfo.name) - 1] = '\0';

        printf("[ %d ], fileinfo: %s \n", listdir_callback_loops++, fileinfo.name);

        /* This encodes the header for the field, based on the constant info
         * from pb_field_t. */
        if (!pb_encode_tag_for_field(stream, field))
            return false;
        
        /* This encodes the data for the field, based on our FileInfo structure. */
        if (!pb_encode_submessage(stream, FileInfo_fields, &fileinfo))
            return false;
    }
    
    /* Because the main program uses pb_encode_delimited(), this callback will be
     * called twice. Rewind the directory for the next call. */
    rewinddir(dir);

    return true;
}
#endif

#if 0
/* Handle one arriving client connection.
 * Clients are expected to send a ListFilesRequest, terminated by a '0'.
 * Server will respond with a ListFilesResponse message.
 */
void handle_connection(int connfd)
{
    DIR *directory = NULL;
    
    while(1)
    {
    
    /* Decode the message from the client and open the requested directory. */
    {
        COMM_Msg request = {};
        pb_istream_t input = pb_istream_from_socket(connfd);
        
        if (!pb_decode(&input, COMM_Msg_fields, &request))
        {
            printf("Decode failed: %s \n", PB_GET_ERROR(&input));
            return;
        }

        printf("-------------- parsing cmd ------------------ \n");
        printf("[id] : %d \n", request.msg_id);
        printf("[target] : %d \n", request.target);
        printf("[ack] : %d \n", request.ack_flag);
        printf("[len]: %d \n", request.data.size);
        if(request.has_data) {
            printf("[data] : \n");
            printf("%s \n", request.data.bytes);
        }
        else {
            printf("[data] : NONE. \n");
        }
    }
    
    /* List the files in the directory and transmit the response to client */
    {
        COMM_Msg response;
        pb_ostream_t output = pb_ostream_from_socket(connfd);

        char str[] = "message from server. hello, client! ";
        response.msg_id = 0x78;   
        response.target = 0x01;
        response.ack_flag = 0x01;
        response.has_data = true;
        response.data.size = strlen(str) + 1;
        strcpy((char*)response.data.bytes, str);
        response.data.bytes[strlen(str)] = '\0';     

        printf(" === send === \n");
        printf("[id] : %d \n", response.msg_id);
        printf("[target] : %d \n", response.target);
        printf("[ack] : %d \n", response.ack_flag);
        printf("[len] : %d \n", response.data.size);
        printf("[data]: %s \n", response.data.bytes);

        if (!pb_encode(&output, COMM_Msg_fields, &response))
        {
            printf("Encoding failed: %s \n", PB_GET_ERROR(&output));
        }


    }
    
    if (directory != NULL)
        closedir(directory);
    }/* while */
}
#endif

void SingleByte2TwoChar(char byte, char *dChar)
{
    // 0x98 -> '9''8'
    // sprintf(dChar, "%02x", byte);
    unsigned char hByte = (byte & 0xF0) >> 4;
    if (hByte <= 9)
    {
        hByte += 0x30;
    }
    else
    {
        hByte = 'A' + (hByte - 10);
    }

    char lByte = byte & 0x0F;
    if (lByte <= 9)
    {
        lByte += 0x30;
    }
    else
    {
        lByte = 'A' + (lByte - 10);
    }

    dChar[0] = hByte;
    dChar[1] = lByte;
}


int String2Hex(char *strData, int strDataLen, char *hexBuf, int hexBufSize)
{
    //'8''0' -> 0x80

    unsigned int byte = 0;

    if (strDataLen % 2 != 0)
    {
        perror("invalid string bytes");
        return -1;
    }

    for (int i = 0; i < strDataLen; i++)
    {
        char a = strData[i];

        switch (a)
        {
            case '0' ... '9':
                byte = a - '0';
                break;
            case 'a' ... 'f':
                byte = a - 'a' + 10;
                break;
            case 'A' ... 'F':
                byte = a - 'A' + 10;
                break;
            default:
                perror("invalid Hex bytes");
                return -1;
        }

        if (i % 2 == 0)
        {
            hexBuf[i / 2] = (byte << 4) & 0xF0;
        }
        else
        {
            hexBuf[i / 2] += (byte & 0x0F);
        }
    }

    return strDataLen / 2;
}

#if 1
int main(int argc, char **argv)
{
#    ifdef TCP
    int                listenfd, connfd;
    struct sockaddr_in servaddr;
    int                reuse = 1;

    /* Listen on localhost:1234 for TCP connections */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port        = htons(1234);
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("bind");
        return 1;
    }

    if (listen(listenfd, 5) != 0)
    {
        perror("listen");
        return 1;
    }

    for (;;)
    {
        /* Wait for a client */
        connfd = accept(listenfd, NULL, NULL);

        if (connfd < 0)
        {
            perror("accept");
            return 1;
        }

        printf("Got connection. \n");

        handle_connection(connfd);

        printf("Closing connection. \n");

        close(connfd);
    }
#    endif

#    ifdef UART
    char dev[] = "/dev/ttyUSB0";
    int  baund = 115200;

    int fd = connectTTY(dev, baund, 8, 'N', '1');
    if (fd < 0)
    {
        return -1;
    }


    // for(;;)
    {

        printf("Got connection. \n");

        handle_connection(fd);

        printf("Closing connection. \n");

        close(fd);
    }
#    endif

    return 0;
}


#endif

#if 0
int main()
{
    int listenfd, connfd;
    struct sockaddr_in servaddr;
    int reuse = 1;

    /* Listen on localhost:1234 for TCP connections */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port = htons(6001);
    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        perror("bind");
        return 1;
    }
    
    if (listen(listenfd, 5) != 0)
    {
        perror("listen");
        return 1;
    }
    
    printf("listening on port: 6001 \n");

    static char str[] = "hello, client.";
    static char str_recv[1024];

    // for(;;)
    {
        /* Wait for a client */
        connfd = accept(listenfd, NULL, NULL);
        
        if (connfd < 0)
        {
            perror("accept");
            return 1;
        }
        
        printf("Got connection. \n");

    unsigned int loops = 0;
    for(;;) 
    {    
        int result = send(connfd, str, strlen(str), 0);
        if(result == strlen(str))
            printf("[ %d send] %s \n", loops++, str);

        /* 阻塞式 */
        result = recv(connfd, str_recv, 1024, MSG_DONTWAIT /* MSG_WAITALL */);
        if(result > 0)
            printf("[recv] %s \n", str_recv);

        usleep(100000);
    }    
        
    }

        close(connfd);

        return 0;
}

#endif
