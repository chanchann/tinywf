#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>
#include "list.h"
#include "rbtree.h"
#include "WFGlobal.h"
#include "CommScheduler.h"
#include "EndpointParams.h"
#include "RouteManager.h"
#include "StringUtil.h"

#define GET_CURRENT_SECOND	std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
#define MTTR_SECOND			30

using RouteTargetTCP = RouteManager::RouteTarget;

//  protocol_name\n user\n pass\n dbname\n ai_addr ai_addrlen \n....
//

struct RouteParams
{
	TransportType transport_type;
	const struct addrinfo *addrinfo;
	uint64_t sha1_64;
	int connect_timeout;
	int response_timeout;
	size_t max_connections;
	const std::string& hostname;
};

class RouteResultEntry
{
public:
	struct rb_node rb;
	CommSchedObject *request_object;
	CommSchedGroup *group;
	std::mutex mutex;
	std::vector<CommSchedTarget *> targets;
	struct list_head breaker_list;
	uint64_t sha1_64;
	int nleft;
	int nbreak;

	RouteResultEntry():
		request_object(NULL),
		group(NULL)
	{
		INIT_LIST_HEAD(&this->breaker_list);
		this->nleft = 0;
		this->nbreak = 0;
	}

public:
	int init(const struct RouteParams *params);
	void deinit();

	void notify_unavailable(CommSchedTarget *target);
	void notify_available(CommSchedTarget *target);
	void check_breaker();

private:
	void free_list();
	CommSchedTarget *create_target(const struct RouteParams *params,
								   const struct addrinfo *addrinfo);
	int add_group_targets(const struct RouteParams *params);
};

struct __breaker_node
{
	CommSchedTarget *target;
	int64_t timeout;
	struct list_head breaker_list;
};

CommSchedTarget *RouteResultEntry::create_target(const struct RouteParams *params,
												 const struct addrinfo *addr)
{
	CommSchedTarget *target;

	switch (params->transport_type)
	{

	case TT_TCP:
			target = new RouteTargetTCP();
		break;
	default:
		errno = EINVAL;
		return NULL;
	}

	if (target->init(addr->ai_addr, addr->ai_addrlen,
					 params->connect_timeout,
					 params->response_timeout, params->max_connections) < 0)
	{
		delete target;
		target = NULL;
	}

	return target;
}

int RouteResultEntry::init(const struct RouteParams *params)
{
	const struct addrinfo *addr = params->addrinfo;
	CommSchedTarget *target;

	if (addr == NULL)//0
	{
		errno = EINVAL;
		return -1;
	}

	if (addr->ai_next == NULL)//1
	{
		target = this->create_target(params, addr);
		if (target)
		{
			this->targets.push_back(target);
			this->request_object = target;
			this->sha1_64 = params->sha1_64;
			return 0;
		}

		return -1;
	}

	this->group = new CommSchedGroup();
	if (this->group->init() >= 0)
	{
		if (this->add_group_targets(params) >= 0)
		{
			this->request_object = this->group;
			this->sha1_64 = params->sha1_64;
			return 0;
		}

		this->group->deinit();
	}

	delete this->group;
	return -1;
}

int RouteResultEntry::add_group_targets(const struct RouteParams *params)
{
	const struct addrinfo *addr;
	CommSchedTarget *target;

	for (addr = params->addrinfo; addr; addr = addr->ai_next)
	{
		target = this->create_target(params, addr);
		if (target)
		{
			if (this->group->add(target) >= 0)
			{
				this->targets.push_back(target);
				this->nleft++;
				continue;
			}

			target->deinit();
			delete target;
		}

		for (auto *target : this->targets)
		{
			this->group->remove(target);
			target->deinit();
			delete target;
		}

		return -1;
	}

	return 0;
}

void RouteResultEntry::deinit()
{
	for (auto *target : this->targets)
	{
		if (this->group)
			this->group->remove(target);

		target->deinit();
		delete target;
	}

	if (this->group)
	{
		this->group->deinit();
		delete this->group;
	}

	struct list_head *pos, *tmp;
	__breaker_node *node;

	list_for_each_safe(pos, tmp, &this->breaker_list)
	{
		node = list_entry(pos, __breaker_node, breaker_list);
		list_del(pos);
		delete node;
	}
}

void RouteResultEntry::notify_unavailable(CommSchedTarget *target)
{
	if (this->targets.size() <= 1)
		return;

	int errno_bak = errno;
	std::lock_guard<std::mutex> lock(this->mutex);

	if (this->nleft <= 1)
		return;

	if (this->group->remove(target) < 0)
	{
		errno = errno_bak;
		return;
	}

	auto *node = new __breaker_node;

	node->target = target;
	node->timeout = GET_CURRENT_SECOND + MTTR_SECOND;
	list_add_tail(&node->breaker_list, &this->breaker_list);
	this->nbreak++;
	this->nleft--;
}

void RouteResultEntry::notify_available(CommSchedTarget *target)
{
	if (this->targets.size() <= 1 || this->nbreak == 0)
		return;

	int errno_bak = errno;
	std::lock_guard<std::mutex> lock(this->mutex);

	if (this->group->add(target) == 0)
		this->nleft++;
	else
		errno = errno_bak;
}

