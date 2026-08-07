#ifndef CGE_EVENT_TEST_SUPPORT_H
#define CGE_EVENT_TEST_SUPPORT_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <semaphore>
#include <string>
#include <thread>
#include <vector>

#include <partest/log.h>
#include <partest/runner.h>
#include <partest/testbase.h>

#include "dispatcher.h"
#include "event.h"
#include "listener.h"

namespace cge::test
{
	// Builds a dispatcher of one concrete flavor. The deterministic contract
	// suites are written against DispatcherBase and run once per flavor, so a
	// new dispatcher inherits the whole suite by adding a flavor here rather
	// than by copying tests.
	using DispatcherFactory = std::function<std::unique_ptr<cge::event::DispatcherBase>(const std::string &, cge::event::EventChannelRegistry *)>;

	struct DispatcherFlavor
	{
		std::string name;
		DispatcherFactory create;

		DispatcherFlavor(const std::string &name, const DispatcherFactory &create)
			: name(name)
			, create(create)
		{
		}
	};

	// Every flavor the deterministic suites are expected to satisfy.
	const std::vector<DispatcherFlavor> &dispatcherFlavors();

	// A set-up dispatcher and the registry backing it. Registry is declared
	// first so it outlives the dispatcher under reverse destruction order.
	// Tests needing a dispatcher at some other lifecycle point build one from
	// the flavor directly instead of using this.
	class EventHarness
	{
	public:
		EventHarness(const DispatcherFlavor &flavor, const std::string &name)
			: registry()
			, m_dispatcher(flavor.create(name, &registry))
		{
			m_dispatcher->setUp();
		}

		~EventHarness()
		{
			m_dispatcher->tearDown();
		}

		EventHarness(const EventHarness &) = delete;
		EventHarness &operator=(const EventHarness &) = delete;

		cge::event::DispatcherBase &dispatcher() { return *m_dispatcher; }

		cge::event::EventChannelRegistry registry;

	private:
		std::unique_ptr<cge::event::DispatcherBase> m_dispatcher;
	};

	// The engine's actual cycle. Commands drain before the events so a request
	// made last frame is live for this frame's traffic, and again afterwards so
	// anything a handler asked for during the drain applies within the frame it
	// was asked in rather than sitting queued until the next one.
	//
	// Tests whose subject is deferral itself still call the halves separately:
	// proving that a registration waits for the command drain means running the
	// event drain without one.
	inline void frame(cge::event::DispatcherBase &dispatcher)
	{
		dispatcher.dispatchCommands();
		dispatcher.dispatchEvents();
		dispatcher.dispatchCommands();
	}

	struct CountingListener : public cge::event::ListenerBase
	{
		std::vector<int> received;

		explicit CountingListener(cge::event::DispatcherBase *dispatcher)
			: ListenerBase(dispatcher)
		{
		}

		void onInt(const int &value)
		{
			received.push_back(value);
		}
	};

	// Thread-safe reference / receive log for the load suites.
	class PayloadLog
	{
	public:
		void record(int value)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_values.push_back(value);
		}

