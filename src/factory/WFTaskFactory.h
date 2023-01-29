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

class WFTaskFactory
{
public:
	static WFHttpTask *create_http_task(const std::string& url,
										int redirect_max,
										int retry_max,
										http_callback_t callback);
};


template<class REQ, class RESP, typename CTX = bool>
class WFComplexClientTask : public WFClientTask<REQ, RESP>
{
protected:
	using task_callback_t = std::function<void (WFNetworkTask<REQ, RESP> *)>;

public:
	WFComplexClientTask(int retry_max, task_callback_t&& cb):
		WFClientTask<REQ, RESP>(NULL, WFGlobal::get_scheduler(), std::move(cb))
	{
		type_ = TT_TCP;
		fixed_addr_ = false;
		retry_max_ = retry_max;
		retry_times_ = 0;
		redirect_ = false;
	}

protected:
	// new api for children
	virtual bool init_success() { return true; }
	virtual void init_failed() {}
	virtual bool check_request() { return true; }
	virtual bool finish_once() { return true; }

public:
	void init(const ParsedURI& uri)
	{
		uri_ = uri;
		init_with_uri();
	}

	void init(ParsedURI&& uri)
	{
		uri_ = std::move(uri);
		init_with_uri();
	}

	void init(TransportType type,
			  const struct sockaddr *addr,
			  socklen_t addrlen,
			  const std::string& info);

	void set_transport_type(TransportType type)
	{
		type_ = type;
	}

	TransportType get_transport_type() const { return type_; }

	virtual const ParsedURI *get_current_uri() const { return &uri_; }

	void set_redirect(const ParsedURI& uri)
	{
		redirect_ = true;
		init(uri);
	}

	void set_redirect(TransportType type, const struct sockaddr *addr,
					  socklen_t addrlen, const std::string& info)
	{
		redirect_ = true;
		init(type, addr, addrlen, info);
	}

	bool is_fixed_addr() const { return this->fixed_addr_; }

protected:
	void set_fixed_addr(int fixed) { this->fixed_addr_ = fixed; }

	void set_info(const std::string& info)
	{
		info_.assign(info);
	}

	void set_info(const char *info)
	{
		info_.assign(info);
	}

protected:
	virtual void dispatch();
	virtual SubTask *done();

	void clear_resp()
	{
		size_t size = this->resp.get_size_limit();

		this->resp.~RESP();
		new(&this->resp) RESP();
		this->resp.set_size_limit(size);
	}

	void disable_retry()
	{
		retry_times_ = retry_max_;
	}

protected:
	TransportType type_;
	ParsedURI uri_;
	std::string info_;
	bool fixed_addr_;
	bool redirect_;
	CTX ctx_;
	int retry_max_;
	int retry_times_;
	RouteManager::RouteResult route_result_;

public:
	CTX *get_mutable_ctx() { return &ctx_; }

private:
	void clear_prev_state();
	void init_with_uri();
	bool set_port();
	void router_callback(void *t);
	void switch_callback(void *t);
};

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::clear_prev_state()
{
	route_result_.clear();
	retry_times_ = 0;
	this->state = WFT_STATE_UNDEFINED;
	this->error = 0;
	this->timeout_reason = TOR_NOT_TIMEOUT;
}

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::init(TransportType type,
											   const struct sockaddr *addr,
											   socklen_t addrlen,
											   const std::string& info)
{
	if (redirect_)
		clear_prev_state();

	auto params = WFGlobal::get_global_settings()->endpoint_params;
	struct addrinfo addrinfo = { };
	addrinfo.ai_family = addr->sa_family;
	addrinfo.ai_addr = (struct sockaddr *)addr;
	addrinfo.ai_addrlen = addrlen;

	type_ = type;
	info_.assign(info);
	if (WFGlobal::get_route_manager()->get(type, &addrinfo, info_, &params,
										   "", route_result_) < 0)
	{
		this->state = WFT_STATE_SYS_ERROR;
		this->error = errno;
	}
	else if (this->init_success())
		return;

	this->init_failed();
}

