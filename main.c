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
const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 768;
const int GAME_WIDTH = 1500;
const int GAME_HEIGHT = 1125;
const real FPS_LIMIT = 60;
const real WORLD_MAX_WIDTH = 6000;
const real WORLD_MAX_HEIGHT = 6000;
const real PLAYER_START_X = WORLD_MAX_WIDTH / 2;
const real PLAYER_START_Y = WORLD_MAX_HEIGHT / 2;
const real PLAYER_BASE_ROTATE_SPEED = VK2D_PI / 120;
const real PLAYER_BASE_ACCELERATION = 0.05;
const real PLAYER_BASE_TOP_SPEED = 15;
const real PLAYER_FRICTION = 0.02;

/********************* Structs **********************/
typedef struct {
	real x;
	real y;
	real dir;
	real velocity;
} Player;

/********************* Globals *********************/
Assets *gAssets = NULL;
VK2DCameraIndex gCam = -1;
Player gPlayer = {};

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

/********************* Player functions *********************/
void playerStart() {
	gPlayer.x = PLAYER_START_X;
	gPlayer.y = PLAYER_START_Y;
	gPlayer.dir = gPlayer.velocity = 0;
}

void playerUpdate() {
	// Rotate the ship
	gPlayer.dir += (-((real)juKeyboardGetKey(SDL_SCANCODE_A)) + ((real)juKeyboardGetKey(SDL_SCANCODE_D))) * PLAYER_BASE_ROTATE_SPEED;

	// Accelerate the ship
	if (juKeyboardGetKey(SDL_SCANCODE_SPACE))
		gPlayer.velocity += PLAYER_BASE_ACCELERATION;
	else
		gPlayer.velocity -= PLAYER_FRICTION;

	// Apply velocity and clamp cooridnates
	gPlayer.velocity = juClamp(gPlayer.velocity, 0, PLAYER_BASE_TOP_SPEED);
	gPlayer.x += juCastX(gPlayer.velocity, -gPlayer.dir);
	gPlayer.y += juCastY(gPlayer.velocity, -gPlayer.dir);
	gPlayer.x = juClamp(gPlayer.x, 0, WORLD_MAX_WIDTH);
	gPlayer.y = juClamp(gPlayer.y, 0, WORLD_MAX_HEIGHT);
}

void playerDraw() {
	VK2DTexture player;
	if (juKeyboardGetKey(SDL_SCANCODE_SPACE))
		player = gAssets->texPlayerThruster;
	else
		player = gAssets->texPlayer;
	vk2dDrawTextureExt(player, gPlayer.x - (player->img->width / 2), gPlayer.y - (player->img->height / 2), 1, 1, gPlayer.dir + (VK2D_PI / 2), player->img->width / 2, player->img->height / 2);
}

void playerEnd() {

}

/********************* Game functions *********************/
void gameStart() {
	playerStart();
}

gamestate gameUpdate() {
	// Update entities
	playerUpdate();

	// Update camera around player
	VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
	spec.x = gPlayer.x - (spec.w / 2);
	spec.y = gPlayer.y - (spec.h / 2);
	spec.x = juClamp(spec.x, 0, WORLD_MAX_WIDTH - spec.w);
	spec.y = juClamp(spec.y, 0, WORLD_MAX_HEIGHT - spec.h);
	vk2dCameraUpdate(gCam, spec);

	// Draw world
	vk2dRendererLockCameras(gCam);
	drawTiledBackground(gAssets->texBackground, 0.7);
	drawTiledBackground(gAssets->texForeground, 0.5);
	playerDraw();
	vk2dRendererUnlockCameras();

	return GAMESTATE_GAME;
}

void gameEnd() {
	playerEnd();
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
	// Initialize a billion things
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
	JUClock fpsLock;
	juClockStart(&fpsLock);

	// Game loop, just calls either menu or game update and swaps between them when necessary
	while (!stopRunning) {
		juUpdate();
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				stopRunning = true;
			}
		}

		// Adjust for possible new window size
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
		spec.wOnScreen = w;
		spec.hOnScreen = h;
		spec.w = GAME_WIDTH;
		spec.h = (float)GAME_WIDTH * ((float)h / (float)w);
		vk2dCameraUpdate(gCam, spec);

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

		// Get average time
		if (juClockTime(&framerateTimer) >= 1) {
			average = totalTime / iters;
			totalTime = 0;
			iters = 0;
			juClockStart(&framerateTimer);
		} else {
			totalTime += juDelta();
			iters += 1;
		}
		juClockFramerate(&fpsLock, FPS_LIMIT); // Lock framerate
		vk2dRendererEndFrame();
	}

	// Cleanup
	vk2dRendererWait();
	destroyAssets(gAssets);
	juQuit();
	vk2dRendererQuit();
	SDL_DestroyWindow(window);
	return 0;
}
