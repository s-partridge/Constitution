#ifndef ENGINE_TEST_H
#define ENGINE_TEST_H

#include <partest/testbase.h>

class EngineTest : public partest::TestBase
{
public:
	EngineTest() : TestBase("EngineTest", "Validation for the engine library.")
	{
	}
};

#endif