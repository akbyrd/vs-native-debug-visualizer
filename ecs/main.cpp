#include "types.h"

int main()
{
	Component component = {};
	component.x = 42;

	Archetype archetype = {};
	archetype.entityCount = 1;
	archetype.componentType = "Component";
	archetype.componentArray = &component;

	return 0;
}
