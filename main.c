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
const int WINDOW_WIDTH  = 1024;
const int WINDOW_HEIGHT = 768;
const int GAME_WIDTH    = 1500;
const int GAME_HEIGHT   = 1125;
const real FPS_LIMIT    = 60;
const real ZOOM_MIN     = 0.5;
const real ZOOM_MAX     = 2;
const real ZOOM_SPEED   = 0.25;
const int FONT_WIDTH    = 40;
const int FONT_HEIGHT   = 70;

const real WORLD_MAX_WIDTH  = 60000;
const real WORLD_MAX_HEIGHT = 60000;

const real SUN_POS_X = 400;
const real SUN_POS_Y = 400;

const real PLAYER_START_X = WORLD_MAX_WIDTH / 2;
const real PLAYER_START_Y = WORLD_MAX_HEIGHT / 2;

const real PLAYER_BASE_ROTATE_ACCELERATION = VK2D_PI * 0.003;
const real PLAYER_BASE_ROTATE_FRICTION     = VK2D_PI * 0.001;
const real PLAYER_BASE_ROTATE_TOP_SPEED    = VK2D_PI * 0.02;

const real PLAYER_BASE_ACCELERATION = 0.10;
const real PLAYER_FRICTION          = 0.02;

const real PHYSICS_BASE_TOP_SPEED = 15;

/********************* Structs **********************/

// Physics vector
typedef struct {
	real magnitude; // Pixels
	real direction; // Radians
} Vector;

// Physics physics simulation
typedef struct {
	real x;
	real y;
	Vector velocity;
	real mass; // Kilograms
} Physics;

typedef struct {
	Physics physics;
	real dirVelocity;
	real direction;
} Player;

/********************* Globals *********************/
Assets *gAssets = NULL;
VK2DCameraIndex gCam = -1;
Player gPlayer = {};
real gZoom = 1;
JUFont gFont = NULL;

/********************* Common functions *********************/
void drawTiledBackground(VK2DTexture texture, float rate) {
	VK2DCameraSpec camera = vk2dCameraGetSpec(gCam);

	// Figure out how many tiles we need to draw and where to start drawing
	float tileStartX = camera.x * rate;
	float tileStartY = camera.y * rate;
	tileStartX += floorf((camera.x - tileStartX) / texture->img->width) * texture->img->width;
	tileStartY += floorf((camera.y - tileStartY) / texture->img->height) * texture->img->height;
	//while (tileStartX + texture->img->width < camera.x) tileStartX += texture->img->width;
	//while (tileStartY + texture->img->height < camera.y) tileStartY += texture->img->height;
	float tilesNeededHorizontal = ceilf(camera.w / texture->img->width) + 1;
	float tilesNeededVertical = ceilf(camera.h / texture->img->height) + 1;

	// Loop through and draw them all
	for (int y = 0; y < tilesNeededVertical; y++)
		for (int x = 0; x < tilesNeededHorizontal; x++)
			vk2dDrawTexture(texture, tileStartX + (x * texture->img->width), tileStartY + (y * texture->img->height));
}

/********************* Physics functions *********************/
void physicsStart(Physics *physics, real x, real y) {
	physics->x = x;
	physics->y = y;
	physics->velocity.magnitude = 0;
	physics->velocity.direction = 0;
}

void physicsUpdate(Physics *physics, Vector *acceleration) {
	// Add acceleration vector to the velocity vector then cap velocity
	real rise = (sin(acceleration->direction) * acceleration->magnitude) + (sin(physics->velocity.direction) * physics->velocity.magnitude);
	real run = (cos(acceleration->direction) * acceleration->magnitude) + (cos(physics->velocity.direction) * physics->velocity.magnitude);
	physics->velocity.direction = run == 0 ? 0 : atan2(rise, run);
	physics->velocity.magnitude = sqrt(pow(rise, 2) + pow(run, 2));
	physics->velocity.magnitude = juClamp(physics->velocity.magnitude, -PHYSICS_BASE_TOP_SPEED, PHYSICS_BASE_TOP_SPEED);

	// Apply velocity to coordinates then cap coordinates
	physics->x += juCastX(physics->velocity.magnitude, -physics->velocity.direction);
	physics->y += juCastY(physics->velocity.magnitude, -physics->velocity.direction);
	physics->x = juClamp(physics->x, 0, WORLD_MAX_WIDTH);
	physics->y = juClamp(physics->y, 0, WORLD_MAX_HEIGHT);
}

/********************* Player functions *********************/
void playerStart() {
	physicsStart(&gPlayer.physics, PLAYER_START_X, PLAYER_START_Y);
}

void playerUpdate() {
	// Rotate the ship
	if (juKeyboardGetKey(SDL_SCANCODE_A) || juKeyboardGetKey(SDL_SCANCODE_D)) {
		gPlayer.dirVelocity += (-((real) juKeyboardGetKey(SDL_SCANCODE_A)) + ((real) juKeyboardGetKey(SDL_SCANCODE_D))) * PLAYER_BASE_ROTATE_ACCELERATION;
	} else {
		if (juSign(gPlayer.dirVelocity - juSign(gPlayer.dirVelocity) * PLAYER_BASE_ROTATE_FRICTION) != juSign(gPlayer.dirVelocity))
			gPlayer.dirVelocity = 0;
		else
			gPlayer.dirVelocity -= juSign(gPlayer.dirVelocity) * PLAYER_BASE_ROTATE_FRICTION;
   	}
	gPlayer.dirVelocity = juClamp(gPlayer.dirVelocity, -PLAYER_BASE_ROTATE_TOP_SPEED, PLAYER_BASE_ROTATE_TOP_SPEED);
	gPlayer.direction += gPlayer.dirVelocity;

	// Calculate acceleration vector
	Vector acceleration = {};
	if (juKeyboardGetKey(SDL_SCANCODE_SPACE)) {
		acceleration.magnitude = PLAYER_BASE_ACCELERATION;
		acceleration.direction = gPlayer.direction;
	} else {
		acceleration.magnitude = PLAYER_FRICTION;
		acceleration.direction = gPlayer.physics.velocity.direction + VK2D_PI;
	}

	physicsUpdate(&gPlayer.physics, &acceleration);
}

