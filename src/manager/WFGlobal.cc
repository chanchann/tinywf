#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <ctype.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "CommScheduler.h"
#include "WFTaskError.h"
#include "WFGlobal.h"

class __WFGlobal
{
public:
	static __WFGlobal *get_instance()
	{
		static __WFGlobal kInstance;
		return &kInstance;
	}

	const char *get_default_port(const std::string& scheme)
	{
		const auto it = static_scheme_port_.find(scheme);

		if (it != static_scheme_port_.end())
			return it->second;

		const char *port = NULL;
		user_scheme_port_mutex_.lock();
		const auto it2 = user_scheme_port_.find(scheme);

		if (it2 != user_scheme_port_.end())
			port = it2->second.c_str();

		user_scheme_port_mutex_.unlock();
		return port;
	}

	void register_scheme_port(const std::string& scheme, unsigned short port)
	{
		user_scheme_port_mutex_.lock();
		user_scheme_port_[scheme] = std::to_string(port);
		user_scheme_port_mutex_.unlock();
	}

	void sync_operation_begin()
	{
		bool inc;

		sync_mutex_.lock();
		inc = ++sync_count_ > sync_max_;

		if (inc)
			sync_max_ = sync_count_;
		sync_mutex_.unlock();
		if (inc)
			WFGlobal::get_scheduler()->increase_handler_thread();
	}

	void sync_operation_end()
	{
		sync_mutex_.lock();
		sync_count_--;
		sync_mutex_.unlock();
	}

private:
	__WFGlobal();

private:
	std::unordered_map<std::string, const char *> static_scheme_port_;
	std::unordered_map<std::string, std::string> user_scheme_port_;
	std::mutex user_scheme_port_mutex_;
	std::mutex sync_mutex_;
	int sync_count_;
	int sync_max_;
};

__WFGlobal::__WFGlobal()
{
	static_scheme_port_["http"] = "80";
	static_scheme_port_["Http"] = "80";
	static_scheme_port_["HTTP"] = "80";

	sync_count_ = 0;
	sync_max_ = 0;
}


class __CommManager
{
public:
	static __CommManager *get_instance()
	{
		static __CommManager kInstance;
		__CommManager::created_ = true;
		return &kInstance;
	}

	CommScheduler *get_scheduler() { return &scheduler_; }

	static bool is_created() { return created_; }

private:
	__CommManager()
	{
		const auto *settings = WFGlobal::get_global_settings();
		if (scheduler_.init(settings->poller_threads,
							settings->handler_threads) < 0)
			abort();

		signal(SIGPIPE, SIG_IGN);
	}

	~__CommManager()
	{
		scheduler_.deinit();
	}

private:
	CommScheduler scheduler_;

private:
	static bool created_;
};

bool __CommManager::created_ = false;

#define MAX(x, y)	((x) >= (y) ? (x) : (y))
#define HOSTS_LINEBUF_INIT_SIZE	128

static void __split_merge_str(const char *p, bool is_nameserver,
							  std::string& result)
{
	const char *start;

	if (!isspace(*p))
		return;

	while (1)
	{
		while (isspace(*p))
			p++;

		start = p;
		while (*p && *p != '#' && *p != ';' && !isspace(*p))
			p++;

		if (start == p)
			break;

		if (!result.empty())
			result.push_back(',');

		std::string str(start, p);
		if (is_nameserver)
		{
			struct in6_addr buf;
			if (inet_pton(AF_INET6, str.c_str(), &buf) > 0)
				str = "[" + str + "]";
		}

		result.append(str);
	}
}

static inline const char *__try_options(const char *p, const char *q,
										const char *r)
{
	size_t len = strlen(r);
	if ((size_t)(q - p) >= len && strncmp(p, r, len) == 0)
		return p + len;
	return NULL;
}

static void __set_options(const char *p,
						  int *ndots, int *attempts, bool *rotate)
{
	const char *start;
	const char *opt;

	if (!isspace(*p))
		return;

	while (1)
	{
		while (isspace(*p))
			p++;

		start = p;
		while (*p && *p != '#' && *p != ';' && !isspace(*p))
			p++;

		if (start == p)
			break;

		if ((opt = __try_options(start, p, "ndots:")) != NULL)
			*ndots = atoi(opt);
		else if ((opt = __try_options(start, p, "attempts:")) != NULL)
			*attempts = atoi(opt);
		else if ((opt = __try_options(start, p, "rotate")) != NULL)
			*rotate = true;
	}
}

static int __parse_resolv_conf(const char *path,
							   std::string& url, std::string& search_list,
							   int *ndots, int *attempts, bool *rotate)
{
	size_t bufsize = 0;
	char *line = NULL;
	FILE *fp;
	int ret;

	fp = fopen(path, "r");
	if (!fp)
		return -1;

	while ((ret = getline(&line, &bufsize, fp)) > 0)
	{
		if (strncmp(line, "nameserver", 10) == 0)
			__split_merge_str(line + 10, true, url);
		else if (strncmp(line, "search", 6) == 0)
			__split_merge_str(line + 6, false, search_list);
		else if (strncmp(line, "options", 7) == 0)
			__set_options(line + 7, ndots, attempts, rotate);
	}

	ret = ferror(fp) ? -1 : 0;
	free(line);
	fclose(fp);
	return ret;
}

struct WFGlobalSettings WFGlobal::settings_ = GLOBAL_SETTINGS_DEFAULT;
RouteManager WFGlobal::route_manager_;

bool WFGlobal::is_scheduler_created()
{
	return __CommManager::is_created();
}

CommScheduler *WFGlobal::get_scheduler()
{
	return __CommManager::get_instance()->get_scheduler();
}

int WFGlobal::sync_operation_begin()
{
	if (WFGlobal::is_scheduler_created() &&
		WFGlobal::get_scheduler()->is_handler_thread())
	{
		__WFGlobal::get_instance()->sync_operation_begin();
		return 1;
	}

	return 0;
}

void WFGlobal::sync_operation_end(int cookie)
{
	if (cookie)
		__WFGlobal::get_instance()->sync_operation_end();
}

void WORKFLOW_library_init(const struct WFGlobalSettings *settings)
{
	WFGlobal::set_global_settings(settings);
}

