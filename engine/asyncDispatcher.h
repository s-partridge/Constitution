#ifndef ASYNC_DISPATCHER_H
#define ASYNC_DISPATCHER_H

#include <thread>
#include <mutex>

#include "dispatcher.h"

namespace cge::event
{
	class AsyncDispatcher : public DispatcherBase
	{
	public:
		AsyncDispatcher(const std::string &name, EventChannelRegistry *registry) : DispatcherBase(name, registry) {}
		~AsyncDispatcher() = default;

		void onSetUp() override;
		void onTearDown() override;

		void dispatchEvents() override;
		void dispatchCommands() override;
	protected:
		bool onPushEvent(const EventChannelBase &channel, std::unique_ptr<EventBase> event) override;
		bool onPushCommand(const EventChannelBase &channel, std::unique_ptr<EventBase> event) override;

	private:
		std::deque<EventPair> m_eventSwap;
		std::deque<EventPair> m_commandSwap;

		std::mutex m_eventQueueMutex;
		std::mutex m_commandQueueMutex;
	};
}

#endif // ASYNC_DISPATCHER_H