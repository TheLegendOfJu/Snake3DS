#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <cstdio>
#include <cstring>

#define SCREEN_TOP_W 400.0f
#define SCREEN_H 240.0f
#define MAX_LEVELS 5
#define MAX_PLATFORMS 24
#define MAX_SPIKES 16
#define MAX_COINS 12
#define MAX_MOVERS 6
#define MAX_DECOR 10
#define GRAVITY 0.55f
#define JUMP_VELOCITY -9.0f
#define MOVE_SPEED 2.6f
#define MAX_FALL_SPEED 9.0f
#define PLAYER_W 16.0f
#define PLAYER_H 20.0f
#define START_LIVES 3
#define TRANSITION_FRAMES 90
#define INVULN_FRAMES 60

enum GameState {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_LEVEL_TRANSITION,
    STATE_GAME_OVER,
    STATE_GAME_COMPLETE
};

struct Rect {
    float x;
    float y;
    float w;
    float h;
};

struct Coin {
    float x;
    float y;
    float w;
    float h;
    bool collected;
};

struct MovingPlatform {
    float x;
    float y;
    float w;
    float h;
    float minX;
    float maxX;
    float speed;
    int dir;
};

struct Decoration {
    float x;
    float y;
    float w;
    float h;
    u32 color;
};

struct Level {
    Rect platforms[MAX_PLATFORMS];
    int platformCount;
    Rect spikes[MAX_SPIKES];
    int spikeCount;
    Coin coins[MAX_COINS];
    int coinCount;
    MovingPlatform movers[MAX_MOVERS];
    int moverCount;
    Decoration decor[MAX_DECOR];
    int decorCount;
    float startX;
    float startY;
    Rect goal;
    float levelWidth;
    u32 bgColor;
    const char* name;
};

struct Player {
    float x;
    float y;
    float vx;
    float vy;
    bool onGround;
    bool facingRight;
    int invulnTimer;
};

static Level levels[MAX_LEVELS];
static int currentLevelIndex = 0;
static int nextLevelPending = 0;
static Player player;
static int lives = START_LIVES;
static int score = 0;
static int transitionTimer = 0;
static int frameCounter = 0;
static GameState state = STATE_TITLE;
static float cameraX = 0.0f;
static C3D_RenderTarget* g_top = nullptr;

static bool AabbOverlap(const Rect& a, float bx, float by, float bw, float bh) {
    return (a.x < bx + bw) && (a.x + a.w > bx) && (a.y < by + bh) && (a.y + a.h > by);
}

static bool AabbOverlapRect(const Rect& a, const Rect& b) {
    return AabbOverlap(a, b.x, b.y, b.w, b.h);
}

static void AddPlatform(Level& lvl, float x, float y, float w, float h) {
    if (lvl.platformCount < MAX_PLATFORMS) {
        lvl.platforms[lvl.platformCount].x = x;
        lvl.platforms[lvl.platformCount].y = y;
        lvl.platforms[lvl.platformCount].w = w;
        lvl.platforms[lvl.platformCount].h = h;
        lvl.platformCount++;
    }
}

static void AddSpike(Level& lvl, float x, float y, float w, float h) {
    if (lvl.spikeCount < MAX_SPIKES) {
        lvl.spikes[lvl.spikeCount].x = x;
        lvl.spikes[lvl.spikeCount].y = y;
        lvl.spikes[lvl.spikeCount].w = w;
        lvl.spikes[lvl.spikeCount].h = h;
        lvl.spikeCount++;
    }
}

static void AddCoin(Level& lvl, float x, float y, float w, float h) {
    if (lvl.coinCount < MAX_COINS) {
        lvl.coins[lvl.coinCount].x = x;
        lvl.coins[lvl.coinCount].y = y;
        lvl.coins[lvl.coinCount].w = w;
        lvl.coins[lvl.coinCount].h = h;
        lvl.coins[lvl.coinCount].collected = false;
        lvl.coinCount++;
    }
}

static void AddMover(Level& lvl, float x, float y, float w, float h, float minX, float maxX, float speed) {
    if (lvl.moverCount < MAX_MOVERS) {
        lvl.movers[lvl.moverCount].x = x;
        lvl.movers[lvl.moverCount].y = y;
        lvl.movers[lvl.moverCount].w = w;
        lvl.movers[lvl.moverCount].h = h;
        lvl.movers[lvl.moverCount].minX = minX;
        lvl.movers[lvl.moverCount].maxX = maxX;
        lvl.movers[lvl.moverCount].speed = speed;
        lvl.movers[lvl.moverCount].dir = 1;
        lvl.moverCount++;
    }
}