void RouteResultEntry::check_breaker()
{
	if (this->targets.size() <= 1 || this->nbreak == 0)
		return;

	struct list_head *pos, *tmp;
	__breaker_node *node;
	int errno_bak = errno;
	int64_t cur_time = GET_CURRENT_SECOND;
	std::lock_guard<std::mutex> lock(this->mutex);

	list_for_each_safe(pos, tmp, &this->breaker_list)
	{
		node = list_entry(pos, __breaker_node, breaker_list);
		if (cur_time >= node->timeout)
		{
			if (this->group->add(node->target) == 0)
				this->nleft++;
			else
				errno = errno_bak;

			list_del(pos);
			delete node;
			this->nbreak--;
		}
	}
}

static inline int __addr_cmp(const struct addrinfo *x, const struct addrinfo *y)
{
	//todo ai_protocol
	if (x->ai_addrlen == y->ai_addrlen)
		return memcmp(x->ai_addr, y->ai_addr, x->ai_addrlen);
	else if (x->ai_addrlen < y->ai_addrlen)
		return -1;
	else
		return 1;
}

static inline bool __addr_less(const struct addrinfo *x, const struct addrinfo *y)
{
	return __addr_cmp(x, y) < 0;
}

static uint64_t __generate_key(TransportType type,
							   const struct addrinfo *addrinfo,
							   const std::string& other_info,
							   const struct EndpointParams *ep_params,
							   const std::string& hostname)
{
	std::string buf((const char *)&type, sizeof (TransportType));
	uint64_t sha1[3];
	SHA_CTX ctx;

	if (!other_info.empty())
		buf += other_info;

	buf.append((const char *)&ep_params->max_connections, sizeof (size_t));
	buf.append((const char *)&ep_params->connect_timeout, sizeof (int));
	buf.append((const char *)&ep_params->response_timeout, sizeof (int));

	if (addrinfo->ai_next)
	{
		std::vector<const struct addrinfo *> sorted_addr;

		sorted_addr.push_back(addrinfo);
		addrinfo = addrinfo->ai_next;
		do
		{
			sorted_addr.push_back(addrinfo);
			addrinfo = addrinfo->ai_next;
		} while (addrinfo);

		std::sort(sorted_addr.begin(), sorted_addr.end(), __addr_less);
		for (const struct addrinfo *p : sorted_addr)
		{
			buf.append((const char *)&p->ai_addrlen, sizeof (socklen_t));
			buf.append((const char *)p->ai_addr, p->ai_addrlen);
		}
	}
	else
	{
		buf.append((const char *)&addrinfo->ai_addrlen, sizeof (socklen_t));
		buf.append((const char *)addrinfo->ai_addr, addrinfo->ai_addrlen);
	}

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, buf.c_str(), buf.size());
	SHA1_Final((unsigned char *)sha1, &ctx);
	return sha1[1];
}

RouteManager::~RouteManager()
{
	RouteResultEntry *entry;

	while (cache_.rb_node)
	{
		entry = rb_entry(cache_.rb_node, RouteResultEntry, rb);
		rb_erase(cache_.rb_node, &cache_);
		entry->deinit();
		delete entry;
	}
}

int RouteManager::get(TransportType type,
					  const struct addrinfo *addrinfo,
					  const std::string& other_info,
					  const struct EndpointParams *endpoint_params,
					  const std::string& hostname,
					  RouteResult& result)
{
	uint64_t sha1_64 = __generate_key(type, addrinfo, other_info,
									  endpoint_params, hostname);
	struct rb_node **p = &cache_.rb_node;
	struct rb_node *parent = NULL;
	RouteResultEntry *bound = NULL;
	RouteResultEntry *entry;
	std::lock_guard<std::mutex> lock(mutex_);

	while (*p)
	{
		parent = *p;
		entry = rb_entry(*p, RouteResultEntry, rb);
		if (sha1_64 <= entry->sha1_64)
		{
			bound = entry;
			p = &(*p)->rb_left;
		}
		else
			p = &(*p)->rb_right;
	}

	if (bound && bound->sha1_64 == sha1_64)
	{
		entry = bound;
		entry->check_breaker();
	}
	else
	{
		struct RouteParams params = {
			.transport_type			=	type,
			.addrinfo 				= 	addrinfo,
			.sha1_64				=	sha1_64,
			.connect_timeout		=	endpoint_params->connect_timeout,
			.response_timeout		=	endpoint_params->response_timeout,
			.max_connections		=	endpoint_params->max_connections,
			.hostname				=	hostname,
		};

		if (StringUtil::start_with(other_info, "?maxconn="))
		{
			int maxconn = atoi(other_info.c_str() + 9);
			if (maxconn > 0)
				params.max_connections = maxconn;
		}

		entry = new RouteResultEntry;
		if (entry->init(&params) >= 0)
		{
			rb_link_node(&entry->rb, parent, p);
			rb_insert_color(&entry->rb, &cache_);
		}
		else
		{
			delete entry;
			return -1;
		}
	}

	result.cookie = entry;
	result.request_object = entry->request_object;
	return 0;
}

void RouteManager::notify_unavailable(void *cookie, CommTarget *target)
{
	if (cookie && target)
		((RouteResultEntry *)cookie)->notify_unavailable((CommSchedTarget *)target);
}

void RouteManager::notify_available(void *cookie, CommTarget *target)
{
	if (cookie && target)
		((RouteResultEntry *)cookie)->notify_available((CommSchedTarget *)target);
}