template<class REQ, class RESP, typename CTX>
bool WFComplexClientTask<REQ, RESP, CTX>::set_port()
{
	if (uri_.port)
	{
		int port = atoi(uri_.port);

		if (port <= 0 || port > 65535)
		{
			this->state = WFT_STATE_TASK_ERROR;
			this->error = WFT_ERR_URI_PORT_INVALID;
			return false;
		}

		return true;
	}

	if (uri_.scheme)
	{
		const char *port_str = WFGlobal::get_default_port(uri_.scheme);

		if (port_str)
		{
			uri_.port = strdup(port_str);
			if (uri_.port)
				return true;

			this->state = WFT_STATE_SYS_ERROR;
			this->error = errno;
			return false;
		}
	}

	this->state = WFT_STATE_TASK_ERROR;
	this->error = WFT_ERR_URI_SCHEME_INVALID;
	return false;
}

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::init_with_uri()
{
	if (redirect_)
	{
		clear_prev_state();
	}

	if (uri_.state == URI_STATE_SUCCESS)
	{
		if (this->set_port())
		{
			if (this->init_success())
				return;
		}
	}
	else if (uri_.state == URI_STATE_ERROR)
	{
		this->state = WFT_STATE_SYS_ERROR;
		this->error = uri_.error;
	}
	else
	{
		this->state = WFT_STATE_TASK_ERROR;
		this->error = WFT_ERR_URI_PARSE_FAILED;
	}

	this->init_failed();
}

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::dispatch()
{
	switch (this->state)
	{
	case WFT_STATE_UNDEFINED:
		if (this->check_request())
		{
			if (this->route_result_.request_object)
			{
	case WFT_STATE_SUCCESS:
				this->set_request_object(route_result_.request_object);
				this->WFClientTask<REQ, RESP>::dispatch();
				return;
			}

			router_task_ = this->route();
			series_of(this)->push_front(this);
			series_of(this)->push_front(router_task_);
		}

	default:
		break;
	}

	this->subtask_done();
}

template<class REQ, class RESP, typename CTX>
void WFComplexClientTask<REQ, RESP, CTX>::switch_callback(void *t)
{
	if (!redirect_)
	{
		if (this->state == WFT_STATE_SYS_ERROR && this->error < 0)
		{
			this->state = WFT_STATE_SSL_ERROR;
			this->error = -this->error;
		}

		if (tracing_.deleter)
		{
			tracing_.deleter(tracing_.data);
			tracing_.deleter = NULL;
		}

		if (this->callback)
			this->callback(this);
	}

	if (redirect_)
	{
		redirect_ = false;
		clear_resp();
		this->target = NULL;
		series_of(this)->push_front(this);
	}
	else
		delete this;
}

template<class REQ, class RESP, typename CTX>
SubTask *WFComplexClientTask<REQ, RESP, CTX>::done()
{
	SeriesWork *series = series_of(this);

	if (router_task_)
	{
		router_task_ = NULL;
		return series->pop();
	}

	bool is_user_request = this->finish_once();

	if (ns_policy_)
	{
		if (this->state == WFT_STATE_SYS_ERROR ||
			this->state == WFT_STATE_DNS_ERROR)
		{
			ns_policy_->failed(&route_result_, &tracing_, this->target);
		}
		else if (route_result_.request_object)
		{
			ns_policy_->success(&route_result_, &tracing_, this->target);
		}
	}

	if (this->state == WFT_STATE_SUCCESS)
	{
		if (!is_user_request)
			return this;
	}
	else if (this->state == WFT_STATE_SYS_ERROR)
	{
		if (retry_times_ < retry_max_)
		{
			redirect_ = true;
			if (ns_policy_)
				route_result_.clear();

			this->state = WFT_STATE_UNDEFINED;
			this->error = 0;
			this->timeout_reason = 0;
			retry_times_++;
		}
	}

	/*
	 * When target is NULL, it's very likely that we are in the caller's
	 * thread or DNS thread (dns failed). Running a timer will switch callback
	 * function to a handler thread, and this can prevent stack overflow.
	 */
	if (!this->target)
	{
		auto&& cb = std::bind(&WFComplexClientTask::switch_callback,
							  this,
							  std::placeholders::_1);
		WFTimerTask *timer;

		timer = WFTaskFactory::create_timer_task(0, 0, std::move(cb));
		series->push_front(timer);
	}
	else
		this->switch_callback(NULL);

	return series->pop();
}

template<class REQ, class RESP>
class WFNetworkTaskFactory
{
private:
	using T = WFNetworkTask<REQ, RESP>;

public:
	// Template Network Factory
	static T *create_client_task(TransportType type,
								 const std::string& url,
								 int retry_max,
								 std::function<void (T *)> callback)
	{
		auto *task = new WFComplexClientTask<REQ, RESP>(retry_max, std::move(callback));
		ParsedURI uri;

		URIParser::parse(url, uri);
		task->init(std::move(uri));
		task->set_transport_type(type);
		return task;
	}


public:
	static T *create_server_task(CommService *service,
								 std::function<void (T *)>& process)
	{
		return new WFServerTask<REQ, RESP>(service, WFGlobal::get_scheduler(),
										process);
	}
};

#endif

