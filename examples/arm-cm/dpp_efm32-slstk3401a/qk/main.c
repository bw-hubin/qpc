/*****************************************************************************
* Product: DPP example
* Last updated for version 7.1.0
* Last updated on  2022-08-31
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
#include "dpp.h"
#include "bsp.h"

Q_DEFINE_THIS_FILE

static QTicker l_ticker0;
QActive *the_Ticker0 = &l_ticker0.super;

/*..........................................................................*/
int main() {
    static QEvt const *tableQueueSto[N_PHILO];
    static QEvt const *philoQueueSto[N_PHILO][N_PHILO];
    static QSubscrList subscrSto[MAX_PUB_SIG];
    static QF_MPOOL_EL(TableEvt) smlPoolSto[2*N_PHILO]; /* small pool */
    QTicker_ctor(&l_ticker0, 0U); /* ticker AO for tick rate 0 */

    QF_init();    /* initialize the framework and the underlying RT kernel */
    BSP_init();   /* initialize the Board Support Package */

    /* initialize publish-subscribe... */
    QF_psInit(subscrSto, Q_DIM(subscrSto));

    /* initialize event pools... */
    QF_poolInit(smlPoolSto, sizeof(smlPoolSto), sizeof(smlPoolSto[0]));

    QACTIVE_START(the_Ticker0, Q_PRIO(1U, 1U), 0, 0, 0, 0, 0);

    /* start the active objects... */
    for (uint8_t n = 0U; n < N_PHILO; ++n) {
        Philo_ctor(n); /* instantiate Philo[n] AO */
        QACTIVE_START(AO_Philo[n],   /* AO to start */
            Q_PRIO((n + 2U), 1U),    /* QF-priority/preemption-threshold */
            philoQueueSto[n],        /* event queue storage */
            Q_DIM(philoQueueSto[n]), /* queue length [events] */
            (void *)0,               /* stack storage (not used) */
            0U,                      /* size of the stack [bytes] */
            (QEvt *)0);              /* initialization event */
    }

    Table_ctor(); /* instantiate the Table active object */
    QACTIVE_START(AO_Table,          /* AO to start */
        Q_PRIO((N_PHILO + 2U), 2U),  /* QF-priority/preemption-threshold */
        tableQueueSto,               /* event queue storage */
        Q_DIM(tableQueueSto),        /* queue length [events] */
        (void *)0,                   /* stack storage (not used) */
        0U,                          /* size of the stack [bytes] */
        (QEvt *)0);                  /* initialization event */

    return QF_run(); /* run the QF application */
}

