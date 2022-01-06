struct NoVis
{
	int x;
};

struct DefaultVis
{
	int x;
};

struct CustomVis
{
	int x;
};

int main()
{
	NoVis noVis = {};
	NoVis  avNo[] = {  noVis };
	NoVis* apNo[] = { &noVis };

	DefaultVis defaultVis = {};
	DefaultVis  avDefault[] = {  defaultVis };
	DefaultVis* apDefault[] = { &defaultVis };

	CustomVis customVis = {};
	CustomVis  avCustom[] = {  customVis };
	CustomVis* apCustom[] = { &customVis };

	return 0;
}
