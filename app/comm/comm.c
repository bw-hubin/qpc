/**
 * @file comm.c
 * @author zhengdgao
 * @brief CPU与MCU串口通信
 * @version 0.1
 * @date 2022-06-07
 * @note
 * [几点说明] \n
 * 1，串口配置：B115200，8N1 \n
 * 2，数据以短帧发送，若超过设定大小，分包传输和重组 \n
 * 3，通信协议采用protobuf定义，由comm.proto描述，nanpb编译生成C文件:comm.pb.c,comm.pb.h \n
 * 4, 数据在实际传输时采用ASC字符传送 \n
 * 5, 数据结束标志为单字节‘\n’（0x10），接收端通过检测该结束标志是否存在来判断一帧是否结束 \n
 * \n
 * generator-bin/protoc --nanopb_out=. myprotocol.proto \n
 *
 * @copyright Copyright (c) 2022
 *
 */


#include "comm.h"

#include <EC20/serialcomm.h>
#include <NetIO/protocol.h>
#include <assert.h>
#include <common/helpfunc.h>
#include <common/ring.h>
#include <inc/type.h>
#include <model/qtbox.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "comm.pb.h"
#include "common.h"
#include "errcode.h"
#include "gps.h"
#include "param.h"
#include "param.pb.h"
#include "qpc.h"
#include "qtbox.h"
#include "type.h"
#include "uart.h"

/// 模块名
#define MODULE "comm"

/// 日志标签
#if defined(LOG_TAG)
#    undef LOG_TAG
#    define LOG_TAG MODULE
#endif

/** 系统参数，param.c中定义 */
extern Sys_param *p_sys_param;

/** 是否使用设备板上串口，定义则用 */
#define BOARD_COMM


/**
 * @brief  decode回调函数，对于用“repeated”修饰的字段需要重复调用
 * @note
 * @param  *stream: 输入流
 * @param  *field: 对象格式
 * @param  **arg: 参数，未用
 * @retval false 失败，true 成功
 */
static bool parse_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    CAN_FRAME block = {};

    static u16 loops = 0;

    // 解析单个CAN报文数据
    if (!pb_decode(stream, CAN_FRAME_fields, &block))
        return false;

    /* log_d("can frame parse data: length = %d, data = \n", block.data.size);
    PrintHex(block.data.bytes, block.data.size); */

    if (block.data.size > 0)
    {
        COMMCanMsgEvt *pe = Q_NEW(COMMCanMsgEvt, COMM_CANMSG_SIG);
        assert(NULL != pe);
        if (pe)
        {
            pe->can_id = block.id;
            memcpy(pe->can_data, block.data.bytes, block.data.size);

            log_i("[COMM] post CAN Msg to AO_ProtocolParsing, size is %d", block.data.size);
            QACTIVE_POST(AO_ProtocolParsing, &pe->super, (void *)0);
        }
    }
    else
    {
        /* log_d("[COMM] Receive CAN Frame size is 0 \n"); */
    }

#if 0
    log_d("[COMM] ----- CAN FRAME,loops: %d ----- \n", loops);
    log_d("ID: %x, DATA: [ %x %x %x %x %x %x %x %x ] \n",
          block.id,
          block.data.bytes[0],
          block.data.bytes[1],
          block.data.bytes[2],
          block.data.bytes[3],
          block.data.bytes[4],
          block.data.bytes[5],
          block.data.bytes[6],
          block.data.bytes[7]);
#endif

    return true;
}


u8 u8CanMockData = 0;

u8 getMockData()
{
    if (u8CanMockData < 0xFE)
    {
        u8CanMockData++;
    }
    else
    {
        u8CanMockData = 0;
    }
    return u8CanMockData;
}

#if 0
 /**
  * @brief  网络层向COMM提交数据发送任务
  * @note   此处为COMM数据发送的统一和唯一入口
  *         注意COMM发送数据指针指向网络任务的数据体，并未重新申请内存，因此COMM发送任务如果没有处理完毕，网络任务不能结束
  * @param  task_id: 发起COMM数据传输的上层网络任务序号，便于COMM向网络层反馈发送和命令应答状态
  * @param  cmd: 命令字
  * @param  target: 目标
  * @param  ack_Flag: 应答标志，对于命令，0xFE；否则是应答
  * @param  pData: 数据
  * @param  datalen: 数据长度
  * @retval
  */
void COMM_Submit_Task(u8 task_id, u8 cmd, u8 target, u8 ack_Flag, u8* pData, u16 datalen)
{
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SUBMIT_SIG);
    assert(pe != NULL);
    assert(pData != NULL);
    assert(datalen > 0);

    pe->task_id = task_id;
    pe->cmd_id = cmd;
    pe->target = target;
    pe->ack_flag = ack_Flag;
    pe->data_len = datalen;
    pe->data = pData;

    QACTIVE_POST(AO_CommMgr, &pe->super, (void*)0);
}
#endif

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
int COMM_Send_Cmd(int fd, u8 session_id, u8 cmd, u8 target, u8 ack_Flag, u8 *pData, u16 datalen)
{
    COMM_Msg msg = COMM_Msg_init_default;

    msg.session_id = session_id; /* 保存会话上下文标识 */
    msg.msg_id     = cmd;
    msg.ack_flag   = ack_Flag;
    msg.target     = target;
    msg.which_data = COMM_Msg_cmd_param_tag; /* 指示为命令参数数据 */
    if (datalen > 0)
    {
        assert(NULL != pData);
        msg.data.cmd_param.has_data  = TRUE; /* 有数据 */
        msg.data.cmd_param.data.size = datalen;
        memcpy(msg.data.cmd_param.data.bytes, pData, datalen);
    }
    else
    {
        msg.data.cmd_param.has_data = FALSE; /* 无数据 */
    }

    return COMM_send_msg(fd, &msg);
}

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
int COMM_Send_FileInfo(int fd, u8 session_id, u8 cmd, u8 target, u8 ack_Flag, char *file_name, u32 file_size, u16 block_size, char *version_hw, char *version_sw)
{
    COMM_Msg msg = COMM_Msg_init_default;

    msg.session_id = session_id; /* 保存会话上下文标识 */
    msg.msg_id     = eCMDEx_OTA_FILEINFO;
    msg.ack_flag   = ack_Flag;
    msg.target     = target;
    msg.which_data = COMM_Msg_file_info_tag; /* 指示为文件头信息 */
    strcpy(msg.data.file_info.file_name, file_name);
    msg.data.file_info.file_size = file_size;
    strcpy(msg.data.file_info.version_hw, version_hw);
    strcpy(msg.data.file_info.version_sw, version_sw);
    msg.data.file_info.encrype    = 0;
    msg.data.file_info.block_size = block_size;

    return COMM_send_msg(fd, &msg);
}

