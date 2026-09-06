#include <3ds.h>
#include <citro2d.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>

using std::vector;
using std::string;

static C3D_RenderTarget* top = nullptr;
static C3D_RenderTarget* bottom = nullptr;





static const int SCREEN_W = 400;
static const int SCREEN_H = 240;
static const int TILE = 16;

static const float GRAVITY = 0.42f;
static const float JUMP_VEL = -6.8f;
static const float WALK_SPEED = 1.8f;
static const float SPRINT_SPEED = 3.25f;
static const float MAX_FALL = 8.0f;

static const int LEVEL_COUNT = 15;





static void rect(float x, float y, float w, float h, u32 color)
{
    C2D_DrawRectSolid(x, y, w, h, color);
    C2D_DrawRectSolid(x, y, w, 1.5f, C2D_Color32(0,0,0,255));
    C2D_DrawRectSolid(x, y + h - 1.5f, w, 1.5f, C2D_Color32(0,0,0,255));
    C2D_DrawRectSolid(x, y, 1.5f, h, C2D_Color32(0,0,0,255));
    C2D_DrawRectSolid(x + w - 1.5f, y, 1.5f, h, C2D_Color32(0,0,0,255));
}

static void line(float x1, float y1, float x2, float y2, float width, u32 color)
{
    C2D_DrawLine(x1, y1, x2, y2, color, color, width);
}

static C2D_Font systemFont;
static C2D_TextBuf textBuf;

static void drawOutlinedText(float x, float y, float sx, float sy, u32 color,
                             const char* fmt, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    C2D_Text t;
    C2D_TextParse(&t, textBuf, buffer);
    C2D_TextOptimize(&t);

    const float o = 1.25f;
    C2D_DrawText(&t, C2D_WithColor, x-o, y, 0.0f, sx, sy, C2D_Color32(0,0,0,255));
    C2D_DrawText(&t, C2D_WithColor, x+o, y, 0.0f, sx, sy, C2D_Color32(0,0,0,255));
    C2D_DrawText(&t, C2D_WithColor, x, y-o, 0.0f, sx, sy, C2D_Color32(0,0,0,255));
    C2D_DrawText(&t, C2D_WithColor, x, y+o, 0.0f, sx, sy, C2D_Color32(0,0,0,255));
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.0f, sx, sy, color);
}

static void drawText(float x, float y, float sx, float sy, u32 color,
                     const char* fmt, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    C2D_Text t;
    C2D_TextParse(&t, textBuf, buffer);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.0f, sx, sy, color);
}





struct Theme
{
    u32 sky;
    u32 sky2;
    u32 mountain;
    u32 ground;
    u32 groundTop;
    u32 accent;
    u32 player;
    u32 playerDark;
    u32 coin;
};

static const Theme themes[] =
{
    { C2D_Color32(72, 150, 245, 255), C2D_Color32(145, 215, 255, 255),
      C2D_Color32(65, 110, 165, 255), C2D_Color32(65, 70, 85, 255),
      C2D_Color32(105, 180, 80, 255), C2D_Color32(255, 235, 70, 255),
      C2D_Color32(255, 95, 90, 255), C2D_Color32(170, 45, 55, 255),
      C2D_Color32(255, 205, 45, 255) },

    { C2D_Color32(40, 35, 80, 255), C2D_Color32(95, 75, 145, 255),
      C2D_Color32(50, 45, 85, 255), C2D_Color32(45, 42, 60, 255),
      C2D_Color32(120, 95, 190, 255), C2D_Color32(90, 230, 220, 255),
      C2D_Color32(100, 220, 255, 255), C2D_Color32(40, 110, 155, 255),
      C2D_Color32(255, 220, 75, 255) },

    { C2D_Color32(205, 235, 255, 255), C2D_Color32(245, 250, 255, 255),
      C2D_Color32(145, 185, 220, 255), C2D_Color32(80, 90, 105, 255),
      C2D_Color32(115, 185, 110, 255), C2D_Color32(65, 145, 255, 255),
      C2D_Color32(255, 115, 80, 255), C2D_Color32(175, 65, 45, 255),
      C2D_Color32(255, 190, 30, 255) }
};





struct Rect
{
    float x, y, w, h;
};

struct Coin
{
    float x, y;
    bool taken;
    float phase;
};

struct Enemy
{
    float x, y;
    float vx;
    float left;
    float right;
    bool alive;
};

struct Level
{
    int width;
    int height;
    vector<string> map;
    vector<Coin> coins;
    vector<Enemy> enemies;
    int spawnX;
    int spawnY;
    int goalX;
    int goalY;
    int theme;
    const char* name;
};

static vector<Level> levels;





struct Player
{
    float x, y;
    float vx, vy;
    float w, h;
    bool grounded;
    bool wasGrounded;
    int lives;
    int coins;
    int animation;
};

static Player player;

static int score = 0;
static int bestScore[LEVEL_COUNT] = {};
static int levelTime = 0;
static int combo = 0;
static int checkpointX = 0;
static int checkpointY = 0;
static bool checkpointActive = false;
static bool dashAvailable = true;
static bool dashActive = false;
static int dashFrames = 0;
static bool doubleJumpAvailable = true;

struct Particle
{
    float x;
    float y;
    float vx;
    float vy;
    float life;
    float size;
    u32 color;
};

static vector<Particle> particles;

static void spawnParticle(float x, float y, u32 color, int count)
{
    for (int i = 0; i < count; ++i)
    {
        Particle p;
        p.x = x;
        p.y = y;
        p.vx = ((i * 17) % 9 - 4) * 0.22f;
        p.vy = -((i * 13) % 7) * 0.16f;
        p.life = 18.0f + (i % 12);
        p.size = 1.5f + (i % 3);
        p.color = color;
        particles.push_back(p);
    }
}

static void updateParticles()
{
    for (size_t i = 0; i < particles.size();)
    {
        Particle& p = particles[i];
        p.x += p.vx;
        p.y += p.vy;
        p.vy += 0.035f;
        p.life -= 1.0f;

        if (p.life <= 0)
        {
            particles.erase(particles.begin() + i);
            continue;
        }
        ++i;
    }

    if (particles.size() > 180)
        particles.erase(particles.begin(), particles.begin() + 40);
}

