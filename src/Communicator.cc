#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mpoller.h"
#include "Communicator.h"

int Communicator::init(size_t poller_threads, size_t handler_threads)
{
	if (poller_threads == 0)
	{
		errno = EINVAL;
		return -1;
	}

	if (this->create_poller(poller_threads) >= 0)
	{
		if (this->create_handler_threads(handler_threads) >= 0)
		{
			this->stop_flag = 0;
			return 0;
		}

		mpoller_stop(this->mpoller);
		mpoller_destroy(this->mpoller);
		msgqueue_destroy(this->queue);
	}

	return -1;
}
