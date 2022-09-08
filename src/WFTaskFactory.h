
#ifndef _WFTASKFACTORY_H_
#define _WFTASKFACTORY_H_

#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <utility>
#include <functional>
#include "URIParser.h"
#include "HttpMessage.h"
#include "Workflow.h"
#include "WFTask.h"
#include "EndpointParams.h"

// Network Client/Server tasks

using WFHttpTask = WFNetworkTask<protocol::HttpRequest,
								 protocol::HttpResponse>;
using http_callback_t = std::function<void (WFHttpTask *)>;

// 各种task创建都放在这下面
class WFTaskFactory {
 public:
  static WFHttpTask *create_http_task(const std::string &url, int redirect_max,
                                      int retry_max, http_callback_t callback);
};

template<class REQ, class RESP>
class WFNetworkTaskFactory
{
private:
	using T = WFNetworkTask<REQ, RESP>;

public:
	static T *create_client_task(TransportType type,
								 const std::string& url,
								 int retry_max,
								 std::function<void (T *)> callback);

public:
	static T *create_server_task(CommService *service,
								 std::function<void (T *)>& process);
};

#include "WFTaskFactory.inl"

#endif