static void drawParticles()
{
    for (const Particle& p : particles)
    {
        float a = std::max(0.0f, std::min(1.0f, p.life / 20.0f));
        u8 alpha = (u8)(255.0f * a);
        u32 c = (p.color & 0xFFFFFF00) | alpha;
        rect(p.x - cameraX, p.y - cameraY, p.size, p.size, c);
    }
}

static void activateCheckpoint()
{
    checkpointX = (int)player.x;
    checkpointY = (int)player.y;
    checkpointActive = true;
    spawnParticle(player.x, player.y, C2D_Color32(90,230,120,255), 18);
}

static void resetToCheckpoint()
{
    if (checkpointActive)
    {
        player.x = checkpointX;
        player.y = checkpointY;
        player.vx = 0;
        player.vy = 0;
    }
    else
    {
        resetPlayer();
    }

    player.grounded = false;
    dashAvailable = true;
    dashActive = false;
    dashFrames = 0;
    doubleJumpAvailable = true;
}

static void updateCheckpoint()
{
    const Level& l = levels[currentLevel];

    for (int y = 0; y < l.height; ++y)
    {
        for (int x = 0; x < l.width; ++x)
        {
            if (l.map[y][x] != '!')
                continue;

            float px = x * TILE;
            float py = y * TILE;

            if (intersects(player.x, player.y, player.w, player.h,
                           px, py, TILE, TILE))
            {
                if (!checkpointActive || checkpointX != (int)px)
                    activateCheckpoint();
            }
        }
    }
}

static void updateDash()
{
    u32 d = down();
    u32 k = held();

    if (!dashActive && dashAvailable && (d & KEY_R))
    {
        dashActive = true;
        dashAvailable = false;
        dashFrames = 10;

        if (k & KEY_LEFT)
            player.vx = -7.0f;
        else if (k & KEY_RIGHT)
            player.vx = 7.0f;
        else
            player.vx = player.vx >= 0 ? 7.0f : -7.0f;

        player.vy = 0;
        spawnParticle(player.x, player.y + 8,
                       C2D_Color32(100,220,255,255), 10);
    }

    if (dashActive)
    {
        dashFrames--;

        if ((frameCounter % 2) == 0)
            spawnParticle(player.x, player.y + 7,
                          C2D_Color32(100,220,255,255), 2);

        if (dashFrames <= 0)
            dashActive = false;
    }
}

static void updateDoubleJump()
{
    u32 d = down();

    if (player.grounded)
        doubleJumpAvailable = true;

    if ((d & KEY_A) || (d & KEY_B))
    {
        if (!player.grounded && doubleJumpAvailable)
        {
            player.vy = JUMP_VEL * 0.88f;
            doubleJumpAvailable = false;
            spawnParticle(player.x + 6, player.y + 14,
                          C2D_Color32(255,255,255,255), 8);
        }
    }
}

static void updateScore()
{
    if ((frameCounter % 60) == 0)
        levelTime++;

    if (player.coins > 0 && (frameCounter % 120) == 0)
        score += player.coins;

    if (score > bestScore[currentLevel])
        bestScore[currentLevel] = score;
}

static void drawScoreEffects()
{
    if (combo > 0)
    {
        drawOutlinedText(300, 210, 0.48f, 0.48f,
                         C2D_Color32(255,235,70,255),
                         "COMBO x%d", combo);
    }

    if (dashActive)
    {
        drawOutlinedText(315, 28, 0.43f, 0.43f,
                         C2D_Color32(100,220,255,255),
                         "DASH");
    }
}

static void drawWorldParticles()
{
    drawParticles();
}

static void updateAdvancedSystems()
{
    updateDash();
    updateDoubleJump();
    updateCheckpoint();
    updateParticles();
    updateScore();
}



static int currentLevel = 0;
static int unlockedLevel = 0;
static int gameState = 0;


static bool running = true;
static bool levelCompleted = false;
static bool levelRestartRequested = false;

static u64 frameCounter = 0;
static float cameraX = 0.0f;
static float cameraY = 0.0f;





static Level makeLevel(const char* name, int theme, const vector<string>& rows)
{
    Level l;
    l.name = name;
    l.theme = theme;
    l.height = (int)rows.size();
    l.width = rows.empty() ? 25 : (int)rows[0].size();
    l.map = rows;
    l.spawnX = 32;
    l.spawnY = 120;
    l.goalX = (l.width - 3) * TILE;
    l.goalY = (l.height - 3) * TILE;

    for (int y = 0; y < l.height; ++y)
    {
        for (int x = 0; x < l.width; ++x)
        {
            char c = l.map[y][x];

            if (c == 'S')
            {
                l.spawnX = x * TILE;
                l.spawnY = y * TILE;
                l.map[y][x] = '.';
            }
            else if (c == 'G')
            {
                l.goalX = x * TILE;
                l.goalY = y * TILE;
                l.map[y][x] = '.';
            }
            else if (c == 'C')
            {
                Coin coin;
                coin.x = x * TILE + 8;
                coin.y = y * TILE + 8;
                coin.taken = false;
                coin.phase = float((x * 17 + y * 9) % 100) / 10.0f;
                l.coins.push_back(coin);
                l.map[y][x] = '.';
            }
            else if (c == 'E')
            {
                Enemy e;
                e.x = x * TILE;
                e.y = y * TILE;
                e.vx = 0.65f;
                e.left = std::max(0, x - 4) * TILE;
                e.right = std::min(l.width - 1, x + 4) * TILE;
                e.alive = true;
                l.enemies.push_back(e);
                l.map[y][x] = '.';
            }
        }
    }

    return l;
}

