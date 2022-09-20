#include "model/bluetooth.h"
#include "qpc.h"

Q_DEFINE_THIS_FILE

int main(int argc, char *argv[]) {
    (void)argc;
    (void *)argv;

    /* statically allocate event queue buffer for the Blinky AO */
    static QEvt const *blinky_queueSto[10];

    QF_init(); /* initialize the framework */

    Blinky_ctor(); /* explicitly call the "constructor" */
    QACTIVE_START(AO_Blinky,
                  1U, /* priority */
                  blinky_queueSto, Q_DIM(blinky_queueSto),
                  (void *)0, 0U, /* no stack */
                  (void *)0);    /* no initialization parameter */
    return QF_run(); /* run the QF application */
}
