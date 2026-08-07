#ifndef CGE_DISPATCHER_TOPOLOGY_TEST_H
#define CGE_DISPATCHER_TOPOLOGY_TEST_H

#include "eventTestSupport.h"

namespace cge::test
{
	// Routing boundaries between dispatchers and registries. Expected to grow
	// once more than one dispatcher flavor exists and mixed topologies become
	// possible.
	class DispatcherTopologyTest : public DispatcherFlavorSuite
	{
	public:
		explicit DispatcherTopologyTest(const DispatcherFlavor &flavor);

	private:
		void isolation();
	};
}

#endif // CGE_DISPATCHER_TOPOLOGY_TEST_H
