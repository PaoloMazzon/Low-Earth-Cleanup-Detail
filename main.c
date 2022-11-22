#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <VK2D/VK2D.h>
#include <time.h>

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
typedef enum {
	ENTITY_TYPE_NONE = 0,
	ENTITY_TYPE_PLAYER = 1,
	ENTITY_TYPE_TRASH = 2,
	ENTITY_TYPE_DRONE = 3,
	ENTITY_TYPE_MINE = 4,
	ENTITY_TYPE_GARBAGE_DISPOSAL = 5,
	ENTITY_TYPE_MAX = 6,
} entitytype;

/********************* Constants **********************/
const int   WINDOW_WIDTH   = 1024;
const int   WINDOW_HEIGHT  = 768;
const int   GAME_WIDTH     = 1500;
const int   GAME_HEIGHT    = 1125;
const real  FPS_LIMIT      = 60;
const real  ZOOM_MIN       = 0.5;
const real  ZOOM_MAX       = 2;
const real  ZOOM_SPEED     = 0.25;
const int   FONT_WIDTH     = 40;
const int   FONT_HEIGHT    = 70;
const float CAMERA_SPEED   = 0.2;
const int   NO_TRASH       = -1;

const real WORLD_MAX_WIDTH  = 60000;
const real WORLD_MAX_HEIGHT = 60000;

const real SUN_POS_X = 400;
const real SUN_POS_Y = 400;

const real PLAYER_START_X = WORLD_MAX_WIDTH / 2;
const real PLAYER_START_Y = WORLD_MAX_HEIGHT / 2;

const real PLAYER_BASE_ROTATE_ACCELERATION = VK2D_PI * 0.003;
const real PLAYER_BASE_ROTATE_FRICTION     = VK2D_PI * 0.001;
const real PLAYER_BASE_ROTATE_TOP_SPEED    = VK2D_PI * 0.02;
const real PLAYER_BASE_TRASH_GRAB_DISTANCE = 200;
const real PLAYER_BASE_TRASH_THROW_SPEED   = 20; // MUST NOT BE IN THE RANGE OF [TRASH_MIN_VELOCITY, TRASH_MAX_VELOCITY]
const real PLAYER_TRASH_DRAW_DISTANCE      = 200;

const real PLAYER_BASE_ACCELERATION = 0.10;
const real PLAYER_FRICTION          = 0.02;

const real PHYSICS_BASE_TOP_SPEED = 15;

const real TRASH_MIN_VELOCITY              = 2;
const real TRASH_MAX_VELOCITY              = 10;
const real TRASH_MIN_ROT_SPEED             = VK2D_PI * 0.01;
const real TRASH_MAX_ROT_SPEED             = VK2D_PI * 0.03;
const real TRASH_PLAYER_DIRECTION_ACCURACY = VK2D_PI * 0.1;
const int  TRASH_LIFETIME                  = FPS_LIMIT * 15;
const int  TRASH_FADE_OUT_TIME             = FPS_LIMIT * 3;
const real TRASH_SPAWN_DISTANCE            = 1000;

const real DRONE_BASE_ACCELERATION        = 0.15;
const int  DRONE_DYING_TIMER              = FPS_LIMIT * 3;
const real DRONE_DYING_ROTATE_SPEED       = VK2D_PI * 0.05;
const real DRONE_TRASH_COLLISION_DISTANCE = 100;
const real DRONE_SPAWN_DISTANCE           = 2000;
const real DRONE_DYING_SPEED              = 3;

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
	real dirVelocity;
	real direction;
	int grabbedTrash; // Index of the grabbed trash
} Player;

typedef struct {
	VK2DTexture tex;
	int framesLeftAlive;
	real rot;
	real rotSpeed;
	bool grabbed;
} Trash;

typedef struct {
	bool dying;
	int dyingTimer;
	real dyingRotation;
} Drone;

// Any kind of entity in the world
typedef struct {
	entitytype type; // entities can delete themselves by setting this to ENTITY_TYPE_NONE
	Physics physics;
	union {
		Player player;
		Trash trash;
		Drone drone;
	};
} Entity;

