/*****************************************************************************
* Model: bwgpsrecorder.qm
* File:  qmcode/qcan.c
*
* This code has been generated by QM tool (see state-machine.com/qm).
* DO NOT EDIT THIS FILE MANUALLY. All your changes will be lost.
*
* This program is open source software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published
* by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*****************************************************************************/
/* @(/3/8) .................................................................*/
#include "qp_port.h"
#include "bsp.h"
#include "stm32_gpio.h"
#include "can.h"
#include "parameter.h"
#include "qevents.h"
#include "type.h"

Q_DEFINE_THIS_MODULE("qcan.c")

#define _test_can_free_ //����CAN�������ߣ��̳�ʱ

#define TIMEOUT_SERVICEHANGUP_TICKS   (BSP_TICKS_PER_SEC * 3600 * 2)  //�������ʱ2H
#ifndef _test_can_free_
#define TIMEOUT_CAN_FREE_SECONDS  (30 * 60)  //CAN���߿�������ʱ����30��
#else
#define TIMEOUT_CAN_FREE_SECONDS  (10)//CAN���߿�������ʱ����10s������
#endif
#define TIMEOUT_RETRIEVE_TICKS  (BSP_TICKS_PER_SEC / 100)

/* Active object class -----------------------------------------------------*/
/* @(/1/7) .................................................................*/
typedef struct QCANTag {
/* protected: */
    QActive super;

/* private: */
    QTimeEvt m_rptTimer;
    QTimeEvt m_tickTimer;
    QTimeEvt m_hangTimer;
    uint8_t m_bRecvFrame;
    uint32_t m_canfreeTicks;
    uint8_t m_ledFlashCnt;
} QCAN;

/* protected: */
static QState QCAN_initial(QCAN * const me, QEvt const * const e);
static QState QCAN_Normal_Mode(QCAN * const me, QEvt const * const e);
static QState QCAN_non_conflicting(QCAN * const me, QEvt const * const e);
static QState QCAN_ACC_Off(QCAN * const me, QEvt const * const e);
static QState QCAN_ACC_On(QCAN * const me, QEvt const * const e);
static QState QCAN_Sleep_Mode(QCAN * const me, QEvt const * const e);



/* Local objects -----------------------------------------------------------*/
static QCAN l_Can; /* the single instance of the Table active object */

/* Global-scope objects ----------------------------------------------------*/
QActive * const AO_Can = &l_Can.super; /* "opaque" AO pointer */