static void AddDecor(Level& lvl, float x, float y, float w, float h, u32 color) {
    if (lvl.decorCount < MAX_DECOR) {
        lvl.decor[lvl.decorCount].x = x;
        lvl.decor[lvl.decorCount].y = y;
        lvl.decor[lvl.decorCount].w = w;
        lvl.decor[lvl.decorCount].h = h;
        lvl.decor[lvl.decorCount].color = color;
        lvl.decorCount++;
    }
}

static void PlaySfxJump() {
    frameCounter = frameCounter;
}

static void PlaySfxCoin() {
    frameCounter = frameCounter;
}

static void PlaySfxHit() {
    frameCounter = frameCounter;
}

static void PlaySfxWin() {
    frameCounter = frameCounter;
}

static void InitLevels() {
    {
        Level& lvl = levels[0];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name = "Wiese";
        lvl.bgColor = C2D_Color32(0x87, 0xCE, 0xEB, 0xFF);
        lvl.levelWidth = 500.0f;
        lvl.startX = 20.0f;
        lvl.startY = 150.0f;

        u32 hillColor = C2D_Color32(0x5C, 0x9C, 0x3A, 0xFF);
        u32 cloudColor = C2D_Color32(0xFF, 0xFF, 0xFF, 0xC0);
        AddDecor(lvl, 40, 205, 140, 30, hillColor);
        AddDecor(lvl, 260, 210, 150, 25, hillColor);
        AddDecor(lvl, 70, 40, 60, 18, cloudColor);
        AddDecor(lvl, 320, 55, 70, 20, cloudColor);

        AddPlatform(lvl, 0, 200, 150, 40);
        AddPlatform(lvl, 170, 170, 40, 10);
        AddPlatform(lvl, 200, 200, 150, 40);
        AddPlatform(lvl, 300, 150, 60, 10);
        AddPlatform(lvl, 400, 200, 100, 40);

        AddCoin(lvl, 190, 150, 10, 10);
        AddCoin(lvl, 320, 120, 10, 10);
        AddCoin(lvl, 430, 170, 10, 10);

        lvl.goal = { 470, 150, 20, 50 };
    }

    {
        Level& lvl = levels[1];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name = "Stachelwiese";
        lvl.bgColor = C2D_Color32(0x6B, 0xB8, 0x5A, 0xFF);
        lvl.levelWidth = 650.0f;
        lvl.startX = 20.0f;
        lvl.startY = 150.0f;

        u32 hillColor = C2D_Color32(0x3E, 0x7A, 0x2E, 0xFF);
        u32 cloudColor = C2D_Color32(0xFF, 0xFF, 0xFF, 0xC0);
        AddDecor(lvl, 60, 205, 160, 30, hillColor);
        AddDecor(lvl, 320, 210, 170, 25, hillColor);
        AddDecor(lvl, 500, 205, 140, 30, hillColor);
        AddDecor(lvl, 90, 45, 60, 18, cloudColor);

        AddPlatform(lvl, 0, 200, 200, 40);
        AddSpike(lvl, 120, 190, 20, 10);
        AddPlatform(lvl, 215, 160, 35, 10);
        AddPlatform(lvl, 260, 200, 150, 40);
        AddSpike(lvl, 300, 190, 20, 10);
        AddMover(lvl, 415, 150, 35, 10, 400, 460, 0.9f);
        AddPlatform(lvl, 460, 200, 190, 40);
        AddSpike(lvl, 560, 190, 20, 10);

        AddCoin(lvl, 130, 170, 10, 10);
        AddCoin(lvl, 320, 180, 10, 10);
        AddCoin(lvl, 430, 120, 10, 10);
        AddCoin(lvl, 600, 170, 10, 10);

        lvl.goal = { 610, 150, 20, 50 };
    }

    {
        Level& lvl = levels[2];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name = "Treppenschlucht";
        lvl.bgColor = C2D_Color32(0x8A, 0x5A, 0xC0, 0xFF);
        lvl.levelWidth = 800.0f;
        lvl.startX = 20.0f;
        lvl.startY = 150.0f;

        u32 hillColor = C2D_Color32(0x5A, 0x3A, 0x7A, 0xFF);
        AddDecor(lvl, 20, 205, 150, 30, hillColor);
        AddDecor(lvl, 400, 210, 180, 25, hillColor);
        AddDecor(lvl, 650, 205, 150, 30, hillColor);

        AddPlatform(lvl, 0, 200, 120, 40);
        AddPlatform(lvl, 150, 190, 60, 10);
        AddPlatform(lvl, 250, 170, 60, 10);
        AddSpike(lvl, 265, 160, 20, 10);
        AddMover(lvl, 340, 150, 50, 10, 330, 420, 1.0f);
        AddPlatform(lvl, 450, 170, 60, 10);
        AddPlatform(lvl, 550, 190, 60, 10);
        AddSpike(lvl, 565, 180, 20, 10);
        AddPlatform(lvl, 650, 200, 150, 40);

        AddCoin(lvl, 170, 170, 10, 10);
        AddCoin(lvl, 270, 150, 10, 10);
        AddCoin(lvl, 470, 150, 10, 10);
        AddCoin(lvl, 570, 170, 10, 10);

        lvl.goal = { 760, 150, 20, 50 };
    }

    {
        Level& lvl = levels[3];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name = "Schluchtensprung";
        lvl.bgColor = C2D_Color32(0xC0, 0x70, 0x20, 0xFF);
        lvl.levelWidth = 900.0f;
        lvl.startX = 20.0f;
        lvl.startY = 150.0f;

        u32 hillColor = C2D_Color32(0x8A, 0x4A, 0x10, 0xFF);
        AddDecor(lvl, 30, 205, 140, 30, hillColor);
        AddDecor(lvl, 420, 210, 160, 25, hillColor);
        AddDecor(lvl, 750, 205, 140, 30, hillColor);

        AddPlatform(lvl, 0, 200, 100, 40);
        AddPlatform(lvl, 180, 200, 80, 40);
        AddSpike(lvl, 200, 190, 20, 10);
        AddMover(lvl, 300, 170, 40, 10, 290, 340, 0.8f);
        AddPlatform(lvl, 340, 200, 80, 40);
        AddSpike(lvl, 370, 190, 20, 10);
        AddPlatform(lvl, 500, 200, 80, 40);
        AddMover(lvl, 600, 160, 40, 10, 590, 650, 1.1f);
        AddPlatform(lvl, 660, 200, 80, 40);
        AddSpike(lvl, 685, 190, 20, 10);
        AddPlatform(lvl, 820, 200, 80, 40);

        AddCoin(lvl, 220, 170, 10, 10);
        AddCoin(lvl, 360, 170, 10, 10);
        AddCoin(lvl, 530, 170, 10, 10);
        AddCoin(lvl, 700, 170, 10, 10);

        lvl.goal = { 860, 150, 20, 50 };
    }

    {
        Level& lvl = levels[4];
        std::memset(&lvl, 0, sizeof(Level));
        lvl.name = "Endstation";
        lvl.bgColor = C2D_Color32(0x90, 0x20, 0x20, 0xFF);
        lvl.levelWidth = 1000.0f;
        lvl.startX = 20.0f;
        lvl.startY = 150.0f;

        u32 hillColor = C2D_Color32(0x60, 0x10, 0x10, 0xFF);
        AddDecor(lvl, 20, 205, 130, 30, hillColor);
        AddDecor(lvl, 400, 210, 160, 25, hillColor);
        AddDecor(lvl, 700, 205, 150, 30, hillColor);
        AddDecor(lvl, 900, 210, 100, 25, hillColor);

        AddPlatform(lvl, 0, 200, 100, 40);
        AddPlatform(lvl, 160, 190, 50, 10);
        AddSpike(lvl, 170, 180, 20, 10);
        AddMover(lvl, 240, 160, 40, 10, 230, 300, 1.0f);
        AddPlatform(lvl, 360, 190, 50, 10);
        AddSpike(lvl, 375, 180, 20, 10);
        AddPlatform(lvl, 460, 150, 50, 10);
        AddMover(lvl, 540, 170, 40, 10, 530, 600, 1.2f);
        AddSpike(lvl, 610, 165, 20, 10);
        AddPlatform(lvl, 660, 200, 80, 40);
        AddPlatform(lvl, 800, 190, 60, 10);
        AddSpike(lvl, 815, 180, 20, 10);
        AddPlatform(lvl, 900, 200, 100, 40);

        AddCoin(lvl, 190, 170, 10, 10);
        AddCoin(lvl, 390, 170, 10, 10);
        AddCoin(lvl, 490, 130, 10, 10);
        AddCoin(lvl, 690, 170, 10, 10);
        AddCoin(lvl, 830, 170, 10, 10);
        AddCoin(lvl, 930, 170, 10, 10);

        lvl.goal = { 960, 150, 20, 50 };
    }
}

