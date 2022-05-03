#ifndef _COMMUNICATOR_H_
#define _COMMUNICATOR_H_

class CommMessageOut
{
public:
	virtual ~CommMessageOut() = default;
};

class CommMessageIn 
{
public:
	virtual ~CommMessageIn() = default;
};

class CommService
{

};

class Communicator 
{
public:
	int init(size_t poller_threads, size_t handler_threads);
	
	void deinit();
	
	virtual ~Communicator() = default;

private:
	struct mpoller *mpoller;
	struct msgqueue *queue;
	struct thrdpool *thrdpool;
	int stop_flag;
};

#endif