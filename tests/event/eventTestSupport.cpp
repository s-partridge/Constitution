#include "eventTestSupport.h"

#include <algorithm>
#include <unordered_map>

#include <partest/assert.h>

#include "asyncDispatcher.h"

namespace cge::test
{
	const std::vector<DispatcherFlavor> &dispatcherFlavors()
	{
		static const std::vector<DispatcherFlavor> flavors = []() {
			std::vector<DispatcherFlavor> built;
			built.push_back(DispatcherFlavor("Async",
				[](const std::string &name, cge::event::EventChannelRegistry *registry) {
					return std::unique_ptr<cge::event::DispatcherBase>(
						new cge::event::AsyncDispatcher(name, registry));
				}));
			return built;
		}();

		return flavors;
	}

	unsigned smokeProducerCount()
	{
		unsigned hc = std::thread::hardware_concurrency();
		if(hc < 2)
			return 2;
		if(hc > 4)
			return 4;
		return hc;
	}

	unsigned loadWorkerCount()
	{
		unsigned hc = std::thread::hardware_concurrency();
		if(hc < 2)
			return 2;
		if(hc > 8)
			return 8;
		return hc;
	}

	void EventLoadSuite::assertPayloadsPreserved(const std::vector<LoadPayload> &sent, const std::vector<LoadPayload> &received)
	{
		ASSERT_EQUAL(received.size(), sent.size());

		std::vector<LoadPayload> expected = sent;
		std::vector<LoadPayload> actual = received;
		std::sort(expected.begin(), expected.end());
		std::sort(actual.begin(), actual.end());

		const size_t bound = std::min(expected.size(), actual.size());
		for(size_t i = 0; i < bound; ++i)
		{
			if(!(expected[i] == actual[i]))
			{
				// Report the first divergence rather than every one of them.
				ASSERT_EQUAL(actual[i], expected[i]);
				return;
			}
		}
	}

	void EventLoadSuite::assertProducerOrderPreserved(const std::vector<LoadPayload> &received)
	{
		// Last payload seen per worker. Within a worker the ordering reduces to
		// frame then sequence, so comparing whole payloads is the ordering test.
		std::unordered_map<unsigned, LoadPayload> lastSeen;

		for(size_t i = 0; i < received.size(); ++i)
		{
			const LoadPayload &payload = received[i];

			std::unordered_map<unsigned, LoadPayload>::iterator it = lastSeen.find(payload.worker);
			if(it != lastSeen.end() && payload < it->second)
			{
				// One report per run: a reordering usually cascades, and the
				// first offending pair is the one worth reading.
				ASSERT_GREATER_EQUAL(payload, it->second);
				return;
			}

			lastSeen[payload.worker] = payload;
		}
	}
}