static void buildLevels()
{
    levels.clear();

    levels.push_back(makeLevel("Green Hills", 0, {
        ".................................",
        ".................................",
        "......................C..........",
        "............C....................",
        "......###..............###.......",
        ".................................",
        "..............C.............G....",
        "..............###................",
        "..S.............................",
        "#######################..........",
        "###############################..",
        "################################"
    }));

    levels.push_back(makeLevel("First Steps", 0, {
        ".........................................",
        ".........................................",
        "........C..............C................",
        ".......###............###...............",
        "..S..................................G.",
        "#########.....####................######",
        "########################################",
        "########################################"
    }));

    levels.push_back(makeLevel("Enemy Alley", 0, {
        ".........................................",
        ".........................................",
        "..C..............C.................C...",
        ".........................................",
        "..S.......E........E........E.........G.",
        "#####....#####....#####....#####....####",
        "########################################",
        "########################################"
    }));

    levels.push_back(makeLevel("Sky Bridges", 1, {
        ".........................................",
        "....................C...................",
        "...........###................###.......",
        ".........................................",
        "..S...###........###....###...........G.",
        ".........................................",
        "#####.........#####........#####.........",
        "########################################",
        "########################################"
    }));

    levels.push_back(makeLevel("Night Run", 1, {
        ".........................................",
        "....C.................C................",
        ".........................................",
        "..S......E....###........E...........G.",
        "#######.......#####.......##############",
        "########################################",
        "########################################"
    }));

    levels.push_back(makeLevel("Moving Shadows", 1, {
        "................................................",
        "........C...............................C.....",
        "........###...............###..................",
        "..S................E.......................G..",
        "######.....#####........#####......###########",
        "##############################################"
    }));

    levels.push_back(makeLevel("Cold Peaks", 2, {
        "........................................",
        "..................C.....................",
        "...........###..............C...........",
        "..S..................................G.",
        "######....#####.....#####....##########",
        "########################################"
    }));

    levels.push_back(makeLevel("Peak Jump", 2, {
        "..............................................",
        "....C..............C.........................",
        "..S....###........###.......###............G.",
        "..............................................",
        "######......######......######......##########",
        "##############################################"
    }));

    levels.push_back(makeLevel("Long Road", 0, {
        "................................................................",
        "..C................C..................C.........................",
        "................................................................",
        "..S.......###..........E.......###.........................G....",
        "######.........#########.........###########....###############",
        "################################################################"
    }));

    levels.push_back(makeLevel("The Gauntlet", 1, {
        "........................................................",
        "..C......E.....C......E.....C......E....................",
        "........................................................",
        "..S...#####...#####...#####...#####...#####.........G..",
        "########################################################",
        "########################################################"
    }));

    levels.push_back(makeLevel("Tiny Platforms", 2, {
        "................................................",
        "........C...........C...........C..............",
        "..S..###....###....###....###....###.........G.",
        "................................................",
        "######....######....######....######....########",
        "################################################"
    }));

    levels.push_back(makeLevel("Speed Zone", 0, {
        ".........................................................",
        "C..................C...................C................",
        ".........................................................",
        "..S....###....E....###....E....###....E....###........G.",
        "########################################################",
        "########################################################"
    }));

    levels.push_back(makeLevel("Moon Base", 1, {
        "................................................",
        ".............C.............C..................",
        "..S......###.........###.........###..........G",
        "................................................",
        "#####..........#####..........#####..........##",
        "################################################"
    }));

    levels.push_back(makeLevel("Final Climb", 2, {
        "........................................................",
        "..C.......###........C.......###........C..............",
        "..S................................................G..",
        "######....######....######....######....#############",
        "######################################################",
        "######################################################"
    }));

    levels.push_back(makeLevel("Champion Road", 0, {
        "....................................................................",
        "..C.....C.....C.....C.....C.....C.....C.....C.......................",
        "....................................................................",
        "..S...###...E...###...E...###...E...###...E...###...............G..",
        "####################################################################",
        "####################################################################"
    }));
}





static bool solidAt(int tx, int ty)
{
    if (ty < 0 || ty >= levels[currentLevel].height ||
        tx < 0 || tx >= levels[currentLevel].width)
        return true;

    return levels[currentLevel].map[ty][tx] == '#';
}

static bool solidRect(const Rect& r)
{
    int left = (int)floor(r.x / TILE);
    int right = (int)floor((r.x + r.w - 0.01f) / TILE);
    int topY = (int)floor(r.y / TILE);
    int bottom = (int)floor((r.y + r.h - 0.01f) / TILE);

    for (int y = topY; y <= bottom; ++y)
        for (int x = left; x <= right; ++x)
            if (solidAt(x, y))
                return true;

    return false;
}

static Rect playerRect()
{
    return { player.x + 2, player.y + 1, player.w - 4, player.h - 2 };
}

static bool intersects(float ax, float ay, float aw, float ah,
                       float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx &&
           ay < by + bh && ay + ah > by;
}





static void resetPlayer()
{
    const Level& l = levels[currentLevel];

    player.x = (float)l.spawnX;
    player.y = (float)l.spawnY;
    player.vx = 0;
    player.vy = 0;
    player.w = 13;
    player.h = 15;
    player.grounded = false;
    player.wasGrounded = false;
    player.animation = 0;
    player.coins = 0;

    for (auto& c : levels[currentLevel].coins)
        c.taken = false;

    for (auto& e : levels[currentLevel].enemies)
    {
        e.alive = true;
        e.vx = std::abs(e.vx);
    }

    cameraX = 0;
    cameraY = 0;
    levelCompleted = false;
}

static void startLevel(int index)
{
    if (index < 0 || index >= LEVEL_COUNT)
        return;

    currentLevel = index;
    player.lives = 3;
    resetPlayer();
    gameState = 1;
}





static u32 held()
{
    return hidKeysHeld();
}

static u32 down()
{
    return hidKeysDown();
}





static void moveHorizontal(float amount)
{
    player.x += amount;

    Rect r = playerRect();

    if (!solidRect(r))
        return;

    if (amount > 0)
    {
        while (solidRect(playerRect()))
            player.x -= 0.5f;
        player.vx = 0;
    }
    else if (amount < 0)
    {
        while (solidRect(playerRect()))
            player.x += 0.5f;
        player.vx = 0;
    }
}

static void moveVertical(float amount)
{
    player.y += amount;
    Rect r = playerRect();

    if (!solidRect(r))
    {
        player.grounded = false;
        return;
    }

    if (amount > 0)
    {
        while (solidRect(playerRect()))
            player.y -= 0.5f;

        player.vy = 0;
        player.grounded = true;
    }
    else
    {
        while (solidRect(playerRect()))
            player.y += 0.5f;

        player.vy = 0;
    }
}

