// This code was automatically generated by GenHeader.py
#pragma once
#include "JamUtil.h"

// Forward declare the asset array
extern JULoadedAsset ASSETS[18];

#ifdef ASSETS_IMPLEMENTATION
JULoadedAsset ASSETS[] = {
    {"assets/Arrow.png"},
    {"assets/Background.png"},
    {"assets/Drone.png"},
    {"assets/Font.png"},
    {"assets/Foreground.png"},
    {"assets/GarbageDisposal.obj"},
    {"assets/GarbageDisposal.png"},
    {"assets/HP.png"},
    {"assets/Midground.png"},
    {"assets/Mine.png"},
    {"assets/Player.png"},
    {"assets/PlayerThruster.png"},
    {"assets/Sun.png"},
    {"assets/tex.frag.spv"},
    {"assets/tex.vert.spv"},
    {"assets/title.png"},
    {"assets/Trash1.png"},
    {"assets/Trash2.png"},
};
#endif

typedef struct Assets {
    JULoader loader;
    VK2DTexture texArrow;
    VK2DTexture texBackground;
    VK2DTexture texDrone;
    VK2DTexture texFont;
    VK2DTexture texForeground;
    JUBuffer bufGarbageDisposal;
    VK2DTexture texGarbageDisposal;
    VK2DTexture texHP;
    VK2DTexture texMidground;
    VK2DTexture texMine;
    VK2DTexture texPlayer;
    VK2DTexture texPlayerThruster;
    VK2DTexture texSun;
    VK2DTexture textitle;
    VK2DTexture texTrash1;
    VK2DTexture texTrash2;
} Assets;

// Functions to create and destroy the asset struct
Assets *buildAssets();
void destroyAssets(Assets *s);

#ifdef ASSETS_IMPLEMENTATION
Assets *buildAssets() {
    Assets *s = malloc(sizeof(struct Assets));
    s->loader = juLoaderCreate(ASSETS, 18);
    s->texArrow = juLoaderGetTexture(s->loader, "assets/Arrow.png");
    s->texBackground = juLoaderGetTexture(s->loader, "assets/Background.png");
    s->texDrone = juLoaderGetTexture(s->loader, "assets/Drone.png");
    s->texFont = juLoaderGetTexture(s->loader, "assets/Font.png");
    s->texForeground = juLoaderGetTexture(s->loader, "assets/Foreground.png");
    s->bufGarbageDisposal = juLoaderGetBuffer(s->loader, "assets/GarbageDisposal.obj");
    s->texGarbageDisposal = juLoaderGetTexture(s->loader, "assets/GarbageDisposal.png");
    s->texHP = juLoaderGetTexture(s->loader, "assets/HP.png");
    s->texMidground = juLoaderGetTexture(s->loader, "assets/Midground.png");
    s->texMine = juLoaderGetTexture(s->loader, "assets/Mine.png");
    s->texPlayer = juLoaderGetTexture(s->loader, "assets/Player.png");
    s->texPlayerThruster = juLoaderGetTexture(s->loader, "assets/PlayerThruster.png");
    s->texSun = juLoaderGetTexture(s->loader, "assets/Sun.png");
    s->textitle = juLoaderGetTexture(s->loader, "assets/title.png");
    s->texTrash1 = juLoaderGetTexture(s->loader, "assets/Trash1.png");
    s->texTrash2 = juLoaderGetTexture(s->loader, "assets/Trash2.png");
    return s;
}

void destroyAssets(Assets *s) {
    juLoaderFree(s->loader);
    free(s);
}
#endif