static void ResetPlayerToStart() {
    Level& lvl = levels[currentLevelIndex];
    player.x = lvl.startX;
    player.y = lvl.startY;
    player.vx = 0.0f;
    player.vy = 0.0f;
    player.onGround = false;
    player.facingRight = true;
    player.invulnTimer = INVULN_FRAMES;
}

static void LoadLevel(int index) {
    currentLevelIndex = index;
    ResetPlayerToStart();
    cameraX = 0.0f;
    state = STATE_PLAYING;
}

static void KillPlayer() {
    PlaySfxHit();
    lives--;
    if (lives <= 0) {
        state = STATE_GAME_OVER;
    } else {
        ResetPlayerToStart();
    }
}

static void UpdateMovers(Level& lvl) {
    for (int i = 0; i < lvl.moverCount; i++) {
        MovingPlatform& m = lvl.movers[i];
        m.x += m.speed * m.dir;
        if (m.x > m.maxX) {
            m.x = m.maxX;
            m.dir = -1;
        }
        if (m.x < m.minX) {
            m.x = m.minX;
            m.dir = 1;
        }
    }
}

static void UpdatePlayer(u32 kHeld, u32 kDown) {
    Level& lvl = levels[currentLevelIndex];

    player.vx = 0.0f;
    if (kHeld & (KEY_LEFT | KEY_CPAD_LEFT)) {
        player.vx = -MOVE_SPEED;
        player.facingRight = false;
    }
    if (kHeld & (KEY_RIGHT | KEY_CPAD_RIGHT)) {
        player.vx = MOVE_SPEED;
        player.facingRight = true;
    }

    if ((kDown & KEY_A) && player.onGround) {
        player.vy = JUMP_VELOCITY;
        player.onGround = false;
        PlaySfxJump();
    }

    player.vy += GRAVITY;
    if (player.vy > MAX_FALL_SPEED) {
        player.vy = MAX_FALL_SPEED;
    }

    player.x += player.vx;
    if (player.x < 0.0f) {
        player.x = 0.0f;
    }
    if (player.x + PLAYER_W > lvl.levelWidth) {
        player.x = lvl.levelWidth - PLAYER_W;
    }

    for (int i = 0; i < lvl.platformCount; i++) {
        Rect& p = lvl.platforms[i];
        if (AabbOverlap(p, player.x, player.y, PLAYER_W, PLAYER_H)) {
            if (player.vx > 0.0f) {
                player.x = p.x - PLAYER_W;
            } else if (player.vx < 0.0f) {
                player.x = p.x + p.w;
            }
        }
    }

    for (int i = 0; i < lvl.moverCount; i++) {
        MovingPlatform& m = lvl.movers[i];
        Rect r = { m.x, m.y, m.w, m.h };
        if (AabbOverlap(r, player.x, player.y, PLAYER_W, PLAYER_H)) {
            if (player.vx > 0.0f) {
                player.x = m.x - PLAYER_W;
            } else if (player.vx < 0.0f) {
                player.x = m.x + m.w;
            }
        }
    }

    player.onGround = false;
    player.y += player.vy;

    int landedMoverIndex = -1;

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

    for (int i = 0; i < lvl.moverCount; i++) {
        MovingPlatform& m = lvl.movers[i];
        Rect r = { m.x, m.y, m.w, m.h };
        if (AabbOverlap(r, player.x, player.y, PLAYER_W, PLAYER_H)) {
            if (player.vy > 0.0f) {
                player.y = m.y - PLAYER_H;
                player.vy = 0.0f;
                player.onGround = true;
                landedMoverIndex = i;
            } else if (player.vy < 0.0f) {
                player.y = m.y + m.h;
                player.vy = 0.0f;
            }
        }
    }

    if (landedMoverIndex >= 0) {
        MovingPlatform& m = lvl.movers[landedMoverIndex];
        player.x += m.speed * m.dir;
        if (player.x < 0.0f) {
            player.x = 0.0f;
        }
        if (player.x + PLAYER_W > lvl.levelWidth) {
            player.x = lvl.levelWidth - PLAYER_W;
        }
    }

    if (player.invulnTimer > 0) {
        player.invulnTimer--;
    }

    if (player.invulnTimer <= 0) {
        for (int i = 0; i < lvl.spikeCount; i++) {
            if (AabbOverlap(lvl.spikes[i], player.x, player.y, PLAYER_W, PLAYER_H)) {
                KillPlayer();
                return;
            }
        }
    }

    for (int i = 0; i < lvl.coinCount; i++) {
        Coin& c = lvl.coins[i];
        if (!c.collected) {
            Rect r = { c.x, c.y, c.w, c.h };
            if (AabbOverlap(r, player.x, player.y, PLAYER_W, PLAYER_H)) {
                c.collected = true;
                score += 10;
                PlaySfxCoin();
            }
        }
    }

    if (player.y > SCREEN_H + 40.0f) {
        KillPlayer();
        return;
    }

    if (AabbOverlap(lvl.goal, player.x, player.y, PLAYER_W, PLAYER_H)) {
        PlaySfxWin();
        if (currentLevelIndex + 1 < MAX_LEVELS) {
            nextLevelPending = currentLevelIndex + 1;
        } else {
            nextLevelPending = -1;
        }
        transitionTimer = TRANSITION_FRAMES;
        state = STATE_LEVEL_TRANSITION;
        return;
    }

    float target = player.x - SCREEN_TOP_W / 2.0f + PLAYER_W / 2.0f;
    float maxCam = lvl.levelWidth - SCREEN_TOP_W;
    if (maxCam < 0.0f) {
        maxCam = 0.0f;
    }
    if (target < 0.0f) {
        target = 0.0f;
    }
    if (target > maxCam) {
        target = maxCam;
    }
    cameraX = target;
}

