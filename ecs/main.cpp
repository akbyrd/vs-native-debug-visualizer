#include "types.h"

Archetype  vFunc(Archetype* archetype) { return *archetype; }
Archetype& rFunc(Archetype* archetype) { return *archetype; }
Archetype* pFunc(Archetype* archetype) { return  archetype; }

using vArray = Archetype[1];
using pArray = Archetype*[1];

const vArray& avFunc(Archetype* archetype) { static vArray arr; arr[0] = *archetype; return arr; }
const pArray& apFunc(Archetype* archetype) { static pArray arr; arr[0] =  archetype; return arr; }

int main()
{
	Component component = {};
	component.x = 42;

	Archetype archetype = {};
	archetype.entityCount = 1;
	archetype.componentType = "Component";
	archetype.componentArray = &component;

	// Test Various Scenarios
	Archetype  vArchetype =  archetype;
	Archetype& rArchetype =  archetype;
	Archetype* pArchetype = &archetype;

	Archetype  avArchetype[] = {  archetype };
	Archetype* apArchetype[] = { &archetype };

	vFunc(&archetype);
	rFunc(&archetype);
	pFunc(&archetype);

	avFunc(&archetype);
	apFunc(&archetype);

	return 0;
}