/**
 * @brief  发送实时数据
 * @note
 * @param  fd: 串口设备号
 * @param  warning: 报警标志，0：非报警数据；1：报警数据
 * @param  upl_req: 是否上传标志，0：不需要上传；1：需要上传
 * @param  pData: 实时数据
 * @param  datalen: 实时数据长度
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_RealInfo(int fd, u8 warning, u8 upl_req, u8 *pData, u16 datalen)
{
    COMM_Msg msg = COMM_Msg_init_default;

    msg.session_id               = 0xFF;
    msg.msg_id                   = PROTOCOL_CMD_GB_REALINFO;
    msg.ack_flag                 = 0x01;
    msg.target                   = eECUType_CPU;
    msg.which_data               = COMM_Msg_real_info_tag;
    msg.data.real_info.upl       = upl_req;
    msg.data.real_info.warning   = warning;
    msg.data.real_info.data.size = datalen;
    memcpy(msg.data.real_info.data.bytes, pData, datalen);

    return COMM_send_msg(fd, &msg);
}


/**
 * @brief  发送升级文件元信息
 * @note
 * @param  *pFileInfo: 升级文件元信息结构
 * @retval
 */
int COMM_Send_MCUFileInfo(DATA_FileInfo *pFileInfo)
{
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
    assert(pe != NULL);
    assert(pFileInfo != NULL);

    if (pe)
    {
        pe->task_id   = 0xFF;
        pe->data_type = eDATAType_FILE_INFO;
        pe->cmd_id    = eCMDEx_OTA_FILEINFO;
        pe->target    = eECUType_TBOX;
        pe->ack_flag  = 0xFE;
        pe->data_len  = sizeof(DATA_FileInfo);
        pe->data      = (u8 *)pFileInfo;

        QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
    }
    return 0;
}

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
int COMM_send_BlockData(int fd, u8 session_id, u8 cmd, u8 target, u8 ack_Flag, u16 block_amount, u16 block_id, u8 *pData, u16 data_len)
{
    COMM_Msg msg;

    msg.session_id              = session_id; /* 保存会话上下文标识 */
    msg.msg_id                  = eCMDEx_OTA_BLOCK;
    msg.ack_flag                = ack_Flag;
    msg.target                  = target;
    msg.which_data              = COMM_Msg_block_tag; /* 指示为分块数据 */
    msg.data.block.block_amount = block_amount;
    msg.data.block.block_id     = block_id;
    msg.data.block.checksum     = checksum_XOR(pData, data_len);
    // if(nBytesWritten > COMM_BLOCK_SIZE) {
    //     return -ERR_COMMON_ILLEGAL_PARAM;
    // }
    msg.data.block.data.size = data_len;
    memcpy(msg.data.block.data.bytes, pData, data_len);

    return COMM_send_msg(fd, &msg);
}

/**
 * @brief  发送升级数据块，CPU->MCU
 * @note
 * @param  *pBlockData: 块数据结构体
 * @retval 0=成功，否则错误代码
 */
int COMM_send_MCUBlockData(DATA_Block *pBlockData)
{
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
    assert(pe != NULL);
    assert(pBlockData != NULL);

    if (pe)
    {
        pe->task_id   = 0xFF;
        pe->data_type = eDATAType_BLOCK_DATA;
        pe->cmd_id    = eCMDEx_OTA_BLOCK;
        pe->target    = eECUType_TBOX;
        pe->ack_flag  = 0xFE;
        pe->data_len  = sizeof(DATA_Block);
        pe->data      = (u8 *)pBlockData;

        QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
    }
    return 0;
}

/**
 * @brief  发送注册数据请求，CPU->MCU，MCU应答包应为实际注册包
 * @note
 * @param  *ptTime: [IN] 系统时间
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_RegistorRequest(tSYSTIME *ptTime)
{
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
    return 0;
}

/**
 * @brief 发送登入数据请求，CPU->MCU，MCU应答
 *
 * @return int 0：成功，否则错误代码
 */
int COMM_Send_LoginRequest()
{
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
    assert(pe != NULL);

    if (pe)
    {
        pe->task_id   = 0xFF;
        pe->data_type = eDATAType_DT_VEH_LOGIN;
        pe->cmd_id    = PROTOCOL_CMD_GB_VEHICLE_LOGIN;
        pe->target    = eECUType_TBOX;
        pe->ack_flag  = 0xFE;
        pe->data_len  = 0;

        QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
    }
    return 0;
}

/**
 * @brief  发送命令接收确认应答数据包，CPU->MCU
 * @note
 * @param  session_id: SESSION号
 * @param  u8Cmd: u8Cmd: 命令字
 * @param  ack_flag: 应答标志
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Ack_Echo(u8 session_id, u8 u8Cmd, u8 ack_flag)
{
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
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
    }

    return 0;
}

/**
 * @brief  查询MCU端状态信息，CPU->MCU，MCU应答01
 * @note
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Query_MCU_Status()
{
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
    assert(pe != NULL);

    if (pe)
    {
        pe->task_id   = 0xFF; /* 无session */
        pe->data_type = eDATAType_MCU_STAT;
        pe->cmd_id    = eCMDEx_MCU_STATUS;
        pe->target    = eECUType_TBOX;
        pe->ack_flag  = 0xFE; /* 需应答 */
        pe->data      = NULL;
        pe->data_len  = 0;

        QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
    }

    return 0;
}

