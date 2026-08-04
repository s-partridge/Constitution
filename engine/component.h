#ifndef CGE_COMPONENT_H
#define CGE_COMPONENT_H

namespace cge
{
	class ComponentBase
	{
	public:
		ComponentBase() = default;
		virtual ~ComponentBase() = default;

		/**
		* Perform initialization after being added to an owner.
		* 
		* Override to perform initialization that requires access to the
		* owner or other components. Called once after add_component().
		*/
		virtual void setUp() = 0;

		/**
		* Release resources before owner destruction.
		* 
		* Override to release resources, unsubscribe from events, etc.
		* Called once before the owner is destroyed.
		*/
		virtual void tearDown() = 0;
	};
}

#endif