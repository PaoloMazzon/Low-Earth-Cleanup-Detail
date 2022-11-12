#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <VK2D/VK2D.h>

#define ASSETS_IMPLEMENTATION
#include "Assets.h"
#include "JamUtil/JamUtil.h"

/********************* Types *********************/
typedef double real;
typedef enum {
	GAMESTATE_MENU = 0,
	GAMESTATE_GAME = 1,
	GAMESTATE_QUIT = 2,
	GAMESTATE_MAX = 3,
} gamestate;
/********************* Constants **********************/
int WINDOW_WIDTH = 800;
int WINDOW_HEIGHT = 600;

/********************* Globals *********************/
Assets *gAssets = NULL;
VK2DCameraIndex gCam = -1;

/********************* Structs **********************/
typedef struct {
	real x;
	real y;
	real velx;
	real vely;
} Player;

/********************* Common functions *********************/
void drawTiledBackground(VK2DTexture texture, float rate) {
	VK2DCameraSpec camera = vk2dCameraGetSpec(gCam);

	// Figure out how many tiles we need to draw and where to start drawing
	float tileStartX = camera.x * rate;
	while (tileStartX + texture->img->width < camera.x) tileStartX += texture->img->width;
	float tileStartY = camera.y * rate;
	while (tileStartY + texture->img->height < camera.y) tileStartY += texture->img->height;
	float tilesNeededHorizontal = ceilf(camera.w / texture->img->width) + 1;
	float tilesNeededVertical = ceilf(camera.h / texture->img->height) + 1;

	// Loop through and draw them all
	for (int y = 0; y < tilesNeededVertical; y++)
		for (int x = 0; x < tilesNeededHorizontal; x++)
			vk2dDrawTexture(texture, tileStartX + (x * texture->img->width), tileStartY + (y * texture->img->height));
}

/********************* Game functions *********************/
void gameStart() {

}

gamestate gameUpdate() {
	VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
	if (juKeyboardGetKey(SDL_SCANCODE_UP))
		spec.y -= 1;
	if (juKeyboardGetKey(SDL_SCANCODE_DOWN))
		spec.y += 1;
	if (juKeyboardGetKey(SDL_SCANCODE_LEFT))
		spec.x -= 1;
	if (juKeyboardGetKey(SDL_SCANCODE_RIGHT))
		spec.x += 1;
	vk2dCameraUpdate(gCam, spec);

	// Draw background
	vk2dRendererLockCameras(gCam);
	drawTiledBackground(gAssets->texBackground, 0.7);
	drawTiledBackground(gAssets->texForeground, 0.5);
	vk2dRendererUnlockCameras();

	return GAMESTATE_GAME;
}

void gameEnd() {

}

/********************* Menu functions *********************/
void menuStart() {

}

gamestate menuUpdate() {
	return GAMESTATE_GAME;
}

void menuEnd() {

}

/********************* Main *********************/
int main() {
	SDL_Window *window = SDL_CreateWindow("VK2D", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	SDL_Event e;
	VK2DRendererConfig config = {msaa_32x, sm_TripleBuffer, ft_Nearest};
	juInit(window, 3, 1);
	vk2dRendererInit(window, config);
	vec4 clearColour = {0, 0, 0, 1}; // Black
	vk2dRendererSetColourMod(VK2D_DEFAULT_COLOUR_MOD);
	bool stopRunning = false;
	double totalTime = 0;
	double iters = 0;
	double average = 1;
	JUClock framerateTimer;
	juClockReset(&framerateTimer);
	VK2DCameraSpec spec = {ct_Default, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 1, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
	gCam = vk2dCameraCreate(spec);
	gAssets = buildAssets();
	gamestate state = GAMESTATE_MENU;
	menuStart();

	while (!stopRunning) {
		juUpdate();
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				stopRunning = true;
			}
		}

		vk2dRendererStartFrame(clearColour);

		if (state == GAMESTATE_MENU) {
			state = menuUpdate();
			if (state == GAMESTATE_GAME) {
				menuEnd();
				gameStart();
			} else if (state == GAMESTATE_QUIT){
				stopRunning = true;
			}
		} else if (state == GAMESTATE_GAME) {
			state = gameUpdate();
			if (state == GAMESTATE_MENU) {
				gameEnd();
				menuStart();
			} else if (state == GAMESTATE_QUIT){
				stopRunning = true;
			}
		}

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
