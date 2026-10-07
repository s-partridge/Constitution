#ifndef CLOCK_TEST_H
#define CLOCK_TEST_H

#include <chrono>
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
		addTest("RegisterChannelFailsForDuplicateType", flags, PARTEST_CTX(this) { return this->registerChannelFailsForDuplicateType(ctx); });
		addTest("RegisterChannelFailsForUpdateAfterConstruction", flags, PARTEST_CTX(this) { return this->registerChannelFailsForUpdateAfterConstruction(ctx); });
		addTest("RegisteredChannelHasExpectedInitialState", flags, PARTEST_CTX(this) { return this->registeredChannelHasExpectedInitialState(ctx); });
		addTest("TickIncrementsCount", flags, PARTEST_CTX(this) { return this->tickIncrementsCount(ctx); });
		addTest("RawTickIncrementsCount", flags, PARTEST_CTX(this) { return this->rawTickIncrementsCount(ctx); });
		addTest("SetIntervalUpdatesChannel", flags, PARTEST_CTX(this) { return this->setIntervalUpdatesChannel(ctx); });
		addTest("SetTimeScaleUpdatesChannel", flags, PARTEST_CTX(this) { return this->setTimeScaleUpdatesChannel(ctx); });
		addTest("TimeScaleAppliesToTick", flags, PARTEST_CTX(this) { return this->timeScaleAppliesToTick(ctx); });
		addTest("DedicatedUpdateAccessorsMatchGeneric", flags, PARTEST_CTX(this) { return this->dedicatedUpdateAccessorsMatchGeneric(ctx); });
		addTest("DedicatedPhysicsAccessorsMatchGeneric", flags, PARTEST_CTX(this) { return this->dedicatedPhysicsAccessorsMatchGeneric(ctx); });
	}

	void constructionRegistersUpdateAndPhysics(partest::TestContext &ctx)
	{
		cge::Clock clock;

		const cge::TickChannel &update = clock.getUpdateChannel();
		const cge::TickChannel &physics = clock.getPhysicsChannel();

		ASSERT_TRUE(update.type == cge::TickTypes::Update);
		ASSERT_TRUE(physics.type == cge::TickTypes::Physics);
	}

	void registerChannelSucceedsForNewType(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;

		bool result = clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		ASSERT_TRUE(result);
	}

	void registerChannelFailsForDuplicateType(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;

		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));
		bool result = clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		ASSERT_FALSE(result);
	}

	void registerChannelFailsForUpdateAfterConstruction(partest::TestContext &ctx)
	{
		cge::Clock clock;

		bool result = clock.registerChannel(cge::TickTypes::Update, "Update", std::chrono::milliseconds(16));

		ASSERT_FALSE(result);
	}

	void registeredChannelHasExpectedInitialState(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		std::chrono::steady_clock::duration interval = std::chrono::milliseconds(25);

		clock.registerChannel(custom, "Custom", interval);
		const cge::TickChannel &channel = clock.getChannel(custom);

		ASSERT_TRUE(channel.type == custom);
		ASSERT_EQUAL(channel.name, "Custom");
		ASSERT_EQUAL(channel.count, 0u);
		ASSERT_TRUE(channel.interval == interval);
		ASSERT_APPROX_EQUAL(channel.timeScale, 1.0, 0.0001);
	}

	void tickIncrementsCount(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));

		clock.tick(custom);
		clock.tick(custom);

		ASSERT_EQUAL(clock.getChannel(custom).count, 2u);
	}

	void rawTickIncrementsCount(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));

		clock.rawTick(custom);
		clock.rawTick(custom);
		clock.rawTick(custom);

		ASSERT_EQUAL(clock.getChannel(custom).count, 3u);
	}

	void setIntervalUpdatesChannel(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		clock.setInterval(custom, std::chrono::milliseconds(50));

		ASSERT_TRUE(clock.getChannel(custom).interval == std::chrono::milliseconds(50));
	}

	void setTimeScaleUpdatesChannel(partest::TestContext &ctx)
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		clock.setTimeScale(custom, 2.0);

		ASSERT_APPROX_EQUAL(clock.getChannel(custom).timeScale, 2.0, 0.0001);
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

	void dedicatedUpdateAccessorsMatchGeneric(partest::TestContext &ctx)
	{
		cge::Clock clock;

		clock.tickUpdate();

		ASSERT_EQUAL(clock.getUpdateChannel().count, 1u);
		ASSERT_TRUE(clock.getChannel(cge::TickTypes::Update).type == cge::TickTypes::Update);
	}

	void dedicatedPhysicsAccessorsMatchGeneric(partest::TestContext &ctx)
	{
		cge::Clock clock;

		clock.rawTickPhysics();

		ASSERT_EQUAL(clock.getPhysicsChannel().count, 1u);
		ASSERT_TRUE(clock.getChannel(cge::TickTypes::Physics).type == cge::TickTypes::Physics);
	}
};

#endif