void playerDraw() {
	VK2DTexture player;
	if (juKeyboardGetKey(SDL_SCANCODE_SPACE))
		player = gAssets->texPlayerThruster;
	else
		player = gAssets->texPlayer;
	vk2dDrawTextureExt(player, gPlayer.physics.x - (player->img->width / 2), gPlayer.physics.y - (player->img->height / 2), 1, 1, gPlayer.direction + (VK2D_PI / 2), player->img->width / 2, player->img->height / 2);
}

void playerEnd() {

}

/********************* Game functions *********************/

void gameDrawUI() {
	// Get screen w/h
	VK2DCameraSpec spec = vk2dCameraGetSpec(VK2D_DEFAULT_CAMERA);

	// Test
	juFontDraw(gFont, 0, 0, "The quick brown fox jumps over the lazy dog.");

	// Player velocity
	vec4 outline = {0, 0.2, 0, 1};
	vec4 fill = {0, 0.4, 0, 1};
	vec4 pointerRise = {0, 0, 0.9, 1};
	vec4 pointerRun = {0.9, 0, 0, 1};
	vec4 pointerHypotenuse = {1, 1, 1, 1};
	float topLeftX = 10;
	float topLeftY = spec.h - 110;
	float w = 120;
	float h = 80;
	float centerX = topLeftX + (w / 2);
	float centerY = topLeftY + (h / 2);
	float rise = (sin(gPlayer.physics.velocity.direction) * gPlayer.physics.velocity.magnitude) * 2.6;
	float run = (cos(gPlayer.physics.velocity.direction) * gPlayer.physics.velocity.magnitude) * 3.2;
	vk2dRendererSetColourMod(fill);
	vk2dDrawRectangle(topLeftX, topLeftY, w, h); // Background
	vk2dRendererSetColourMod(outline);
	vk2dDrawRectangleOutline(topLeftX, topLeftY, w, h, 1); // Outline
	vk2dDrawLine(centerX, topLeftY, centerX, topLeftY + h); // Cross section up down
	vk2dDrawLine(topLeftX, centerY, topLeftX + w, centerY); // Cross section left right
	vk2dRendererSetColourMod(pointerRise);
	vk2dDrawLine(centerX + run, centerY, centerX + run, centerY + rise); // Rise
	vk2dRendererSetColourMod(pointerRun);
	vk2dDrawLine(centerX, centerY, centerX + run, centerY); // Run
	vk2dRendererSetColourMod(pointerHypotenuse);
	vk2dDrawLine(centerX, centerY, centerX + run, centerY + rise); // Hyp
	vk2dRendererSetColourMod(VK2D_DEFAULT_COLOUR_MOD);
}

void gameStart() {
	playerStart();
}

gamestate gameUpdate() {
	// Update entities
	playerUpdate();

	// Update camera around player
	VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
	spec.x = gPlayer.physics.x - (spec.w / 2);
	spec.y = gPlayer.physics.y - (spec.h / 2);
	spec.x = juClamp(spec.x, 0, WORLD_MAX_WIDTH - spec.w);
	spec.y = juClamp(spec.y, 0, WORLD_MAX_HEIGHT - spec.h);
	vk2dCameraUpdate(gCam, spec);

	// Lock camera to world camera and draw world
	vk2dRendererLockCameras(gCam);
	vk2dDrawTexture(gAssets->texSun, spec.x + SUN_POS_X - spec.x * 0.05, spec.y + SUN_POS_Y - spec.y * 0.05);
	drawTiledBackground(gAssets->texBackground, 0.8);
	drawTiledBackground(gAssets->texMidground, 0.6);
	drawTiledBackground(gAssets->texForeground, 0.5);
	playerDraw();

	// UI is drawn to the default camera
	vk2dRendererLockCameras(VK2D_DEFAULT_CAMERA);
	gameDrawUI();

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
	vec4 clearColour = {0, 0, 13.0/255.0, 1}; // Black
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
	gFont = juFontLoadFromImage("assets/Font.png", 32, 128, FONT_WIDTH, FONT_HEIGHT);
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

		// Handle zoom
		gZoom += (((real) juKeyboardGetKeyPressed(SDL_SCANCODE_Q)) - ((real) juKeyboardGetKeyPressed(SDL_SCANCODE_E))) * ZOOM_SPEED;
		gZoom = juClamp(gZoom, ZOOM_MIN, ZOOM_MAX);

		// Adjust for possible new window size
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
		spec.wOnScreen = w;
		spec.hOnScreen = h;
		spec.w = (float)GAME_WIDTH * gZoom;
		spec.h = ((float)GAME_WIDTH * gZoom) * ((float)h / (float)w);
		vk2dCameraUpdate(gCam, spec);

		// Fix for default camera being wonky???
		spec = vk2dCameraGetSpec(VK2D_DEFAULT_CAMERA);
		spec.y = spec.yOnScreen = 0;
		spec.zoom = 1;
		vk2dCameraUpdate(VK2D_DEFAULT_CAMERA, spec);

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
	juFontFree(gFont);
	destroyAssets(gAssets);
	juQuit();
	vk2dRendererQuit();
	SDL_DestroyWindow(window);
	return 0;
}