static void RenderTop() {
    Level& lvl = levels[currentLevelIndex];

    C2D_TargetClear(g_top, lvl.bgColor);
    C2D_SceneBegin(g_top);

    u32 platformColor = C2D_Color32(0x8B, 0x5A, 0x2B, 0xFF);
    u32 moverColor = C2D_Color32(0xA0, 0x70, 0x40, 0xFF);
    u32 spikeColor = C2D_Color32(0xE0, 0x20, 0x20, 0xFF);
    u32 playerColor = C2D_Color32(0x30, 0x90, 0xE0, 0xFF);
    u32 eyeColor = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
    u32 poleColor = C2D_Color32(0x60, 0x60, 0x60, 0xFF);
    u32 flagColor = C2D_Color32(0xF0, 0xD0, 0x20, 0xFF);
    u32 coinColor = C2D_Color32(0xFF, 0xD7, 0x00, 0xFF);

    float parallax = cameraX * 0.4f;
    for (int i = 0; i < lvl.decorCount; i++) {
        Decoration& d = lvl.decor[i];
        float sx = d.x - parallax;
        if (sx + d.w < 0.0f || sx > SCREEN_TOP_W) {
            continue;
        }
        C2D_DrawRectSolid(sx, d.y, 0.0f, d.w, d.h, d.color);
    }

    for (int i = 0; i < lvl.platformCount; i++) {
        Rect& p = lvl.platforms[i];
        float sx = p.x - cameraX;
        if (sx + p.w < 0.0f || sx > SCREEN_TOP_W) {
            continue;
        }
        C2D_DrawRectSolid(sx, p.y, 0.0f, p.w, p.h, platformColor);
    }

    for (int i = 0; i < lvl.moverCount; i++) {
        MovingPlatform& m = lvl.movers[i];
        float sx = m.x - cameraX;
        if (sx + m.w < 0.0f || sx > SCREEN_TOP_W) {
            continue;
        }
        C2D_DrawRectSolid(sx, m.y, 0.0f, m.w, m.h, moverColor);
    }

    for (int i = 0; i < lvl.spikeCount; i++) {
        Rect& s = lvl.spikes[i];
        float sx = s.x - cameraX;
        if (sx + s.w < 0.0f || sx > SCREEN_TOP_W) {
            continue;
        }
        C2D_DrawTriangle(sx, s.y + s.h, spikeColor, sx + s.w, s.y + s.h, spikeColor, sx + s.w / 2.0f, s.y, spikeColor, 0.0f);
    }

    for (int i = 0; i < lvl.coinCount; i++) {
        Coin& c = lvl.coins[i];
        if (c.collected) {
            continue;
        }
        float sx = c.x - cameraX;
        if (sx + c.w < 0.0f || sx > SCREEN_TOP_W) {
            continue;
        }
        C2D_DrawRectSolid(sx, c.y, 0.0f, c.w, c.h, coinColor);
    }

    {
        Rect& g = lvl.goal;
        float sx = g.x - cameraX;
        C2D_DrawRectSolid(sx, g.y, 0.0f, 4.0f, g.h, poleColor);
        C2D_DrawTriangle(sx + 4.0f, g.y, flagColor, sx + 4.0f, g.y + 16.0f, flagColor, sx + 20.0f, g.y + 8.0f, flagColor, 0.0f);
    }

    bool drawPlayer = true;
    if (player.invulnTimer > 0 && (frameCounter / 4) % 2 == 0) {
        drawPlayer = false;
    }

    if (drawPlayer) {
        float sx = player.x - cameraX;
        C2D_DrawRectSolid(sx, player.y, 0.0f, PLAYER_W, PLAYER_H, playerColor);
        float eyeX = player.facingRight ? sx + PLAYER_W - 5.0f : sx + 2.0f;
        C2D_DrawRectSolid(eyeX, player.y + 4.0f, 0.0f, 3.0f, 3.0f, eyeColor);
    }
}

