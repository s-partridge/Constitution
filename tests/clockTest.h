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

	void constructionRegistersUpdateAndPhysics()
	{
		cge::Clock clock;

		const cge::TickChannel &update = clock.getUpdateChannel();
		const cge::TickChannel &physics = clock.getPhysicsChannel();

		ASSERT_TRUE(update.type == cge::TickTypes::Update);
		ASSERT_TRUE(physics.type == cge::TickTypes::Physics);
	}

	void registerChannelSucceedsForNewType()
	{
		cge::Clock clock;
		cge::TickType custom;

		bool result = clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		ASSERT_TRUE(result);
	}

	void registerChannelFailsForDuplicateType()
	{
		cge::Clock clock;
		cge::TickType custom;

		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));
		bool result = clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		ASSERT_FALSE(result);
	}

	void registerChannelFailsForUpdateAfterConstruction()
	{
		cge::Clock clock;

		bool result = clock.registerChannel(cge::TickTypes::Update, "Update", std::chrono::milliseconds(16));

		ASSERT_FALSE(result);
	}

	void registeredChannelHasExpectedInitialState()
	{
		cge::Clock clock;
		cge::TickType custom;
		std::chrono::steady_clock::duration interval = std::chrono::milliseconds(25);

		clock.registerChannel(custom, "Custom", interval);
		const cge::TickChannel &channel = clock.getChannel(custom);

		ASSERT_TRUE(channel.type == custom);
		ASSERT_EQUAL(channel.name, "Custom");
		ASSERT_EQUAL(channel.count, static_cast<size_t>(0));
		ASSERT_TRUE(channel.interval == interval);
		ASSERT_APPROX_EQUAL(channel.timeScale, 1.0, 0.0001);
	}

	void tickIncrementsCount()
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));

		clock.tick(custom);
		clock.tick(custom);

		ASSERT_EQUAL(clock.getChannel(custom).count, static_cast<size_t>(2));
	}

	void rawTickIncrementsCount()
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));

		clock.rawTick(custom);
		clock.rawTick(custom);
		clock.rawTick(custom);

		ASSERT_EQUAL(clock.getChannel(custom).count, static_cast<size_t>(3));
	}

	void setIntervalUpdatesChannel()
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		clock.setInterval(custom, std::chrono::milliseconds(50));

		ASSERT_TRUE(clock.getChannel(custom).interval == std::chrono::milliseconds(50));
	}

	void setTimeScaleUpdatesChannel()
	{
		cge::Clock clock;
		cge::TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		clock.setTimeScale(custom, 2.0);

		ASSERT_APPROX_EQUAL(clock.getChannel(custom).timeScale, 2.0, 0.0001);
	}

	// Sleeps for a real, known duration then checks that a doubled timeScale roughly
	// doubles the reported delta. Generous threshold to absorb scheduler jitter.
	void timeScaleAppliesToTick()
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

	void dedicatedUpdateAccessorsMatchGeneric()
	{
		cge::Clock clock;

		clock.tickUpdate();

		ASSERT_EQUAL(clock.getUpdateChannel().count, static_cast<size_t>(1));
		ASSERT_TRUE(clock.getChannel(cge::TickTypes::Update).type == cge::TickTypes::Update);
	}

	void dedicatedPhysicsAccessorsMatchGeneric()
	{
		cge::Clock clock;

		clock.rawTickPhysics();

		ASSERT_EQUAL(clock.getPhysicsChannel().count, static_cast<size_t>(1));
		ASSERT_TRUE(clock.getChannel(cge::TickTypes::Physics).type == cge::TickTypes::Physics);
	}
};

#endif
