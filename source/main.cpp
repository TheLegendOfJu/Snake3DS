// =====================================================================
// main.cpp - Einfaches 2D Jump'n'Run fuer den Nintendo 3DS
// =====================================================================
//
// Technik: libctru + citro2d/citro3d (Standard-Homebrew-Grafiklib)
// Enthaelt 5 fest vorgefertigte Level, einfache Plattform-Physik,
// Spikes (Hindernisse), ein Ziel pro Level, Leben-System und ein
// HUD auf dem unteren Bildschirm (Steuerung/Status).
//
// Steuerung:
//   D-Pad / Circle-Pad links/rechts  -> Bewegen
//   A                                -> Springen
//   START                            -> Spiel beenden
//   SELECT                           -> Neustart (bei Game Over / Sieg)
//
// Kompilieren (devkitARM / devkitPro 3DS-Toolchain):
//   Verwende das Standard-3DS-Makefile-Template und trage in LIBS ein:
//       LIBS := -lcitro2d -lcitro3d -lctru -lm
//   und in LIBDIRS die Pfade zu portlibs/libctru (wie im Standard-
//   Template ueblich). Dann einfach "make" ausfuehren -> erzeugt .3dsx
//
// =====================================================================

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------
// Konstanten
// ---------------------------------------------------------------------
#define SCREEN_TOP_W   400.0f
#define SCREEN_H       240.0f

#define MAX_LEVELS     5
#define MAX_PLATFORMS  24
#define MAX_SPIKES     16

#define GRAVITY        0.55f
#define JUMP_VELOCITY  -9.0f
#define MOVE_SPEED     2.6f
#define MAX_FALL_SPEED 9.0f

#define PLAYER_W       16.0f
#define PLAYER_H       20.0f
#define START_LIVES    3

// ---------------------------------------------------------------------
// Datentypen
// ---------------------------------------------------------------------
enum GameState {
    STATE_PLAYING,
    STATE_GAME_OVER,
    STATE_GAME_COMPLETE
};

struct Rect {
    float x, y, w, h;
};

struct Level {
    Rect platforms[MAX_PLATFORMS];
    int  platformCount;

    Rect spikes[MAX_SPIKES];
    int  spikeCount;

    float startX, startY;
    Rect  goal;
    float levelWidth;
    u32   bgColor;
    const char* name;
};

struct Player {
    float x, y;
    float vx, vy;
    bool  onGround;
};

// ---------------------------------------------------------------------
// Globale Variablen
// ---------------------------------------------------------------------
static Level  levels[MAX_LEVELS];
static int    currentLevelIndex = 0;
static Player player;
static int    lives = START_LIVES;
static GameState state = STATE_PLAYING;
static float  cameraX = 0.0f;

static C3D_RenderTarget* g_top = nullptr;

// ---------------------------------------------------------------------
// Hilfsfunktionen: Level-Aufbau
// ---------------------------------------------------------------------
static bool AabbOverlap(const Rect& a, float bx, float by, float bw, float bh) {
    return (a.x < bx + bw) && (a.x + a.w > bx) &&
           (a.y < by + bh) && (a.y + a.h > by);
}

static void AddPlatform(Level& lvl, float x, float y, float w, float h) {
    if (lvl.platformCount < MAX_PLATFORMS) {
        lvl.platforms[lvl.platformCount++] = { x, y, w, h };
    }
}

static void AddSpike(Level& lvl, float x, float y, float w, float h) {
    if (lvl.spikeCount < MAX_SPIKES) {
        lvl.spikes[lvl.spikeCount++] = { x, y, w, h };
    }
}

