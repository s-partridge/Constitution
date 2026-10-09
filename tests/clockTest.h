#ifndef CLOCK_TEST_H
#define CLOCK_TEST_H

#include <chrono>
#include <string>
#include <thread>

#include <partest/testbase.h>

#include "clock.h"

class ClockTest : public partest::TestBase
{
public:
	ClockTest() : TestBase("ClockTest", "Validation for the Clock component.")
	{
		partest::TestFlags flags = partest::TEST_FLAGS_INHERIT;

		addTest("ConstructionRegistersUpdateAndPhysics", flags, PARTEST_CTX(this) { return this->constructionRegistersUpdateAndPhysics(ctx); });
		addTest("RegisterChannelSucceedsForNewType", flags, PARTEST_CTX(this) { return this->registerChannelSucceedsForNewType(ctx); });
		addTest("RegisterChannelUpdatesExistingType", flags, PARTEST_CTX(this) { return this->registerChannelUpdatesExistingType(ctx); });
		addTest("RegisterChannelKeepsTickState", flags, PARTEST_CTX(this) { return this->registerChannelKeepsTickState(ctx); });
		addTest("RegisteredChannelHasExpectedInitialState", flags, PARTEST_CTX(this) { return this->registeredChannelHasExpectedInitialState(ctx); });
		addTest("UnregisteredChannelReadsAsFresh", flags, PARTEST_CTX(this) { return this->unregisteredChannelReadsAsFresh(ctx); });
		addTest("TickCreatesUnregisteredChannel", flags, PARTEST_CTX(this) { return this->tickCreatesUnregisteredChannel(ctx); });
		addTest("TickIncrementsCount", flags, PARTEST_CTX(this) { return this->tickIncrementsCount(ctx); });
		addTest("RawTickIncrementsCount", flags, PARTEST_CTX(this) { return this->rawTickIncrementsCount(ctx); });
		addTest("SetIntervalUpdatesChannel", flags, PARTEST_CTX(this) { return this->setIntervalUpdatesChannel(ctx); });
		addTest("SetTimeScaleUpdatesChannel", flags, PARTEST_CTX(this) { return this->setTimeScaleUpdatesChannel(ctx); });
		addTest("TimeScaleAppliesToTick", flags, PARTEST_CTX(this) { return this->timeScaleAppliesToTick(ctx); });
		addTest("ResetDurationsRestartsChannels", flags, PARTEST_CTX(this) { return this->resetDurationsRestartsChannels(ctx); });
		addTest("DedicatedUpdateTicksOnlyUpdate", flags, PARTEST_CTX(this) { return this->dedicatedUpdateTicksOnlyUpdate(ctx); });
		addTest("DedicatedPhysicsTicksOnlyPhysics", flags, PARTEST_CTX(this) { return this->dedicatedPhysicsTicksOnlyPhysics(ctx); });
	}

	// The built-in channels carry the names they were registered with, where an
	// unregistered type would read as "Tick N".
	void constructionRegistersUpdateAndPhysics(partest::TestContext &ctx)
	{
		cge::Clock clock;

		ASSERT_EQUAL(clock.getChannelName(cge::TickTypes::Update), std::string("Update"));
		ASSERT_EQUAL(clock.getChannelName(cge::TickTypes::Physics), std::string("Physics"));
	}

	void registerChannelSucceedsForNewType(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;

		bool result = clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		ASSERT_TRUE(result);
		ASSERT_EQUAL(clock.getChannelName(custom), std::string("Custom"));
	}

	// Registering a type that already has a channel replaces its name and
	// interval rather than refusing.
	void registerChannelUpdatesExistingType(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;

		clock.registerChannel(custom, "First", std::chrono::milliseconds(10));
		bool result = clock.registerChannel(custom, "Second", std::chrono::milliseconds(50));

		ASSERT_TRUE(result);
		ASSERT_EQUAL(clock.getChannelName(custom), std::string("Second"));
		ASSERT_TRUE(clock.getInterval(custom) == std::chrono::milliseconds(50));
	}

	// Re-registering changes configuration only. A channel already ticking keeps
	// its count and time scale.
	void registerChannelKeepsTickState(partest::TestContext &ctx)
	{
		cge::Clock clock;
		clock.setTimeScale(cge::TickTypes::Update, 2.0);
		clock.tickUpdate();

		clock.registerChannel(cge::TickTypes::Update, "Renamed", std::chrono::milliseconds(16));

		ASSERT_EQUAL(clock.getTickCount(cge::TickTypes::Update), 1u);
		ASSERT_APPROX_EQUAL(clock.getTimeScale(cge::TickTypes::Update), 2.0, 0.0001);
		ASSERT_EQUAL(clock.getChannelName(cge::TickTypes::Update), std::string("Renamed"));
	}