/**
 * @brief  发送查询状态，MCU->CPU
 * @note
 * @param  *pMCUInfo: 状态信息
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Ack_Query_MCU_Status(DATA_MCUInfo *pMCUInfo)
{
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
    assert(pe != NULL);
    assert(pMCUInfo != NULL);

    if (pe)
    {
        pe->task_id   = 0xFF; /* 无session */
        pe->data_type = eDATAType_MCU_STAT;
        pe->cmd_id    = eCMDEx_MCU_STATUS;
        pe->target    = eECUType_CPU;
        pe->ack_flag  = 0x01; /* 需应答 */
        pe->data_len  = sizeof(DATA_MCUInfo);
        pe->data      = (u8 *)pMCUInfo;

        QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
    }

    return 0;
}

/**
 * @brief  发送MPU端状态信息，CPU->MCU
 * @note
 * @param  *pModemInfo: MPU状态信息
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_CPU_Status(DATA_ModemInfo *pModemInfo)
{
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
    assert(pe != NULL);
    assert(pModemInfo != NULL);

    if (pe)
    {
        pe->task_id   = 0xFF; /* 无session */
        pe->data_type = eDATAType_MODEM_STAT;
        pe->cmd_id    = eCMDEx_CPU_STATUS;
        pe->target    = eECUType_TBOX;
        pe->ack_flag  = 0xFE;
        pe->data      = (u8 *)pModemInfo;
        pe->data_len  = sizeof(DATA_ModemInfo);

        QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
    }
    return 0;
}

/**
 * @brief  读取MODEM内部状态
 * @note
 * @param  *pModemInfo: [IN] 状态信息
 * @retval 0：成功，否则错误代码
 */
int COMM_Get_CPU_All_Stat(DATA_ModemInfo *pModemInfo)
{
    if (NULL == pModemInfo)
    {
        return -ERR_COMMON_ILLEGAL_PARAM;
    }

    /** ICCID */
    pModemInfo->has_iccid = 1;
    strcpy(pModemInfo->iccid, DEV_Get_ICCID());

    /** CSQ */
    pModemInfo->has_csq = 0;
    // DEV_Get_CSQ(&pModemInfo->csq);

    /** VERSION */
    pModemInfo->has_version = 1;
    strcpy(pModemInfo->version, GET_VERSION());

    /** 网络工作模式 */
    pModemInfo->has_eMode = 1;
    pModemInfo->eMode     = DEV_Get_Mode();

    /** 网络状态，0：无网络，1：网络注册成功 */
    pModemInfo->has_eNetStat = 1;
    pModemInfo->eNetStat     = DEV_Get_NetStatus();

    /** 天线状态，MCU端检测，不处理 */
    pModemInfo->has_anteStat = 0;

    /** 通信模块工作状态 */
    pModemInfo->has_eDevStat = 1;
    pModemInfo->eDevStat     = DEV_Get_DevStatus();

    /** 定位状态 */
    pModemInfo->has_gps_info = 1;
#if 0
    pModemInfo->gps_info.angle = 78.3;
    pModemInfo->gps_info.latitude = 31.49;
    pModemInfo->gps_info.longitude = 117.5;
    pModemInfo->gps_info.speed = 17.1;
    pModemInfo->gps_info.valid = 1;

    pModemInfo->gps_info.year = 2020;
    pModemInfo->gps_info.month = 10;
    pModemInfo->gps_info.day = 27;
    pModemInfo->gps_info.hour = 11;
    pModemInfo->gps_info.min = 5;
    pModemInfo->gps_info.sec = 23;

#else
    DEV_Get_GPSStatus_CAN(&pModemInfo->gps_info);
    // DEV_Get_GPSStatus(&pModemInfo->gps_info);
#endif
    /** OTA升级状态 */
    pModemInfo->has_eOTAStat = 1;
    pModemInfo->eOTAStat     = DEV_Get_OTA_Status();

    return 0;
}

/**
 * @brief  发送MPU端状态信息，CPU->MCU，应答MCU的查询请求
 * @note
 * @param  *pModemInfo:
 * @retval 0：成功，否则错误代码
 */