		std::vector<int> snapshot()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_values;
		}

	private:
		std::mutex m_mutex;
		std::vector<int> m_values;
	};

	// Unique payload: frame, worker and sequence stay recoverable from the
	// value, which is what lets the load suites check per-producer ordering
	// as well as set equality.
	int makePayload(unsigned frame, unsigned worker, unsigned seq);

	unsigned frameFromPayload(int payload);
	unsigned workerFromPayload(int payload);
	unsigned seqFromPayload(int payload);

	// Worker counts: light concurrent smoke versus frame-scale load.
	unsigned smokeProducerCount();
	unsigned loadWorkerCount();

	// Persistent workers behind a frame gate. Each frame the caller releases
	// every worker, waits for all of them to finish producing, then runs
	// onFrameComplete. Same idea as a job fence.
	//
	// Returns false on watchdog timeout.
	//
	// The watchdog does not rescue anything. A worker parked on beginFrame is
	// freed by the stop flag below, but one wedged inside produce never gets
	// back to read it, and the join then blocks for ever. Abandoning it instead
	// would mean detaching a thread that still holds references into the test
	// function's frame, and a timeout does not prove a deadlock anyway - a slow
	// enough frame trips it too - so there is no way to tell the safe case from
	// the unsafe one from in here.
	//
	// What it can do is say so before the join swallows it. TestBase::recordLog
	// is out of reach from a free function, but the runner's own recordLog takes
	// no frame and is safe from any thread, which suits a harness-level event
	// that belongs to no test in particular.
	//
	// Recording without a frame means the entry arrives in the readout attached
	// to nothing, so the message has to carry its own context. That is what
	// label is for: the caller names itself, since several tests share this and
	// the log would otherwise say a watchdog fired without saying whose.
	//
	// The real fix is for the caller to own the deadline and run this inside a
	// thread it can abandon. Until then the log is the only thing that survives
	// the hang.
	template<typename ProduceFn, typename FrameCompleteFn>
	bool runPersistentFrameGated(
		const std::string &label,
		unsigned workers,
		unsigned frameCount,
		unsigned pushesPerWorkerPerFrame,
		ProduceFn produce,
		FrameCompleteFn onFrameComplete)
	{
		// One gate per worker rather than a count on a shared one. A single
		// counting semaphore lets a worker that finishes early loop round and
		// take a second permit from the same frame, producing that frame's
		// sequence twice while a slower sibling produces it none, which shows up
		// as its own sequence numbers running backwards.
		std::vector<std::unique_ptr<std::binary_semaphore>> beginFrame;
		beginFrame.reserve(workers);
		for(unsigned w = 0; w < workers; ++w)
			beginFrame.push_back(std::unique_ptr<std::binary_semaphore>(new std::binary_semaphore(0)));

		std::counting_semaphore<> endFrame(0);
		std::atomic<unsigned> currentFrame(0);
		std::atomic<bool> stop(false);

		std::vector<std::thread> threads;
		threads.reserve(workers);
		for(unsigned w = 0; w < workers; ++w)
		{
			threads.emplace_back([&, w]() {
				while(true)
				{
					beginFrame[w]->acquire();
					if(stop.load(std::memory_order_acquire))
						break;

					const unsigned frame = currentFrame.load(std::memory_order_acquire);
					for(unsigned seq = 0; seq < pushesPerWorkerPerFrame; ++seq)
						produce(frame, w, seq);

					endFrame.release();
				}
			});
		}

		bool completed = true;
		for(unsigned frame = 0; frame < frameCount && completed; ++frame)
		{
			currentFrame.store(frame, std::memory_order_release);
			for(unsigned w = 0; w < workers; ++w)
				beginFrame[w]->release();

			unsigned reported = 0;
			for(unsigned w = 0; w < workers; ++w)
			{
				if(!endFrame.try_acquire_for(std::chrono::seconds(30)))
				{
					completed = false;
					break;
				}

				++reported;
			}

			if(!completed)
			{
				partest::TestRunner::getInstance().recordLog(
					partest::LogLevel::Error,
					partest::LOG_TYPE_DEFAULT,
					label + ": watchdog expired on frame " + std::to_string(frame) + " after 30s. "
						+ std::to_string(reported) + " of " + std::to_string(workers)
						+ " workers reported. The join that follows blocks until the wedged worker clears.");
				break;
			}

			onFrameComplete();
		}

		stop.store(true, std::memory_order_release);
		for(unsigned w = 0; w < workers; ++w)
			beginFrame[w]->release();
		for(std::thread &t : threads)
			t.join();
		return completed;
	}

	// Base for event suites that run once per dispatcher flavor. It carries the
	// flavor and folds its name into the suite name, so one suite registered for
	// two flavors reports as PayloadTest.Async and PayloadTest.Immediate.
	//
	// Not a test itself and never registered. The constructor is protected so it
	// cannot be instantiated on its own and appear as a suite with no tests in
	// it. The unit suites derive from partest::TestBase directly, since they
	// need no dispatcher and so have no flavor to vary.
	class DispatcherFlavorSuite : public partest::TestBase
	{
	protected:
		DispatcherFlavorSuite(const std::string &name, const std::string &description, const DispatcherFlavor &flavor)
			: TestBase(name + "." + flavor.name, description)
			, m_flavor(flavor)
		{
		}

		const DispatcherFlavor &flavor() const { return m_flavor; }

	private:
		DispatcherFlavor m_flavor;
	};

	// Adds the two payload checks the volume suites share. Both contain ASSERT_*
	// macros, so they record into the derived suite's own frame and are subject
	// to the same rule as any assertion: test thread only, after the workers
	// have been joined.
	class EventLoadSuite : public DispatcherFlavorSuite
	{
	protected:
		EventLoadSuite(const std::string &name, const std::string &description, const DispatcherFlavor &flavor)
			: DispatcherFlavorSuite(name, description, flavor)
		{
		}

		// Sorted comparison: arrival order across producers is not a contract,
		// so only set equality is required here. Asserts record and continue,
		// so the scan stays in bounds whether or not the size check passed.
		void assertPayloadsPreserved(const std::vector<int> &sent, const std::vector<int> &received);

		// Per-producer ordering is a contract even though cross-producer
		// ordering is not: events from one producer must arrive in the order
		// that producer sent them. Checks each worker's subsequence in
		// isolation, ignoring how they interleave.
		void assertProducerOrderPreserved(const std::vector<int> &received);
	};
}

#endif // CGE_EVENT_TEST_SUPPORT_H
