#include "eventTestSupport.h"

#include <algorithm>
#include <unordered_map>

#include <partest/assert.h>

#include "asyncDispatcher.h"

namespace cge::test
{
	namespace
	{
		// Payload packing. Generous for the volumes the load suites use:
		// frames * workers * seq fits well inside 32 bits.
		const unsigned kFrameShift = 20;
		const unsigned kWorkerShift = 12;
		const unsigned kWorkerMask = 0xFF;
		const unsigned kSeqMask = 0xFFF;
	}

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

	int makePayload(unsigned frame, unsigned worker, unsigned seq)
	{
		return static_cast<int>((frame << kFrameShift) | (worker << kWorkerShift) | seq);
	}

	unsigned frameFromPayload(int payload)
	{
		return static_cast<unsigned>(payload) >> kFrameShift;
	}

	unsigned workerFromPayload(int payload)
	{
		return (static_cast<unsigned>(payload) >> kWorkerShift) & kWorkerMask;
	}

	unsigned seqFromPayload(int payload)
	{
		return static_cast<unsigned>(payload) & kSeqMask;
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

	void EventTestBase::assertPayloadsPreserved(const std::vector<int> &sent, const std::vector<int> &received)
	{
		ASSERT_EQUAL(received.size(), sent.size());

		std::vector<int> expected = sent;
		std::vector<int> actual = received;
		std::sort(expected.begin(), expected.end());
		std::sort(actual.begin(), actual.end());

		const size_t bound = std::min(expected.size(), actual.size());
		for(size_t i = 0; i < bound; ++i)
		{
			if(expected[i] != actual[i])
			{
				// Report the first divergence rather than every one of them.
				ASSERT_EQUAL(actual[i], expected[i]);
				return;
			}
		}
	}

	void EventTestBase::assertProducerOrderPreserved(const std::vector<int> &received)
	{
		// Last (frame, seq) seen per worker, packed back into a single value so
		// the comparison is a plain ordering test.
		std::unordered_map<unsigned, unsigned> lastSeen;

		for(size_t i = 0; i < received.size(); ++i)
		{
			const int payload = received[i];
			const unsigned worker = workerFromPayload(payload);
			const unsigned position = (frameFromPayload(payload) << kFrameShift) | seqFromPayload(payload);

			std::unordered_map<unsigned, unsigned>::iterator it = lastSeen.find(worker);
			if(it != lastSeen.end() && position < it->second)
			{
				// One report per run: a reordering usually cascades, and the
				// first offending pair is the one worth reading.
				ASSERT_GREATER_EQUAL(position, it->second);
				return;
			}

			lastSeen[worker] = position;
		}
	}
}