// All entities in the game
typedef struct {
	Entity *entities; // Vector of entities
	int size;         // Number of entities in the game
} Population;

/********************* Globals *********************/
Assets *gAssets = NULL;
VK2DCameraIndex gCam = -1;
Entity gPlayer = {ENTITY_TYPE_PLAYER};
real gZoom = 1;
JUFont gFont = NULL;
Population gPopulation = {};

/********************* Common functions *********************/
// Returns a real from 0-1
real random() {
	return (real)(rand() % 1000) / 1000.0;
}

// Returns an int from [low, high)
int randomRange(int low, int high) {
	return low + (int)floor(random() * (high - low));
}

// Returns a real number from low to high
real randomRangeReal(real low, real high) {
	return low + (random() * (high - low));
}

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
	if (acceleration != NULL) {
		// Add acceleration vector to the velocity vector then cap velocity
		real rise = (sin(acceleration->direction) * acceleration->magnitude) + (sin(physics->velocity.direction) * physics->velocity.magnitude);
		real run = (cos(acceleration->direction) * acceleration->magnitude) + (cos(physics->velocity.direction) * physics->velocity.magnitude);
		physics->velocity.direction = run == 0 ? 0 : atan2(rise, run);
		physics->velocity.magnitude = sqrt(pow(rise, 2) + pow(run, 2));
		physics->velocity.magnitude = juClamp(physics->velocity.magnitude, -PHYSICS_BASE_TOP_SPEED, PHYSICS_BASE_TOP_SPEED);
	}

	// Apply velocity to coordinates then cap coordinates
	physics->x += juCastX(physics->velocity.magnitude, -physics->velocity.direction);
	physics->y += juCastY(physics->velocity.magnitude, -physics->velocity.direction);
	physics->x = juClamp(physics->x, 0, WORLD_MAX_WIDTH);
	physics->y = juClamp(physics->y, 0, WORLD_MAX_HEIGHT);
}

/********************* Trash functions *********************/
void trashStart(Entity *entity) {
	VK2DTexture tex[] = {gAssets->texTrash1, gAssets->texTrash2};
	entity->type = ENTITY_TYPE_TRASH;
	entity->trash.tex = tex[randomRange(0, 2)];
	entity->trash.rotSpeed = randomRangeReal(TRASH_MIN_ROT_SPEED, TRASH_MAX_ROT_SPEED);
	entity->trash.rot = 0;
	entity->trash.framesLeftAlive = TRASH_LIFETIME;
	entity->trash.grabbed = false;

	// Physics
	VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
	if (randomRange(0, 2)) { // Left/right of the screen
		entity->physics.x = randomRange(0, 2) ? spec.x - TRASH_SPAWN_DISTANCE : spec.x + spec.w + TRASH_SPAWN_DISTANCE;
		entity->physics.y = randomRangeReal(spec.y, spec.y + spec.h);
	} else { // Top/bottom of the screen
		entity->physics.x = randomRangeReal(spec.x, spec.x + spec.w);
		entity->physics.y = randomRange(0, 2) ? spec.y - TRASH_SPAWN_DISTANCE : spec.y + spec.h + TRASH_SPAWN_DISTANCE;
	}
	real angle = juPointAngle(gPlayer.physics.x, gPlayer.physics.y, entity->physics.x, entity->physics.y);// - (VK2D_PI / 2);
	entity->physics.velocity.direction = randomRangeReal(angle - TRASH_PLAYER_DIRECTION_ACCURACY, angle + TRASH_PLAYER_DIRECTION_ACCURACY);
	entity->physics.velocity.magnitude = randomRangeReal(TRASH_MIN_VELOCITY, TRASH_MAX_VELOCITY);
}

void trashEnd(Entity *entity) {
	entity->type = ENTITY_TYPE_NONE; // carted
}