// ---------------------------------------------------------------------
// 5 vorgefertigte Level
// ---------------------------------------------------------------------
static void InitLevels() {
    // ---------------- Level 1: Wiese (Einstieg) ----------------
    {
        Level& lvl = levels[0];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name       = "Wiese";
        lvl.bgColor    = C2D_Color32(0x87, 0xCE, 0xEB, 0xFF);
        lvl.levelWidth = 500.0f;
        lvl.startX     = 20.0f;
        lvl.startY     = 150.0f;

        AddPlatform(lvl,   0, 200, 150, 40);
        AddPlatform(lvl, 170, 170,  40, 10);
        AddPlatform(lvl, 200, 200, 150, 40);
        AddPlatform(lvl, 300, 150,  60, 10);
        AddPlatform(lvl, 400, 200, 100, 40);

        lvl.goal = { 470, 150, 20, 50 };
    }

    // ---------------- Level 2: Stachelwiese ----------------
    {
        Level& lvl = levels[1];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name       = "Stachelwiese";
        lvl.bgColor    = C2D_Color32(0x6B, 0xB8, 0x5A, 0xFF);
        lvl.levelWidth = 650.0f;
        lvl.startX     = 20.0f;
        lvl.startY     = 150.0f;

        AddPlatform(lvl,   0, 200, 200, 40);
        AddSpike   (lvl, 120, 190,  20, 10);
        AddPlatform(lvl, 215, 160,  35, 10);
        AddPlatform(lvl, 260, 200, 150, 40);
        AddSpike   (lvl, 300, 190,  20, 10);
        AddPlatform(lvl, 415, 160,  35, 10);
        AddPlatform(lvl, 460, 200, 190, 40);
        AddSpike   (lvl, 560, 190,  20, 10);

        lvl.goal = { 610, 150, 20, 50 };
    }

    // ---------------- Level 3: Treppenschlucht ----------------
    {
        Level& lvl = levels[2];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name       = "Treppenschlucht";
        lvl.bgColor    = C2D_Color32(0x8A, 0x5A, 0xC0, 0xFF);
        lvl.levelWidth = 800.0f;
        lvl.startX     = 20.0f;
        lvl.startY     = 150.0f;

        AddPlatform(lvl,   0, 200, 120, 40);
        AddPlatform(lvl, 150, 190,  60, 10);
        AddPlatform(lvl, 250, 170,  60, 10);
        AddSpike   (lvl, 265, 160,  20, 10);
        AddPlatform(lvl, 350, 150,  60, 10);
        AddPlatform(lvl, 450, 170,  60, 10);
        AddPlatform(lvl, 550, 190,  60, 10);
        AddSpike   (lvl, 565, 180,  20, 10);
        AddPlatform(lvl, 650, 200, 150, 40);

        lvl.goal = { 760, 150, 20, 50 };
    }

    // ---------------- Level 4: Schluchtensprung ----------------
    {
        Level& lvl = levels[3];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name       = "Schluchtensprung";
        lvl.bgColor    = C2D_Color32(0xC0, 0x70, 0x20, 0xFF);
        lvl.levelWidth = 900.0f;
        lvl.startX     = 20.0f;
        lvl.startY     = 150.0f;

        AddPlatform(lvl,   0, 200, 100, 40);
        AddPlatform(lvl, 180, 200,  80, 40);
        AddSpike   (lvl, 200, 190,  20, 10);
        AddPlatform(lvl, 340, 200,  80, 40);
        AddSpike   (lvl, 370, 190,  20, 10);
        AddPlatform(lvl, 500, 200,  80, 40);
        AddPlatform(lvl, 660, 200,  80, 40);
        AddSpike   (lvl, 685, 190,  20, 10);
        AddPlatform(lvl, 820, 200,  80, 40);

        lvl.goal = { 860, 150, 20, 50 };
    }

    // ---------------- Level 5: Endstation (Finale) ----------------
    {
        Level& lvl = levels[4];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name       = "Endstation";
        lvl.bgColor    = C2D_Color32(0x90, 0x20, 0x20, 0xFF);
        lvl.levelWidth = 1000.0f;
        lvl.startX     = 20.0f;
        lvl.startY     = 150.0f;

        AddPlatform(lvl,   0, 200, 100, 40);
        AddPlatform(lvl, 160, 190,  50, 10);
        AddSpike   (lvl, 170, 180,  20, 10);
        AddPlatform(lvl, 260, 160,  50, 10);
        AddPlatform(lvl, 360, 190,  50, 10);
        AddSpike   (lvl, 375, 180,  20, 10);
        AddPlatform(lvl, 460, 150,  50, 10);
        AddPlatform(lvl, 560, 180,  50, 10);
        AddSpike   (lvl, 575, 170,  20, 10);
        AddPlatform(lvl, 660, 200,  80, 40);
        AddPlatform(lvl, 800, 190,  60, 10);
        AddSpike   (lvl, 815, 180,  20, 10);
        AddPlatform(lvl, 900, 200, 100, 40);

        lvl.goal = { 960, 150, 20, 50 };
    }
}

