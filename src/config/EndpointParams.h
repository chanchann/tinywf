#ifndef _ENDPOINTPARAMS_H_
#define _ENDPOINTPARAMS_H_

#include <stddef.h>

/**
 * @file   EndpointParams.h
 * @brief  Network config for client task
 */

enum TransportType
{
	TT_TCP,
};

struct EndpointParams
{
	size_t max_connections;
	int connect_timeout;
	int response_timeout;
};

static constexpr struct EndpointParams ENDPOINT_PARAMS_DEFAULT =
{
	.max_connections		=	200,
	.connect_timeout		=	10 * 1000,
	.response_timeout		=	10 * 1000,
};

#endif