void droneEnd(Entity *entity);
void trashUpdate(Entity *entity) {
	// Updating
	if (!entity->trash.grabbed) {
		physicsUpdate(&entity->physics, NULL);
		entity->trash.rot += entity->trash.rotSpeed;
		entity->trash.framesLeftAlive -= 1;
		if (entity->trash.framesLeftAlive <= 0)
			trashEnd(entity);
	} else {
		entity->trash.framesLeftAlive = TRASH_LIFETIME;
	}

	// Check if we were thrown and as such check if we hit any enemies
	if (entity->physics.velocity.magnitude == PLAYER_BASE_TRASH_THROW_SPEED) {
		for (int i = 0; i < gPopulation.size && entity->physics.velocity.magnitude == PLAYER_BASE_TRASH_THROW_SPEED; i++) {
			Entity *drone = &gPopulation.entities[i];
			if (drone->type == ENTITY_TYPE_DRONE && !drone->drone.dying && juPointDistance(entity->physics.x, entity->physics.y, drone->physics.x, drone->physics.y) < DRONE_TRASH_COLLISION_DISTANCE) {
				droneEnd(drone);
				drone->physics.velocity = entity->physics.velocity;
				entity->physics.velocity.magnitude = DRONE_DYING_SPEED;
			}
		}
	}

	// Drawing
	vec4 alpha = {1, 1, 1, 1};
	if (entity->trash.framesLeftAlive <= TRASH_FADE_OUT_TIME)
		alpha[3] = (float)entity->trash.framesLeftAlive / (float)TRASH_FADE_OUT_TIME;
	float originX = entity->trash.tex->img->width / 2;
	float originY = entity->trash.tex->img->height / 2;
	vk2dRendererSetColourMod(alpha);
	vk2dDrawTextureExt(entity->trash.tex, entity->physics.x - originX, entity->physics.y - originY, alpha[3], alpha[3], entity->trash.rot, originX, originY);
	vk2dRendererSetColourMod(VK2D_DEFAULT_COLOUR_MOD);
}

/********************* Drone functions *********************/
void droneStart(Entity *entity) {
	// Zero entity
	memset(entity, 0, sizeof(Entity));
	entity->type = ENTITY_TYPE_DRONE;

	// Spawn off screen
	VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
	if (randomRange(0, 2)) { // Left/right of the screen
		entity->physics.x = randomRange(0, 2) ? spec.x - DRONE_SPAWN_DISTANCE : spec.x + spec.w + DRONE_SPAWN_DISTANCE;
		entity->physics.y = randomRangeReal(spec.y, spec.y + spec.h);
	} else { // Top/bottom of the screen
		entity->physics.x = randomRangeReal(spec.x, spec.x + spec.w);
		entity->physics.y = randomRange(0, 2) ? spec.y - DRONE_SPAWN_DISTANCE : spec.y + spec.h + DRONE_SPAWN_DISTANCE;
	}
}

void droneEnd(Entity *entity) {
	entity->drone.dying = true;
	entity->drone.dyingTimer = DRONE_DYING_TIMER;
}

void droneUpdate(Entity *entity) {
	float originX = gAssets->texDrone->img->width / 2;
	float originY = gAssets->texDrone->img->height / 2;
	if (!entity->drone.dying) {
		Vector acceleration = {DRONE_BASE_ACCELERATION, (VK2D_PI / 2) - juPointAngle(entity->physics.x, entity->physics.y, gPlayer.physics.x, gPlayer.physics.y) - (VK2D_PI / 2)};
		physicsUpdate(&entity->physics, &acceleration);
		vk2dDrawTextureExt(gAssets->texDrone, entity->physics.x - originX, entity->physics.y - originY, 1, 1, entity->physics.velocity.direction, originX, originY);
	} else {
		physicsUpdate(&entity->physics, NULL);
		entity->drone.dyingTimer -= 1;
		entity->drone.dyingRotation += DRONE_DYING_ROTATE_SPEED;
		float scale = (float)entity->drone.dyingTimer / (float)DRONE_DYING_TIMER;
		vk2dDrawTextureExt(gAssets->texDrone, entity->physics.x - originX, entity->physics.y - originY, scale, scale, entity->drone.dyingRotation, originX, originY);
	}
}

/********************* Mine functions *********************/
void mineStart(Entity *entity) {

}

void mineUpdate(Entity *entity) {

}

void mineEnd(Entity *entity) {

}

