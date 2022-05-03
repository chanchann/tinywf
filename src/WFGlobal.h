#ifndef _WFGLOBAL_H_
#define _WFGLOBAL_H_

struct WFGlobalSettings
{
	int poller_threads;
	int handler_threads;
	int compute_threads;			//< auto-set by system CPU number if value<=0
};

static constexpr struct WFGlobalSettings GLOBAL_SETTINGS_DEFAULT =
{
	.poller_threads		=	4,
	.handler_threads	=	20,
	.compute_threads	=	-1
};

extern void WORKFLOW_library_init(const WFGlobalSettings &settings);

class WFGlobal
{
public:
	static void set_global_settings(const WFGlobalSettings &settings)
	{
		settings_ = settings;
	}
    
	static const WFGlobalSettings &get_global_settings()
	{
		return settings_;
	}

	static class CommScheduler &get_scheduler();

private:
	static WFGlobalSettings settings_;
};

#endif