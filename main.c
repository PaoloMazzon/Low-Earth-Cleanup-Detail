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
const int   WINDOW_WIDTH     = 1024;
const int   WINDOW_HEIGHT    = 768;
const int   GAME_WIDTH       = 1500;
const int   GAME_HEIGHT      = 1125;
const real  FPS_LIMIT        = 60;
const real  ZOOM_MIN         = 0.5;
const real  ZOOM_MAX         = 2;
const real  ZOOM_SPEED       = 0.25;
const int   FONT_WIDTH       = 40;
const int   FONT_HEIGHT      = 70;
const float CAMERA_SPEED     = 0.2;
const int   NO_TRASH         = -1;
const bool  DEBUG            = false;
const char  HIGHSCORE_FILE[] = "score.bin";
const int   GAME_OVER_DELAY  = FPS_LIMIT * 3;

const real WORLD_MAX_WIDTH  = 60000;
const real WORLD_MAX_HEIGHT = 60000;

const real SUN_POS_X = -600;
const real SUN_POS_Y = -600;

const real PLAYER_START_X = WORLD_MAX_WIDTH / 2;
const real PLAYER_START_Y = WORLD_MAX_HEIGHT / 2;

const real PLAYER_BASE_ROTATE_ACCELERATION  = VK2D_PI * 0.003;
const real PLAYER_BASE_ROTATE_FRICTION      = VK2D_PI * 0.001;
const real PLAYER_BASE_ROTATE_TOP_SPEED     = VK2D_PI * 0.02;
const real PLAYER_BASE_TRASH_GRAB_DISTANCE  = 200;
const real PLAYER_BASE_TRASH_THROW_SPEED    = 20; // MUST NOT BE IN THE RANGE OF [TRASH_MIN_VELOCITY, TRASH_MAX_VELOCITY]
const real PLAYER_TRASH_DRAW_DISTANCE       = 200;
const int  PLAYER_DAMAGED_IFRAMES           = FPS_LIMIT * 3;
const int  PLAYER_DAMAGED_BLINKING_INTERVAL = 10;
const real PLAYER_BASE_ACCELERATION         = 0.15;
const real PLAYER_FRICTION                  = 0.02;
const real PLAYER_BASE_HP                   = 5;
const real PLAYER_DYING_ROTATE_SPEED        = VK2D_PI * 0.05;

const real PHYSICS_BASE_TOP_SPEED = 15;

const real TRASH_MIN_VELOCITY              = 2;
const real TRASH_MAX_VELOCITY              = 10;
const real TRASH_MIN_ROT_SPEED             = VK2D_PI * 0.01;
const real TRASH_MAX_ROT_SPEED             = VK2D_PI * 0.03;
const real TRASH_PLAYER_DIRECTION_ACCURACY = VK2D_PI * 0.1;
const int  TRASH_LIFETIME                  = FPS_LIMIT * 15;
const int  TRASH_FADE_OUT_TIME             = FPS_LIMIT * 3;
const real TRASH_SPAWN_DISTANCE            = 1000;
#define    TRASH_MAX                         ((int)4000)
const real TRASH_SPAWN_INTERVAL            = 0.3; // trash spawns every TRASH_SPAWN_INTERVAL seconds
const real TRASH_MIN_VALUE                 = 0.15;
const real TRASH_MAX_VALUE                 = 2;

const real DRONE_BASE_ACCELERATION        = 0.15;
const int  DRONE_DYING_TIMER              = FPS_LIMIT * 3;
const real DRONE_DYING_ROTATE_SPEED       = VK2D_PI * 0.05;
const real DRONE_TRASH_COLLISION_DISTANCE = 100;
const real DRONE_SPAWN_DISTANCE           = 2000;
const real DRONE_DYING_SPEED              = 3;
const real DRONE_DAMAGE_RADIUS            = 150;
const real DRONE_SPAWN_INTERVAL           = 10; // trash spawns every TRASH_SPAWN_INTERVAL seconds
const real DRONE_MAX_INTERVAL             = 50; // how many seconds between the max number of enemies increases
const int  DRONE_SPAWN_DELAY              = FPS_LIMIT * 15; // dont start spawning enemies until this far in
const real DRONE_FIGHTER_CHANCE           = 0.3; // chance for a drone to be a fighter drone

