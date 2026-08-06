#include <iostream>

#include <partest/bootstrap.h>
#include "engineTest.h"
#include "clockTest.h"
#include "eventSystemTest.h"
#include "event/channelRegistryTest.h"
#include "event/dispatchCycleTest.h"
#include "event/dispatcherLifecycleTest.h"
#include "event/listenerRegistrationTest.h"
#include "event/payloadTest.h"

int main(int argc, const char **argv)
{
	partest::initializeSuite(argc, argv);
	partest::addTestClass(partest::make_unique<EngineTest>());
	partest::addTestClass(partest::make_unique<ClockTest>());
	partest::addTestClass(partest::make_unique<cge::test::ChannelRegistryTest>());
	partest::addTestClass(partest::make_unique<AsyncDispatcherTest>());
	partest::addTestClass(partest::make_unique<EventSystemLoadTest>());
	partest::addTestClass(partest::make_unique<CommanderBaseTest>());
	// Base contracts run once per dispatcher flavor.
	for(const cge::test::DispatcherFlavor &flavor : cge::test::dispatcherFlavors())
	{
		partest::addTestClass(partest::make_unique<cge::test::DispatcherLifecycleTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::DispatchCycleTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::ListenerRegistrationTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::PayloadTest>(flavor));
	}

	partest::runAllTests();
	partest::displayAllTests();
	size_t assertions = partest::getAssertionFailureCount();
	size_t results = partest::getTopLevelFailures();

	std::cout << "Total assertion failures: " << assertions << std::endl;
	std::cout << "Total top-level failures: " << results << std::endl;

	return (int)results;
}