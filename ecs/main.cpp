struct NoVis
{
	int x;
};

struct DefaultVis
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

	return 0;
}