static void updateEnemies()
{
    for (auto& e : levels[currentLevel].enemies)
    {
        if (!e.alive)
            continue;

        e.x += e.vx;

        if (e.x < e.left)
        {
            e.x = e.left;
            e.vx = std::abs(e.vx);
        }

        if (e.x > e.right)
        {
            e.x = e.right;
            e.vx = -std::abs(e.vx);
        }

        if (intersects(player.x, player.y, player.w, player.h,
                       e.x + 2, e.y + 3, 12, 12))
        {
            if (player.vy > 1.0f && player.y + player.h < e.y + 10)
            {
                e.alive = false;
                player.vy = -4.0f;
            }
            else
            {
                player.lives--;

                if (player.lives <= 0)
                {
                    gameState = 5;
                }
                else
                {
                    resetPlayer();
                }
                return;
            }
        }
    }
}

static void collectCoins()
{
    for (auto& c : levels[currentLevel].coins)
    {
        if (c.taken)
            continue;

        if (intersects(player.x, player.y, player.w, player.h,
                       c.x - 5, c.y - 5, 10, 10))
        {
            c.taken = true;
            player.coins++;
        }
    }
}

static void checkGoal()
{
    const Level& l = levels[currentLevel];

    if (intersects(player.x, player.y, player.w, player.h,
                   l.goalX, l.goalY, 16, 32))
    {
        levelCompleted = true;
        if (currentLevel >= unlockedLevel && unlockedLevel < LEVEL_COUNT - 1)
            unlockedLevel = currentLevel + 1;

        gameState = 4;
    }
}

static void updatePlayer()
{
    u32 k = held();
    u32 d = down();

    bool left = k & KEY_LEFT;
    bool right = k & KEY_RIGHT;
    bool sprint = (k & KEY_X) || (k & KEY_Y);
    bool jump = (d & KEY_A) || (d & KEY_B);

    float target = 0;

    if (left) target = sprint ? -SPRINT_SPEED : -WALK_SPEED;
    if (right) target = sprint ? SPRINT_SPEED : WALK_SPEED;

    if (target != 0)
    {
        player.vx += (target - player.vx) * 0.35f;
        player.animation++;
    }
    else
    {
        player.vx *= 0.72f;
        if (std::abs(player.vx) < 0.05f)
            player.vx = 0;
    }

    if (jump && player.grounded)
    {
        player.vy = JUMP_VEL;
        player.grounded = false;
    }

    player.wasGrounded = player.grounded;

    moveHorizontal(player.vx);

    player.vy += GRAVITY;
    if (player.vy > MAX_FALL)
        player.vy = MAX_FALL;

    player.grounded = false;
    moveVertical(player.vy);

    collectCoins();
    updateEnemies();
    checkGoal();

    if (player.y > levels[currentLevel].height * TILE + 50)
    {
        player.lives--;

        if (player.lives <= 0)
            gameState = 5;
        else
            resetPlayer();
    }

    float targetCam = player.x - 145;
    float maxCam = levels[currentLevel].width * TILE - SCREEN_W;

    if (targetCam < 0) targetCam = 0;
    if (targetCam > maxCam) targetCam = (float)maxCam;

    cameraX += (targetCam - cameraX) * 0.12f;
}





static void drawBackground(const Theme& t)
{
    rect(0, 0, SCREEN_W, SCREEN_H, t.sky);

    for (int i = 0; i < 7; ++i)
    {
        float px = fmodf(i * 105.0f - cameraX * 0.12f, 470.0f) - 35;
        float ph = 45 + (i % 3) * 18;
        rect(px, 130 - ph, 110, ph + 70, t.sky2);
    }

    for (int i = 0; i < 9; ++i)
    {
        float px = fmodf(i * 82.0f - cameraX * 0.20f, 470.0f) - 35;
        float h = 30 + (i % 4) * 14;


        for (int s = 0; s < 8; ++s)
        {
            float yy = 175 - h + s * 5;
            float ww = 12 + s * 10;
            rect(px + 55 - ww / 2, yy, ww, 8, t.mountain);
        }
    }
}





static void drawTiles(const Theme& t)
{
    const Level& l = levels[currentLevel];

    int startX = std::max(0, (int)(cameraX / TILE) - 2);
    int endX = std::min(l.width, startX + 30);

    for (int y = 0; y < l.height; ++y)
    {
        for (int x = startX; x < endX; ++x)
        {
            if (l.map[y][x] != '#')
                continue;

            float sx = x * TILE - cameraX;
            float sy = y * TILE - cameraY;

            rect(sx, sy, TILE, TILE, t.ground);

            if (!solidAt(x, y - 1))
                rect(sx, sy, TILE, 3, t.groundTop);


            rect(sx + 2, sy + 6, 3, 2, C2D_Color32(255,255,255,25));
            rect(sx + 10, sy + 11, 2, 2, C2D_Color32(0,0,0,30));
        }
    }
}

static void drawGoal(const Theme& t)
{
    const Level& l = levels[currentLevel];

    float x = l.goalX - cameraX;
    float y = l.goalY - cameraY - 16;

    rect(x + 6, y, 3, 34, C2D_Color32(45,45,55,255));
    rect(x + 9, y + 1, 13, 9, t.accent);
    rect(x + 10, y + 2, 5, 3, C2D_Color32(255,255,255,130));
}

static void drawCoins(const Theme& t)
{
    for (const auto& c : levels[currentLevel].coins)
    {
        if (c.taken)
            continue;

        float bob = sinf((float)frameCounter * 0.08f + c.phase) * 2.0f;
        float x = c.x - cameraX;
        float y = c.y - cameraY + bob;

        rect(x - 5, y - 6, 10, 12, t.coin);
        rect(x - 3, y - 4, 6, 8, C2D_Color32(255,240,120,255));
        rect(x - 1, y - 4, 2, 8, C2D_Color32(255,180,20,255));
    }
}

static void drawEnemies()
{
    for (const auto& e : levels[currentLevel].enemies)
    {
        if (!e.alive)
            continue;

        float x = e.x - cameraX;
        float y = e.y - cameraY;

        rect(x + 2, y + 5, 12, 10, C2D_Color32(90,65,160,255));
        rect(x + 4, y + 2, 8, 6, C2D_Color32(130,90,200,255));
        rect(x + 4, y + 4, 3, 3, C2D_Color32(255,255,255,255));
        rect(x + 10, y + 4, 3, 3, C2D_Color32(255,255,255,255));
        rect(x + 5, y + 5, 2, 2, C2D_Color32(25,25,40,255));
        rect(x + 11, y + 5, 2, 2, C2D_Color32(25,25,40,255));
    }
}

