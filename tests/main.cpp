#include <iostream>

#include <partest/bootstrap.h>
#include "engineTest.h"
#include "clockTest.h"

int main(int argc, const char **argv)
{
	partest::initializeSuite(argc, argv);
	partest::addTestClass(partest::make_unique<EngineTest>());
	partest::addTestClass(partest::make_unique<ClockTest>());

	partest::runAllTests();
	partest::displayAllTests();
	size_t assertions = partest::getAssertionFailureCount();
	size_t results = partest::getTopLevelFailures();

	std::cout << "Total assertion failures: " << assertions << std::endl;
	std::cout << "Total top-level failures: " << results << std::endl;

	return (int)results;
}