static void RenderBottomHud() {
    consoleClear();
    printf("\x1b[1;1HJump'n'Run - 3DS Demo\n");

    if (state == STATE_TITLE) {
        printf("\x1b[3;1HDruecke START um zu beginnen");
        printf("\x1b[5;1HSteuerung:");
        printf("\x1b[6;1H D-Pad / Circle-Pad: Bewegen");
        printf("\x1b[7;1H A: Springen");
        printf("\x1b[8;1H SELECT: Pause");
        return;
    }

    printf("\x1b[3;1HLevel: %d/%d (%s)", currentLevelIndex + 1, MAX_LEVELS, levels[currentLevelIndex].name);
    printf("\x1b[4;1HLeben: %d", lives);
    printf("\x1b[5;1HPunkte: %d", score);

    printf("\x1b[7;1HSteuerung:");
    printf("\x1b[8;1H D-Pad / Circle-Pad: Bewegen");
    printf("\x1b[9;1H A: Springen");
    printf("\x1b[10;1H SELECT: Pause  START: Beenden");

    if (state == STATE_PAUSED) {
        printf("\x1b[12;1HPAUSE");
        printf("\x1b[13;1HA: Weiter  SELECT: Zum Titel  START: Beenden");
    } else if (state == STATE_LEVEL_TRANSITION) {
        if (nextLevelPending >= 0) {
            printf("\x1b[12;1HLevel geschafft!");
        } else {
            printf("\x1b[12;1HLetztes Level geschafft!");
        }
    } else if (state == STATE_GAME_OVER) {
        printf("\x1b[12;1HGAME OVER!");
        printf("\x1b[13;1HA: Neustart  START: Beenden");
    } else if (state == STATE_GAME_COMPLETE) {
        printf("\x1b[12;1HGlueckwunsch! Alle Level geschafft!");
        printf("\x1b[13;1HA: Neustart  START: Beenden");
    }
}