	void registeredChannelHasExpectedInitialState(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		std::chrono::steady_clock::duration interval = std::chrono::milliseconds(25);

		clock.registerChannel(custom, "Custom", interval);

		ASSERT_EQUAL(clock.getChannelName(custom), std::string("Custom"));
		ASSERT_EQUAL(clock.getTickCount(custom), 0u);
		ASSERT_TRUE(clock.getInterval(custom) == interval);
		ASSERT_APPROX_EQUAL(clock.getTimeScale(custom), 1.0, 0.0001);
	}

	// Reading a type that has no channel reports the state a fresh channel would
	// have, and does not create one.
	void unregisteredChannelReadsAsFresh(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		const std::string expectedName = "Tick " + std::to_string(custom.id());

		ASSERT_EQUAL(clock.getChannelName(custom), expectedName);
		ASSERT_EQUAL(clock.getTickCount(custom), 0u);
		ASSERT_TRUE(clock.getInterval(custom) == cge::TickTime::Zero);
		ASSERT_APPROX_EQUAL(clock.getTimeScale(custom), 1.0, 0.0001);
	}

	// Ticking a type with no channel creates one with the same default name the
	// read reported, so the name does not change once the channel exists.
	void tickCreatesUnregisteredChannel(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		const std::string expectedName = "Tick " + std::to_string(custom.id());

		clock.tick(custom);

		ASSERT_EQUAL(clock.getTickCount(custom), 1u);
		ASSERT_EQUAL(clock.getChannelName(custom), expectedName);
	}

	void tickIncrementsCount(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));

		clock.tick(custom);
		clock.tick(custom);

		ASSERT_EQUAL(clock.getTickCount(custom), 2u);
	}

	void rawTickIncrementsCount(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));

		clock.rawTick(custom);
		clock.rawTick(custom);
		clock.rawTick(custom);

		ASSERT_EQUAL(clock.getTickCount(custom), 3u);
	}

	void setIntervalUpdatesChannel(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		clock.setInterval(custom, std::chrono::milliseconds(50));

		ASSERT_TRUE(clock.getInterval(custom) == std::chrono::milliseconds(50));
	}

	void setTimeScaleUpdatesChannel(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		clock.setTimeScale(custom, 2.0);

		ASSERT_APPROX_EQUAL(clock.getTimeScale(custom), 2.0, 0.0001);
	}

	// Sleeps for a real, known duration then checks that a doubled timeScale roughly
	// doubles the reported delta. Generous threshold to absorb scheduler jitter.
	void timeScaleAppliesToTick(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));
		clock.setTimeScale(custom, 2.0);

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		std::chrono::steady_clock::duration scaled = clock.tick(custom);

		double scaledSeconds = std::chrono::duration<double>(scaled).count();

		ASSERT_GREATER(scaledSeconds, 0.03);
	}

	// Time spent before the reset is not reported by the next tick. The sleep is
	// long against the threshold, so a missed reset is unambiguous and scheduler
	// jitter is not.
	void resetDurationsRestartsChannels(partest::TestContext &ctx)
	{
		cge::Clock clock;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		clock.resetDurations();

		double updateSeconds = std::chrono::duration<double>(clock.rawTickUpdate()).count();
		double physicsSeconds = std::chrono::duration<double>(clock.rawTickPhysics()).count();

		ASSERT_LESS(updateSeconds, 0.02);
		ASSERT_LESS(physicsSeconds, 0.02);
	}

	// The dedicated functions tick their own channel and no other.
	void dedicatedUpdateTicksOnlyUpdate(partest::TestContext &ctx)
	{
		cge::Clock clock;

		clock.tickUpdate();

		ASSERT_EQUAL(clock.getTickCount(cge::TickTypes::Update), 1u);
		ASSERT_EQUAL(clock.getTickCount(cge::TickTypes::Physics), 0u);
	}

	void dedicatedPhysicsTicksOnlyPhysics(partest::TestContext &ctx)
	{
		cge::Clock clock;

		clock.rawTickPhysics();

		ASSERT_EQUAL(clock.getTickCount(cge::TickTypes::Physics), 1u);
		ASSERT_EQUAL(clock.getTickCount(cge::TickTypes::Update), 0u);
	}
};

#endif
