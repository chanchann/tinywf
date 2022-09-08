#ifndef _WFHTTPSERVER_H_
#define _WFHTTPSERVER_H_

#include <utility>
#include "HttpMessage.h"
#include "WFServer.h"
#include "WFTaskFactory.h"

using http_process_t = std::function<void (WFHttpTask *)>;
using WFHttpServer = WFServer<protocol::HttpRequest,
							  protocol::HttpResponse>;

static constexpr struct WFServerParams HTTP_SERVER_PARAMS_DEFAULT =
{
	.max_connections		=	2000,
	.peer_response_timeout	=	10 * 1000,
	.receive_timeout		=	-1,
	.keep_alive_timeout		=	60 * 1000,
	.request_size_limit		=	(size_t)-1,
	.ssl_accept_timeout		=	10 * 1000,
};

template<>
inline WFHttpServer::WFServer(http_process_t proc) :
	WFServerBase(&HTTP_SERVER_PARAMS_DEFAULT),
	process(std::move(proc))
{
}

template<>
inline CommSession *WFHttpServer::new_session(long long seq, CommConnection *conn)
{
	WFHttpTask *task;

	task = WFServerTaskFactory::create_http_task(this, this->process);
	task->set_keep_alive(this->params.keep_alive_timeout);
	task->set_receive_timeout(this->params.receive_timeout);
	task->get_req()->set_size_limit(this->params.request_size_limit);

	return task;
}

#endif
