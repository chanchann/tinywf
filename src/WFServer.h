
#ifndef _WFSERVER_H_
#define _WFSERVER_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <openssl/ssl.h>
#include "WFTaskFactory.h"

struct WFServerParams
{
	size_t max_connections;
	int peer_response_timeout;	/* timeout of each read or write operation */
	int receive_timeout;	/* timeout of receiving the whole message */
	int keep_alive_timeout;
	size_t request_size_limit;
	int ssl_accept_timeout;	/* if not ssl, this will be ignored */
};

static constexpr struct WFServerParams SERVER_PARAMS_DEFAULT =
{
	.max_connections		=	2000,
	.peer_response_timeout	=	10 * 1000,
	.receive_timeout		=	-1,
	.keep_alive_timeout		=	60 * 1000,
	.request_size_limit		=	(size_t)-1,
	.ssl_accept_timeout		=	10 * 1000,
};

class WFServerBase : protected CommService
{
public:
	WFServerBase(const struct WFServerParams *params) :
		conn_count(0)
	{
		this->params = *params;
		this->unbind_finish = false;
		this->listen_fd = -1;
	}

public:
	int start(const char *host, unsigned short port)
	{
		return start(AF_INET, host, port, NULL, NULL);
	}

	/* stop() is a blocking operation. */
	void stop()
	{
		this->shutdown();
		this->wait_finish();
	}

	/* Nonblocking terminating the server. For stopping multiple servers.
	 * Typically, call shutdown() and then wait_finish().
	 * But indeed wait_finish() can be called before shutdown(), even before
	 * start() in another thread. */
	void shutdown();
	void wait_finish();

public:
	size_t get_conn_count() const { return this->conn_count; }

	/* Get the listening address. This is often used after starting
	 * server on a random port (start() with port == 0). */
	int get_listen_addr(struct sockaddr *addr, socklen_t *addrlen) const
	{
		if (this->listen_fd >= 0)
			return getsockname(this->listen_fd, addr, addrlen);

		errno = ENOTCONN;
		return -1;
	}

protected:
	WFServerParams params;

protected:
	virtual WFConnection *new_connection(int accept_fd);
	void delete_connection(WFConnection *conn);

private:
	int init(const struct sockaddr *bind_addr, socklen_t addrlen,
			 const char *cert_file, const char *key_file);
	virtual int create_listen_fd();
	virtual void handle_unbound();

protected:
	std::atomic<size_t> conn_count;

private:
	int listen_fd;
	bool unbind_finish;

	std::mutex mutex;
	std::condition_variable cond;

	class CommScheduler *scheduler;
};

template<class REQ, class RESP>
class WFServer : public WFServerBase
{
public:
	WFServer(const struct WFServerParams *params,
			 std::function<void (WFNetworkTask<REQ, RESP> *)> proc) :
		WFServerBase(params),
		process(std::move(proc))
	{
	}

	WFServer(std::function<void (WFNetworkTask<REQ, RESP> *)> proc) :
		WFServerBase(&SERVER_PARAMS_DEFAULT),
		process(std::move(proc))
	{
	}

protected:
	virtual CommSession *new_session(long long seq, CommConnection *conn);

protected:
	std::function<void (WFNetworkTask<REQ, RESP> *)> process;
};

template<class REQ, class RESP>
CommSession *WFServer<REQ, RESP>::new_session(long long seq, CommConnection *conn)
{
	using factory = WFNetworkTaskFactory<REQ, RESP>;
	WFNetworkTask<REQ, RESP> *task;

	task = factory::create_server_task(this, this->process);
	task->set_keep_alive(this->params.keep_alive_timeout);
	task->set_receive_timeout(this->params.receive_timeout);
	task->get_req()->set_size_limit(this->params.request_size_limit);

	return task;
}

#endif
