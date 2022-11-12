#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <VK2D/VK2D.h>

#define ASSETS_IMPLEMENTATION
#include "Assets.h"
#include "JamUtil/JamUtil.h"

/********************* Types *********************/
typedef double real;

/********************* Constants **********************/
int WINDOW_WIDTH = 800;
int WINDOW_HEIGHT = 600;

/********************* Globals *********************/
Assets *gAssets = NULL;

/********************* Main *********************/
int main() {
	SDL_Window *window = SDL_CreateWindow("VK2D", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	SDL_Event e;
	VK2DRendererConfig config = {msaa_32x, sm_TripleBuffer, ft_Nearest};
	juInit(window, 10, 3);
	vk2dRendererInit(window, config);
	vec4 clearColour = {0, 0, 0, 1}; // Black
	bool stopRunning = false;
	double totalTime = 0;
	double iters = 0;
	double average = 1;
	JUClock framerateTimer;
	juClockReset(&framerateTimer);
	gAssets = buildAssets();

	while (!stopRunning) {
		juUpdate();
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				stopRunning = true;
			}
		}

		vk2dRendererStartFrame(clearColour);

		// Frame timing
		if (juClockTime(&framerateTimer) >= 1) {
			average = totalTime / iters;
			totalTime = 0;
			iters = 0;
			juClockStart(&framerateTimer);
		} else {
			totalTime += juDelta();
			iters += 1;
		}
		vk2dRendererEndFrame();
	}

	vk2dRendererWait();
	destroyAssets(gAssets);
	juQuit();
	vk2dRendererQuit();
	SDL_DestroyWindow(window);
	return 0;
}