static void drawPlayer(const Theme& t)
{
    float x = player.x - cameraX;
    float y = player.y - cameraY;

    bool moving = std::abs(player.vx) > 0.2f;
    int bob = moving ? ((player.animation / 5) % 2) : 0;


    rect(x + 1, y + 14, 12, 3, C2D_Color32(0,0,0,65));


    rect(x + 2, y + 5 - bob, 10, 10, t.player);
    rect(x + 3, y + 1 - bob, 8, 7, t.player);


    rect(x + 4, y + 4 - bob, 2, 2, C2D_Color32(255,255,255,255));
    rect(x + 9, y + 4 - bob, 2, 2, C2D_Color32(255,255,255,255));
    rect(x + 5, y + 5 - bob, 1, 1, C2D_Color32(25,25,35,255));
    rect(x + 9, y + 5 - bob, 1, 1, C2D_Color32(25,25,35,255));


    rect(x + 1, y + 14, 5, 2, t.playerDark);
    rect(x + 8, y + 14, 5, 2, t.playerDark);
}





static void drawHUD(const Theme& t)
{
    rect(0, 0, SCREEN_W, 24, C2D_Color32(20,25,35,205));

    drawOutlinedText(10, 5, 0.55f, 0.55f, C2D_Color32(255,255,255,255),
             "LEVEL %02d  %s", currentLevel + 1, levels[currentLevel].name);

    drawOutlinedText(260, 5, 0.52f, 0.52f, t.coin,
             "COINS %02d", player.coins);

    drawOutlinedText(340, 5, 0.52f, 0.52f, C2D_Color32(255,130,130,255),
             "HP %d", player.lives);


    if ((held() & KEY_X) || (held() & KEY_Y))
    {
        rect(8, 215, 65, 18, C2D_Color32(20,25,35,210));
        drawOutlinedText(14, 219, 0.45f, 0.45f,
                 C2D_Color32(255,255,255,255), "SPRINT!");
    }
}





static void drawTopFrame()
{
    const u32 black = C2D_Color32(0,0,0,255);
    C2D_DrawRectSolid(0, 0, 400, 3, black);
    C2D_DrawRectSolid(0, 237, 400, 3, black);
    C2D_DrawRectSolid(0, 0, 3, 240, black);
    C2D_DrawRectSolid(397, 0, 3, 240, black);
}

static void drawGame()
{
    const Theme& t = themes[levels[currentLevel].theme];

    C2D_TargetClear(top, t.sky);

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_SceneBegin(top);

    drawBackground(t);
    drawTiles(t);
    drawGoal(t);
    drawCoins(t);
    drawEnemies();
    drawWorldParticles();
    drawPlayer(t);
    drawHUD(t);
    drawScoreEffects();
    drawExtraHUD();
    drawTopFrame();

    C3D_FrameEnd(0);
}





static void bottomPanel(u32 color = C2D_Color32(25,28,38,255))
{
    C2D_TargetClear(bottom, color);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_SceneBegin(bottom);
}

static void bottomEnd()
{
    C3D_FrameEnd(0);
}

static void drawButton(float x, float y, float w, float h,
                       bool selected, const char* label)
{
    u32 bg = selected
        ? C2D_Color32(70,105,190,255)
        : C2D_Color32(45,50,68,255);

    rect(x, y, w, h, bg);

    if (selected)
        rect(x, y, 4, h, C2D_Color32(255,225,75,255));

    drawText(x + 10, y + 9, 0.58f, 0.58f,
             C2D_Color32(255,255,255,255), "%s", label);
}





static int menuSelection = 0;

static void updateMenu()
{
    u32 d = down();

    if (d & KEY_DOWN)
        menuSelection = (menuSelection + 1) % 3;

    if (d & KEY_UP)
        menuSelection = (menuSelection + 2) % 3;

    if (d & KEY_A)
    {
        if (menuSelection == 0)
            startLevel(0);
        else if (menuSelection == 1)
            gameState = 2;
        else
            running = false;
    }
}

static void drawMenu()
{
    bottomPanel(C2D_Color32(18,22,34,255));


    rect(0, 0, 320, 240, C2D_Color32(28,38,65,255));
    rect(0, 0, 320, 7, C2D_Color32(90,180,255,255));

    drawText(36, 30, 1.25f, 1.25f,
             C2D_Color32(255,235,75,255), "PIXEL");
    drawText(36, 58, 1.25f, 1.25f,
             C2D_Color32(100,220,255,255), "RUNNER");

    drawText(38, 91, 0.48f, 0.48f,
             C2D_Color32(205,215,235,255),
             "A small 3DS platform adventure");

    drawButton(35, 122, 250, 28, menuSelection == 0, "START GAME");
    drawButton(35, 155, 250, 28, menuSelection == 1, "LEVEL SELECT");
    drawButton(35, 188, 250, 28, menuSelection == 2, "QUIT");

    drawText(40, 222, 0.40f, 0.40f,
             C2D_Color32(170,180,200,255),
             "A/B jump   X/Y sprint   D-Pad move");

    bottomEnd();
}





static int levelCursor = 0;

static void updateLevelSelect()
{
    u32 d = down();

    if (d & KEY_RIGHT)
        levelCursor = std::min(levelCursor + 1, unlockedLevel);

    if (d & KEY_LEFT)
        levelCursor = std::max(levelCursor - 1, 0);

    if (d & KEY_DOWN)
        levelCursor = std::min(levelCursor + 5, unlockedLevel);

    if (d & KEY_UP)
        levelCursor = std::max(levelCursor - 5, 0);

    if (d & KEY_B)
    {
        gameState = 0;
        return;
    }

    if ((d & KEY_A) && levelCursor <= unlockedLevel)
        startLevel(levelCursor);
}

