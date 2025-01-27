/*****************************************************************************
* Product: QUTEST fixture for the DPP components
* Last updated for: @ref qpc_7_0_0
* Last updated on  2022-02-16
*
*                    Q u a n t u m  L e a P s
*                    ------------------------
*                    Modern Embedded Software
*
* Copyright (C) 2005 Quantum Leaps, LLC. All rights reserved.
*
* This program is open source software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* Alternatively, this program may be distributed and modified under the
* terms of Quantum Leaps commercial licenses, which expressly supersede
* the GNU General Public License and are specifically designed for
* licensees interested in retaining the proprietary status of their code.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <www.gnu.org/licenses/>.
*
* Contact information:
* <www.state-machine.com/licensing>
* <info@state-machine.com>
*****************************************************************************/
#include "qpc.h"
#include "bsp.h"
#include "dpp.h"

//#include "safe_std.h" /* portable "safe" <stdio.h>/<string.h> facilities */

Q_DEFINE_THIS_FILE

/*..........................................................................*/
/* dummy Philo HSMs... */
static QHsmDummy l_dummyPhilo[N_PHILO];
QHsm * const SM_Philo[N_PHILO] = { /* opaque pointers to Philo instances */
    &l_dummyPhilo[0].super,
    &l_dummyPhilo[1].super,
    &l_dummyPhilo[2].super,
    &l_dummyPhilo[3].super,
    &l_dummyPhilo[4].super
};
void Philo_ctor(uint8_t n) {
    QHsmDummy_ctor(&l_dummyPhilo[n]);
}

/*..........................................................................*/
int main(int argc, char *argv[]) {
    static QEvt const *tableQueueSto[N_PHILO];
    static QSubscrList subscrSto[MAX_PUB_SIG];
    static QF_MPOOL_EL(TableEvt) smlPoolSto[2*N_PHILO]; /* small pool */

    QF_init();    /* initialize the framework and the underlying RT kernel */
    BSP_init(argc, argv); /* initialize the Board Support Package */

    /* pause execution of the test and wait for the test script to continue */
    QS_TEST_PAUSE();

    /* initialize publish-subscribe... */
    QF_psInit(subscrSto, Q_DIM(subscrSto));

    /* initialize event pools... */
    QF_poolInit(smlPoolSto, sizeof(smlPoolSto), sizeof(smlPoolSto[0]));

    /* start the active objects... */
    Table_ctor(); /* instantiate the Table active object */
    QACTIVE_START(AO_Table,             /* AO to start */
                  Q_PRIO(2U, 1U),       /* QF-priority/preemption-threshold */
                  tableQueueSto,        /* event queue storage */
                  Q_DIM(tableQueueSto), /* queue length [events] */
                  (void *)0,            /* stack storage (not used) */
                  0U,                   /* size of the stack [bytes] */
                  (void *)0);           /* initialization param */

    return QF_run(); /* run the QF application */
}

/*..........................................................................*/
void QS_onTestSetup(void) {
    //PRINTF_S("%s\n", "QS_onTestSetup");
}
void QS_onTestTeardown(void) {
    //PRINTF_S("%s\n", "QS_onTestTeardown");
}

/*..........................................................................*/
/*! callback function to execute user commands */
void QS_onCommand(uint8_t cmdId,
                  uint32_t param1, uint32_t param2, uint32_t param3)
{
    (void)cmdId;
    (void)param1;
    (void)param2;
    (void)param3;

    switch (cmdId) {
       case 0U: {
           QEvt const e = { PAUSE_SIG, 0U, 0U };
           QHSM_DISPATCH(&AO_Table->super, &e, AO_Table->prio);
           break;
       }
       case 1U: {
           QEvt const e = { SERVE_SIG, 0U, 0U };
           QHSM_DISPATCH(&AO_Table->super, &e, AO_Table->prio);
           break;
       }
       default:
           break;
    }
}

/****************************************************************************/
/*! Host callback function to "massage" the event, if necessary */
void QS_onTestEvt(QEvt *e) {
    (void)e;
#ifdef Q_HOST  /* is this test compiled for a desktop Host computer? */
#else /* embedded Target */
#endif /* embedded Target */
}
/*..........................................................................*/
/*! callback function to output the posted QP events (not used here) */
void QS_onTestPost(void const *sender, QActive *recipient,
                   QEvt const *e, bool status)
{
    (void)sender;
    (void)status;
    switch (e->sig) {
        case EAT_SIG:
        case DONE_SIG:
        case HUNGRY_SIG:
            QS_BEGIN_ID(QUTEST_ON_POST, 0U) /* app-specific record */
                QS_SIG(e->sig, recipient);
                QS_U8(0, Q_EVT_CAST(TableEvt)->philoNum);
            QS_END()
            break;
        default:
            break;
    }
}
