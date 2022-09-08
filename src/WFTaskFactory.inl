#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>
#include <stdio.h>
#include <new>
#include <string>
#include <functional>
#include <utility>
#include <atomic>
#include "WFGlobal.h"
#include "Workflow.h"
#include "WFTask.h"
#include "RouteManager.h"
#include "URIParser.h"
#include "WFTaskError.h"
#include "EndpointParams.h"
#include "WFNameService.h"

template<class REQ, class RESP>
WFNetworkTask<REQ, RESP> *
WFNetworkTaskFactory<REQ, RESP>::create_client_task(TransportType type,
													const std::string& url,
													int retry_max,
													std::function<void (WFNetworkTask<REQ, RESP> *)> callback)
{
	auto *task = new WFComplexClientTask<REQ, RESP>(retry_max, std::move(callback));
	ParsedURI uri;

	URIParser::parse(url, uri);
	task->init(std::move(uri));
	task->set_transport_type(type);
	return task;
}

template<class REQ, class RESP>
WFNetworkTask<REQ, RESP> *
WFNetworkTaskFactory<REQ, RESP>::create_server_task(CommService *service,
				std::function<void (WFNetworkTask<REQ, RESP> *)>& process)
{
	return new WFServerTask<REQ, RESP>(service, WFGlobal::get_scheduler(),
									   process);
}

class WFServerTaskFactory
{
public:
	static WFHttpTask *create_http_task(CommService *service,
					std::function<void (WFHttpTask *)>& process);
};
