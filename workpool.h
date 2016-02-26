#ifndef __WORKPOOL_H__
#define __WORKPOOL_H__

#include "thread_pool.h"

#define WORKPOOL_DEF_MIN    5
#define WORKPOOL_DEF_MAX    100

typedef void (*WorkJobT)(void *arg);

class WorkPool
{
public:
    WorkPool(unsigned min = WORKPOOL_DEF_MIN, unsigned max = WORKPOOL_DEF_MAX);
    virtual ~WorkPool();
    
    int DoJob(WorkJobT job, void *arg);
    float GetBusyThreshold(void);
    int SetBusyThreshold(float bt);
    unsigned GetManageInterval(void);
    int SetManageInterval(unsigned mi);

protected:

private:

    int InitInner(void);

    unsigned mMinNr, mMaxNr;
    TpThreadPool *mPool;

};

#endif // __WORKPOOL_H__

