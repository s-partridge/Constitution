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

		addTest("ConstructionRegistersUpdateAndPhysics", flags, [this]() { return this->constructionRegistersUpdateAndPhysics(); });
		addTest("RegisterChannelSucceedsForNewType", flags, [this]() { return this->registerChannelSucceedsForNewType(); });
		addTest("RegisterChannelFailsForDuplicateType", flags, [this]() { return this->registerChannelFailsForDuplicateType(); });
		addTest("RegisterChannelFailsForUpdateAfterConstruction", flags, [this]() { return this->registerChannelFailsForUpdateAfterConstruction(); });
		addTest("RegisteredChannelHasExpectedInitialState", flags, [this]() { return this->registeredChannelHasExpectedInitialState(); });
		addTest("TickIncrementsCount", flags, [this]() { return this->tickIncrementsCount(); });
		addTest("RawTickIncrementsCount", flags, [this]() { return this->rawTickIncrementsCount(); });
		addTest("SetIntervalUpdatesChannel", flags, [this]() { return this->setIntervalUpdatesChannel(); });
		addTest("SetTimeScaleUpdatesChannel", flags, [this]() { return this->setTimeScaleUpdatesChannel(); });
		addTest("TimeScaleAppliesToTick", flags, [this]() { return this->timeScaleAppliesToTick(); });
		addTest("DedicatedUpdateAccessorsMatchGeneric", flags, [this]() { return this->dedicatedUpdateAccessorsMatchGeneric(); });
		addTest("DedicatedPhysicsAccessorsMatchGeneric", flags, [this]() { return this->dedicatedPhysicsAccessorsMatchGeneric(); });
	}

	void constructionRegistersUpdateAndPhysics()
	{
		Clock clock;

		const TickChannel &update = clock.getUpdateChannel();
		const TickChannel &physics = clock.getPhysicsChannel();

		ASSERT_TRUE(update.type == TickTypes::Update);
		ASSERT_TRUE(physics.type == TickTypes::Physics);
	}

	void registerChannelSucceedsForNewType()
	{
		Clock clock;
		TickType custom;

		bool result = clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		ASSERT_TRUE(result);
	}

	void registerChannelFailsForDuplicateType()
	{
		Clock clock;
		TickType custom;

		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));
		bool result = clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		ASSERT_FALSE(result);
	}

	void registerChannelFailsForUpdateAfterConstruction()
	{
		Clock clock;

		bool result = clock.registerChannel(TickTypes::Update, "Update", std::chrono::milliseconds(16));

		ASSERT_FALSE(result);
	}

	void registeredChannelHasExpectedInitialState()
	{
		Clock clock;
		TickType custom;
		std::chrono::steady_clock::duration interval = std::chrono::milliseconds(25);

		clock.registerChannel(custom, "Custom", interval);
		const TickChannel &channel = clock.getChannel(custom);

		ASSERT_TRUE(channel.type == custom);
		ASSERT_EQUAL(channel.name, "Custom");
		ASSERT_EQUAL(channel.count, static_cast<size_t>(0));
		ASSERT_TRUE(channel.interval == interval);
		ASSERT_APPROX_EQUAL(channel.timeScale, 1.0, 0.0001);
	}

	void tickIncrementsCount()
	{
		Clock clock;
		TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));

		clock.tick(custom);
		clock.tick(custom);

		ASSERT_EQUAL(clock.getChannel(custom).count, static_cast<size_t>(2));
	}

	void rawTickIncrementsCount()
	{
		Clock clock;
		TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));

		clock.rawTick(custom);
		clock.rawTick(custom);
		clock.rawTick(custom);

		ASSERT_EQUAL(clock.getChannel(custom).count, static_cast<size_t>(3));
	}

	void setIntervalUpdatesChannel()
	{
		Clock clock;
		TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		clock.setInterval(custom, std::chrono::milliseconds(50));

		ASSERT_TRUE(clock.getChannel(custom).interval == std::chrono::milliseconds(50));
	}

	void setTimeScaleUpdatesChannel()
	{
		Clock clock;
		TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(10));

		clock.setTimeScale(custom, 2.0);

		ASSERT_APPROX_EQUAL(clock.getChannel(custom).timeScale, 2.0, 0.0001);
	}

	// Sleeps for a real, known duration then checks that a doubled timeScale roughly
	// doubles the reported delta. Generous threshold to absorb scheduler jitter.
	void timeScaleAppliesToTick()
	{
		Clock clock;
		TickType custom;
		clock.registerChannel(custom, "Custom", std::chrono::milliseconds(0));
		clock.setTimeScale(custom, 2.0);

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		std::chrono::steady_clock::duration scaled = clock.tick(custom);

		double scaledSeconds = std::chrono::duration<double>(scaled).count();

		ASSERT_GREATER(scaledSeconds, 0.03);
	}

	void dedicatedUpdateAccessorsMatchGeneric()
	{
		Clock clock;

		clock.tickUpdate();

		ASSERT_EQUAL(clock.getUpdateChannel().count, static_cast<size_t>(1));
		ASSERT_TRUE(clock.getChannel(TickTypes::Update).type == TickTypes::Update);
	}

	void dedicatedPhysicsAccessorsMatchGeneric()
	{
		Clock clock;

		clock.rawTickPhysics();

		ASSERT_EQUAL(clock.getPhysicsChannel().count, static_cast<size_t>(1));
		ASSERT_TRUE(clock.getChannel(TickTypes::Physics).type == TickTypes::Physics);
	}
};

#endif
