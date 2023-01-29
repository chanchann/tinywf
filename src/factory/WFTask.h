#ifndef _WFTASK_H_
#define _WFTASK_H_

#include <errno.h>
#include <string.h>
#include <assert.h>
#include <atomic>
#include <utility>
#include <functional>
#include "Communicator.h"
#include "CommScheduler.h"
#include "CommRequest.h"
#include "Workflow.h"
#include "WFConnection.h"

enum
{
	WFT_STATE_UNDEFINED = -1,
	WFT_STATE_SUCCESS = CS_STATE_SUCCESS,
	WFT_STATE_TOREPLY = CS_STATE_TOREPLY,		/* for server task only */
	WFT_STATE_NOREPLY = CS_STATE_TOREPLY + 1,	/* for server task only */
	WFT_STATE_SYS_ERROR = CS_STATE_ERROR,
	WFT_STATE_SSL_ERROR = 65,
	WFT_STATE_DNS_ERROR = 66,					/* for client task only */
	WFT_STATE_TASK_ERROR = 67,
	WFT_STATE_ABORTED = CS_STATE_STOPPED
};

template<class REQ, class RESP>
class WFNetworkTask : public CommRequest
{
public:
	/* start(), dismiss() are for client tasks only. */
	void start()
	{
		assert(!series_of(this));
		Workflow::start_series_work(this, nullptr);
	}

	void dismiss()
	{
		assert(!series_of(this));
		delete this;
	}

public:
	REQ *get_req() { return &this->req; }
	RESP *get_resp() { return &this->resp; }

public:
	void *user_data;

public:
	int get_state() const { return this->state; }
	int get_error() const { return this->error; }

	/* Call when error is ETIMEDOUT, return values:
	 * TOR_NOT_TIMEOUT, TOR_WAIT_TIMEOUT, TOR_CONNECT_TIMEOUT,
	 * TOR_TRANSMIT_TIMEOUT (send or receive).
	 * SSL connect timeout also returns TOR_CONNECT_TIMEOUT. */
	int get_timeout_reason() const { return this->timeout_reason; }

	/* Call only in callback or server's process. */
	long long get_task_seq() const
	{
		if (!this->target)
		{
			errno = ENOTCONN;
			return -1;
		}

		return this->get_seq();
	}

	int get_peer_addr(struct sockaddr *addr, socklen_t *addrlen) const;

	virtual WFConnection *get_connection() const = 0;

public:
	/* All in milliseconds. timeout == -1 for unlimited. */
	void set_send_timeout(int timeout) { this->send_timeo = timeout; }
	void set_receive_timeout(int timeout) { this->receive_timeo = timeout; }
	void set_keep_alive(int timeout) { this->keep_alive_timeo = timeout; }

public:
	/* noreply(), push() are for server tasks only. */
	void noreply()
	{
		if (this->state == WFT_STATE_TOREPLY)
			this->state = WFT_STATE_NOREPLY;
	}

public:
	void set_callback(std::function<void (WFNetworkTask<REQ, RESP> *)> cb)
	{
		this->callback = std::move(cb);
	}

protected:
	virtual int send_timeout() { return this->send_timeo; }
	virtual int receive_timeout() { return this->receive_timeo; }
	virtual int keep_alive_timeout() { return this->keep_alive_timeo; }

protected:
	int send_timeo;
	int receive_timeo;
	int keep_alive_timeo;
	REQ req;
	RESP resp;
	std::function<void (WFNetworkTask<REQ, RESP> *)> callback;

protected:
	WFNetworkTask(CommSchedObject *object, CommScheduler *scheduler,
				  std::function<void (WFNetworkTask<REQ, RESP> *)>&& cb) :
		CommRequest(object, scheduler),
		callback(std::move(cb))
	{
		this->send_timeo = -1;
		this->receive_timeo = -1;
		this->keep_alive_timeo = 0;
		this->target = NULL;
		this->timeout_reason = TOR_NOT_TIMEOUT;
		this->user_data = NULL;
		this->state = WFT_STATE_UNDEFINED;
		this->error = 0;
	}

	virtual ~WFNetworkTask() { }
};


template<class REQ, class RESP>
class WFClientTask : public WFNetworkTask<REQ, RESP>
{
protected:
	virtual CommMessageOut *message_out()
	{
		/* By using prepare function, users can modify request after
		 * the connection is established. */
		if (this->prepare)
			this->prepare(this);

		return &this->req;
	}

	virtual CommMessageIn *message_in() { return &this->resp; }

protected:
	virtual WFConnection *get_connection() const
	{
		CommConnection *conn;

		if (this->target)
		{
			conn = this->CommSession::get_connection();
			if (conn)
				return (WFConnection *)conn;
		}

		errno = ENOTCONN;
		return NULL;
	}

protected:
	virtual SubTask *done()
	{
		SeriesWork *series = series_of(this);

		if (this->state == WFT_STATE_SYS_ERROR && this->error < 0)
		{
			this->state = WFT_STATE_SSL_ERROR;
			this->error = -this->error;
		}

		if (this->callback)
			this->callback(this);

		delete this;
		return series->pop();
	}

public:
	void set_prepare(std::function<void (WFNetworkTask<REQ, RESP> *)> prep)
	{
		this->prepare = std::move(prep);
	}

protected:
	std::function<void (WFNetworkTask<REQ, RESP> *)> prepare;

public:
	WFClientTask(CommSchedObject *object, CommScheduler *scheduler,
				 std::function<void (WFNetworkTask<REQ, RESP> *)>&& cb) :
		WFNetworkTask<REQ, RESP>(object, scheduler, std::move(cb))
	{
	}

protected:
	virtual ~WFClientTask() { }
};


#endif

