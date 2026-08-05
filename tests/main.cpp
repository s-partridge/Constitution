#include <iostream>

#include <partest/bootstrap.h>
#include "engineTest.h"
#include "clockTest.h"
#include "eventSystemTest.h"

int main(int argc, const char **argv)
{
	partest::initializeSuite(argc, argv);
	partest::addTestClass(partest::make_unique<EngineTest>());
	partest::addTestClass(partest::make_unique<ClockTest>());
	partest::addTestClass(partest::make_unique<EventChannelRegistryTest>());
	partest::addTestClass(partest::make_unique<AsyncDispatcherTest>());
	partest::addTestClass(partest::make_unique<EventSystemLoadTest>());
	partest::addTestClass(partest::make_unique<BroadcasterBaseTest>());
	partest::addTestClass(partest::make_unique<CommanderBaseTest>());
	partest::addTestClass(partest::make_unique<ListenerBaseTest>());

	partest::runAllTests();
	partest::displayAllTests();
	size_t assertions = partest::getAssertionFailureCount();
	size_t results = partest::getTopLevelFailures();

	std::cout << "Total assertion failures: " << assertions << std::endl;
	std::cout << "Total top-level failures: " << results << std::endl;

	return (int)results;
}