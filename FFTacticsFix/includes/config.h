#include <wtypes.h>

struct GameConfig
{
	bool PreferMovies = false;
	bool DisableFilter = false;
	int RenderScale = 4;
};

extern GameConfig Config;