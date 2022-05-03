#ifndef _WFTASKFACTORY_H_
#define _WFTASKFACTORY_H_

#include <functional>

#include "HttpMessage.h"
#include "WFTask.h"

using WFHttpTask = WFNetworkTask<protocol::HttpRequest,
								 protocol::HttpResponse>;
using http_callback_t = std::function<void (WFHttpTask *)>;

template<class REQ, class RESP>
class WFNetworkTaskFactory
{
private:
	using T = WFNetworkTask<REQ, RESP>;

public:
	static T *create_server_task(CommService *service,
								 std::function<void (T *)>& process)
	{
		return new WFServerTask<REQ, RESP>(service, 
											WFGlobal::get_scheduler(),
											process);
	}
};


#endif

