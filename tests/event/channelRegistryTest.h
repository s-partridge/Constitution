#ifndef CGE_CHANNEL_REGISTRY_TEST_H
#define CGE_CHANNEL_REGISTRY_TEST_H

#include <partest/testbase.h>

namespace cge::test
{
	// Channel identity and construction policy. Nothing here touches a
	// dispatcher, so this suite runs once rather than once per flavor.
	class ChannelRegistryTest : public partest::TestBase
	{
	public:
		ChannelRegistryTest();

	private:
		void lookup();
		void constructionPolicy();
	};
}

#endif // CGE_CHANNEL_REGISTRY_TEST_H