const real GARBAGE_DISPOSAL_START_X        = PLAYER_START_X + 1000;
const real GARBAGE_DISPOSAL_START_Y        = PLAYER_START_Y;
const real GARBAGE_DISPOSAL_GRAVITY_RADIUS = 800;
const real GARBAGE_DISPOSAL_GRAB_RADIUS    = 100;
const real GARBAGE_DISPOSAL_GRAVITY        = 0.8;
const real GARBAGE_DISPOSAL_WIDTH          = 100;
const real GARBAGE_DISPOSAL_HEIGHT         = 100;

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
	real hp;
	int iframes; // iframes left after getting damaged
} Player;

typedef struct {
	VK2DTexture tex;
	int framesLeftAlive;
	real rot;
	real rotSpeed;
	bool grabbed;
	bool trashAnimation;
	bool wasThrown;
} Trash;

typedef struct {
	bool dying;
	int dyingTimer;
	real dyingRotation;
	bool fighter;
} Drone;

typedef struct {
	// frig
} GarbageDisposal;

// Any kind of entity in the world
typedef struct {
	entitytype type; // entities can delete themselves by setting this to ENTITY_TYPE_NONE
	Physics physics;
	union {
		Player player;
		Trash trash;
		Drone drone;
		GarbageDisposal garbageDisposal;
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
VK2DCameraIndex g3DCam = -1;
Entity gPlayer = {ENTITY_TYPE_PLAYER};
int gGarbageDisposal = -1;
real gZoom = 1;
JUFont gFont = NULL;
Population gPopulation = {};
real gScore = 0;
VK2DModel gGarbageModel;
real gLastGarbageTime = 0;
real gLastEnemyTime = 0;
int gEnemyCount;
int gEnemyMax = 1;
real gEnemyCountLastTime = 0;
VK2DShader gShader = NULL;
int gSpawnDelay = 0;
real gHighscore = 0;
bool gNewHighscore = false;
int gGameoverDelay = 0;
VK2DTexture gGarbageDisposalTexture;

// Expirimental
VK2DDrawInstance gEntityBuffer1[TRASH_MAX];
VK2DDrawInstance gEntityBuffer2[TRASH_MAX];
VK2DDrawInstance *gEntityBuffers[] = {gEntityBuffer1, gEntityBuffer2};

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
	tileStartX += floorf((camera.x - tileStartX) / vk2dTextureWidth(texture)) * vk2dTextureWidth(texture);
	tileStartY += floorf((camera.y - tileStartY) / vk2dTextureHeight(texture)) * vk2dTextureHeight(texture);
	//while (tileStartX + texture->img->width < camera.x) tileStartX += texture->img->width;
	//while (tileStartY + texture->img->height < camera.y) tileStartY += texture->img->height;
	float tilesNeededHorizontal = ceilf(camera.w / vk2dTextureWidth(texture)) + 1;
	float tilesNeededVertical = ceilf(camera.h / vk2dTextureHeight(texture)) + 1;

	// Loop through and draw them all
	for (int y = 0; y < tilesNeededVertical; y++)
		for (int x = 0; x < tilesNeededHorizontal; x++)
			vk2dDrawTexture(texture, tileStartX + (x * vk2dTextureWidth(texture)), tileStartY + (y * vk2dTextureHeight(texture)));
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

Vector addVectors(Vector *v1, Vector *v2) {
	real rise = (sin(v2->direction) * v2->magnitude) + (sin(v1->direction) * v1->magnitude);
	real run = (cos(v2->direction) * v2->magnitude) + (cos(v1->direction) * v1->magnitude);
	v1->direction = run == 0 ? 0 : atan2(rise, run);
	v1->magnitude = sqrt(pow(rise, 2) + pow(run, 2));
	v1->magnitude = juClamp(v1->magnitude, -PHYSICS_BASE_TOP_SPEED, PHYSICS_BASE_TOP_SPEED);
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
	entity->trash.trashAnimation = false;
	entity->trash.wasThrown = false;

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
	Entity *garbage = &gPopulation.entities[gGarbageDisposal];
	real dist = juPointDistance(entity->physics.x, entity->physics.y, garbage->physics.x, garbage->physics.y);

	// Updating
	if (!entity->trash.grabbed) {
		if (dist > GARBAGE_DISPOSAL_GRAB_RADIUS && dist < GARBAGE_DISPOSAL_GRAVITY_RADIUS && entity->trash.wasThrown) {
			real angle = (VK2D_PI / 2) - juPointAngle(entity->physics.x, entity->physics.y, garbage->physics.x, garbage->physics.y) - (VK2D_PI / 2);
			real speed = GARBAGE_DISPOSAL_GRAVITY;
			Vector gravity;
			gravity.direction = angle;
			gravity.magnitude = speed;
			physicsUpdate(&entity->physics, &gravity);
		} else if (dist < GARBAGE_DISPOSAL_GRAB_RADIUS && entity->trash.wasThrown && !entity->trash.trashAnimation) {
			// Start the garbage spin animation
			entity->trash.trashAnimation = true;
			gScore += randomRangeReal(TRASH_MIN_VALUE, TRASH_MAX_VALUE);
			entity->trash.framesLeftAlive = TRASH_FADE_OUT_TIME;
		} else {
			physicsUpdate(&entity->physics, NULL);
		}
		entity->trash.rot += entity->trash.rotSpeed;
		entity->trash.framesLeftAlive -= 1;
		if (entity->trash.framesLeftAlive <= 0)
			trashEnd(entity);
	} else {
		entity->trash.framesLeftAlive = TRASH_LIFETIME;
	}

	// If the trash is in the dying animation just spin out in the garbage disposal
	if (entity->trash.trashAnimation) {
		entity->physics.x = garbage->physics.x;
		entity->physics.y = garbage->physics.y;
	}

	// Check if we were thrown and as such check if we hit any enemies
	if (entity->trash.wasThrown) {
		for (int i = 0; i < gPopulation.size && entity->physics.velocity.magnitude == PLAYER_BASE_TRASH_THROW_SPEED; i++) {
			Entity *drone = &gPopulation.entities[i];
			if (drone->type == ENTITY_TYPE_DRONE && !drone->drone.dying && juPointDistance(entity->physics.x, entity->physics.y, drone->physics.x, drone->physics.y) < DRONE_TRASH_COLLISION_DISTANCE) {
				droneEnd(drone);
				drone->physics.velocity = entity->physics.velocity;
				drone->physics.velocity.magnitude = DRONE_DYING_SPEED;
				entity->physics.velocity.magnitude /= 2;
			}
		}
	}

	// Drawing
	vec4 alpha = {1, 1, 1, 1};
	if (entity->trash.framesLeftAlive <= TRASH_FADE_OUT_TIME)
		alpha[3] = (float)entity->trash.framesLeftAlive / (float)TRASH_FADE_OUT_TIME;
	float drawOriginX = (vk2dTextureWidth(entity->trash.tex) / 2) - ((1 - alpha[3]) * (vk2dTextureWidth(entity->trash.tex) / 2));
	float drawOriginY = (vk2dTextureHeight(entity->trash.tex) / 2) - ((1 - alpha[3]) * (vk2dTextureHeight(entity->trash.tex) / 2));
	float originX = (vk2dTextureWidth(entity->trash.tex) / 2);
	float originY = (vk2dTextureHeight(entity->trash.tex) / 2);
	vk2dRendererSetColourMod(alpha);
	vk2dDrawTextureExt(entity->trash.tex, entity->physics.x - drawOriginX, entity->physics.y - drawOriginY, alpha[3], alpha[3], entity->trash.rot, originX, originY);
	vk2dRendererSetColourMod(VK2D_DEFAULT_COLOUR_MOD);

	if (DEBUG) {
		vk2dDrawCircle(entity->physics.x, entity->physics.y, 4);
	}
}

/********************* Drone functions *********************/
void playerTakeDamage(Entity *entity);
void droneStart(Entity *entity) {
	// Zero entity
	memset(entity, 0, sizeof(Entity));
	entity->type = ENTITY_TYPE_DRONE;
	gEnemyCount++;
	entity->drone.fighter = randomRangeReal(0, 1) < DRONE_FIGHTER_CHANCE;

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
	gEnemyCount--;
}

void droneUpdate(Entity *entity) {
	float originX = vk2dTextureWidth(gAssets->texDrone) / 2;
	float originY = vk2dTextureHeight(gAssets->texDrone) / 2;
	if (!entity->drone.dying) {
		// Accelerate towards the player
		Vector acceleration = {DRONE_BASE_ACCELERATION, (VK2D_PI / 2) - juPointAngle(entity->physics.x, entity->physics.y, gPlayer.physics.x, gPlayer.physics.y) - (VK2D_PI / 2)};
		if (!entity->drone.fighter) {
			physicsUpdate(&entity->physics, &acceleration);
		} else {
			float dir = juPointAngle(entity->physics.x, entity->physics.y, gPlayer.physics.x, gPlayer.physics.y);
			entity->physics.x += juCastX(PHYSICS_BASE_TOP_SPEED * 0.75, dir);
			entity->physics.y += juCastY(PHYSICS_BASE_TOP_SPEED * 0.75, dir);
			entity->physics.velocity.direction = acceleration.direction;
		}

		// Check if we're damaging the player
		if (juPointDistance(entity->physics.x, entity->physics.y, gPlayer.physics.x, gPlayer.physics.y) <= DRONE_DAMAGE_RADIUS) {
			playerTakeDamage(entity);
			entity->physics.velocity.direction += VK2D_PI;
			entity->physics.velocity.magnitude *= 0.5;
		}

		// Draw
		if (entity->drone.fighter) {
			vec4 c;
			vk2dColourHex(c, "#20326e");
			vk2dRendererSetColourMod(c);
		}
		vk2dDrawTextureExt(gAssets->texDrone, entity->physics.x - originX, entity->physics.y - originY, 1, 1, entity->physics.velocity.direction, originX, originY);
		vk2dRendererSetColourMod(VK2D_DEFAULT_COLOUR_MOD);

		if (DEBUG) {
			vk2dDrawCircleOutline(entity->physics.x, entity->physics.y, DRONE_DAMAGE_RADIUS, 1);
			vk2dDrawCircle(entity->physics.x, entity->physics.y, 4);
		}
	} else {
		// Dying animation
		physicsUpdate(&entity->physics, NULL);
		entity->drone.dyingTimer -= 1;
		entity->drone.dyingRotation += DRONE_DYING_ROTATE_SPEED;
		float scale = (float)entity->drone.dyingTimer / (float)DRONE_DYING_TIMER;
		vk2dDrawTextureExt(gAssets->texDrone, entity->physics.x - originX, entity->physics.y - originY, scale, scale, entity->drone.dyingRotation, originX, originY);

		// Delete drone when animation is done
		if (entity->drone.dyingTimer <= 0)
			entity->type = ENTITY_TYPE_NONE;
	}
}

/********************* Mine functions *********************/
void mineStart(Entity *entity) {
	memset(entity, 0, sizeof(Entity));
	entity->type = ENTITY_TYPE_MINE;
	physicsStart(&entity->physics, 0, 0);
}

void mineUpdate(Entity *entity) {

}

void mineEnd(Entity *entity) {

}

/********************* Garbage disposal functions *********************/
void garbageDisposalStart(Entity *entity) {
	memset(entity, 0, sizeof(Entity));
	entity->type = ENTITY_TYPE_GARBAGE_DISPOSAL;
	physicsStart(&entity->physics, GARBAGE_DISPOSAL_START_X, GARBAGE_DISPOSAL_START_Y);
}

void garbageDisposalUpdate(Entity *entity) {
	float scale = 6;
	vk2dDrawTextureExt(gGarbageDisposalTexture, entity->physics.x - ((vk2dTextureWidth(gGarbageDisposalTexture) * scale) / 2), entity->physics.y - ((vk2dTextureHeight(gGarbageDisposalTexture) * scale) / 2), scale, scale, 0, 0, 0);

	if (DEBUG) {
		vk2dDrawCircle(entity->physics.x, entity->physics.y, 4);
		vk2dDrawCircleOutline(entity->physics.x, entity->physics.y, GARBAGE_DISPOSAL_GRAVITY_RADIUS, 1);
		vk2dDrawCircleOutline(entity->physics.x, entity->physics.y, GARBAGE_DISPOSAL_GRAB_RADIUS, 1);
	}
}

void garbageDisposalEnd(Entity *entity) {
	vk2dRendererWait();
}

/********************* Population functions *********************/
void popInit() {
	gPopulation.entities = NULL;
	gPopulation.size = 0;
}

// Returns a pointer to an entity you can fill out that will be in the population
Entity* popGetNewEntity(int *location) {
	int found = -1;
	for (int i = 0; i < gPopulation.size; i++)
		if (gPopulation.entities[i].type == ENTITY_TYPE_NONE)
			found = i;

	if (found == -1) {
		gPopulation.entities = realloc(gPopulation.entities, (gPopulation.size + 5) * sizeof(Entity));
		for (int i = gPopulation.size; i < gPopulation.size + 5; i++)
			gPopulation.entities[i].type = ENTITY_TYPE_NONE;
		found = gPopulation.size;
		gPopulation.size += 5;
	}

	if (location != NULL)
		*location = found;
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

Entity* popGet(int location) {
	return &gPopulation.entities[location];
}

/********************* Player functions *********************/
void playerStart() {
	memset(&gPlayer, 0, sizeof(Entity));
	gPlayer.type = ENTITY_TYPE_PLAYER;
	physicsStart(&gPlayer.physics, PLAYER_START_X, PLAYER_START_Y);
	gPlayer.player.grabbedTrash = NO_TRASH;
	gPlayer.player.hp = PLAYER_BASE_HP;
}

void playerUpdate() {
	if (gPlayer.player.hp > 0) {
		// Rotate the ship
		if (juKeyboardGetKey(SDL_SCANCODE_A) || juKeyboardGetKey(SDL_SCANCODE_D)) {
			gPlayer.player.dirVelocity +=
					(-((real) juKeyboardGetKey(SDL_SCANCODE_A)) + ((real) juKeyboardGetKey(SDL_SCANCODE_D))) *
					PLAYER_BASE_ROTATE_ACCELERATION;
		} else {
			if (juSign(gPlayer.player.dirVelocity - juSign(gPlayer.player.dirVelocity) * PLAYER_BASE_ROTATE_FRICTION) !=
				juSign(gPlayer.player.dirVelocity))
				gPlayer.player.dirVelocity = 0;
			else
				gPlayer.player.dirVelocity -= juSign(gPlayer.player.dirVelocity) * PLAYER_BASE_ROTATE_FRICTION;
		}
		gPlayer.player.dirVelocity = juClamp(gPlayer.player.dirVelocity, -PLAYER_BASE_ROTATE_TOP_SPEED,
											 PLAYER_BASE_ROTATE_TOP_SPEED);
		gPlayer.player.direction += gPlayer.player.dirVelocity;

		// Calculate acceleration vector
		Vector acceleration = {};
		if (juKeyboardGetKey(SDL_SCANCODE_W)) {
			acceleration.magnitude = PLAYER_BASE_ACCELERATION;
			acceleration.direction = gPlayer.player.direction;
		} else {
			acceleration.magnitude = PLAYER_FRICTION;
			acceleration.direction = gPlayer.physics.velocity.direction + VK2D_PI;
		}

		// Check if the player grabs some trash
		if (juKeyboardGetKeyPressed(SDL_SCANCODE_SPACE)) {
			for (int i = 0; i < gPopulation.size && gPlayer.player.grabbedTrash == NO_TRASH; i++) {
				if (gPopulation.entities[i].type == ENTITY_TYPE_TRASH &&
					juPointDistance(gPlayer.physics.x, gPlayer.physics.y, gPopulation.entities[i].physics.x,
									gPopulation.entities[i].physics.y) < PLAYER_BASE_TRASH_GRAB_DISTANCE) {
					gPlayer.player.grabbedTrash = i;
					gPopulation.entities[i].trash.grabbed = true;
				}
			}
		} else if (juKeyboardGetKeyReleased(SDL_SCANCODE_SPACE) && gPlayer.player.grabbedTrash != NO_TRASH) {
			Entity *trash = &gPopulation.entities[gPlayer.player.grabbedTrash];
			trash->physics.velocity.direction = gPlayer.player.direction;
			trash->physics.velocity.magnitude = PLAYER_BASE_TRASH_THROW_SPEED;
			trash->trash.wasThrown = true;
			trash->trash.grabbed = false;
			gPlayer.player.grabbedTrash = NO_TRASH;
		}

		// Do stuff with grabbed trash
		if (gPlayer.player.grabbedTrash != NO_TRASH) {
			Entity *trash = &gPopulation.entities[gPlayer.player.grabbedTrash];
			trash->physics.x = gPlayer.physics.x + juCastX(PLAYER_TRASH_DRAW_DISTANCE, -gPlayer.player.direction);
			trash->physics.y = gPlayer.physics.y + juCastY(PLAYER_TRASH_DRAW_DISTANCE, -gPlayer.player.direction);
		}

		// IFrames
		if (gPlayer.player.iframes > 0)
			gPlayer.player.iframes -= 1;

		physicsUpdate(&gPlayer.physics, &acceleration);
	} else {
		// Dying animation
		gPlayer.player.direction += PLAYER_DYING_ROTATE_SPEED;
		physicsUpdate(&gPlayer.physics, NULL);
	}
}

void playerDraw() {
	VK2DTexture player;
	if (juKeyboardGetKey(SDL_SCANCODE_W))
		player = gAssets->texPlayerThruster;
	else
		player = gAssets->texPlayer;

	// Account for iframe blinking
	if (gPlayer.player.iframes <= 0 || (gPlayer.player.iframes / PLAYER_DAMAGED_BLINKING_INTERVAL) % 2 == 0) {
		vk2dDrawTextureExt(player, gPlayer.physics.x - (vk2dTextureWidth(player) / 2),
						   gPlayer.physics.y - (vk2dTextureHeight(player) / 2), 1, 1,
						   gPlayer.player.direction + (VK2D_PI / 2), vk2dTextureWidth(player) / 2,
						   vk2dTextureHeight(player) / 2);
	}

	if (DEBUG) {
		vk2dDrawCircleOutline(gPlayer.physics.x, gPlayer.physics.y, PLAYER_BASE_TRASH_GRAB_DISTANCE, 1);
		vk2dDrawCircle(gPlayer.physics.x, gPlayer.physics.y, 4);
	}
}

void playerEnd() {

}

bool recordHighscore();
void playerTakeDamage(Entity *entity) {
	if (gPlayer.player.hp > 0 && gPlayer.player.iframes <= 0) {
		gPlayer.player.hp -= 1;
		gPlayer.player.iframes = PLAYER_DAMAGED_IFRAMES;
		gPlayer.physics.velocity = entity->physics.velocity;

		// Player just died
		if (gPlayer.player.hp <= 0) {
			gNewHighscore = recordHighscore();
			gGameoverDelay = 0;
		}
	}
}

/********************* Game functions *********************/

// Returns true if a new highscore was recorded
bool recordHighscore() {
	if (gScore > gHighscore) {
		FILE *f = fopen(HIGHSCORE_FILE, "wb");
		gHighscore = gScore;
		fwrite(&gHighscore, sizeof(real), 1, f);
		fclose(f);
		return true;
	} else {
		return false;
	}
}

void readHighscore() {
	FILE *f = fopen(HIGHSCORE_FILE, "rb");
	if (f != NULL) {
		fread(&gHighscore, sizeof(real), 1, f);
		fclose(f);
	} else {
		gHighscore = 0;
	}
}

void gameDrawUI() {
	// Get screen w/h
	VK2DCameraSpec spec = vk2dCameraGetSpec(VK2D_DEFAULT_CAMERA);
	VK2DCameraSpec gameWorldCameraSpec = vk2dCameraGetSpec(gCam);
	Entity *gd = popGet(gGarbageDisposal);

	// Point to garbage disposal
	if (juPointDistance(gPlayer.physics.x, gPlayer.physics.y, gd->physics.x, gd->physics.y) > gameWorldCameraSpec.h / 2) {
		float angle = juPointAngle(gPlayer.physics.x, gPlayer.physics.y, gd->physics.x, gd->physics.y);
		float originX = vk2dTextureWidth(gAssets->texArrow) / 2;
		float originY = vk2dTextureHeight(gAssets->texArrow) / 2;
		float x = (spec.x + (spec.w / 2)) + juCastX((spec.w / 2) - originX, angle);
		float y = (spec.y + (spec.h / 2)) + juCastY((spec.h / 2) - originY, angle);
		vk2dDrawTextureExt(gAssets->texArrow, x - originX, y - originY, 1, 1, -angle + (VK2D_PI / 2), originX, originY);
	}

	// Player life
	for (int i = 0; i < gPlayer.player.hp; i++) {
		vk2dDrawTexture(gAssets->texHP, 10 + (i * vk2dTextureWidth(gAssets->texHP)), 10);
	}

	// Score
	char score[100];
	snprintf(score, 100, "$%.2f", gScore);
	juFontDraw(gFont, spec.w - 10 - (strlen(score) * gFont->characters[0].w), 10, score);

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

	// Draw game over the player died
	if (gGameoverDelay >= GAME_OVER_DELAY) {
		vec4 blackOverlay = {0, 0, 0, 0.5};
		vk2dRendererSetColourMod(blackOverlay);
		vk2dRendererClear();
		vk2dRendererSetColourMod(VK2D_DEFAULT_COLOUR_MOD);
		const char *s = "You're fired.";
		juFontDraw(gFont, (spec.w / 2) - ((strlen(s) * gFont->characters[0].w) / 2), (spec.h / 2) - 30, s);

		if (gScore == gHighscore) {
			s = "New highscore!";
			juFontDraw(gFont, (spec.w / 2) - ((strlen(s) * gFont->characters[0].w) / 2), (spec.h / 2) + 30, s);
		}
	} else if (gPlayer.player.hp <= 0) {
		gGameoverDelay += 1;
	}
}

void gameStart() {
	srand(time(NULL));
	popInit();
	playerStart();
	garbageDisposalStart(popGetNewEntity(&gGarbageDisposal));
	gSpawnDelay = 0;
	gLastGarbageTime = juTime();
	gNewHighscore = false;
}

gamestate gameUpdate() {
	real time = juTime();
	if (time - gLastGarbageTime >= TRASH_SPAWN_INTERVAL) {
		gLastGarbageTime = time;
		trashStart(popGetNewEntity(NULL));
	}

	// Enemy spawning
	if (gSpawnDelay >= DRONE_SPAWN_DELAY) {
		if (time - gLastEnemyTime >= DRONE_SPAWN_INTERVAL) {
			gLastEnemyTime = time;
			if (gEnemyCount < gEnemyMax) {
				droneStart(popGetNewEntity(NULL));
			} else if (time - gEnemyCountLastTime >= DRONE_MAX_INTERVAL) {
				gEnemyCountLastTime = time;
				gEnemyMax++;
			}
		}
	} else {
		gSpawnDelay++;
		gLastEnemyTime = juTime();
	}

	// Update camera around player
	VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
	spec.x += ((gPlayer.physics.x - (spec.w / 2)) - spec.x) * CAMERA_SPEED;
	spec.y += ((gPlayer.physics.y - (spec.h / 2)) - spec.y) * CAMERA_SPEED;
	spec.x = juClamp(spec.x, 0, WORLD_MAX_WIDTH - spec.w);
	spec.y = juClamp(spec.y, 0, WORLD_MAX_HEIGHT - spec.h);
	vk2dCameraUpdate(gCam, spec);

	// Lock camera to world camera and draw world
	vk2dRendererLockCameras(gCam);
	float cx = spec.x + (spec.w / 2);
	float cy = spec.y + (spec.h / 2);
	vk2dDrawTexture(gAssets->texSun, cx + SUN_POS_X, cy + SUN_POS_Y);
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

	if (juKeyboardGetKeyPressed(SDL_SCANCODE_SPACE) && gPlayer.player.hp <= 0 && gGameoverDelay >= GAME_OVER_DELAY)
		return GAMESTATE_MENU;
	return GAMESTATE_GAME;
}

void gameEnd() {
	popEnd();
	gGarbageDisposal = 0;
	playerEnd();
}

/********************* Menu functions *********************/
void menuStart() {
	readHighscore();
}

gamestate menuUpdate() {
	// Space background
	VK2DCameraSpec spec = vk2dCameraGetSpec(gCam);
	spec.x += 1;
	spec.y += 0.5;
	vk2dCameraUpdate(gCam, spec);
	vk2dRendererLockCameras(gCam);
	drawTiledBackground(gAssets->texBackground, 0.8);
	drawTiledBackground(gAssets->texMidground, 0.6);
	drawTiledBackground(gAssets->texForeground, 0.5);

	// 2nd layer background
	vk2dRendererLockCameras(VK2D_DEFAULT_CAMERA);
	spec = vk2dCameraGetSpec(VK2D_DEFAULT_CAMERA);
	vec4 blackOverlay = {0, 0, 0, 0.5};
	vk2dRendererSetColourMod(blackOverlay);
	vk2dRendererClear();
	vk2dRendererSetColourMod(VK2D_DEFAULT_COLOUR_MOD);
	float scale = 4;
	vk2dDrawTextureExt(gGarbageDisposalTexture, (spec.w / 2) - ((GARBAGE_DISPOSAL_WIDTH * scale) / 2), (spec.h / 2) - ((GARBAGE_DISPOSAL_HEIGHT * scale) / 2) + (spec.h  * 0.1), scale, scale, 0, 0, 0);
	float bgscale = spec.h / vk2dTextureHeight(gAssets->textitle);
	float drawX = (spec.w - (vk2dTextureWidth(gAssets->textitle) * bgscale)) / 2;
	vk2dDrawTextureExt(gAssets->textitle, drawX, 0, bgscale, bgscale, 0, 0, 0);

	// Text
	const char *s = "Press space to play";
	juFontDraw(gFont, (spec.w / 2) - ((strlen(s) * gFont->characters[0].w) / 2), (spec.h / 2) - 30, s);

	if (gHighscore != 0) {
		char score[100];
		snprintf(score, 99, "Highscore: $%0.2f", gHighscore);
		juFontDraw(gFont, drawX + 2, 2, score);
	}

	if (juKeyboardGetKeyPressed(SDL_SCANCODE_SPACE))
		return GAMESTATE_GAME;
	else if (juKeyboardGetKeyPressed(SDL_SCANCODE_ESCAPE))
		return GAMESTATE_QUIT;
	else
		return GAMESTATE_MENU;
}

void menuEnd() {

}

/********************* Main *********************/
int main() {
	// Initialize a billion things
	SDL_Window *window = SDL_CreateWindow("LECD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	SDL_Event e;
	VK2DRendererConfig config = {VK2D_MSAA_1X, VK2D_SCREEN_MODE_TRIPLE_BUFFER, VK2D_FILTER_TYPE_NEAREST};
	juInit(window, 3, 1);
	vk2dRendererInit(window, config, NULL);
	vec4 clearColour = {0, 0, 13.0/255.0, 1}; // Black
	vk2dRendererSetColourMod(VK2D_DEFAULT_COLOUR_MOD);
	bool stopRunning = false;
	double totalTime = 0;
	double iters = 0;
	double average = 1;
	JUClock framerateTimer;
	juClockReset(&framerateTimer);
	VK2DCameraSpec spec = {VK2D_CAMERA_TYPE_DEFAULT, PLAYER_START_X - (GAME_WIDTH / 2), PLAYER_START_Y - (GAME_HEIGHT / 2), WINDOW_WIDTH, WINDOW_HEIGHT, 1, 0, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
	VK2DCameraSpec spec3D = {VK2D_CAMERA_TYPE_PERSPECTIVE, 0, 0, GARBAGE_DISPOSAL_WIDTH, GARBAGE_DISPOSAL_HEIGHT, 1};
	spec3D.Perspective.fov = VK2D_PI * 0.2;
	spec3D.Perspective.eyes[2] = 3;
	spec3D.Perspective.up[1] = 1;
	gCam = vk2dCameraCreate(spec);
	g3DCam = vk2dCameraCreate(spec3D);
	gAssets = buildAssets();
	gFont = juFontLoadFromImage("assets/Font.png", 32, 128, FONT_WIDTH, FONT_HEIGHT);
	gamestate state = GAMESTATE_MENU;
	gGarbageModel = vk2dModelLoad("assets/GarbageDisposal.obj", gAssets->texGarbageDisposal);
	gShader = vk2dShaderLoad("assets/tex.vert.spv", "assets/tex.frag.spv", 4);
	gGarbageDisposalTexture = vk2dTextureCreate(GARBAGE_DISPOSAL_WIDTH, GARBAGE_DISPOSAL_HEIGHT);
	menuStart();
	JUClock fpsLock;
	gZoom = ZOOM_MAX;
	juClockStart(&fpsLock);

	// Game loop, just calls either menu or game update and swaps between them when necessary
	while (!stopRunning) {
		juUpdate();
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT) {
				stopRunning = true;
				abort();
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

		vk2dRendererSetTarget(gGarbageDisposalTexture);
		vk2dRendererEmpty();
		vk2dRendererLockCameras(g3DCam);
		vec3 axis = {0, 1, 0};
		vk2dRendererDrawModel(gGarbageModel, 0, 0, 0, 1, 1, 1, sin(juTime() * 0.5) * 0.5, axis, 0, 0, 0);
		vk2dRendererLockCameras(gCam);
		vk2dRendererSetTarget(VK2D_TARGET_SCREEN);

		if (state == GAMESTATE_MENU) {
			state = menuUpdate();
			if (state == GAMESTATE_GAME) {
				menuEnd();
				gameStart();
			} else if (state == GAMESTATE_QUIT){
				stopRunning = true;
				abort();
				gameEnd();
			}
		} else if (state == GAMESTATE_GAME) {
			state = gameUpdate();
			if (state == GAMESTATE_MENU) {
				gameEnd();
				menuStart();
			} else if (state == GAMESTATE_QUIT){
				stopRunning = true;
				abort();
				gameEnd();
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
	vk2dModelFree(gGarbageModel);
	vk2dShaderFree(gShader);
	vk2dTextureFree(gGarbageDisposalTexture);
	destroyAssets(gAssets);
	juQuit();
	vk2dRendererQuit();
	SDL_DestroyWindow(window);
	return 0;
}