static void drawLevelSelect()
{
    bottomPanel(C2D_Color32(17,20,30,255));

    drawText(18, 12, 0.85f, 0.85f,
             C2D_Color32(255,235,75,255), "LEVEL SELECT");

    drawText(19, 38, 0.43f, 0.43f,
             C2D_Color32(175,185,205,255),
             "Choose a level - unlocked: %d/%d",
             unlockedLevel + 1, LEVEL_COUNT);

    for (int i = 0; i < LEVEL_COUNT; ++i)
    {
        int col = i % 5;
        int row = i / 5;

        float x = 15 + col * 60;
        float y = 62 + row * 48;

        bool unlocked = i <= unlockedLevel;
        bool selected = i == levelCursor;

        u32 bg;

        if (!unlocked)
            bg = C2D_Color32(38,40,48,255);
        else if (selected)
            bg = C2D_Color32(70,105,190,255);
        else
            bg = C2D_Color32(45,50,65,255);

        rect(x, y, 50, 36, bg);

        if (selected)
            rect(x, y, 50, 3, C2D_Color32(255,225,75,255));

        if (unlocked)
        {
            drawText(x + 15, y + 8, 0.75f, 0.75f,
                     C2D_Color32(255,255,255,255),
                     "%02d", i + 1);
            drawText(x + 5, y + 26, 0.33f, 0.33f,
                     C2D_Color32(190,205,225,255),
                     "%s", levels[i].name);
        }
        else
        {
            drawText(x + 18, y + 7, 0.75f, 0.75f,
                     C2D_Color32(115,120,135,255), "LOCK");
        }
    }

    drawText(18, 218, 0.43f, 0.43f,
             C2D_Color32(170,180,200,255),
             "D-Pad select   A play   B back");

    bottomEnd();
}





static int pauseSelection = 0;

static void updatePause()
{
    u32 d = down();

    if (d & KEY_UP)
        pauseSelection = (pauseSelection + 2) % 3;

    if (d & KEY_DOWN)
        pauseSelection = (pauseSelection + 1) % 3;

    if (d & KEY_B)
    {
        gameState = 1;
        return;
    }

    if (d & KEY_A)
    {
        if (pauseSelection == 0)
            gameState = 1;
        else if (pauseSelection == 1)
        {
            resetPlayer();
            gameState = 1;
        }
        else
            gameState = 0;
    }
}

static void drawPause()
{
    bottomPanel(C2D_Color32(20,23,34,255));

    drawText(95, 25, 1.0f, 1.0f,
             C2D_Color32(255,235,75,255), "PAUSED");

    drawButton(55, 70, 210, 30, pauseSelection == 0, "RESUME");
    drawButton(55, 108, 210, 30, pauseSelection == 1, "RESTART");
    drawButton(55, 146, 210, 30, pauseSelection == 2, "MAIN MENU");

    drawText(72, 199, 0.45f, 0.45f,
             C2D_Color32(170,180,200,255),
             "START also resumes");

    bottomEnd();
}





static void updateWin()
{
    u32 d = down();

    if (d & KEY_A)
    {
        if (currentLevel < LEVEL_COUNT - 1)
            startLevel(currentLevel + 1);
        else
            gameState = 0;
    }

    if (d & KEY_B)
        gameState = 2;
}

static void drawWin()
{
    bottomPanel(C2D_Color32(20,28,38,255));

    drawText(72, 28, 1.05f, 1.05f,
             C2D_Color32(255,235,75,255), "LEVEL CLEAR!");

    drawText(75, 70, 0.60f, 0.60f,
             C2D_Color32(235,240,250,255),
             "%s", levels[currentLevel].name);

    drawText(75, 102, 0.55f, 0.55f,
             C2D_Color32(255,215,60,255),
             "Coins collected: %d", player.coins);

    if (currentLevel < LEVEL_COUNT - 1)
    {
        drawText(75, 145, 0.52f, 0.52f,
                 C2D_Color32(100,220,255,255),
                 "Next level unlocked!");

        drawButton(75, 174, 170, 28, true, "A  NEXT LEVEL");
    }
    else
    {
        drawText(70, 145, 0.60f, 0.60f,
                 C2D_Color32(255,235,75,255),
                 "ALL 15 LEVELS CLEAR!");

        drawButton(75, 174, 170, 28, true, "A  MAIN MENU");
    }

    bottomEnd();
}

static void updateGameOver()
{
    u32 d = down();

    if (d & KEY_A)
    {
        startLevel(currentLevel);
    }

    if (d & KEY_B)
    {
        gameState = 0;
    }
}

static void drawGameOver()
{
    bottomPanel(C2D_Color32(35,20,28,255));

    drawText(78, 42, 1.0f, 1.0f,
             C2D_Color32(255,105,105,255), "GAME OVER");

    drawText(70, 84, 0.52f, 0.52f,
             C2D_Color32(220,225,235,255),
             "You reached level %02d", currentLevel + 1);

    drawButton(65, 125, 190, 30, true, "A  TRY AGAIN");

    drawButton(65, 165, 190, 30, false, "B  MAIN MENU");

    bottomEnd();
}





static void handleGlobalInput()
{
    u32 d = down();

    if (gameState == 1 && (d & KEY_START))
        gameState = 3;
    else if (gameState == 3 && (d & KEY_START))
        gameState = 1;
}





static bool initGraphics()
{
    gfxInitDefault();

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    systemFont = C2D_FontLoadSystem(CFG_REGION_USA_EU);
    textBuf = C2D_TextBufNew(32768);

    return top != nullptr && bottom != nullptr;
}

static void shutdownGraphics()
{
    if (textBuf)
        C2D_TextBufDelete(textBuf);

    if (systemFont)
        C2D_FontFree(systemFont);

    C2D_Fini();
    C3D_Fini();
    gfxExit();
}






