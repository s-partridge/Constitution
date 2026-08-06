#include <iostream>

#include <partest/bootstrap.h>
#include "engineTest.h"
#include "clockTest.h"
#include "event/unit/asyncDispatcherTest.h"
#include "event/unit/broadcasterTest.h"
#include "event/unit/eventTest.h"
#include "event/unit/dispatcherTest.h"
#include "event/unit/listenerTest.h"
#include "event/integration/dispatchCycleTest.h"
#include "event/integration/dispatcherLifecycleTest.h"
#include "event/integration/listenerRegistrationTest.h"
#include "event/integration/commanderTest.h"
#include "event/integration/dispatcherTopologyTest.h"
#include "event/integration/eventConcurrencyTest.h"
#include "event/integration/eventLoadTest.h"
#include "event/integration/payloadTest.h"

int main(int argc, const char **argv)
{
	partest::initializeSuite(argc, argv);
	partest::addTestClass(partest::make_unique<EngineTest>());
	partest::addTestClass(partest::make_unique<ClockTest>());
	partest::addTestClass(partest::make_unique<cge::test::EventUnitTest>());
	partest::addTestClass(partest::make_unique<cge::test::BroadcasterUnitTest>());
	partest::addTestClass(partest::make_unique<cge::test::ListenerUnitTest>());
	partest::addTestClass(partest::make_unique<cge::test::DispatcherUnitTest>());
	partest::addTestClass(partest::make_unique<cge::test::AsyncDispatcherUnitTest>());
	// Base contracts run once per dispatcher flavor.
	for(const cge::test::DispatcherFlavor &flavor : cge::test::dispatcherFlavors())
	{
		partest::addTestClass(partest::make_unique<cge::test::DispatcherLifecycleTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::DispatchCycleTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::ListenerRegistrationTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::PayloadTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::CommanderTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::DispatcherTopologyTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::EventConcurrencyTest>(flavor));
		partest::addTestClass(partest::make_unique<cge::test::EventLoadTest>(flavor));
	}

	partest::runAllTests();
	partest::displayAllTests();
	size_t assertions = partest::getAssertionFailureCount();
	size_t results = partest::getTopLevelFailures();

	std::cout << "Total assertion failures: " << assertions << std::endl;
	std::cout << "Total top-level failures: " << results << std::endl;

	return (int)results;
}