/*..........................................................................*/
/* @(/1/31) ................................................................*/
void QCAN_ctor(void) {
    QCAN *me = &l_Can;
    QActive_ctor(&me->super, Q_STATE_CAST(&QCAN_initial));

    //QTimeEvt_ctor(&me->m_rptTimer, VMS_DATAFLOW_TIMEOUT_SIG);
    //QTimeEvt_ctor(&me->m_tickTimer, PER_SECOND_SIG);
    //QTimeEvt_ctor(&me->m_hangTimer, CAN_CONFLICT_TIMEOUT_SIG);
}
/* @(/1/7) .................................................................*/
/* @(/1/7/6) ...............................................................*/
/* @(/1/7/6/0) */
static QState QCAN_initial(QCAN * const me, QEvt const * const e) {
    QActive_subscribe(&me->super, ACC_ON_SIG);
    QActive_subscribe(&me->super, ACC_OFF_SIG);
    QActive_subscribe(&me->super, WAKEUP_REQ_SIG);
    QActive_subscribe(&me->super, SLEEP_REQ_SIG);

    CAN_MemInit(); //��ʼ��


    return Q_TRAN(&QCAN_Normal_Mode);
}
/* @(/1/7/6/1) .............................................................*/
static QState QCAN_Normal_Mode(QCAN * const me, QEvt const * const e) {
    QState status_;
    switch (e->sig) {
        /* @(/1/7/6/1) */
        case Q_ENTRY_SIG: {
            CAN1_PWR_ON();//ģ�鿪��
            CAN_RECIEVE_ENABLE();//ʹ�ܽ���

            me->m_bRecvFrame = 0;
            me->m_canfreeTicks = 0;
            me->m_ledFlashCnt = 0;

            /* ʹ���붨ʱ�� */
            QTimeEvt_postEvery(&me->m_tickTimer, &me->super, (BSP_TICKS_PER_SEC));
            status_ = Q_HANDLED();
            break;
        }
        /* @(/1/7/6/1) */
        case Q_EXIT_SIG: {
            QTimeEvt_disarm(&me->m_tickTimer);
            status_ = Q_HANDLED();
            break;
        }
        /* @(/1/7/6/1/0) */
        case Q_INIT_SIG: {
            status_ = Q_TRAN(&QCAN_non_conflicting);
            break;
        }
        /* @(/1/7/6/1/1) */
        case PER_SECOND_SIG: {
            if(1 == me->m_bRecvFrame)
            {
                if(me->m_ledFlashCnt++ % 3 == 0)  //���Ƶ���˸��3s
                {
                    CAN_FLASH_LED();
                }
                me->m_bRecvFrame = 0;
                me->m_canfreeTicks = 0;
            }
            else
            {
                me->m_canfreeTicks ++;

            //    if(me->m_canfreeTicks > TIMEOUT_CAN_FREE_SECONDS)
            //    {
                    //����CANģ������
            //        static const QEvent sleep_Evt = {CAN_RXIDLE_TIMEOUT_SIG, 0};
            //        QACTIVE_POST(AO_Can, (QEvt*)&sleep_Evt, (void*)0);
                    CAN_LED_OFF();
            //    }
            }

            status_ = Q_HANDLED();
            break;
        }
        /* @(/1/7/6/1/2) */
        case CAN_NEWRECVMSG_SIG: {
            DATANODE *pNode = CAN_TakeRxMsg();
            MP_FreeNode(pNode);
            status_ = Q_HANDLED();
            break;
        }
        default: {
            status_ = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status_;
}
/* @(/1/7/6/1/3) ...........................................................*/
static QState QCAN_non_conflicting(QCAN * const me, QEvt const * const e) {
    QState status_;
    switch (e->sig) {
        /* @(/1/7/6/1/3/0) */
        case Q_INIT_SIG: {
            status_ = Q_TRAN(&QCAN_ACC_Off);
            break;
        }
        /* @(/1/7/6/1/3/1) */
        case CAN_NEWRECVMSG_SIG: {
            //��ȡCAN֡������
            DATANODE *pNode = CAN_TakeRxMsg();
            if(pNode)
            {
                ///doDispatchMsg(pNode);
            }

            me->m_bRecvFrame = 1;
            status_ = Q_HANDLED();
            break;
        }
        default: {
            status_ = Q_SUPER(&QCAN_Normal_Mode);
            break;
        }
    }
    return status_;
}
/* @(/1/7/6/1/3/2) .........................................................*/
static QState QCAN_ACC_Off(QCAN * const me, QEvt const * const e) {
    QState status_;
    switch (e->sig) {
        /* @(/1/7/6/1/3/2/0) */
        case ACC_ON_SIG: {
            status_ = Q_TRAN(&QCAN_ACC_On);
            break;
        }
        /* @(/1/7/6/1/3/2/1) */
        case PER_SECOND_SIG: {
            if(1 == me->m_bRecvFrame)
            {
                if(me->m_ledFlashCnt++ % 3 == 0)  //���Ƶ���˸��3s
                {
                    CAN_FLASH_LED();
                }
                me->m_bRecvFrame = 0;
                me->m_canfreeTicks = 0;
            }
            else
            {
                me->m_canfreeTicks ++;

            //    if(me->m_canfreeTicks > TIMEOUT_CAN_FREE_SECONDS)
            //    {
                    //����CANģ������
            //        static const QEvent sleep_Evt = {CAN_RXIDLE_TIMEOUT_SIG, 0};
            //        QACTIVE_POST(AO_Can, (QEvt*)&sleep_Evt, (void*)0);
                    CAN_LED_OFF();
            //    }
            }


            /* @(/1/7/6/1/3/2/1/0) */
            if (me->m_canfreeTicks > TIMEOUT_CAN_FREE_SECONDS) {
                //ϵͳ����������ACC OFF��CAN����������ʱ������ָ��ʱ��
                static const QEvent sleep_Evt = {SLEEP_REQ_SIG, 0};
                QF_publish(&sleep_Evt, me); //�㲥˯��������Ϣ��֪ͨ����ģ���������̬

                TRACE_(QS_USER, NULL, "[CAN] ----- PowerMgr: SLEEP -----");
                status_ = Q_TRAN(&QCAN_Sleep_Mode);
            }
            /* @(/1/7/6/1/3/2/1/1) */
            else {
                status_ = Q_HANDLED();
            }
            break;
        }
        default: {
            status_ = Q_SUPER(&QCAN_non_conflicting);
            break;
        }
    }
    return status_;
}
/* @(/1/7/6/1/3/3) .........................................................*/
static QState QCAN_ACC_On(QCAN * const me, QEvt const * const e) {
    QState status_;
    switch (e->sig) {
        /* @(/1/7/6/1/3/3) */
        case Q_ENTRY_SIG: {
            /* ʹ����������ʱ�ϴ���ʱ�� */
            ///QTimeEvt_postEvery(&me->m_rptTimer, &me->super, (BSP_TICKS_PER_SEC * ptWorkParam->u16AutoReportCycle));
            status_ = Q_HANDLED();
            break;
        }
        /* @(/1/7/6/1/3/3) */
        case Q_EXIT_SIG: {
            ///QTimeEvt_disarm(&me->m_rptTimer);
            status_ = Q_HANDLED();
            break;
        }
        /* @(/1/7/6/1/3/3/0) */
        case ACC_OFF_SIG: {
            status_ = Q_TRAN(&QCAN_ACC_Off);
            break;
        }
        default: {
            status_ = Q_SUPER(&QCAN_non_conflicting);
            break;
        }
    }
    return status_;
}
/* @(/1/7/6/2) .............................................................*/
static QState QCAN_Sleep_Mode(QCAN * const me, QEvt const * const e) {
    QState status_;
    switch (e->sig) {
        /* @(/1/7/6/2) */
        case Q_ENTRY_SIG: {
            CAN1_PWR_OFF();//ģ��ص�
            SetSysSleepState(MODULE_SLEEP_STATE_CAN);//����ģ��˯��̬��־
            status_ = Q_HANDLED();
            break;
        }
        /* @(/1/7/6/2/0) */
        case WAKEUP_REQ_SIG: {
            TRACE_(QS_USER, NULL, "[CAN] ----- PowerMgr: WAKEUP -----");
            ClearSysSleepState(MODULE_SLEEP_STATE_CAN);//���ģ������̬
            status_ = Q_TRAN(&QCAN_Normal_Mode);
            break;
        }
        default: {
            status_ = Q_SUPER(&QHsm_top);
            break;
        }
    }
    return status_;
}
