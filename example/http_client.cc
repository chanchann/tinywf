#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include "tinywf/WFTaskFactory.h"

int main(int argc, char *argv[])
{
	WFHttpTask *task;

	task = WFTaskFactory::create_http_task("http://127.0.0.1:8888/test", 4, 2,
										   [](WFHttpTask *task)
										   {
											   protocol::HttpRequest *req = task->get_req();
											   /* Print request. */
											   fprintf(stderr, "%s %s %s\r\n", req->get_method(),
													   req->get_http_version(),
													   req->get_request_uri());
										   });
	protocol::HttpRequest *req = task->get_req();
	req->add_header_pair("Accept", "*/*");
	req->add_header_pair("User-Agent", "Wget/1.14 (linux-gnu)");
	req->add_header_pair("Connection", "close");
	task->start();

	getchar();
	return 0;
}