int COMM_Send_Ack_CPU_Status(DATA_ModemInfo *pModemInfo)
{
    CommSendEvt *pe = Q_NEW(CommSendEvt, COMM_SEND_SIG);
    assert(pe != NULL);
    assert(pModemInfo != NULL);

    if (pe)
    {
        pe->task_id   = 0xFF; /* 无session */
        pe->data_type = eDATAType_MODEM_STAT;
        pe->cmd_id    = eCMDEx_CPU_STATUS;
        pe->target    = eECUType_TBOX;
        pe->ack_flag  = 0x01;
        pe->data      = (u8 *)pModemInfo;
        pe->data_len  = sizeof(DATA_ModemInfo);

        QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
    }
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
    COMM_Msg msg = COMM_Msg_init_default;

    assert((data_type == eDATAType_CMD_PARAM) ||
           (data_type == eDATAType_FILE_INFO) ||
           (data_type == eDATAType_BLOCK_DATA) ||
           (data_type == eDATAType_ECHO) ||
           (data_type == eDATAType_MCU_STAT) ||
           (data_type == eDATAType_MODEM_STAT));

    msg.session_id = session_id; /* 保存会话上下文标识 */
    msg.msg_id     = cmd;
    msg.ack_flag   = ack_Flag;
    msg.target     = target;

    switch (data_type)
    {
        /* 命令消息 */
        case eDATAType_CMD_PARAM:

            msg.which_data = COMM_Msg_cmd_param_tag;

            if (datalen > 0)
            {
                assert(NULL != pData);
                msg.data.cmd_param.has_data  = TRUE; /* 有数据 */
                msg.data.cmd_param.data.size = datalen;
                memcpy(msg.data.cmd_param.data.bytes, pData, datalen);
            }
            else
            {
                msg.data.cmd_param.has_data = FALSE; /* 无数据 */
            }

            break;
        /** 升级文件元信息 */
        case eDATAType_FILE_INFO:

            msg.which_data       = COMM_Msg_file_info_tag;
            DATA_FileInfo *pInfo = (DATA_FileInfo *)pData;
            strcpy(msg.data.file_info.file_name, pInfo->file_name);
            msg.data.file_info.file_size = pInfo->file_size;
            strcpy(msg.data.file_info.version_sw, pInfo->version_sw);
            msg.data.file_info.encrype    = pInfo->encrype;
            msg.data.file_info.block_size = pInfo->block_size;

            break;
        /** 升级文件块数据 */
        case eDATAType_BLOCK_DATA:

            msg.which_data              = COMM_Msg_block_tag;
            DATA_Block *pBlock          = (DATA_Block *)pData;
            msg.data.block.block_amount = pBlock->block_amount;
            msg.data.block.block_id     = pBlock->block_id;
            msg.data.block.checksum     = pBlock->checksum;
            msg.data.block.data.size    = pBlock->data.size;
            memcpy(msg.data.block.data.bytes, pBlock->data.bytes, pBlock->data.size);

            log_d(" COMM_Send_Data, block[ %d / %d ] \n", msg.data.block.block_id, msg.data.block.block_amount);

            break;
        /** ECHO */
        case eDATAType_ECHO:

            msg.which_data             = COMM_Msg_echo_tag;
            msg.data.echo.has_errcode  = 0;
            msg.data.echo.errcode      = 0;
            msg.data.echo.has_block_id = 0;
            msg.data.echo.block_id     = 0;

            log_d("[COMM] send ECHO: cmd = %02x \n", cmd);
            break;
        /** MPU状态 */
        case eDATAType_MODEM_STAT:

            msg.which_data = COMM_Msg_modem_info_tag;
            memcpy((u8 *)&msg.data.modem_info, (u8 *)pData, sizeof(DATA_ModemInfo));
            DATA_ModemInfo *pModemInfo = (DATA_ModemInfo *)pData;

            log_d("[COMM] send cmd: CPU_STAT, iccid: %s, version: %s \n",
                  pModemInfo->iccid,
                  pModemInfo->version);
            break;

        /** MCU状态信息 */
        case eDATAType_MCU_STAT:

            msg.which_data                    = COMM_Msg_mcu_info_tag;
            DATA_MCUInfo *pMCUInfo            = (DATA_MCUInfo *)pData;
            msg.data.mcu_info.has_battery_cap = 0;
            msg.data.mcu_info.has_power_mode  = 0;
            msg.data.mcu_info.has_sleep_ctrl  = 0;
            msg.data.mcu_info.has_version     = 0;

            log_d("[COMM] send cmd: MCU_STAT \n");
            break;

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

    log_d(" opening  %s \n", dev);
    fd = UART_connectTTY(dev, 115200, 8, 'N', '1');
    if (fd < 0)
        log_d("COMM open error. \n");

    return fd;
#else

    /* 打开4G模块串口 */
    int fd = Serialcomm_Open();
    return fd;
#endif
}

/**
 * @brief  提取串口数据，proto标准方式
 * @note   暂未实现
 *
 * @param  fd: 串口设备句柄
 * @retval 0：成功，否则失败
 */
int COMM_poll_msg_proto(int fd)
{
    if (fd < 0)
    {
        return -1;
    }

    COMM_Msg msg = {};

    pb_istream_t input = pb_istream_from_socket(fd);

    if (!pb_decode_delimited(&input, COMM_Msg_fields, &msg))
    {
        log_d("Decode failed: %s \n", PB_GET_ERROR(&input));
        return -1;
    }

    /* parse msg, to be defined... */

    return 0;
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
        return -ERR_COMMON_ILLEGAL_PARAM;
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
 * @brief  读取串口数据，入环，非阻塞读
 * @note
 * @param  fd: 串口设备文件
 * @param  *pBuf: 接收缓区
 * @param  u16BufSize: 接收缓区大小
 * @param  u16ReadBytes: 待读取字节数
 * @retval 实际读取字节数，或错误代码
 */
int COMM_poll_msg_Ring(int fd, tRINGBUF *pRing)
{
    if ((fd < 0) || (NULL == pRing))
    {
        return -ERR_COMMON_ILLEGAL_PARAM;
    }

    static u8 buf[COMM_MSG_SIZE];

    int nread = read(fd, buf, COMM_MSG_SIZE);
    if (nread > 0)
    {
        int ret = RING_Attach_Data(pRing, buf, nread);
        return ret;
    }

    return nread;
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
        return -ERR_COMMON_ILLEGAL_PARAM;
    }

    static u8 msgData[COMM_MSG_SIZE];
    static u8 hexData[COMM_MSG_SIZE];
    u16       msg_len    = 0;
    u16       hexDataLen = 0;

    // log_d("[COMM] retrieving msg, ring.rd = %d, ring.wr = %d \n", pRing->rd, pRing->wr);

    /* 从环提取帧 */
    int len = RING_Retrieve_Msg(pRing, msgData, COMM_MSG_SIZE);
    if (len <= 0)
    {
        log_d("[COMM] retrieving msg, ret = %d \n", len);
        return len; /* error or empty */
    }

    log_d("[COMM] retrive msg from ring, len = %d, data: [%s] \n", strlen(msgData), msgData);

    /* 接收到一个完整帧，字符串转HEX数据 */
    assert(msgData[len - 1] == '\n');
    hexDataLen = String2Hex(msgData, len - 1, hexData, COMM_MSG_SIZE);
    if (hexDataLen > 0)
    {
        // PrintHex_v2(1, LOG_TAG, "msg to Hex: ", hexData, hexDataLen);

        /* 解析数据 */
        /* proto格式转换成结构体定义 */
        COMM_Msg     tMsg;
        pb_istream_t input = pb_istream_from_buffer(hexData, hexDataLen);
        if (!pb_decode(&input, COMM_Msg_fields, &tMsg))
        {
            log_d("Decoding failed: %s \n", PB_GET_ERROR(&input));
            return -ERR_CPU_COMM_INVALID_MSG;
        }

        /* 转换成功，此处解包并处理
            可能的几类消息：1）应答消息，对CPU侧命令的应答
                         2）主动上报消息，MCU侧周期性上报消息，和主动告警消息
        */
        return COMM_Parse_Msg(&tMsg);
    }

    return 0;
}

/**
 * @brief  解包并处理
 * @note   可能的几类消息：1）应答消息，对CPU侧命令的应答
 *                      2）主动上报消息，MCU侧周期性上报消息，和主动告警消息
 * @param  *pMsg:
 * @retval
 */
int COMM_Parse_Msg(COMM_Msg *pMsg)
{
    assert(NULL != pMsg);
    if (NULL == pMsg)
    {
        return -ERR_COMMON_ILLEGAL_PARAM;
    }

    // static u8 msg_mb[FRAME_SIZE];   /* 多包消息缓存 */
    // static u16 msg_mb_len = 0;  /* 多包消息总长 */
    // static u16 msg_block_amount = 0;
    // static u16 msg_block_id = 0;

    log_d("[COMM] MSG PARSING : session_id = %d, msg_id = %02x, ack = %02x, which_data = %d \n", pMsg->session_id, pMsg->msg_id, pMsg->ack_flag, pMsg->which_data);

    /* 单帧应答包或命令包 */
    if (pMsg->which_data == COMM_Msg_cmd_param_tag)
    {
        /* TBOX命令，Server应答 */
        if (pMsg->ack_flag == 0xFE)
        {
#if 1
            if (pMsg->msg_id == 0x24)
            {
                /** 回复MCU命令接收ECHO */
                COMM_Send_Ack_Echo(0xFF, pMsg->msg_id, 0x01);
                return 0;
            }
#endif

            /** 普通命令，遵循通用处理流程 */
            /* 命令应答直接转发给TCP上传AO，会分配NetTask，启动一个新的会话，管理该命令的处理流程 */
            NetSubmitEvt *pe = Q_NEW(NetSubmitEvt, NET_SUBMIT_SIG);
            if (pe)
            {
                pe->cmd_id   = pMsg->msg_id;
                pe->target   = pMsg->target;
                pe->ack_flag = pMsg->ack_flag;
                if (pMsg->data.cmd_param.has_data)
                {
                    pe->data_len = pMsg->data.cmd_param.data.size;
                    if (pe->data_len > 0)
                    {
                        memcpy(pe->data, pMsg->data.cmd_param.data.bytes, pe->data_len);
                    }
                }
                else
                {
                    pe->data_len = 0;
                }

                const char *msg = "++++++++++++++++++********** Msg from MCU ********+++++++++++++++++\n";
                PrintHex_v2(1, LOG_TAG, msg, pe->data, pe->data_len);

                QACTIVE_POST(AO_NetTaskMgr, &pe->super, (void *)0);
            }

            /** 回复MCU命令接收ECHO */
            COMM_Send_Ack_Echo(0xFF, pMsg->msg_id, 0x01);
            return 0;
        }
        /* Server命令，TBOX应答包；或TBOX主动上传包，但不需要服务器应答，如实时信息 */
        else
        {
            /** 握手应答 */
            if (eCMDEx_COMM_HEART == pMsg->msg_id)
            {
                log_e("recv eCMDEx_COMM_HEART");
                /** COMM链路维护由AO_CommMgr负责 */
                static const QEvt te = {COMM_PINGPANG_SIG, 0, 0};
                QACTIVE_POST(AO_CommMgr, &te, (void *)0);
            }
            /**
             * 登入命令的处理与普通命令稍有不同。登入命令用于建立通信链路，CPU通知MCU发送登入数据，
             * TBOX在接收到服务器的登入应答包后，认为通信链路建立成功，方可进行正常上下行命令处理。
             */
            else if (PROTOCOL_CMD_GB_VEHICLE_LOGIN == pMsg->msg_id)
            {
                /** VIN 保存到.param文件中 */
                pMsg->data.veh_login.vin[VIN_LENGTH] = '\0';
                param_Set_VIN(pMsg->data.veh_login.vin);
                /** TIN 保存到.param文件中 */
                pMsg->data.veh_login.tin[TIN_LENGTH] = '\0';
                param_Set_TIN(pMsg->data.veh_login.tin);

                /** 带有实际数据的应答消息 */
                CommAckLoginEvt *pe = Q_NEW(CommAckLoginEvt, COMM_ACKED_SIG);
                assert(NULL != pe);
                if (pe)
                {

                    pe->cmd_id   = pMsg->msg_id;
                    pe->target   = pMsg->target;
                    pe->ack_flag = pMsg->ack_flag;
                    strcpy(pe->vin, param_Get_VIN);
                    strcpy(pe->tin, pMsg->data.veh_login.tin);
                    pe->batteryPackNum     = pMsg->data.veh_login.batteryPackNum;
                    pe->batteryPackCodeLen = pMsg->data.veh_login.batteryPackCodeLen;
                    strcpy(pe->batteryPackCodes, pMsg->data.veh_login.batteryPackCodes);
                    if (pMsg->data.veh_login.has_vehModel)
                    {
                        pe->vMode = pMsg->data.veh_login.vehModel;
                    }
                    if (pMsg->data.veh_login.has_swVer)
                    {
                        pe->swVer = pMsg->data.veh_login.swVer;
                    }

                    /** 车辆登入数据封装成事件参数发给TCPSessionMgr */
                    QACTIVE_POST(AO_TCPSessionMgr, &pe->super, (void *)0);
                }

                /** 通知AO_CommMgr接收到应答数据 */
                CommEchoEvt *pe_reg = Q_NEW(CommEchoEvt, COMM_ECHOED_SIG);
                assert(NULL != pe_reg);
                if (pe)
                {
                    pe_reg->cmd_id = pMsg->msg_id;
                    QACTIVE_POST(AO_CommMgr, &pe_reg->super, (void *)0);
                }
            }
            else
            {
                /* Server命令，TBOX应答包，转发给相应的NetTask */
                CommAckEvt *pe = Q_NEW(CommAckEvt, COMM_ACKED_SIG);
                if (pe)
                {
                    u8 session_id = pMsg->session_id;
                    assert((session_id >= 0) && (session_id < N_TASK));

                    if ((session_id >= 0) && (session_id < N_TASK))
                    {
                        pe->cmd_id   = pMsg->msg_id;
                        pe->target   = pMsg->target;
                        pe->ack_flag = pMsg->ack_flag;
                        if (pMsg->data.cmd_param.has_data)
                        {
                            pe->data_len = pMsg->data.cmd_param.data.size;
                            if (pe->data_len > 0)
                            {
                                memcpy(pe->data, pMsg->data.cmd_param.data.bytes, pe->data_len);
                            }
                        }
                        else
                        {
                            pe->data_len = 0;
                        }

                        /* 通知CPU侧网络任务的应答状态 */
                        QACTIVE_POST(AO_NetTask[session_id], &pe->super, (void *)0);

                        log_d("[COMM] post COMM_ACKED_SIG to AO_NetTask[%d] \n", session_id);
                    }
                }
            }
        }

        return 0;
    }
    /**
     * TBOX对于完整升级文件数据接收状态的应答
     * 对于MCU升级文件数据，CPU每发送一个block，MCU回一个echo包，所有block发送完成后，最后MCU回一个ACK包
     */
    else if (pMsg->which_data == COMM_Msg_block_tag)
    {

        CommAckEvt *pe = Q_NEW(CommAckEvt, COMM_ACKED_SIG);
        assert(NULL != pe);
        if (NULL != pe)
        {
            pe->cmd_id   = pMsg->msg_id;
            pe->target   = pMsg->target;
            pe->ack_flag = pMsg->ack_flag;
            // pe->data = NULL;    /** 无数据 */
            pe->data_len = 0;

            QACTIVE_POST(AO_NetOTA, &pe->super, (void *)0);
        }
#if 0
        /* 多块消息，MCU侧主动发起，或是应答数据 */
        /* 先应答单包，通知CPU侧串口发送任务应答状态 */
        // QACTIVE_POST(AO_CommMgr, Q_NEW(QEvt, COMM_ACKED_SIG), (void*)0);

        /* 多包消息再次组包完成后，再通知网络发送 */
        /* 多包消息接收，默认所有分包接收完毕后，一次性通知MCU接收状态
            MCU发送要求：按顺序递增包序号发送分包，分包发送时不允许重发。
         */
        if(pMsg->data.block.block_amount > 1) {
            if(pMsg->data.block.block_id != pMsg->data.block.block_amount - 1) {
                assert(BLOCK_SIZE == pMsg->data.block.data.size);
            }
        } else {
            assert(pMsg->data.block.data.size <= BLOCK_SIZE);
        }

        /* 首包复位接收状态 */
        if(pMsg->data.block.block_id == 0) {
            msg_block_id = 0;
            msg_block_amount = pMsg->data.block.block_amount;
            msg_mb_len = 0;
        }

        /* 分包内容校验 */
        u8 checksum = checksum_XOR(pMsg->data.block.data.bytes, pMsg->data.block.data.size);
        if(checksum != pMsg->data.block.checksum) {
            return -ERR_CPU_COMM_INVALID_MSG;
        }

        /* 转存分包数据 */
        memcpy(msg_mb + pMsg->data.block.block_id * BLOCK_SIZE,
                pMsg->data.block.data.bytes,
                pMsg->data.block.data.size);

        /* 累加消息长度 */
        msg_mb_len += pMsg->data.block.data.size;

        /* 确保消息不能超过允许发送数据包最大长度 */
        assert(msg_mb_len <= FRAME_SIZE);

        /* 收取完毕，提交TCP发送 */
        if(pMsg->data.block.block_id == pMsg->data.block.block_amount - 1)
        {
            if(pMsg->ack_flag == 0xFE) {    /* TBOX命令包，Server应答 */

                /* 确保内存不溢出 */
                assert(msg_mb_len <= FRAME_SIZE);

                /* 命令应答直接转发给TCP上传AO，会分配NetTask，启动一个新的会话，管理该命令的处理流程 */
                NetSubmitEvt *pe = Q_NEW(NetSubmitEvt, NET_SUBMIT_SIG);
                pe->cmd_id = pMsg->msg_id;
                pe->target = pMsg->target;
                pe->ack_flag = pMsg->ack_flag;
                pe->data_len = msg_mb_len;
                memcpy(pe->data, msg_mb, msg_mb_len);

                /* 创建新NetTask，启动一个新会话 */
                QACTIVE_POST(AO_NetTaskMgr, &pe->super, (void*)0);

                return 0;

            } else {    /* Server命令，TBOX应答包 */

                /* 转发给相应的NetTask */
                CommAckEvt *pe = Q_NEW(CommAckEvt, COMM_ACKED_SIG);

                u8 session_id = pMsg->session_id;
                assert((session_id >= 0) && (session_id < N_TASK));

                if((session_id >= 0) && (session_id < N_TASK) ) {
                    pe->cmd_id = pMsg->msg_id;
                    pe->target = pMsg->target;
                    pe->ack_flag = pMsg->ack_flag;
                    pe->data_len = msg_mb_len;
                    memcpy(pe->data, msg_mb, msg_mb_len);

                    /* 通知CPU侧串口发送任务应答状态 */
                    QACTIVE_POST(AO_NetTask[session_id], &pe->super, (void*)0);
                }
                log_d("[COMM] post COMM_ACKED_SIG to AO_NetTask[%d], cmd_id = %02x \n", session_id, pe->cmd_id);
            }
        }
#endif
    }
    else if (pMsg->which_data == COMM_Msg_file_info_tag)
    {
        /* MCU不会发送此消息 */
    }
    /** 实时数据 */
    else if (pMsg->which_data == COMM_Msg_real_info_tag)
    {

        /** 拒绝空数据或过长数据 */
        if ((0 == pMsg->data.real_info.data.size) || (pMsg->data.real_info.data.size >= 1200))
        {
            return 0;
        }

#if 0
        /** 回填GPS定位信息 */
        tGPSINFO tGpsInfo;
        memset(&tGpsInfo, 0, sizeof(tGPSINFO));
        GPS_Get_GpsInfo(&tGpsInfo);
        GPS_Inject_GpsInfo_to_RealInfo(pMsg->data.real_info.data.bytes,
                                       pMsg->data.real_info.data.size,
                                       pMsg->session_id, /* GPS位置复用session_id字段指示 */
                                       tGpsInfo.valid,
                                       tGpsInfo.lat,
                                       tGpsInfo.lon,
                                       tGpsInfo.speed,
                                       tGpsInfo.angle);
#endif

        /* hubin 用MCU传来的实时数据模拟CAN Msg */
        /* 消息转发给AO_ProtocolParsing处理，对CAN报文进行解析 */
        // COMMCanMsgEvt *pe = Q_NEW(COMMCanMsgEvt, COMM_CANMSG_SIG);
        // assert(NULL != pe);
        // if (pe)
        // {
        //     pe->can_id = 0x212;
        //     pe->can_data[0] = getMockData();
        //     pe->can_data[1] = getMockData();
        //     pe->can_data[2] = getMockData();
        //     pe->can_data[3] = getMockData();
        //     pe->can_data[4] = getMockData();
        //     pe->can_data[5] = getMockData();
        //     pe->can_data[6] = getMockData();
        //     pe->can_data[7] = getMockData();

        //     QACTIVE_POST(AO_ProtocolParsing, &pe->super, (void *)0);

        //     printf("[COMM] post COMM_CANMSG_SIG to AO_ProtocolParsing");
        // }

#if 0
        // /* 消息转发给AO_RealInfo统一管理发送过程，并根据网络连接状态自动转换成补传数据补发 */
        TCPSendRealEvt *pe = Q_NEW(TCPSendRealEvt, TCP_SENDREAL_SIG);
        assert(NULL != pe);
        if (pe)
        {
            pe->warn = pMsg->data.real_info.warning;
            pe->upl  = pMsg->data.real_info.upl;

            /** 回填好的实时数据提交给上报事件 */
            pe->data_len = pMsg->data.real_info.data.size;
            memcpy(pe->data, pMsg->data.real_info.data.bytes, pe->data_len);

            /*
             * 尚未插入GPS位置信息，在AO_RealInfo中统一插入，为便于区分GPS数据是否已经插入，设此标志位。
             * 如果在此处插入，需要直接访问AO_GPS的gps信息，破坏了模块间的隔离，因此虽然高效但不是好的编程方式。
             */
            // pe->is_gps_injected = 0;

            QACTIVE_POST(AO_RealInfo, &pe->super, (void *)0);

            log_d("[COMM] post TCP_SENDREAL_SIG to AO_RealInfo \n");
        }
#endif

        /** 回复MCU一个命令ECHO消息 */
        COMM_Send_Ack_Echo(0xFF, PROTOCOL_CMD_GB_REALINFO, 0x01);
    }

    /** CAN报文 */
    else if (pMsg->which_data == COMM_Msg_can_data_tag)
    {
#if 0
        const char *msg = "[COMM] Get CAN FRAMES \n";
        PrintHex_v2(1, LOG_TAG, msg, pMsg->data.can_data.data.bytes, pMsg->data.can_data.data.size);
#endif

        // DATA_CANFrames can_frames_out;
        // can_frames_out.frames.funcs.decode = &parse_callback;


        // pb_istream_t stream = pb_istream_from_buffer(pMsg->data.can_data.data.bytes,
        //                                              pMsg->data.can_data.data.size);
        // if (!pb_decode_delimited(&stream, DATA_CANFrames_fields, &can_frames_out))
        // {
        //     log_d("[COMM] CAN decode error! \n");
        //     return -1;
        // }
        // log_d("[COMM] CAN data recv OK! \n");
    }
    /** MCU状态上报 */
    else if (pMsg->which_data == COMM_Msg_mcu_info_tag)
    {
        if (pMsg->data.mcu_info.has_version)
        {
            strcpy(p_sys_param->version_mcu, pMsg->data.mcu_info.version);
            OTAMcuEvt *pe = Q_NEW(OTAMcuEvt, OTA_MCUVER_SIG);
            strcpy(pe->version, pMsg->data.mcu_info.version);
            QACTIVE_POST(AO_NetOTA, &pe->super, (void *)0);
        }

        /* simulate TCP connection unavailable, force real info to post */
        if (pMsg->data.mcu_info.has_sleep_ctrl)
        {
            if (pMsg->data.mcu_info.sleep_ctrl == 0)
            {
                static const QEvt te = {TCP_SIMUON_SIG, 0, 0};
                QACTIVE_POST(AO_TCPSessionMgr, &te, (void *)0);
                log_d("--------------- TCP SIMU ON --------------------------------------- \n");
            }
            else
            {
                static const QEvt te = {TCP_SIMUOFF_SIG, 0, 0};
                QACTIVE_POST(AO_TCPSessionMgr, &te, (void *)0);
                log_d("--------------- TCP SIMU OFF --------------------------------------- \n");
            }
        }

#if 0
        /** MCU强制休眠唤醒信号 */
        if (pMsg->data.mcu_info.has_sleep_ctrl_force)
        {
            if (pMsg->data.mcu_info.sleep_ctrl_force == 1)
            {
                log_i("---------------[COMM] LPM_SLEEPREQ_SIG --------------------------------------- \n");

                param_Set_Sleep_Req(1);

                log_i("[%s] post LPM_SLEEPREQ_SIG to AO_LPMgr \n", MODULE);
                static const QEvt evt_sleep = {LPM_SLEEPREQ_SIG, 0U, 0U};
                QACTIVE_POST(AO_LPMgr, &evt_sleep, (void *)0);
            }
            else
            {
                log_i("---------------[COMM] LPM_WAKEUP_SIG --------------------------------------- \n");
                log_i("[%s] post LPM_WAKEUP_SIG to AO_LPMgr \n", MODULE);
                static const QEvt evt_wakeup = {LPM_WAKEUP_SIG, 0U, 0U};
                QACTIVE_POST(AO_LPMgr, &evt_wakeup, (void *)0);
            }
        }
#else
        /** MCU强制休眠唤醒信号 */
        if (pMsg->data.mcu_info.has_sleep_ctrl_force)
        {
            if (pMsg->data.mcu_info.sleep_ctrl_force == 1)
            {
                // static const QEvt te = {LPM_SLEEPREQ_SIG, 0, 0};
                // QACTIVE_POST(AO_LPMgr, &te, (void*)0);
                printf("---------------[COMM] LPM_SLEEPREQ_SIG ---------------------------------------");

                param_Set_Sleep_Req(1);

                GPS_Stop();

                /* force to shutdown TCP connection */
                // NET_tcp_disconnect();

                /* 广播SLEEP请求信号，待各模块休眠前准备工作完成后再休眠 */
                printf("[%s] publishing evt: LPM_SLEEPREQ_SIG", MODULE);
                static const QEvt evt_sleep = {LPM_SLEEPREQ_SIG, 0U, 0U};
                // QACTIVE_POST(AO_LPMgr, &evt_sleep, (void*)0);
                QF_PUBLISH(&evt_sleep, (void *)0);

                // sleep(10);

                /* defer to LPM to handle sleep proc, 2019.09.17 */
                // /* defer real sleep process till finished */
                // bool brv = lpm_setworkmode(1);
                // const char *pszrv = brv?"success" : "failed";
                // printf("___ set sys tem to low-power mode %s.\r\n", pszrv);
            }
            else
            {
                // static const QEvt te = {LPM_WAKEUP_SIG, 0, 0};
                // QACTIVE_POST(AO_LPMgr, &te, (void*)0);
                printf("---------------[COMM] LPM_WAKEUP_SIG ---------------------------------------");

                bool        brv   = lpm_setworkmode(false);
                const char *pszrv = brv ? "success" : "failed";
                printf("___ set sys tem to wake-up mode %s.\r\n", pszrv);

                GPS_Start();

// 导致导致状态机队列溢出，原因不明，暂时注释掉，待后期查明原因
#    if 1
                /* 广播wakeup信号 */
                printf("[%s] publishing evt: LPM_WAKEUP_SIG", MODULE);
                static const QEvt evt = {LPM_WAKEUP_SIG, 0U, 0U};
                // QACTIVE_POST(AO_LPMgr, &evt, (void*)0);
                QF_PUBLISH(&evt, me);
#    endif
            }
        }
#endif

        if (pMsg->data.mcu_info.has_sn)
        {
            // 保存SN到系统参数文件中
            param_Set_TIN(pMsg->data.mcu_info.sn);
        }
    }
    /** ECHO信息 */
    else if (pMsg->which_data == COMM_Msg_echo_tag)
    {

        /* COMM命令的ECHO确认消息，表示命令已确认接收，只返回给CommMgr */
        CommEchoEvt *pe = Q_NEW(CommEchoEvt, COMM_ECHOED_SIG);
        assert(pe != NULL);
        if (NULL != pe)
        {
            pe->cmd_id   = pMsg->msg_id;   /* ECHO应答命令字 */
            pe->ret      = pMsg->ack_flag; /* 命令接收结果，01：成功，02：拒绝执行或执行失败 */
            pe->block_id = 0xFF;           /* 默认非分块应答包 */

            if (pMsg->data.echo.has_block_id)
            {
                pe->block_id = pMsg->data.echo.block_id;
            }

            /** 升级的ECHO带参数，与普通的ECHO处理不同，此处跳过一般流程，直接交给AO_NetOTA处理 */
            if ((pMsg->msg_id == eCMDEx_OTA_FILEINFO) || (pMsg->msg_id == eCMDEx_OTA_BLOCK))
            {
                QACTIVE_POST(AO_NetOTA, &pe->super, (void *)0);
                log_d("[COMM] post COMM_ECHOED_SIG to AO_NetOTA, cmd_id: %02x \n", pe->cmd_id);
            }
            else if (pMsg->msg_id == eCMDEx_CPU_STATUS)
            {
                QACTIVE_POST(AO_Modem, &pe->super, (void *)0);
                log_d("[COMM] post COMM_ECHOED_SIG to AO_Modem, cmd_id: %02x \n", pe->cmd_id);
            }
            else
            {
                QACTIVE_POST(AO_CommMgr, &pe->super, (void *)0);
                log_d("[COMM] post COMM_ECHOED_SIG to AO_CommMgr, cmd_id: %02x \n", pe->cmd_id);
            }
        }
        return 0;
    }
    else
    {
        /* 非法消息 */
        return -ERR_CPU_COMM_INVALID_MSG;
    }

    return 0;
}

/**
 * @brief  发送数据
 * @note
 * @param  fd: 串口设备文件
 * @param  *pMsg: 待发送消息
 * @retval 0：成功，否则失败
 */
int COMM_send_msg(int fd, COMM_Msg *pMsg)
{
    assert((fd > 0) && (NULL != pMsg));

    if ((fd < 0) || (NULL == pMsg))
    {
        return -ERR_COMMON_ILLEGAL_PARAM;
    }

    /* 以PROTO标准方式发送数据 */
#if COMM_IOTYPE == COMM_PROTO_ENDECODE

    pb_ostream_t output = pb_ostream_from_socket(fd);

    if (!pb_encode_delimited(&output, COMM_Msg_fields, pMsg))
    {
        log_d("Encoding failed: %s \n", PB_GET_ERROR(&output));
        return -1;
    }

    return 0;

#else /* 字符方式发送 */
#    define BUF_SIZE 1000

    static u8 buf[BUF_SIZE];
    u16       u16WrittenBytes = 0;

    /* 待发送数据转换成proto格式的HEX数据 */
    pb_ostream_t output = pb_ostream_from_buffer(buf, BUF_SIZE);

    if (!pb_encode(&output, COMM_Msg_fields, pMsg))
    {
        log_d("Encoding failed: %s \n", PB_GET_ERROR(&output));
        return -1;
    }

    /* 编码后的数据长度 */
    u16WrittenBytes = output.bytes_written;

/* 发送数据，实际发送时，将每个HEX字节转成2个字符，发送 */
#    ifndef BOARD_COMM
    /* 调试机串口发送 */
    PrintHex_v2(1, LOG_TAG, "[ECHO] send data: ", buf, u16WrittenBytes);

    int ret = UART_sendDataAsString(fd, buf, u16WrittenBytes, 0);
    return ret;
#    else
    /* 板上串口发送 */
    PrintHex_v2(1, LOG_TAG, "send data: ", buf, u16WrittenBytes);

    int ret = Serialcomm_SendDataAsString(fd, buf, u16WrittenBytes);
    return ret;
#    endif

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

/**
 * @brief  发送串口自检指令
 * @note
 * @param  fd: fd: 串口设备文件号
 * @retval
 */
int COMM_send_test_msg(int fd)
{
    COMM_Msg msg;

    msg.session_id              = 0xFF;
    msg.msg_id                  = eCMDEx_COMM_HEART;
    msg.target                  = eECUType_TBOX;
    msg.ack_flag                = 0xFE; /* COMM口通信应答命令，扩展 */
    msg.which_data              = COMM_Msg_cmd_param_tag;
    msg.data.cmd_param.has_data = 0; /* 自检无数据 */
    // msg.data.cmd_param.data.size = 0;

    return COMM_send_msg(fd, &msg);
}