// ---------------------------------------------------------------------
// Spiel-Logik
// ---------------------------------------------------------------------
static void ResetPlayerToStart() {
    Level& lvl = levels[currentLevelIndex];
    player.x = lvl.startX;
    player.y = lvl.startY;
    player.vx = 0.0f;
    player.vy = 0.0f;
    player.onGround = false;
}

static void LoadLevel(int index) {
    currentLevelIndex = index;
    ResetPlayerToStart();
    cameraX = 0.0f;
    state = STATE_PLAYING;
}

static void KillPlayer() {
    lives--;
    if (lives <= 0) {
        state = STATE_GAME_OVER;
    } else {
        ResetPlayerToStart();
    }
}

static void UpdatePlayer(u32 kHeld, u32 kDown) {
    Level& lvl = levels[currentLevelIndex];

    // --- Horizontale Eingabe ---
    player.vx = 0.0f;
    if (kHeld & (KEY_LEFT  | KEY_CPAD_LEFT))  player.vx = -MOVE_SPEED;
    if (kHeld & (KEY_RIGHT | KEY_CPAD_RIGHT)) player.vx =  MOVE_SPEED;

    // --- Sprung ---
    if ((kDown & KEY_A) && player.onGround) {
        player.vy = JUMP_VELOCITY;
        player.onGround = false;
    }

    // --- Schwerkraft ---
    player.vy += GRAVITY;
    if (player.vy > MAX_FALL_SPEED) player.vy = MAX_FALL_SPEED;

    // --- X-Bewegung + Kollision ---
    player.x += player.vx;
    if (player.x < 0.0f) player.x = 0.0f;
    if (player.x + PLAYER_W > lvl.levelWidth) player.x = lvl.levelWidth - PLAYER_W;

    for (int i = 0; i < lvl.platformCount; i++) {
        Rect& p = lvl.platforms[i];
        if (AabbOverlap(p, player.x, player.y, PLAYER_W, PLAYER_H)) {
            if (player.vx > 0.0f)      player.x = p.x - PLAYER_W;
            else if (player.vx < 0.0f) player.x = p.x + p.w;
        }
    }

    // --- Y-Bewegung + Kollision ---
    player.onGround = false;
    player.y += player.vy;

    for (int i = 0; i < lvl.platformCount; i++) {
        Rect& p = lvl.platforms[i];
        if (AabbOverlap(p, player.x, player.y, PLAYER_W, PLAYER_H)) {
            if (player.vy > 0.0f) {
                player.y = p.y - PLAYER_H;
                player.vy = 0.0f;
                player.onGround = true;
            } else if (player.vy < 0.0f) {
                player.y = p.y + p.h;
                player.vy = 0.0f;
            }
        }
    }

    // --- Spikes (toedlich) ---
    for (int i = 0; i < lvl.spikeCount; i++) {
        if (AabbOverlap(lvl.spikes[i], player.x, player.y, PLAYER_W, PLAYER_H)) {
            KillPlayer();
            return;
        }
    }

    // --- Runtergefallen ---
    if (player.y > SCREEN_H + 40.0f) {
        KillPlayer();
        return;
    }

    // --- Ziel erreicht ---
    if (AabbOverlap(lvl.goal, player.x, player.y, PLAYER_W, PLAYER_H)) {
        if (currentLevelIndex + 1 < MAX_LEVELS) {
            LoadLevel(currentLevelIndex + 1);
        } else {
            state = STATE_GAME_COMPLETE;
        }
        return;
    }

    // --- Kamera nachfuehren ---
    float target = player.x - SCREEN_TOP_W / 2.0f + PLAYER_W / 2.0f;
    float maxCam = lvl.levelWidth - SCREEN_TOP_W;
    if (maxCam < 0.0f) maxCam = 0.0f;
    if (target < 0.0f) target = 0.0f;
    if (target > maxCam) target = maxCam;
    cameraX = target;
}