static float clampFloat(float v, float lo, float hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static int clampInt(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static float approach(float current, float target, float amount)
{
    if (current < target)
        return std::min(current + amount, target);
    if (current > target)
        return std::max(current - amount, target);
    return current;
}

static bool pointInRect(float px, float py, const Rect& r)
{
    return px >= r.x && px <= r.x + r.w &&
           py >= r.y && py <= r.y + r.h;
}

static bool rectInside(float x, float y, float w, float h,
                       float bx, float by, float bw, float bh)
{
    return x >= bx && y >= by &&
           x + w <= bx + bw &&
           y + h <= by + bh;
}

static float distance2(float ax, float ay, float bx, float by)
{
    float dx = ax - bx;
    float dy = ay - by;
    return dx * dx + dy * dy;
}

static float distanceValue(float ax, float ay, float bx, float by)
{
    return sqrtf(distance2(ax, ay, bx, by));
}

static u32 withAlpha(u32 color, u8 alpha)
{
    return (color & 0xFFFFFF00) | alpha;
}

static u32 brighten(u32 color, int amount)
{
    int r = ((color >> 24) & 255) + amount;
    int g = ((color >> 16) & 255) + amount;
    int b = ((color >> 8) & 255) + amount;

    r = clampInt(r, 0, 255);
    g = clampInt(g, 0, 255);
    b = clampInt(b, 0, 255);

    return C2D_Color32(r, g, b, color & 255);
}

static u32 darken(u32 color, int amount)
{
    return brighten(color, -amount);
}

static void drawPanel(float x, float y, float w, float h, u32 fill)
{
    rect(x, y, w, h, fill);
}

static void drawSeparator(float x, float y, float w)
{
    line(x, y, x + w, y, 1.0f, C2D_Color32(0,0,0,255));
}

static void drawCorner(float x, float y, float size, u32 color)
{
    rect(x, y, size, 2, color);
    rect(x, y, 2, size, color);
}

static void drawCross(float x, float y, float size, u32 color)
{
    rect(x - size, y - 1, size * 2, 2, color);
    rect(x - 1, y - size, 2, size * 2, color);
}

static void drawHealthBar(float x, float y, float w, float h,
                          int value, int maximum)
{
    value = clampInt(value, 0, maximum);
    rect(x, y, w, h, C2D_Color32(30,30,40,255));

    if (maximum > 0)
    {
        float amount = w * ((float)value / (float)maximum);
        if (amount > 0)
            rect(x, y, amount, h, C2D_Color32(245,75,90,255));
    }
}

static void drawProgressBar(float x, float y, float w, float h,
                            float progress, u32 fill)
{
    progress = clampFloat(progress, 0.0f, 1.0f);
    rect(x, y, w, h, C2D_Color32(25,28,38,255));
    if (progress > 0)
        rect(x, y, w * progress, h, fill);
}

static void drawIconCoin(float x, float y, u32 color)
{
    rect(x, y, 9, 11, color);
    rect(x + 2, y + 2, 5, 7, brighten(color, 25));
}

static void drawIconHeart(float x, float y, u32 color)
{
    rect(x + 1, y, 4, 3, color);
    rect(x + 5, y, 4, 3, color);
    rect(x, y + 2, 10, 5, color);
    rect(x + 2, y + 6, 6, 3, color);
    rect(x + 4, y + 9, 2, 2, color);
}

static void drawIconArrow(float x, float y, bool right, u32 color)
{
    if (right)
    {
        rect(x, y + 4, 8, 3, color);
        rect(x + 6, y + 2, 3, 7, color);
    }
    else
    {
        rect(x + 1, y + 4, 8, 3, color);
        rect(x, y + 2, 3, 7, color);
    }
}

static void drawStar(float cx, float cy, float size, u32 color)
{
    rect(cx - 1, cy - size, 2, size * 2, color);
    rect(cx - size, cy - 1, size * 2, 2, color);
    rect(cx - size * 0.6f, cy - size * 0.6f,
         size * 1.2f, 2, color);
    rect(cx - size * 0.6f, cy + size * 0.6f,
         size * 1.2f, 2, color);
}

static void drawCrosshair(float cx, float cy, float size, u32 color)
{
    line(cx - size, cy, cx - 2, cy, 1.0f, color);
    line(cx + 2, cy, cx + size, cy, 1.0f, color);
    line(cx, cy - size, cx, cy - 2, 1.0f, color);
    line(cx, cy + 2, cx, cy + size, 1.0f, color);
}

static void addScore(int amount)
{
    score += amount;
    if (score < 0)
        score = 0;
}

static void addCombo()
{
    combo++;
    addScore(25 * combo);
}

static void clearCombo()
{
    combo = 0;
}

static int remainingCoins()
{
    int result = 0;
    for (const Coin& c : levels[currentLevel].coins)
        if (!c.taken)
            result++;
    return result;
}

static int defeatedEnemies()
{
    int result = 0;
    for (const Enemy& e : levels[currentLevel].enemies)
        if (!e.alive)
            result++;
    return result;
}

static int totalEnemies()
{
    return (int)levels[currentLevel].enemies.size();
}

static float levelCompletion()
{
    int total = (int)levels[currentLevel].coins.size();
    if (total == 0)
        return 1.0f;

    int got = total - remainingCoins();
    return (float)got / (float)total;
}

static void updateCamera()
{
    float target = player.x - 145.0f;
    float maxCamera = levels[currentLevel].width * TILE - SCREEN_W;

    target = clampFloat(target, 0.0f, (float)std::max(0, maxCamera));
    cameraX = approach(cameraX, target, 1.8f);
}

static void updateAnimation()
{
    if (std::abs(player.vx) > 0.2f)
        player.animation++;
    else
        player.animation = 0;
}

static void createLandingEffect()
{
    if (player.grounded && !player.wasGrounded)
        spawnParticle(player.x + player.w * 0.5f,
                      player.y + player.h,
                      C2D_Color32(230,240,255,255), 6);
}

static void createRunEffect()
{
    if (std::abs(player.vx) > 2.5f && player.grounded &&
        (frameCounter % 6) == 0)
    {
        spawnParticle(player.x + player.w * 0.5f,
                      player.y + player.h,
                      C2D_Color32(220,225,230,150), 1);
    }
}

static void updateEffects()
{
    createLandingEffect();
    createRunEffect();
    updateAnimation();
    updateCamera();
}

static void drawLevelProgress()
{
    float progress = levels[currentLevel].width > 0
        ? player.x / (levels[currentLevel].width * TILE)
        : 0.0f;

    drawProgressBar(120, 28, 150, 4, progress,
                    C2D_Color32(255,225,70,255));
}

static void drawTopDecor()
{
    drawLevelProgress();

    if (checkpointActive)
    {
        drawOutlinedText(10, 28, 0.40f, 0.40f,
                         C2D_Color32(90,230,120,255),
                         "CHECKPOINT");
    }
}

static void drawExtraHUD()
{
    drawTopDecor();
}

static void runExtraGameplayLogic()
{
    updateEffects();
}



static bool levelHasCoin(int index)
{
    if (index < 0 || index >= (int)levels.size())
        return false;
    return !levels[index].coins.empty();
}

static bool levelHasEnemies(int index)
{
    if (index < 0 || index >= (int)levels.size())
        return false;
    return !levels[index].enemies.empty();
}

static int levelWidthPixels(int index)
{
    if (index < 0 || index >= (int)levels.size())
        return 0;
    return levels[index].width * TILE;
}

static int levelHeightPixels(int index)
{
    if (index < 0 || index >= (int)levels.size())
        return 0;
    return levels[index].height * TILE;
}

static bool isLevelUnlocked(int index)
{
    return index >= 0 && index <= unlockedLevel && index < LEVEL_COUNT;
}

static void selectPreviousLevel()
{
    if (levelCursor > 0)
        levelCursor--;
}

static void selectNextLevel()
{
    if (levelCursor < unlockedLevel)
        levelCursor++;
}

static void selectPreviousRow()
{
    levelCursor = std::max(0, levelCursor - 5);
}

static void selectNextRow()
{
    levelCursor = std::min(unlockedLevel, levelCursor + 5);
}

static void unlockNextLevel()
{
    if (unlockedLevel < LEVEL_COUNT - 1)
        unlockedLevel++;
}

static void addCoinScore()
{
    addScore(100);
    addCombo();
}

static void addEnemyScore()
{
    addScore(150);
    addCombo();
}

static void loseCombo()
{
    if (combo > 0)
        combo--;
}

static void resetScoreForLevel()
{
    score = 0;
    combo = 0;
    levelTime = 0;
}

static void prepareLevelSystems()
{
    checkpointActive = false;
    dashAvailable = true;
    dashActive = false;
    dashFrames = 0;
    doubleJumpAvailable = true;
    particles.clear();
    resetScoreForLevel();
}

static void finishLevelSystems()
{
    if (score > bestScore[currentLevel])
        bestScore[currentLevel] = score;
}

static void animateCoin(Coin& c)
{
    c.phase += 0.01f;
    if (c.phase > 100.0f)
        c.phase -= 100.0f;
}

static void animateAllCoins()
{
    for (Coin& c : levels[currentLevel].coins)
        if (!c.taken)
            animateCoin(c);
}

static void updateEnemySpeed(Enemy& e)
{
    float speed = 0.55f + currentLevel * 0.025f;
    if (speed > 1.25f)
        speed = 1.25f;

    if (e.vx > 0)
        e.vx = speed;
    else
        e.vx = -speed;
}

static void tuneEnemies()
{
    for (Enemy& e : levels[currentLevel].enemies)
        if (e.alive)
            updateEnemySpeed(e);
}

static bool nearGoal()
{
    const Level& l = levels[currentLevel];
    return distanceValue(player.x, player.y, l.goalX, l.goalY) < 80.0f;
}

static void goalEffect()
{
    if (nearGoal() && (frameCounter % 8) == 0)
        spawnParticle(levels[currentLevel].goalX,
                      levels[currentLevel].goalY,
                      C2D_Color32(255,235,75,255), 1);
}

static void updateWorldEffects()
{
    animateAllCoins();
    tuneEnemies();
    goalEffect();
}

int main()
{
    if (!initGraphics())
        return 1;

    buildLevels();
    startLevel(0);
    gameState = 0;

    while (aptMainLoop() && running)
    {
        hidScanInput();

        handleGlobalInput();

        if (gameState == 0)
            updateMenu();
        else if (gameState == 1)
        {
            updatePlayer();
            updateAdvancedSystems();
            runExtraGameplayLogic();
            updateWorldEffects();
        }
        else if (gameState == 2)
            updateLevelSelect();
        else if (gameState == 3)
            updatePause();
        else if (gameState == 4)
            updateWin();
        else if (gameState == 5)
            updateGameOver();

        if (gameState == 0)
            drawMenu();
        else if (gameState == 1)
        {
            drawGame();


            bottomPanel(C2D_Color32(22,26,37,255));

            drawText(18, 14, 0.78f, 0.78f,
                     C2D_Color32(255,235,75,255),
                     "%s", levels[currentLevel].name);

            drawText(18, 48, 0.50f, 0.50f,
                     C2D_Color32(210,220,235,255),
                     "MOVE");

            drawText(18, 72, 0.46f, 0.46f,
                     C2D_Color32(175,185,205,255),
                     "LEFT / RIGHT");

            drawText(170, 48, 0.50f, 0.50f,
                     C2D_Color32(210,220,235,255),
                     "JUMP");

            drawText(170, 72, 0.46f, 0.46f,
                     C2D_Color32(175,185,205,255),
                     "A / B");

            drawText(18, 112, 0.50f, 0.50f,
                     C2D_Color32(210,220,235,255),
                     "SPRINT");

            drawText(18, 136, 0.46f, 0.46f,
                     C2D_Color32(175,185,205,255),
                     "X / Y");

            drawText(170, 112, 0.50f, 0.50f,
                     C2D_Color32(210,220,235,255),
                     "PAUSE");

            drawText(170, 136, 0.46f, 0.46f,
                     C2D_Color32(175,185,205,255),
                     "START");

            drawText(18, 180, 0.46f, 0.46f,
                     C2D_Color32(130,145,170,255),
                     "Reach the flag to finish the level.");

            drawText(18, 203, 0.46f, 0.46f,
                     C2D_Color32(130,145,170,255),
                     "Collect coins and avoid enemies.");

            drawText(18, 224, 0.38f, 0.38f,
                     C2D_Color32(110,130,160,255),
                     "R = DASH   double jump enabled");

            bottomEnd();
        }
        else if (gameState == 2)
            drawLevelSelect();
        else if (gameState == 3)
            drawPause();
        else if (gameState == 4)
            drawWin();
        else if (gameState == 5)
            drawGameOver();

        frameCounter++;
        gspWaitForVBlank();
    }

    shutdownGraphics();
    return 0;
}
