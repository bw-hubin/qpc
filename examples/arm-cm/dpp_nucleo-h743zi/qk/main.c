/*============================================================================
* QP/C Real-Time Embedded Framework (RTEF)
* Copyright (C) 2005 Quantum Leaps, LLC. All rights reserved.
*
* SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-QL-commercial
*
* This software is dual-licensed under the terms of the open source GNU
* General Public License version 3 (or any later version), or alternatively,
* under the terms of one of the closed source Quantum Leaps commercial
* licenses.
*
* The terms of the open source GNU General Public License version 3
* can be found at: <www.gnu.org/licenses/gpl-3.0>
*
* The terms of the closed source Quantum Leaps commercial licenses
* can be found at: <www.state-machine.com/licensing>
*
* Redistributions in source code must retain this top-level comment block.
* Plagiarizing this software to sidestep the license obligations is illegal.
*
* Contact information:
* <www.state-machine.com>
* <info@state-machine.com>
============================================================================*/
/*!
* @date Last updated on: 2022-08-28
* @version Last updated for: @ref qpc_7_1_0
*
* @file
* @brief DPP example
*/
#include "qpc.h"
#include "dpp.h"
#include "bsp.h"

Q_DEFINE_THIS_FILE

/*..........................................................................*/
int main() {
    static QEvt const *tableQueueSto[N_PHILO];
    static QEvt const *philoQueueSto[N_PHILO][N_PHILO];
    static QSubscrList subscrSto[MAX_PUB_SIG];
    static QF_MPOOL_EL(TableEvt) smlPoolSto[2*N_PHILO]; /* small pool */

    QF_init();    /* initialize the framework and the underlying RT kernel */
    BSP_init();   /* initialize the Board Support Package */

    /* initialize publish-subscribe... */
    QF_psInit(subscrSto, Q_DIM(subscrSto));

    /* initialize event pools... */
    QF_poolInit(smlPoolSto, sizeof(smlPoolSto), sizeof(smlPoolSto[0]));

    /* start the active objects... */
    Philo_ctor(); /* instantiate all Philosopher active objects */
    for (uint8_t n = 0U; n < N_PHILO; ++n) {
        QACTIVE_START(AO_Philo[n], /* AO to start */
            Q_PRIO(n + 1U, 1U),    /* QF-priority/preemption-thre. */
            philoQueueSto[n],      /* event queue storage */
            Q_DIM(philoQueueSto[n]), /* queue length [events] */
            (void *)0,             /* stack storage (not used) */
            0U,                    /* size of the stack [bytes] */
            (void *)0);            /* initialization param */
    }
    Table_ctor(); /* instantiate the Table active object */
    QACTIVE_START(AO_Table,        /* AO to start */
        Q_PRIO(N_PHILO + 1U, 2U),  /* QF-priority/preemption-thre. */
        tableQueueSto,             /* event queue storage */
        Q_DIM(tableQueueSto),      /* queue length [events] */
        (void *)0,                 /* stack storage (not used) */
        0U,                        /* size of the stack [bytes] */
        (void *)0);                /* initialization param */

    return QF_run(); /* run the QF application */
}

