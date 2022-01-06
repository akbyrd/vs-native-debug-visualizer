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
	DefaultVis defaultVis = {};
	DefaultVis  avDefault[] = {  defaultVis };
	DefaultVis* apDefault[] = { &defaultVis };

	CustomVis customVis = {};
	CustomVis  avCustom[] = {  customVis };
	CustomVis* apCustom[] = { &customVis };

	return 0;
}
