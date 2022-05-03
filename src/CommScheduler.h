#ifndef _COMMSCHEDULER_H_
#define _COMMSCHEDULER_H_

#include "Communicator.h"

// wrapper of comm
class CommScheduler
{
public:
	virtual ~CommScheduler() = default;
	
private:
	Communicator comm;
};


#endif

