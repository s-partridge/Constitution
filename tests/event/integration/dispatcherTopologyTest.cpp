#include "dispatcherTopologyTest.h"

#include <memory>

#include <partest/assert.h>

#include "broadcaster.h"

namespace cge::test
{
	DispatcherTopologyTest::DispatcherTopologyTest(const DispatcherFlavor &flavor)
		: EventTestBase("DispatcherTopologyTest", "Routing boundaries between dispatchers.", flavor)
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("Isolation", flags, [this]() { isolation(); });
	}

	void DispatcherTopologyTest::isolation()
	{
		// Two dispatchers share the registry's named registration channels, but
		// queues are per-dispatcher, so traffic must not cross.
		subtest("TwoDispatchersOneRegistry", [&]() {
			cge::event::EventChannelRegistry registry;
			std::unique_ptr<cge::event::DispatcherBase> first = flavor().create("dispatcher-a", &registry);
			std::unique_ptr<cge::event::DispatcherBase> second = flavor().create("dispatcher-b", &registry);
			first->setUp();
			second->setUp();

			const cge::event::EventChannel<int> &channel = registry.getChannel<int>("shared");
			CountingListener firstListener(first.get());
			CountingListener secondListener(second.get());

			firstListener.requestRegister(channel, [&firstListener](const int &v) { firstListener.onInt(v); });
			secondListener.requestRegister(channel, [&secondListener](const int &v) { secondListener.onInt(v); });
			first->dispatchCommands();
			second->dispatchCommands();

			cge::event::BroadcasterBase firstBroadcaster(first.get());
			cge::event::BroadcasterBase secondBroadcaster(second.get());
			firstBroadcaster.broadcast(channel, 1);
			secondBroadcaster.broadcast(channel, 2);
			first->dispatchEvents();
			second->dispatchEvents();

			ASSERT_EQUAL(firstListener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(firstListener.received[0], 1);
			ASSERT_EQUAL(secondListener.received.size(), static_cast<size_t>(1));
			ASSERT_EQUAL(secondListener.received[0], 2);

			first->tearDown();
			second->tearDown();
		});
	}
}
