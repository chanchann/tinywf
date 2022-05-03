#ifndef _WFHTTPSERVER_H_
#define _WFHTTPSERVER_H_

#include "WFServer.h"
#include "HttpMessage.h"

using WFHttpServer = WFServer<protocol::HttpRequest,
							  protocol::HttpResponse>;





#endif