// ---------------------------------------------------------------------
// Rendering: oberer Bildschirm (Spielgrafik)
// ---------------------------------------------------------------------
static void RenderTop() {
    Level& lvl = levels[currentLevelIndex];

    C2D_TargetClear(g_top, lvl.bgColor);
    C2D_SceneBegin(g_top);

    u32 platformColor = C2D_Color32(0x8B, 0x5A, 0x2B, 0xFF);
    u32 spikeColor    = C2D_Color32(0xE0, 0x20, 0x20, 0xFF);
    u32 playerColor   = C2D_Color32(0x30, 0x90, 0xE0, 0xFF);
    u32 poleColor     = C2D_Color32(0x60, 0x60, 0x60, 0xFF);
    u32 flagColor     = C2D_Color32(0xF0, 0xD0, 0x20, 0xFF);

    // Plattformen
    for (int i = 0; i < lvl.platformCount; i++) {
        Rect& p = lvl.platforms[i];
        float sx = p.x - cameraX;
        if (sx + p.w < 0.0f || sx > SCREEN_TOP_W) continue;
        C2D_DrawRectSolid(sx, p.y, 0.0f, p.w, p.h, platformColor);
    }

    // Spikes (als Dreiecke)
    for (int i = 0; i < lvl.spikeCount; i++) {
        Rect& s = lvl.spikes[i];
        float sx = s.x - cameraX;
        if (sx + s.w < 0.0f || sx > SCREEN_TOP_W) continue;
        C2D_DrawTriangle(sx,             s.y + s.h, spikeColor,
                          sx + s.w,       s.y + s.h, spikeColor,
                          sx + s.w / 2.0f, s.y,       spikeColor, 0.0f);
    }

    // Ziel (Fahnenstange + Fahne)
    {
        Rect& g = lvl.goal;
        float sx = g.x - cameraX;
        C2D_DrawRectSolid(sx, g.y, 0.0f, 4.0f, g.h, poleColor);
        C2D_DrawTriangle(sx + 4.0f, g.y,          flagColor,
                          sx + 4.0f, g.y + 16.0f,  flagColor,
                          sx + 20.0f, g.y + 8.0f,  flagColor, 0.0f);
    }

    // Spieler
    {
        float sx = player.x - cameraX;
        C2D_DrawRectSolid(sx, player.y, 0.0f, PLAYER_W, PLAYER_H, playerColor);
    }
}

// ---------------------------------------------------------------------
// Rendering: unterer Bildschirm (HUD via Konsole)
// ---------------------------------------------------------------------
static void RenderBottomHud() {
    consoleClear();
    printf("\x1b[1;1HJump'n'Run - 3DS Demo\n");
    printf("\x1b[3;1HLevel: %d/%d (%s)",
           currentLevelIndex + 1, MAX_LEVELS, levels[currentLevelIndex].name);
    printf("\x1b[4;1HLeben: %d", lives);

    printf("\x1b[6;1HSteuerung:");
    printf("\x1b[7;1H D-Pad / Circle-Pad: Bewegen");
    printf("\x1b[8;1H A: Springen");
    printf("\x1b[9;1H START: Beenden");

    switch (state) {
        case STATE_GAME_OVER:
            printf("\x1b[12;1HGAME OVER!");
            printf("\x1b[13;1HDruecke SELECT fuer Neustart");
            break;
        case STATE_GAME_COMPLETE:
            printf("\x1b[12;1HGlueckwunsch! Alle Level geschafft!");
            printf("\x1b[13;1HDruecke SELECT fuer Neustart");
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------
// main
// ---------------------------------------------------------------------
int main(int argc, char** argv) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

    InitLevels();
    lives = START_LIVES;
    LoadLevel(0);

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        if (kDown & KEY_START) break;

        if (state == STATE_PLAYING) {
            UpdatePlayer(kHeld, kDown);
        } else if ((state == STATE_GAME_OVER || state == STATE_GAME_COMPLETE) &&
                   (kDown & KEY_SELECT)) {
            lives = START_LIVES;
            LoadLevel(0);
        }

        RenderBottomHud();

        C3D_FrameBegin(C3D_FRAME_SYNCNONE);
        RenderTop();
        C3D_FrameEnd(0);
    }

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