/********************* Garbage disposal functions *********************/
void garbageDisposalStart(Entity *entity) {

}

void garbageDisposalUpdate(Entity *entity) {

}

void garbageDisposalEnd(Entity *entity) {

}

/********************* Population functions *********************/
void popInit() {
	gPopulation.entities = NULL;
	gPopulation.size = 0;
}

// Returns a pointer to an entity you can fill out that will be in the population
Entity* popGetNewEntity() {
	int found = -1;
	for (int i = 0; i < gPopulation.size; i++)
		if (gPopulation.entities[i].type == ENTITY_TYPE_NONE)
			found = i;

	if (found == -1) {
		gPopulation.entities = realloc(gPopulation.entities, (gPopulation.size + 5) * sizeof(Entity));
		found = gPopulation.size;
		gPopulation.size += 5;
	}

	return &gPopulation.entities[found];
}

void popUpdateEntities() {
	for (int i = 0; i < gPopulation.size; i++) {
		if (gPopulation.entities[i].type == ENTITY_TYPE_TRASH) {
			trashUpdate(&gPopulation.entities[i]);
		} else if (gPopulation.entities[i].type == ENTITY_TYPE_DRONE) {
			droneUpdate(&gPopulation.entities[i]);
		} else if (gPopulation.entities[i].type == ENTITY_TYPE_MINE) {
			mineUpdate(&gPopulation.entities[i]);
		} else if (gPopulation.entities[i].type == ENTITY_TYPE_GARBAGE_DISPOSAL) {
			garbageDisposalUpdate(&gPopulation.entities[i]);
		}
	}
}

void popEnd() {
	free(gPopulation.entities);
}

/********************* Player functions *********************/
void playerStart() {
	physicsStart(&gPlayer.physics, PLAYER_START_X, PLAYER_START_Y);
	gPlayer.player.grabbedTrash = NO_TRASH;
}

void playerUpdate() {
	// Rotate the ship
	if (juKeyboardGetKey(SDL_SCANCODE_A) || juKeyboardGetKey(SDL_SCANCODE_D)) {
		gPlayer.player.dirVelocity += (-((real) juKeyboardGetKey(SDL_SCANCODE_A)) + ((real) juKeyboardGetKey(SDL_SCANCODE_D))) * PLAYER_BASE_ROTATE_ACCELERATION;
	} else {
		if (juSign(gPlayer.player.dirVelocity - juSign(gPlayer.player.dirVelocity) * PLAYER_BASE_ROTATE_FRICTION) != juSign(gPlayer.player.dirVelocity))
			gPlayer.player.dirVelocity = 0;
		else
			gPlayer.player.dirVelocity -= juSign(gPlayer.player.dirVelocity) * PLAYER_BASE_ROTATE_FRICTION;
   	}
	gPlayer.player.dirVelocity = juClamp(gPlayer.player.dirVelocity, -PLAYER_BASE_ROTATE_TOP_SPEED, PLAYER_BASE_ROTATE_TOP_SPEED);
	gPlayer.player.direction += gPlayer.player.dirVelocity;

	// Calculate acceleration vector
	Vector acceleration = {};
	if (juKeyboardGetKey(SDL_SCANCODE_SPACE)) {
		acceleration.magnitude = PLAYER_BASE_ACCELERATION;
		acceleration.direction = gPlayer.player.direction;
	} else {
		acceleration.magnitude = PLAYER_FRICTION;
		acceleration.direction = gPlayer.physics.velocity.direction + VK2D_PI;
	}

	// Check if the player grabs some trash
	if (juKeyboardGetKeyPressed(SDL_SCANCODE_LSHIFT)) {
		for (int i = 0; i < gPopulation.size && gPlayer.player.grabbedTrash == NO_TRASH; i++) {
			if (gPopulation.entities[i].type == ENTITY_TYPE_TRASH && juPointDistance(gPlayer.physics.x, gPlayer.physics.y, gPopulation.entities[i].physics.x, gPopulation.entities[i].physics.y) < PLAYER_BASE_TRASH_GRAB_DISTANCE) {
				gPlayer.player.grabbedTrash = i;
				gPopulation.entities[i].trash.grabbed = true;
			}
		}
	} else if (juKeyboardGetKeyReleased(SDL_SCANCODE_LSHIFT) && gPlayer.player.grabbedTrash != NO_TRASH) {
		Entity *trash = &gPopulation.entities[gPlayer.player.grabbedTrash];
		trash->physics.velocity.direction = gPlayer.player.direction;
		trash->physics.velocity.magnitude = PLAYER_BASE_TRASH_THROW_SPEED;
		trash->trash.grabbed = false;
		gPlayer.player.grabbedTrash = NO_TRASH;
	}

	// Do stuff with grabbed trash
	if (gPlayer.player.grabbedTrash != NO_TRASH) {
		Entity *trash = &gPopulation.entities[gPlayer.player.grabbedTrash];
		trash->physics.x = gPlayer.physics.x + juCastX(PLAYER_TRASH_DRAW_DISTANCE, -gPlayer.player.direction);
		trash->physics.y = gPlayer.physics.y + juCastY(PLAYER_TRASH_DRAW_DISTANCE, -gPlayer.player.direction);
	}

	physicsUpdate(&gPlayer.physics, &acceleration);
}

