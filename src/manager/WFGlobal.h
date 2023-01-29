
#ifndef _WFGLOBAL_H_
#define _WFGLOBAL_H_

#include <openssl/ssl.h>
#include <string>
#include "CommScheduler.h"
#include "RouteManager.h"
#include "EndpointParams.h"

/**
 * @file    WFGlobal.h
 * @brief   Workflow Global Settings & Workflow Global APIs
 */

/**
 * @brief   Workflow Library Global Setting
 * @details
 * If you want set different settings with default, please call WORKFLOW_library_init at the beginning of the process
*/
struct WFGlobalSettings
{
	struct EndpointParams endpoint_params;
	int poller_threads;
	int handler_threads;
	int compute_threads;			///< auto-set by system CPU number if value<=0
};

/**
 * @brief   Default Workflow Library Global Settings
 */
static constexpr struct WFGlobalSettings GLOBAL_SETTINGS_DEFAULT =
{
	.endpoint_params	=	ENDPOINT_PARAMS_DEFAULT,
	.poller_threads		=	4,
	.handler_threads	=	4,
};

/**
 * @brief      Reset Workflow Library Global Setting
 * @param[in]  settings          custom settings pointer
*/
extern void WORKFLOW_library_init(const struct WFGlobalSettings *settings);

/**
 * @brief   Workflow Global Management Class
 * @details Workflow Global APIs
 */
class WFGlobal
{
public:
	/**
	 * @brief      get current global settings
	 * @return     current global settings const pointer
	 * @note       returnval never NULL
	 */
	static const struct WFGlobalSettings *get_global_settings()
	{
		return &settings_;
	}

	static void set_global_settings(const struct WFGlobalSettings *settings)
	{
		settings_ = *settings;
	}

	static const char *get_error_string(int state, int error);

	// Internal usage only
public:
	static bool is_scheduler_created();
	static class CommScheduler *get_scheduler();

	static class RouteManager *get_route_manager()
	{
		return &route_manager_;
	}
public:
	static int sync_operation_begin();
	static void sync_operation_end(int cookie);

private:
	static struct WFGlobalSettings settings_;
	static RouteManager route_manager_;
};

#endif

