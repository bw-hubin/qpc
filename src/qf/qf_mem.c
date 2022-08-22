/*$file${src::qf::qf_mem.c} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/*
* Model: qpc.qm
* File:  ${src::qf::qf_mem.c}
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
/*$endhead${src::qf::qf_mem.c} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
/*! @file
* @brief ::QMPool implementatin (Memory Pool)
*/
#define QP_IMPL           /* this is QP implementation */
#include "qf_port.h"      /* QF port */
#include "qf_pkg.h"       /* QF package-scope interface */
#include "qassert.h"      /* QP embedded systems-friendly assertions */
#ifdef Q_SPY              /* QS software tracing enabled? */
    #include "qs_port.h"  /* QS port */
    #include "qs_pkg.h"   /* QS facilities for pre-defined trace records */
#else
    #include "qs_dummy.h" /* disable the QS software tracing */
#endif /* Q_SPY */

Q_DEFINE_THIS_MODULE("qf_mem")

/*$skip${QP_VERSION} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/
/* Check for the minimum required QP version */
#if (QP_VERSION < 690U) || (QP_VERSION != ((QP_RELEASE^4294967295U) % 0x3E8U))
#error qpc version 6.9.0 or higher required
#endif
/*$endskip${QP_VERSION} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

/*$define${QF::QMPool} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/

/*${QF::QMPool} ............................................................*/

/*${QF::QMPool::init} ......................................................*/
void QMPool_init(QMPool * const me,
    void * const poolSto,
    uint_fast32_t poolSize,
    uint_fast16_t blockSize)
{
    /*! @pre The memory block must be valid
    * and the poolSize must fit at least one free block
    * and the blockSize must not be too close to the top of the dynamic range
    */
    Q_REQUIRE_ID(100, (poolSto != (void *)0)
            && (poolSize >= (uint_fast32_t)sizeof(QFreeBlock))
            && ((uint_fast16_t)(blockSize + sizeof(QFreeBlock)) > blockSize));

    me->free_head = poolSto;

    /* round up the blockSize to fit an integer # free blocks, no division */
    me->blockSize = (QMPoolSize)sizeof(QFreeBlock);  /* start with just one */

    /* #free blocks that fit in one memory block */
    uint_fast16_t nblocks = 1U;
    while (me->blockSize < (QMPoolSize)blockSize) {
        me->blockSize += (QMPoolSize)sizeof(QFreeBlock);
        ++nblocks;
    }
    blockSize = (uint_fast16_t)me->blockSize; /* round-up to nearest block */

    /* the pool buffer must fit at least one rounded-up block */
    Q_ASSERT_ID(110, poolSize >= blockSize);

    /* chain all blocks together in a free-list... */
    poolSize -= (uint_fast32_t)blockSize; /* don't count the last block */
    me->nTot  = 1U; /* the last block already in the pool */

    /* start at the head of the free list */
    QFreeBlock *fb = (QFreeBlock *)me->free_head;

    /* chain all blocks together in a free-list... */
    while (poolSize >= (uint_fast32_t)blockSize) {
        fb->next = &fb[nblocks]; /* point next link to next block */
        fb = fb->next;           /* advance to the next block */
        poolSize -= (uint_fast32_t)blockSize; /* reduce available pool size */
        ++me->nTot;              /* increment the number of blocks so far */
    }

    fb->next  = (QFreeBlock *)0; /* the last link points to NULL */
    me->nFree = me->nTot;        /* all blocks are free */
    me->nMin  = me->nTot;        /* the minimum number of free blocks */
    me->start = poolSto;         /* the original start this pool buffer */
    me->end   = fb;              /* the last block in this pool */
}

/*${QF::QMPool::get} .......................................................*/
void * QMPool_get(QMPool * const me,
    uint_fast16_t const margin,
    uint_fast8_t const qs_id)
{
    Q_UNUSED_PAR(qs_id); /* when Q_SPY undefined */

    QF_CRIT_STAT_
    QF_CRIT_E_();

    /* have more free blocks than the requested margin? */
    QFreeBlock *fb;
    if (me->nFree > (QMPoolCtr)margin) {
        void *fb_next;
        fb = (QFreeBlock *)me->free_head; /* get a free block */

        /* the pool has some free blocks, so a free block must be available */
        Q_ASSERT_CRIT_(310, fb != (QFreeBlock *)0);

        fb_next = fb->next; /* put volatile to a temporary to avoid UB */

        /* is the pool becoming empty? */
        --me->nFree; /* one less free block */
        if (me->nFree == 0U) {
            /* pool is becoming empty, so the next free block must be NULL */
            Q_ASSERT_CRIT_(320, fb_next == (QFreeBlock *)0);

            me->nMin = 0U; /* remember that the pool got empty */
        }
        else {
            /* pool is not empty, so the next free block must be in range
            *
            * NOTE: the next free block pointer can fall out of range
            * when the client code writes past the memory block, thus
            * corrupting the next block.
            */
            Q_ASSERT_CRIT_(330, QF_PTR_RANGE_(fb_next, me->start, me->end));

            /* is the number of free blocks the new minimum so far? */
            if (me->nMin > me->nFree) {
                me->nMin = me->nFree; /* remember the new minimum */
            }
        }

        me->free_head = fb_next; /* set the head to the next free block */

        QS_BEGIN_NOCRIT_PRE_(QS_QF_MPOOL_GET, qs_id)
            QS_TIME_PRE_();         /* timestamp */
            QS_OBJ_PRE_(me);        /* this memory pool */
            QS_MPC_PRE_(me->nFree); /* # of free blocks in the pool */
            QS_MPC_PRE_(me->nMin);  /* min # free blocks ever in the pool */
        QS_END_NOCRIT_PRE_()
    }
    /* don't have enough free blocks at this point */
    else {
        fb = (QFreeBlock *)0;

        QS_BEGIN_NOCRIT_PRE_(QS_QF_MPOOL_GET_ATTEMPT, qs_id)
            QS_TIME_PRE_();         /* timestamp */
            QS_OBJ_PRE_(me);        /* this memory pool */
            QS_MPC_PRE_(me->nFree); /* # of free blocks in the pool */
            QS_MPC_PRE_(margin);    /* the requested margin */
        QS_END_NOCRIT_PRE_()
    }
    QF_CRIT_X_();

    return fb;  /* return the block or NULL pointer to the caller */
}

/*${QF::QMPool::put} .......................................................*/
void QMPool_put(QMPool * const me,
    void * const b,
    uint_fast8_t const qs_id)
{
    Q_UNUSED_PAR(qs_id); /* when Q_SPY undefined */

    /*! @pre # free blocks cannot exceed the total # blocks and
    * the block pointer must be from this pool.
    */
    Q_REQUIRE_ID(200, (me->nFree < me->nTot)
                      && QF_PTR_RANGE_(b, me->start, me->end));

    QF_CRIT_STAT_
    QF_CRIT_E_();
    ((QFreeBlock *)b)->next = (QFreeBlock *)me->free_head;/* link into list */
    me->free_head = b;      /* set as new head of the free list */
    ++me->nFree;            /* one more free block in this pool */

    QS_BEGIN_NOCRIT_PRE_(QS_QF_MPOOL_PUT, qs_id)
        QS_TIME_PRE_();         /* timestamp */
        QS_OBJ_PRE_(me);        /* this memory pool */
        QS_MPC_PRE_(me->nFree); /* the number of free blocks in the pool */
    QS_END_NOCRIT_PRE_()

    QF_CRIT_X_();
}
/*$enddef${QF::QMPool} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
