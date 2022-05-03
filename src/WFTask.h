#ifndef _WFTASK_H_
#define _WFTASK_H_

template<class REQ, class RESP>
class WFNetworkTask : public CommRequest
{
protected:
	virtual ~WFNetworkTask() = default;
};

#endif
