/*$file${include::qk.h} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/*
* Model: qpc.qm
* File:  ${include::qk.h}
*
* This code has been generated by QM 5.2.1 <www.state-machine.com/qm>.
* DO NOT EDIT THIS FILE MANUALLY. All your changes will be lost.
*
* This code is covered by the following QP license:
* License #    : LicenseRef-QL-dual
* Issued to    : Any user of the QP/C real-time embedded framework
* Framework(s) : qpc
* Support ends : 2023-12-31
* License scope:
*
* Copyright (C) 2005 Quantum Leaps, LLC <state-machine.com>.
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
* <www.state-machine.com/licensing>
* <info@state-machine.com>
*/
/*$endhead${include::qk.h} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
/*! @file
* @brief QK/C (preemptive non-blocking kernel) platform-independent
* public interface.
*/
#ifndef QK_H
#define QK_H

/*==========================================================================*/
/* QF configuration for QK -- data members of the QActive class... */

/* QK event-queue used for AOs */
#define QF_EQUEUE_TYPE      QEQueue

/* QK thread type used for AOs
* QK uses this member to store the private Thread-Local Storage pointer.
*/
#define QF_THREAD_TYPE      void*

#include "qequeue.h"  /* QK kernel uses the native QP event queue */
#include "qmpool.h"   /* QK kernel uses the native QP memory pool */
#include "qf.h"       /* QF framework integrates directly with QK */

/*==========================================================================*/
/*$declare${QK::QK-base} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/

/*${QK::QK-base::Attr} .....................................................*/
/*! @brief QK preemptive non-blocking kernel
* @class QK
*/
typedef struct QK_Attr {
    uint8_t volatile actPrio;    /*!< QF prio of the active AO */
    uint8_t volatile nextPrio;   /*!< QF prio of the next AO to execute */
    uint8_t volatile actThre;    /*!< active preemption-threshold */
    uint8_t volatile lockCeil;   /*!< lock preemption-ceiling (0==no-lock) */
    uint8_t volatile lockHolder; /*!< QF prio of the AO holding the lock */
} QK;

/*${QK::QK-base::attr_} ....................................................*/
/*! attributes of the QK kernel */
extern QK QK_attr_;

/*${QK::QK-base::idle_} ....................................................*/
/*! Idle AO in the QK kernel */
extern QActive const QK_idle_;

/*${QK::QK-base::activate_} ................................................*/
/*! QK activator activates the next active object. The activated AO preempts
* the currently executing AOs.
* @static @private @memberof QK
*
* @details
* QK_activate_() activates ready-to run AOs that are above the initial
* active priority (QK_attr_.actPrio).
*
* @note
* The activator might enable interrupts internally, but always returns with
* interrupts **disabled**.
*/
void QK_activate_(void);

/*${QK::QK-base::sched_} ...................................................*/
/*! QK scheduler finds the highest-priority AO ready to run
* @static @private @memberof QK
*
* @details
* The QK scheduler finds out the priority of the highest-priority AO
* that (1) has events to process and (2) has priority that is above the
* current priority.
*
* @returns
* The QF priority of the the active object, or zero if no eligible
* AO is ready to run.
*
* @attention
* QK_sched_() must be always called with interrupts **disabled** and
* returns with interrupts **disabled**.
*/
uint_fast8_t QK_sched_(void);

/*${QK::QK-base::schedLock} ................................................*/
/*! QK selective scheduler lock
*
* @details
* This function locks the QK scheduler to the specified ceiling.
*
* @param[in] ceiling  preemption ceiling to which the QK scheduler
*                     needs to be locked
*
* @returns
* The previous QK Scheduler lock status, which is to be used to unlock
* the scheduler by restoring its previous lock status in
* QK_schedUnlock().
*
* @note
* QK_schedLock() must be always followed by the corresponding
* QK_schedUnlock().
*
* @sa QK_schedUnlock()
*
* @usage
* The following example shows how to lock and unlock the QK scheduler:
* @include qk_lock.c
*/
QSchedStatus QK_schedLock(uint_fast8_t const ceiling);

/*${QK::QK-base::schedUnlock} ..............................................*/
/*! QK selective scheduler unlock
*
* @details
* This function unlocks the QK scheduler to the previous status.
*
* @param[in]   stat       previous QK Scheduler lock status returned from
*                         QK_schedLock()
* @note
* QK_schedUnlock() must always follow the corresponding
* QK_schedLock().
*
* @sa QK_schedLock()
*
* @usage
* The following example shows how to lock and unlock the QK scheduler:
* @include qk_lock.c
*/
void QK_schedUnlock(QSchedStatus const stat);