void playerDraw() {
	VK2DTexture player;
	if (juKeyboardGetKey(SDL_SCANCODE_SPACE))
		player = gAssets->texPlayerThruster;
	else
		player = gAssets->texPlayer;
	vk2dDrawTextureExt(player, gPlayer.physics.x - (player->img->width / 2), gPlayer.physics.y - (player->img->height / 2), 1, 1, gPlayer.player.direction + (VK2D_PI / 2), player->img->width / 2, player->img->height / 2);
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
	float topLeftY = spec.h - 90;
	float w = 120;
	float h = 80;
	float centerX = topLeftX + (w / 2);
	float centerY = topLeftY + (h / 2);
	float rise = (sin(gPlayer.physics.velocity.direction) * gPlayer.physics.velocity.magnitude) * 2.5;
	float run = (cos(gPlayer.physics.velocity.direction) * gPlayer.physics.velocity.magnitude) * 3.5;
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
	srand(time(NULL));
	popInit();
	playerStart();
}

gamestate gameUpdate() {
	// TODO: Make trash automatically spawn
	if (juKeyboardGetKey(SDL_SCANCODE_RETURN))
		trashStart(popGetNewEntity());
	if (juKeyboardGetKeyPressed(SDL_SCANCODE_P))
		droneStart(popGetNewEntity());

	// Update camera around player
	VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
	spec.x += ((gPlayer.physics.x - (spec.w / 2)) - spec.x) * CAMERA_SPEED;
	spec.y += ((gPlayer.physics.y - (spec.h / 2)) - spec.y) * CAMERA_SPEED;
	spec.x = juClamp(spec.x, 0, WORLD_MAX_WIDTH - spec.w);
	spec.y = juClamp(spec.y, 0, WORLD_MAX_HEIGHT - spec.h);
	vk2dCameraUpdate(gCam, spec);

	// Lock camera to world camera and draw world
	vk2dRendererLockCameras(gCam);
	vk2dDrawTexture(gAssets->texSun, spec.x + SUN_POS_X - spec.x * 0.05, spec.y + SUN_POS_Y - spec.y * 0.05);
	drawTiledBackground(gAssets->texBackground, 0.8);
	drawTiledBackground(gAssets->texMidground, 0.6);
	drawTiledBackground(gAssets->texForeground, 0.5);

	// Update entities
	playerUpdate();
	popUpdateEntities();
	playerDraw();

	// UI is drawn to the default camera
	vk2dRendererLockCameras(VK2D_DEFAULT_CAMERA);
	gameDrawUI();

	vk2dRendererUnlockCameras();

	return GAMESTATE_GAME;
}

void gameEnd() {
	playerEnd();
	popEnd();
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
	VK2DCameraSpec spec = {ct_Default, PLAYER_START_X - (GAME_WIDTH / 2), PLAYER_START_Y - (GAME_HEIGHT / 2), WINDOW_WIDTH, WINDOW_HEIGHT, 1, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
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
