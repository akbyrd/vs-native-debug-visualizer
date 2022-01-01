#pragma once

struct Component
{
	int x = 0;
};

struct Archetype
{
	int         entityCount;
	const char* componentType;
	void*       componentArray;
};