int main(int argc, char** argv) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);

    g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

    InitLevels();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        frameCounter++;

        if (state == STATE_TITLE) {
            if (kDown & KEY_START) {
                lives = START_LIVES;
                score = 0;
                LoadLevel(0);
            }
        } else if (state == STATE_PLAYING) {
            if (kDown & KEY_START) {
                break;
            }
            if (kDown & KEY_SELECT) {
                state = STATE_PAUSED;
            } else {
                Level& lvl = levels[currentLevelIndex];
                UpdateMovers(lvl);
                UpdatePlayer(kHeld, kDown);
            }
        } else if (state == STATE_PAUSED) {
            if (kDown & KEY_A) {
                state = STATE_PLAYING;
            } else if (kDown & KEY_SELECT) {
                state = STATE_TITLE;
            } else if (kDown & KEY_START) {
                break;
            }
        } else if (state == STATE_LEVEL_TRANSITION) {
            transitionTimer--;
            if (transitionTimer <= 0) {
                if (nextLevelPending >= 0) {
                    LoadLevel(nextLevelPending);
                } else {
                    state = STATE_GAME_COMPLETE;
                }
            }
            if (kDown & KEY_START) {
                break;
            }
        } else if (state == STATE_GAME_OVER || state == STATE_GAME_COMPLETE) {
            if (kDown & KEY_A) {
                lives = START_LIVES;
                score = 0;
                LoadLevel(0);
            } else if (kDown & KEY_START) {
                break;
            }
        }

        RenderBottomHud();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        RenderTop();
        C3D_FrameEnd(0);
    }

    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
