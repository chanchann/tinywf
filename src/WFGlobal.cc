#include <signal.h>
#include <cstdlib>
#include "WFGlobal.h"
#include "CommScheduler.h"

void WORKFLOW_library_init(const struct WFGlobalSettings &settings)
{
	WFGlobal::set_global_settings(settings);
}

class CommManager
{
public:
	static CommManager &get_instance()
	{
		static CommManager kInstance;
		return kInstance;
	}

	CommScheduler &get_scheduler() { return scheduler_; }

private:
	CommManager()
	{
		const WFGlobalSettings &settings = WFGlobal::get_global_settings();
		if (scheduler_.init(settings.poller_threads,
							settings.handler_threads) < 0)
			abort();

		signal(SIGPIPE, SIG_IGN);
	}

	~CommManager()
	{
		scheduler_.deinit();
	}

private:
	CommScheduler scheduler_;
};

CommScheduler &WFGlobal::get_scheduler()
{
	return CommManager::get_instance().get_scheduler();
}