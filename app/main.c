#include "model/bluetooth.h"
#include "qpc.h"
#include "ring.h"

Q_DEFINE_THIS_FILE

volatile static tRINGBUF ring_BT;
tRINGBUF *ptRing_BT = &ring_BT;

/* 大尺寸事件池内存预分配 */
union LargeEvents
{
    void   *e0; /* 最小事件大小 */
    uint8_t e1[sizeof(BluetoothSendEvt)];
    uint8_t e2[sizeof(BluetoothEchoEvt)];
    uint8_t e3[sizeof(BluetoothSendRetEvt)];
} largeEvts;

int main(int argc, char *argv[]) {
    (void)argc;
    (void *)argv;

    /* statically allocate event queue buffer for the Blinky AO */
    static QEvt const *blinky_queueSto[10];
    static QEvt const *bluetoothMgr_queueSto[100];

    /* 分配publish-subscribe缓区 */
    static QSubscrList subscrSto[MAX_PUB_SIG];

    /* 创建事件存储区 */
    static QF_MPOOL_EL(largeEvts) largePoolSto[500];

    QF_init(); /* initialize the framework */

    /* 初始化P-S缓区 */
    printf("QF_psInit... \n");
    QF_psInit(subscrSto, Q_DIM(subscrSto));

    /* 初始化事件池，用了小尺寸事件池和大尺寸事件池 */
    printf("QF_poolInit... \n");
    QF_poolInit(largePoolSto, sizeof(largePoolSto), sizeof(largePoolSto[0]));

    Blinky_ctor(); /* explicitly call the "constructor" */
    BluetoothMgr_ctor();

    QACTIVE_START(AO_Blinky,
                  1U, /* priority */
                  blinky_queueSto,
                  Q_DIM(blinky_queueSto),
                  (void *)0,
                  0U, /* no stack */
                  (QEvt *)0);    /* no initialization parameter */

    QACTIVE_START(AO_BluetoothMgr,
                  2U, /* priority */
                  bluetoothMgr_queueSto, Q_DIM(bluetoothMgr_queueSto),
                  (void *)0, 0U, /* no stack */
        (QEvt *)0);    /* no initialization parameter */

    return QF_run(); /* run the QF application */
}