/*${QK::QK-base::onIdle} ...................................................*/
/*! QK idle callback (customized in BSPs for QK)
* @static @public @memberof QK
*
* @details
* QK_onIdle() is called continuously by the QK idle loop. This callback
* gives the application an opportunity to enter a power-saving CPU mode,
* or perform some other idle processing.
*
* @note
* QK_onIdle() is invoked with interrupts enabled and must also return with
* interrupts enabled.
*/
void QK_onIdle(void);

/*${QK::QK-base::onContextSw} ..............................................*/
#ifdef QK_ON_CONTEXT_SW
/*! QK context switch callback (customized in BSPs for QK)
* @static @public @memberof QK
*
* @details
* This callback function provides a mechanism to perform additional
* custom operations when QK switches context from one thread to
* another.
*
* @param[in] prev   pointer to the previous thread (active object)
*                   (prev==0 means that @p prev was the QK idle loop)
* @param[in] next   pointer to the next thread (active object)
*                   (next==0) means that @p next is the QK idle loop)
* @attention
* QK_onContextSw() is invoked with interrupts **disabled** and must also
* return with interrupts **disabled**.
*
* @note
* This callback is enabled by defining the macro #QK_ON_CONTEXT_SW.
*
* @include qk_oncontextsw.c
*/
void QK_onContextSw(
    QActive * prev,
    QActive * next);
#endif /* def QK_ON_CONTEXT_SW */
/*$enddecl${QK::QK-base} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

/*==========================================================================*/
/* interface used only inside QF, but not in applications */
#ifdef QP_IMPL
/* QK-specific scheduler locking and event queue... */
/*$declare${QK-impl} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/

/*${QK-impl::QK_ISR_CONTEXT_} ..............................................*/
#ifndef QK_ISR_CONTEXT_
/*! Internal macro that reports the execution context (ISR vs. thread)
*
* @returns true if the code executes in the ISR context and false
* otherwise
*/
#define QK_ISR_CONTEXT_() (QF_intNest_ != 0U)
#endif /* ndef QK_ISR_CONTEXT_ */

/*${QK-impl::QF_SCHED_STAT_} ...............................................*/
/*! QK scheduler lock status */
#define QF_SCHED_STAT_ QSchedStatus lockStat_;

/*${QK-impl::QF_SCHED_LOCK_} ...............................................*/
/*! QK selective scheduler locking */
#define QF_SCHED_LOCK_(prio_) do { \
    if (QK_ISR_CONTEXT_()) { \
        lockStat_ = 0xFFU; \
    } else { \
        lockStat_ = QK_schedLock((prio_)); \
    } \
} while (false)

/*${QK-impl::QF_SCHED_UNLOCK_} .............................................*/
/*! QK selective scheduler unlocking */
#define QF_SCHED_UNLOCK_() do { \
    if (lockStat_ != 0xFFU) { \
        QK_schedUnlock(lockStat_); \
    } \
} while (false)

/*${QK-impl::QACTIVE_EQUEUE_WAIT_} .........................................*/
/*! QK native event queue waiting */
#define QACTIVE_EQUEUE_WAIT_(me_) \
    (Q_ASSERT_ID(110, (me_)->eQueue.frontEvt != (QEvt *)0))

/*${QK-impl::QACTIVE_EQUEUE_SIGNAL_} .......................................*/
/*! QK native event queue signaling */
#define QACTIVE_EQUEUE_SIGNAL_(me_) do { \
    QPSet_insert(&QF_readySet_, (uint_fast8_t)(me_)->prio); \
    if (!QK_ISR_CONTEXT_()) { \
        if (QK_sched_() != 0U) { \
            QK_activate_(); \
        } \
    } \
} while (false)
/*$enddecl${QK-impl} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

/* Native QF event pool operations... */
/*$declare${QF-QMPool-impl} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/

/*${QF-QMPool-impl::QF_EPOOL_TYPE_} ........................................*/
#define QF_EPOOL_TYPE_ QMPool

/*${QF-QMPool-impl::QF_EPOOL_INIT_} ........................................*/
#define QF_EPOOL_INIT_(p_, poolSto_, poolSize_, evtSize_) \
    (QMPool_init(&(p_), (poolSto_), (poolSize_), (evtSize_)))

/*${QF-QMPool-impl::QF_EPOOL_EVENT_SIZE_} ..................................*/
#define QF_EPOOL_EVENT_SIZE_(p_) ((uint_fast16_t)(p_).blockSize)

/*${QF-QMPool-impl::QF_EPOOL_GET_} .........................................*/
#define QF_EPOOL_GET_(p_, e_, m_, qs_id_) \
    ((e_) = (QEvt *)QMPool_get(&(p_), (m_), (qs_id_)))

/*${QF-QMPool-impl::QF_EPOOL_PUT_} .........................................*/
#define QF_EPOOL_PUT_(p_, e_, qs_id_) \
    (QMPool_put(&(p_), (e_), (qs_id_)))
/*$enddecl${QF-QMPool-impl} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
#endif /* QP_IMPL */

#endif /* QK_H */
