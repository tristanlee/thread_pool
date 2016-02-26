/**
 * @file workpool.c
 * @version 1.0
 * @author Tristan Lee <tristan.lee@qq.com>
 * @brief A cpp interface of Thread Pool
 * 
 * 
 * Change Logs:
 * Date			Author		Notes
 * 2016-01-13	Tristan		the initial version
 *
 */

#include "workpool.h"

WorkPool::WorkPool(unsigned min, unsigned max)
{
    mMinNr = min;
    mMaxNr = max;
    mPool = NULL;
    InitInner();
}

WorkPool::~WorkPool(void)
{
    tp_close(mPool, 1);
    mPool = NULL;
}

int WorkPool::InitInner(void)
{    
    mPool = tp_create(mMinNr, mMaxNr);
    if (!mPool) return -1;

    return 0;
}

int WorkPool::DoJob(WorkJobT job, void *arg)
{
    return tp_process_job(mPool, (process_job)job, arg);
}

float WorkPool::GetBusyThreshold(void)
{
    return tp_get_busy_threshold(mPool);
}

int WorkPool::SetBusyThreshold(float bt)
{
    return tp_set_busy_threshold(mPool, bt);
}

unsigned WorkPool::GetManageInterval(void)
{
    return tp_get_manage_interval(mPool);
}

int WorkPool::SetManageInterval(unsigned mi)
{
    return tp_set_manage_interval(mPool, mi);
}

