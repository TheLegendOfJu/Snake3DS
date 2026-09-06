#include <3ds.h>
#include <citro2d.h>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <memory>

using std::vector;
using std::string;
using std::shared_ptr;
using std::make_shared;

static C3D_RenderTarget* top = nullptr;
static C3D_RenderTarget* bottom = nullptr;

static const int SCREEN_W = 400;
static const int SCREEN_H = 240;
static const int BOTTOM_W = 320;
static const int BOTTOM_H = 240;
static const int TILE = 16;

static const float GRAVITY = 0.42f;
static const float JUMP_VEL = -6.8f;
static const float WALK_SPEED = 1.8f;
static const float SPRINT_SPEED = 3.25f;
static const float MAX_FALL = 8.0f;

static const int CAM_BOTTOM_OFFSET = 32;
static const int CAM_LERP_FACTOR = 2;

class XorShift32 {
    uint32_t state;
public:
    XorShift32(uint32_t seed) : state(seed == 0 ? 123456789 : seed) {}
    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    int nextInt(int min, int max) {
        if (min >= max) return min;
        return min + (next() % (max - min + 1));
    }
    float nextFloat() {
        return (float)next() / (float)UINT32_MAX;
    }
    bool chance(float probability) {
        return nextFloat() <= probability;
    }
};

class InputManager {
public:
    u32 current;
    u32 previous;

    void update() {
        hidScanInput();
        previous = current;
        current = hidKeysHeld();
    }

    bool isHeld(u32 key) const { return (current & key) != 0; }
    bool isPressed(u32 key) const { return ((current & key) != 0) && ((previous & key) == 0); }
    bool isReleased(u32 key) const { return ((current & key) == 0) && ((previous & key) != 0); }
};

static InputManager input;

static void rect(float x, float y, float w, float h, u32 color)
{
    C2D_DrawRectSolid(x, y, 0.0f, w, h, color);
    C2D_DrawRectSolid(x, y, 0.0f, w, 1.5f, C2D_Color32(0,0,0,255));
    C2D_DrawRectSolid(x, y + h - 1.5f, 0.0f, w, 1.5f, C2D_Color32(0,0,0,255));
    C2D_DrawRectSolid(x, y, 0.0f, 1.5f, h, C2D_Color32(0,0,0,255));
    C2D_DrawRectSolid(x + w - 1.5f, y, 0.0f, 1.5f, h, C2D_Color32(0,0,0,255));
}

static void rectSolid(float x, float y, float w, float h, u32 color) {
    C2D_DrawRectSolid(x, y, 0.0f, w, h, color);
}

static void line(float x1, float y1, float x2, float y2, float width, u32 color)
{
    C2D_DrawLine(x1, y1, x2, y2, color, color, width, 0.0f);
}

static C2D_Font systemFont;
static C2D_TextBuf textBuf;

static void drawOutlinedText(float x, float y, float sx, float sy, u32 color, const char* fmt, ...)
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

static void drawShadowText(float x, float y, float sx, float sy, u32 color, const char* fmt, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    C2D_Text t;
    C2D_TextParse(&t, textBuf, buffer);
    C2D_TextOptimize(&t);

    C2D_DrawText(&t, C2D_WithColor, x+2, y+2, 0.0f, sx, sy, C2D_Color32(0,0,0,180));
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.0f, sx, sy, color);
}

static void drawText(float x, float y, float sx, float sy, u32 color, const char* fmt, ...)
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
    u32 mountain2;
    u32 ground;
    u32 groundTop;
    u32 accent;
    u32 player;
    u32 playerDark;
    u32 coin;
    u32 decoration;
    int weatherType; // 0=None, 1=Snow, 2=Rain, 3=Ash
};

static const vector<Theme> themes =
{
    // 0: Classic Green
    { C2D_Color32(72, 150, 245, 255), C2D_Color32(145, 215, 255, 255), C2D_Color32(65, 110, 165, 255), C2D_Color32(50, 90, 140, 255), C2D_Color32(65, 70, 85, 255), C2D_Color32(105, 180, 80, 255), C2D_Color32(255, 235, 70, 255), C2D_Color32(255, 95, 90, 255), C2D_Color32(170, 45, 55, 255), C2D_Color32(255, 205, 45, 255), C2D_Color32(80, 160, 60, 255), 0 },
    // 1: Midnight
    { C2D_Color32(40, 35, 80, 255), C2D_Color32(95, 75, 145, 255), C2D_Color32(50, 45, 85, 255), C2D_Color32(35, 30, 65, 255), C2D_Color32(45, 42, 60, 255), C2D_Color32(120, 95, 190, 255), C2D_Color32(90, 230, 220, 255), C2D_Color32(100, 220, 255, 255), C2D_Color32(40, 110, 155, 255), C2D_Color32(255, 220, 75, 255), C2D_Color32(80, 70, 150, 255), 0 },
    // 2: Winter
    { C2D_Color32(205, 235, 255, 255), C2D_Color32(245, 250, 255, 255), C2D_Color32(145, 185, 220, 255), C2D_Color32(120, 160, 200, 255), C2D_Color32(80, 90, 105, 255), C2D_Color32(220, 240, 255, 255), C2D_Color32(65, 145, 255, 255), C2D_Color32(255, 115, 80, 255), C2D_Color32(175, 65, 45, 255), C2D_Color32(255, 190, 30, 255), C2D_Color32(200, 230, 250, 255), 1 },
    // 3: Autumn
    { C2D_Color32(210, 130, 70, 255), C2D_Color32(245, 180, 110, 255), C2D_Color32(150, 80, 50, 255), C2D_Color32(120, 60, 30, 255), C2D_Color32(70, 50, 40, 255), C2D_Color32(200, 90, 40, 255), C2D_Color32(255, 200, 50, 255), C2D_Color32(80, 180, 255, 255), C2D_Color32(40, 90, 150, 255), C2D_Color32(255, 220, 80, 255), C2D_Color32(180, 70, 30, 255), 0 },
    // 4: Volcano
    { C2D_Color32(120, 30, 20, 255), C2D_Color32(180, 60, 30, 255), C2D_Color32(80, 20, 15, 255), C2D_Color32(50, 10, 10, 255), C2D_Color32(40, 15, 15, 255), C2D_Color32(90, 30, 25, 255), C2D_Color32(255, 120, 40, 255), C2D_Color32(200, 200, 200, 255), C2D_Color32(100, 100, 100, 255), C2D_Color32(255, 180, 50, 255), C2D_Color32(70, 20, 15, 255), 3 },
    // 5: Cyber
    { C2D_Color32(10, 15, 30, 255), C2D_Color32(20, 30, 60, 255), C2D_Color32(30, 45, 90, 255), C2D_Color32(20, 30, 70, 255), C2D_Color32(15, 15, 25, 255), C2D_Color32(0, 255, 128, 255), C2D_Color32(255, 0, 128, 255), C2D_Color32(255, 255, 255, 255), C2D_Color32(150, 150, 150, 255), C2D_Color32(0, 200, 255, 255), C2D_Color32(0, 180, 100, 255), 0 },
    // 6: Rainstorm
    { C2D_Color32(40, 50, 60, 255), C2D_Color32(60, 75, 90, 255), C2D_Color32(30, 40, 50, 255), C2D_Color32(20, 25, 35, 255), C2D_Color32(35, 45, 55, 255), C2D_Color32(50, 90, 60, 255), C2D_Color32(100, 180, 255, 255), C2D_Color32(255, 200, 100, 255), C2D_Color32(180, 120, 50, 255), C2D_Color32(255, 255, 150, 255), C2D_Color32(40, 80, 50, 255), 2 },
    // 7: Desert
    { C2D_Color32(255, 200, 120, 255), C2D_Color32(255, 230, 180, 255), C2D_Color32(200, 140, 80, 255), C2D_Color32(160, 100, 50, 255), C2D_Color32(180, 150, 100, 255), C2D_Color32(230, 200, 130, 255), C2D_Color32(255, 100, 50, 255), C2D_Color32(50, 150, 255, 255), C2D_Color32(30, 90, 180, 255), C2D_Color32(255, 255, 255, 255), C2D_Color32(200, 180, 100, 255), 0 },
    // 8: Toxic Sewers
    { C2D_Color32(20, 40, 20, 255), C2D_Color32(30, 70, 30, 255), C2D_Color32(15, 30, 15, 255), C2D_Color32(10, 20, 10, 255), C2D_Color32(30, 35, 30, 255), C2D_Color32(100, 200, 50, 255), C2D_Color32(200, 255, 100, 255), C2D_Color32(255, 150, 50, 255), C2D_Color32(180, 80, 20, 255), C2D_Color32(50, 255, 50, 255), C2D_Color32(60, 120, 40, 255), 0 },
    // 9: Candy Land
    { C2D_Color32(255, 180, 220, 255), C2D_Color32(255, 220, 240, 255), C2D_Color32(220, 120, 180, 255), C2D_Color32(180, 90, 140, 255), C2D_Color32(150, 80, 120, 255), C2D_Color32(255, 150, 200, 255), C2D_Color32(100, 255, 200, 255), C2D_Color32(50, 180, 255, 255), C2D_Color32(30, 120, 180, 255), C2D_Color32(255, 255, 100, 255), C2D_Color32(255, 100, 150, 255), 0 }
};

struct Rect { float x, y, w, h; };

struct Coin {
    float x, y;
    bool taken;
    float phase;
};

enum EnemyType {
    ENEMY_WALKER,
    ENEMY_FLYER,
    ENEMY_SHOOTER,
    ENEMY_SPIN
};

struct Enemy {
    float x, y;
    float vx, vy;
    float startX, startY;
    float left, right;
    bool alive;
    EnemyType type;
    float timer;
};

struct Projectile {
    float x, y;
    float vx, vy;
    bool active;
    int life;
};

struct MovingPlatform {
    float x, y, w, h;
    float startX, startY;
    float endX, endY;
    float speed;
    float phase;
    bool active;
};

struct Hazard {
    float x, y, w, h;
    int type; // 0=Spikes, 1=Lava
};

struct Platform {
    float x, y, w, h;
    bool top;
};

struct Level {
    int width;
    int height;
    vector<string> map;
    vector<Coin> coins;
    vector<Enemy> enemies;
    vector<MovingPlatform> movPlatforms;
    vector<Hazard> hazards;
    int spawnX;
    int spawnY;
    int goalX;
    int goalY;
    int theme;
    const char* name;
    bool isProcedural;
};

static vector<Level> levels;
static vector<Projectile> activeProjectiles;

enum ChunkType {
    START_CHUNK,
    END_CHUNK,
    FLAT_CHUNK,
    UP_CHUNK,
    DOWN_CHUNK,
    GAP_CHUNK,
    OBSTACLE_CHUNK,
    HAZARD_CHUNK,
    PUZZLE_CHUNK,
    VERTICAL_CHUNK
};

struct ChunkTemplate {
    int id;
    ChunkType type;
    int width;
    int height;
    int entranceY;
    int exitY;
    vector<string> layout;
};

static vector<ChunkTemplate> templateDB;

static void registerChunk(int id, ChunkType type, int entrance, int exit, const vector<string>& layout) {
    ChunkTemplate ct;
    ct.id = id;
    ct.type = type;
    ct.width = layout.empty() ? 0 : layout[0].size();
    ct.height = layout.size();
    ct.entranceY = entrance;
    ct.exitY = exit;
    ct.layout = layout;
    templateDB.push_back(ct);
}

static void initTemplates() {
    templateDB.clear();

    // START CHUNKS
    registerChunk(100, START_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "..S.............",
        "################",
        "################"
    });

    registerChunk(101, START_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "..S........C....",
        "################",
        "################"
    });

    // FLAT CHUNKS
    registerChunk(200, FLAT_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "################",
        "################"
    });

    registerChunk(201, FLAT_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "......C.........",
        "................",
        "................",
        "...........W....", // W = Walker
        "################",
        "################"
    });
    
    registerChunk(202, FLAT_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "...C...C...C....",
        "................",
        ".###..###..###..",
        "................",
        "................",
        "................",
        "################",
        "################"
    });

    registerChunk(203, FLAT_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "......C.........",
        "......#.........",
        "................",
        ".........#......",
        "................",
        "....F...........", // F = Flyer
        "................",
        "................",
        "################",
        "################"
    });

    // UP CHUNKS
    registerChunk(300, UP_CHUNK, 14, 10, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "...............#",
        ".............###",
        "...........#####",
        ".........#######",
        ".......#########",
        "################",
        "################"
    });

    registerChunk(301, UP_CHUNK, 14, 11, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "..............##",
        "..........C.####",
        "........########",
        "....W.##########",
        "################",
        "################"
    });

    registerChunk(302, UP_CHUNK, 14, 8, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "...............#",
        ".............###",
        "...........#####",
        ".........#######",
        ".......#########",
        ".....###########",
        "...#############",
        ".###############",
        "################",
        "################"
    });
    
    registerChunk(303, UP_CHUNK, 14, 6, {
        "................",
        "................",
        "................",
        "................",
        "...............#",
        "..............##",
        ".............###",
        "............####",
        "...........#####",
        "........########",
        ".......#########",
        "......##########",
        "....############",
        "..##############",
        "################",
        "################"
    });

    // DOWN CHUNKS
    registerChunk(400, DOWN_CHUNK, 10, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "#...............",
        "###.............",
        "#####........C..",
        "#######.........",
        "#########.......",
        "################",
        "################"
    });

    registerChunk(401, DOWN_CHUNK, 8, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "#...............",
        "###.............",
        "#####........C..",
        "#######.........",
        "#########.......",
        "###########.....",
        "#############...",
        "###############.",
        "################",
        "################"
    });

    registerChunk(402, DOWN_CHUNK, 6, 14, {
        "................",
        "................",
        "................",
        "................",
        "#...............",
        "##..............",
        "###.............",
        "####............",
        "#####...........",
        "######..........",
        "#######.........",
        "########........",
        "#########.......",
        "##########......",
        "################",
        "################"
    });

    // GAP CHUNKS
    registerChunk(500, GAP_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        ".......C........",
        "......###.......",
        "................",
        "###..........###",
        "###..........###"
    });

    registerChunk(501, GAP_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "....###..###....",
        "................",
        "##............##",
        "##............##"
    });

    registerChunk(502, GAP_CHUNK, 10, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "##..............",
        "###...###.......",
        "###.............",
        "###..........###",
        "###..........###",
        "###..........###",
        "###..........###"
    });

    registerChunk(503, GAP_CHUNK, 14, 10, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "..............##",
        ".......###...###",
        ".............###",
        "###..........###",
        "###..........###",
        "###..........###",
        "###..........###"
    });

    // OBSTACLE & HAZARD CHUNKS
    registerChunk(600, OBSTACLE_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "......####......",
        "......####......",
        "................",
        "................",
        "................",
        "................",
        "....W.......W...",
        "################",
        "################"
    });

    registerChunk(601, OBSTACLE_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "......####......",
        "......####......",
        "......####......",
        "......####......",
        "................",
        "................",
        "................",
        "................",
        "####........####",
        "####........####"
    });

    registerChunk(602, HAZARD_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "....C...C...C...",
        "................",
        "....^^^^^^^^....",
        "####^^^^^^^^####",
        "####^^^^^^^^####"
    });
    
    registerChunk(603, HAZARD_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        ".......F........",
        "................",
        "................",
        "................",
        "................",
        "..^^........^^..",
        "####........####",
        "####........####"
    });

    // PUZZLE CHUNKS (Moving Platforms 'M', Shooters 'T')
    registerChunk(700, PUZZLE_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        ".......C........",
        "................",
        ".......M........",
        "................",
        "................",
        "###..........###",
        "###..........###"
    });

    registerChunk(701, PUZZLE_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "...T........T...", // T = Shooter
        "................",
        ".###........###.",
        "................",
        "................",
        "................",
        "################",
        "################"
    });

    // END CHUNKS
    registerChunk(800, END_CHUNK, 14, 14, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "..........G.....",
        "################",
        "################"
    });
    
    registerChunk(801, END_CHUNK, 10, 10, {
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "..........G.....",
        "################",
        "################",
        "################",
        "################",
        "################",
        "################",
        "################"
    });
}

struct GeneratorConfig {
    int seed;
    int chunkCount;
    int difficulty; // 1 to 5
    int themeId;
};

static vector<ChunkTemplate> getMatchingChunks(int requiredEntranceY, ChunkType allowedTypes[], int numTypes) {
    vector<ChunkTemplate> matches;
    for (const auto& chunk : templateDB) {
        bool typeMatch = false;
        for (int i = 0; i < numTypes; i++) {
            if (chunk.type == allowedTypes[i]) {
                typeMatch = true;
                break;
            }
        }
        if (typeMatch && chunk.entranceY == requiredEntranceY) {
            matches.push_back(chunk);
        }
    }
    return matches;
}

static void applyHybridVariations(vector<string>& mapData, XorShift32& rng, int difficulty) {
    int height = mapData.size();
    if (height == 0) return;
    int width = mapData[0].size();
    
    float holeChance = 0.01f + (difficulty * 0.015f);
    float decorChance = 0.08f;
    float hazardChance = difficulty * 0.02f;

    for (int y = 1; y < height - 1; y++) {
        for (int x = 2; x < width - 2; x++) {
            // Random small gaps
            if (mapData[y][x] == '#' && mapData[y-1][x] == '.') {
                if (rng.chance(holeChance)) {
                    bool safe = true;
                    for (int dy = -3; dy <= 3; dy++) {
                        for (int dx = -3; dx <= 3; dx++) {
                            if (y+dy >= 0 && y+dy < height && x+dx >= 0 && x+dx < width) {
                                char c = mapData[y+dy][x+dx];
                                if (c == 'S' || c == 'G' || c == 'M' || c == 'T') safe = false;
                            }
                        }
                    }
                    if (safe) mapData[y][x] = '.';
                }
            }
            
            // Random Spikes
            if (mapData[y][x] == '.' && mapData[y+1][x] == '#') {
                if (rng.chance(hazardChance)) {
                    bool safe = true;
                    for (int dx = -2; dx <= 2; dx++) {
                        if (x+dx >= 0 && x+dx < width) {
                            if (mapData[y][x+dx] == 'S' || mapData[y][x+dx] == 'G') safe = false;
                        }
                    }
                    if (safe) mapData[y][x] = '^';
                }
            }
            
            // Decorations
            if (mapData[y][x] == '.' && mapData[y+1][x] == '#') {
                if (rng.chance(decorChance)) {
                    mapData[y][x] = 'D';
                }
            }
        }
    }
}

static void alignLevelToBottom(vector<string>& mapData) {
    if (mapData.empty()) return;
    
    int goalY = -1;
    int height = mapData.size();
    int width = mapData[0].size();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (mapData[y][x] == 'G') {
                goalY = y;
                break;
            }
        }
        if (goalY != -1) break;
    }
    
    if (goalY == -1) return;
    
    int floorY = goalY;
    while (floorY < height && mapData[floorY][mapData[goalY].find('G')] != '#') {
        floorY++;
    }
    
    if (floorY >= height) floorY = height - 1;
    
    int targetFloorY = height - 3;
    int shift = targetFloorY - floorY;
    
    if (shift > 0) {
        vector<string> newMap;
        for (int i = 0; i < shift; i++) newMap.push_back(string(width, '.'));
        for (int i = 0; i < height - shift; i++) newMap.push_back(mapData[i]);
        mapData = newMap;
    } else if (shift < 0) {
        int upShift = -shift;
        vector<string> newMap;
        for (int i = upShift; i < height; i++) newMap.push_back(mapData[i]);
        for (int i = 0; i < upShift; i++) newMap.push_back(string(width, '#'));
        mapData = newMap;
    }
}

static Level generateProceduralLevel(const GeneratorConfig& config) {
    if (templateDB.empty()) initTemplates();

    XorShift32 rng(config.seed);
    Level lvl;
    lvl.theme = config.themeId;
    lvl.name = "ProcGen Run";
    lvl.isProcedural = true;

    int maxExpectedHeight = 80;
    int totalWidth = 0;
    
    vector<ChunkTemplate> selectedChunks;
    
    ChunkType startTypes[] = {START_CHUNK};
    vector<ChunkTemplate> starts = getMatchingChunks(14, startTypes, 1);
    if (starts.empty()) starts = { templateDB[0] };
    ChunkTemplate startChunk = starts[rng.nextInt(0, starts.size() - 1)];
    selectedChunks.push_back(startChunk);
    totalWidth += startChunk.width;
    
    int currentExitY = startChunk.exitY;
    int currentWorldYOffset = (maxExpectedHeight / 2) - currentExitY; 
    
    ChunkType bodyTypes[] = {FLAT_CHUNK, UP_CHUNK, DOWN_CHUNK, GAP_CHUNK, OBSTACLE_CHUNK, HAZARD_CHUNK, PUZZLE_CHUNK};
    for (int i = 0; i < config.chunkCount; i++) {
        vector<ChunkTemplate> options = getMatchingChunks(currentExitY, bodyTypes, 7);
        
        if (currentWorldYOffset < 15) {
            options = getMatchingChunks(currentExitY, new ChunkType[1]{DOWN_CHUNK}, 1);
        } else if (currentWorldYOffset > maxExpectedHeight - 25) {
            options = getMatchingChunks(currentExitY, new ChunkType[1]{UP_CHUNK}, 1);
        }

        if (options.empty()) {
            options = getMatchingChunks(currentExitY, new ChunkType[1]{FLAT_CHUNK}, 1);
            if (options.empty()) options = { templateDB[2] };
        }
        
        ChunkTemplate nextChunk = options[rng.nextInt(0, options.size() - 1)];
        selectedChunks.push_back(nextChunk);
        totalWidth += nextChunk.width;
        currentExitY = nextChunk.exitY;
    }
    
    ChunkType endTypes[] = {END_CHUNK};
    vector<ChunkTemplate> ends = getMatchingChunks(currentExitY, endTypes, 1);
    if (ends.empty()) ends = { templateDB[templateDB.size() - 1] };
    ChunkTemplate endChunk = ends[rng.nextInt(0, ends.size() - 1)];
    selectedChunks.push_back(endChunk);
    totalWidth += endChunk.width;

    vector<string> rawMap(maxExpectedHeight, string(totalWidth, '.'));
    
    int currentX = 0;
    int cursorY = maxExpectedHeight / 2;

    for (const auto& chunk : selectedChunks) {
        int shiftY = cursorY - chunk.entranceY;
        for (int y = 0; y < chunk.height; y++) {
            int writeY = y + shiftY;
            if (writeY >= 0 && writeY < maxExpectedHeight) {
                for (int x = 0; x < chunk.width; x++) {
                    rawMap[writeY][currentX + x] = chunk.layout[y][x];
                }
            }
        }
        currentX += chunk.width;
        cursorY = chunk.exitY + shiftY;
    }

    applyHybridVariations(rawMap, rng, config.difficulty);
    alignLevelToBottom(rawMap);

    lvl.width = totalWidth;
    lvl.height = rawMap.size();
    lvl.map = rawMap;
    lvl.spawnX = 32;
    lvl.spawnY = 64;
    lvl.goalX = (lvl.width - 3) * TILE;
    lvl.goalY = (lvl.height - 3) * TILE;

    for (int y = 0; y < lvl.height; ++y) {
        for (int x = 0; x < lvl.width; ++x) {
            char c = lvl.map[y][x];
            if (c == 'S') {
                lvl.spawnX = x * TILE;
                lvl.spawnY = y * TILE;
                lvl.map[y][x] = '.';
            }
            else if (c == 'G') {
                lvl.goalX = x * TILE;
                lvl.goalY = y * TILE;
                lvl.map[y][x] = '.';
            }
            else if (c == 'C') {
                Coin coin;
                coin.x = x * TILE + 8;
                coin.y = y * TILE + 8;
                coin.taken = false;
                coin.phase = rng.nextFloat() * 10.0f;
                lvl.coins.push_back(coin);
                lvl.map[y][x] = '.';
            }
            else if (c == 'W' || c == 'F' || c == 'T') {
                Enemy e;
                e.x = x * TILE;
                e.y = y * TILE;
                e.startX = e.x;
                e.startY = e.y;
                e.vx = 0.65f + (config.difficulty * 0.1f);
                e.vy = 0;
                e.left = std::max(0, x - 4) * TILE;
                e.right = std::min(lvl.width - 1, x + 4) * TILE;
                e.alive = true;
                e.timer = rng.nextFloat() * 100.0f;
                
                if (c == 'W') e.type = ENEMY_WALKER;
                if (c == 'F') e.type = ENEMY_FLYER;
                if (c == 'T') e.type = ENEMY_SHOOTER;
                
                lvl.enemies.push_back(e);
                lvl.map[y][x] = '.';
            }
            else if (c == 'M') {
                MovingPlatform mp;
                mp.x = x * TILE;
                mp.y = y * TILE;
                mp.w = TILE * 3;
                mp.h = TILE;
                mp.startX = mp.x;
                mp.startY = mp.y;
                mp.endX = mp.x + TILE * 4;
                mp.endY = mp.y;
                mp.speed = 1.0f;
                mp.phase = 0;
                mp.active = true;
                lvl.movPlatforms.push_back(mp);
                lvl.map[y][x] = '.';
            }
            else if (c == '^') {
                Hazard h;
                h.x = x * TILE;
                h.y = y * TILE + TILE/2;
                h.w = TILE;
                h.h = TILE/2;
                h.type = 0;
                lvl.hazards.push_back(h);
                lvl.map[y][x] = '.';
            }
        }
    }

    return lvl;
}

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
    bool invincible;
    int invincibilityTimer;
};

static Player player;

static int score = 0;
static const int LEVEL_COUNT = 15;
static int bestScore[LEVEL_COUNT] = {};
static int levelTime = 0;
static int checkpointX = 0;
static int checkpointY = 0;
static bool checkpointActive = false;
static bool dashAvailable = true;
static bool dashActive = false;
static int dashFrames = 0;
static bool doubleJumpAvailable = true;

static int currentLevel = 0;
static int unlockedLevel = 0;
static int gameState = 0; 
static bool running = true;
static bool levelCompleted = false;
static u64 frameCounter = 0;
static float cameraX = 0.0f;
static float cameraY = 0.0f;
static vector<Platform> activePlatforms;
static float platformBuildCameraX = -100000.0f;
static const float PLATFORM_ACTIVE_RANGE = 520.0f;
static float coyoteTimer = 0.0f;
static int jumpBuffer = 0;
static float screenShake = 0.0f;
static float transitionAlpha = 255.0f;
static int transitionState = 0; // 0=none, 1=fade in, 2=fade out

static GeneratorConfig currentProcConfig = {1337, 15, 3, 0};

static void resetPlayer();
static bool intersects(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh);
static bool isNearCamera(float x, float width = 0.0f);

class SaveManager {
public:
    int totalCoins = 0;
    int totalDeaths = 0;
    int totalLevelsBeaten = 0;
    int highestProcDifficulty = 0;
    bool achievements[10] = {false};
    
    void save() {
        // In a real 3DS app, use std::fstream with sdcard
        // Here we stub it to prevent filesystem errors but maintain logic
        printf("Saving game... Coins: %d\n", totalCoins);
    }
    
    void load() {
        printf("Loading game...\n");
        // Stub
    }
    
    void checkAchievements() {
        if (totalCoins >= 100 && !achievements[0]) { achievements[0] = true; save(); }
        if (totalCoins >= 1000 && !achievements[1]) { achievements[1] = true; save(); }
        if (totalLevelsBeaten >= 10 && !achievements[2]) { achievements[2] = true; save(); }
        if (totalDeaths >= 50 && !achievements[3]) { achievements[3] = true; save(); }
        if (highestProcDifficulty >= 5 && !achievements[4]) { achievements[4] = true; save(); }
    }
};

static SaveManager saveMgr;

struct Particle
{
    float x, y;
    float vx, vy;
    float life;
    float maxLife;
    float size;
    u32 color;
    int type; // 0=Square, 1=Spark, 2=Star, 3=Weather
};

static vector<Particle> particles;

static void spawnParticle(float x, float y, u32 color, int count, int type = 0)
{
    for (int i = 0; i < count; ++i)
    {
        Particle p;
        p.x = x;
        p.y = y;
        p.vx = ((i * 17) % 9 - 4) * 0.25f;
        p.vy = -((i * 13) % 7) * 0.20f;
        p.maxLife = p.life = 18.0f + (i % 15);
        p.size = 1.5f + (i % 3);
        p.color = color;
        p.type = type;
        particles.push_back(p);
    }
}

static void spawnWeatherParticle(int themeWeather) {
    if (themeWeather == 0) return;
    
    Particle p;
    p.x = cameraX + (rand() % (SCREEN_W + 100)) - 50;
    p.y = cameraY - 10;
    p.type = 3;
    
    if (themeWeather == 1) { // Snow
        p.vx = -0.5f + (rand() % 100) / 100.0f;
        p.vy = 0.5f + (rand() % 100) / 200.0f;
        p.size = 2.0f;
        p.color = C2D_Color32(255, 255, 255, 200);
        p.maxLife = p.life = 300.0f;
    } else if (themeWeather == 2) { // Rain
        p.vx = -1.5f;
        p.vy = 4.0f + (rand() % 100) / 50.0f;
        p.size = 1.0f;
        p.color = C2D_Color32(150, 200, 255, 180);
        p.maxLife = p.life = 150.0f;
    } else if (themeWeather == 3) { // Ash
        p.vx = -0.2f + (rand() % 100) / 100.0f;
        p.vy = -0.2f - (rand() % 100) / 100.0f;
        p.y = cameraY + SCREEN_H + 10;
        p.size = 1.5f;
        p.color = C2D_Color32(100, 100, 100, 200);
        p.maxLife = p.life = 250.0f;
    }
    particles.push_back(p);
}

static void updateParticles()
{
    const int weather = themes[levels[currentLevel].theme % themes.size()].weatherType;
    if (gameState == 1 && frameCounter % 2 == 0) {
        spawnWeatherParticle(weather);
        spawnWeatherParticle(weather);
    }

    for (size_t i = 0; i < particles.size();)
    {
        Particle& p = particles[i];
        p.x += p.vx;
        p.y += p.vy;
        
        if (p.type != 3) {
            p.vy += 0.045f; // Gravity for physical particles
        } else if (weather == 1) {
            p.vx += sinf(frameCounter * 0.05f + p.y * 0.01f) * 0.02f; // Snow flutter
        }
        
        p.life -= 1.0f;

        if (p.life <= 0)
        {
            particles.erase(particles.begin() + i);
            continue;
        }
        ++i;
    }
    if (particles.size() > 600)
        particles.erase(particles.begin(), particles.begin() + 100);
}

static void drawParticles()
{
    for (const Particle& p : particles)
    {
        if (!isNearCamera(p.x, p.size)) continue;

        float a = std::max(0.0f, std::min(1.0f, p.life / (p.type == 3 ? 100.0f : 20.0f)));
        u8 alpha = (u8)(((p.color & 0x000000FF)) * a);
        u32 c = (p.color & 0xFFFFFF00) | alpha;
        
        float sx = p.x - cameraX;
        float sy = p.y - cameraY;

        if (p.type == 0) {
            rectSolid(sx, sy, p.size, p.size, c);
        } else if (p.type == 1) {
            line(sx, sy, sx + p.vx * 3.0f, sy + p.vy * 3.0f, 1.5f, c);
        } else if (p.type == 2) {
            rectSolid(sx - 1, sy - p.size, 2, p.size * 2, c);
            rectSolid(sx - p.size, sy - 1, p.size * 2, 2, c);
        } else if (p.type == 3) {
            if (themes[levels[currentLevel].theme % themes.size()].weatherType == 2) {
                line(sx, sy, sx + p.vx*2, sy + p.vy*2, 1.0f, c);
            } else {
                rectSolid(sx, sy, p.size, p.size, c);
            }
        }
    }
}

static void addScreenShake(float amount) {
    screenShake += amount;
    if (screenShake > 15.0f) screenShake = 15.0f;
}

static void activateCheckpoint() {
    checkpointX = (int)player.x;
    checkpointY = (int)player.y;
    checkpointActive = true;
    spawnParticle(player.x, player.y, C2D_Color32(90,230,120,255), 18, 2);
}

static void resetToCheckpoint() {
    saveMgr.totalDeaths++;
    saveMgr.checkAchievements();
    addScreenShake(8.0f);
    
    if (checkpointActive) {
        player.x = checkpointX;
        player.y = checkpointY;
        player.vx = 0;
        player.vy = 0;
    } else {
        resetPlayer();
    }
    player.grounded = false;
    dashAvailable = true;
    dashActive = false;
    dashFrames = 0;
    doubleJumpAvailable = true;
    player.invincible = true;
    player.invincibilityTimer = 60;
}

static void updateDash() {
    if (!dashActive && dashAvailable && input.isPressed(KEY_R)) {
        dashActive = true;
        dashAvailable = false;
        dashFrames = 12;
        if (input.isHeld(KEY_LEFT)) player.vx = -8.0f;
        else if (input.isHeld(KEY_RIGHT)) player.vx = 8.0f;
        else player.vx = player.vx >= 0 ? 8.0f : -8.0f;
        player.vy = 0;
        addScreenShake(3.0f);
        spawnParticle(player.x, player.y + 8, C2D_Color32(100,220,255,255), 15, 1);
    }
    if (dashActive) {
        dashFrames--;
        if ((frameCounter % 2) == 0)
            spawnParticle(player.x, player.y + 7, C2D_Color32(100,220,255,255), 3);
        if (dashFrames <= 0) dashActive = false;
    }
}

static void updateDoubleJump() {
    if (player.grounded) doubleJumpAvailable = true;
    if (input.isPressed(KEY_A) || input.isPressed(KEY_B)) {
        if (!player.grounded && doubleJumpAvailable) {
            player.vy = JUMP_VEL * 0.88f;
            doubleJumpAvailable = false;
            spawnParticle(player.x + 6, player.y + 14, C2D_Color32(255,255,255,255), 12, 1);
        }
    }
}

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
    l.isProcedural = false;

    for (int y = 0; y < l.height; ++y)
    {
        for (int x = 0; x < l.width; ++x)
        {
            char c = l.map[y][x];
            if (c == 'S') { l.spawnX = x * TILE; l.spawnY = y * TILE; l.map[y][x] = '.'; }
            else if (c == 'G') { l.goalX = x * TILE; l.goalY = y * TILE; l.map[y][x] = '.'; }
            else if (c == 'C') {
                Coin coin; coin.x = x * TILE + 8; coin.y = y * TILE + 8; coin.taken = false;
                coin.phase = float((x * 17 + y * 9) % 100) / 10.0f;
                l.coins.push_back(coin); l.map[y][x] = '.';
            }
            else if (c == 'E') {
                Enemy e; e.x = x * TILE; e.y = y * TILE; e.vx = 0.65f; e.vy = 0; e.type = ENEMY_WALKER;
                e.left = std::max(0, x - 4) * TILE; e.right = std::min(l.width - 1, x + 4) * TILE;
                e.alive = true; l.enemies.push_back(e); l.map[y][x] = '.';
            }
            else if (c == '^') {
                Hazard h; h.x = x * TILE; h.y = y * TILE + TILE/2; h.w = TILE; h.h = TILE/2; h.type = 0;
                l.hazards.push_back(h); l.map[y][x] = '.';
            }
        }
    }
    return l;
}

static void buildStaticLevels()
{
    levels.clear();
    levels.push_back(makeLevel("1-1: Green Hills", 0, {
        ".................................................",
        ".................................................",
        "......................C..........................",
        "............C....................................",
        "......###..............###...............C.......",
        ".......................................###.......",
        "..............C..................................",
        "..............###...........................G....",
        "..S...........................C..................",
        "#######################......###.................",
        "###############################........##########",
        "################################       ##########"
    }));
    levels.push_back(makeLevel("1-2: Sky Bridges", 1, {
        ".........................................................",
        "....................C....................................",
        "...........###................###........................",
        ".........................................................",
        "..S...###........###....###...........C................G.",
        "......................................##.................",
        "#####.........#####........#####..................#######",
        "########################################^^^^^^^^^^#######",
        "#########################################################"
    }));
    levels.push_back(makeLevel("1-3: Cold Peaks", 2, {
        "................................................",
        "..................C.............................",
        "...........###..............C...................",
        "..S..........................................G.",
        "######....#####.....#####....##########...######",
        "################################################"
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
                
    // Check moving platforms
    for (const auto& mp : levels[currentLevel].movPlatforms) {
        if (intersects(r.x, r.y, r.w, r.h, mp.x, mp.y, mp.w, mp.h)) return true;
    }
    return false;
}

static void refreshNearbyPlatforms()
{
    if (levels.empty()) return;
    if (std::abs(cameraX - platformBuildCameraX) < 24.0f && !activePlatforms.empty()) return;

    const Level& l = levels[currentLevel];
    const float leftWorld = std::max(0.0f, cameraX - PLATFORM_ACTIVE_RANGE);
    const float rightWorld = std::min((float)l.width * TILE, cameraX + SCREEN_W + PLATFORM_ACTIVE_RANGE);
    const int startX = std::max(0, (int)floorf(leftWorld / TILE) - 1);
    const int endX = std::min(l.width, (int)ceilf(rightWorld / TILE) + 1);

    activePlatforms.clear();
    activePlatforms.reserve((endX - startX) * l.height / 2 + 8);

    for (int y = 0; y < l.height; ++y) {
        int x = startX;
        while (x < endX) {
            while (x < endX && l.map[y][x] != '#') ++x;
            if (x >= endX) break;
            const int runStart = x;
            while (x < endX && l.map[y][x] == '#') ++x;
            Platform p;
            p.x = runStart * TILE;
            p.y = y * TILE;
            p.w = (x - runStart) * TILE;
            p.h = TILE;
            p.top = y == 0 || !solidAt(runStart, y - 1);
            if (p.x + p.w >= leftWorld - TILE && p.x <= rightWorld + TILE)
                activePlatforms.push_back(p);
        }
    }
    platformBuildCameraX = cameraX;
}

static bool isNearCamera(float x, float width) {
    return x + width >= cameraX - PLATFORM_ACTIVE_RANGE &&
           x <= cameraX + SCREEN_W + PLATFORM_ACTIVE_RANGE;
}

static Rect playerRect() {
    return { player.x + 2, player.y + 1, player.w - 4, player.h - 2 };
}

static bool intersects(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void resetPlayer() {
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
    player.invincible = false;
    player.invincibilityTimer = 0;

    for (auto& c : levels[currentLevel].coins) c.taken = false;
    for (auto& e : levels[currentLevel].enemies) {
        e.alive = true;
        e.x = e.startX; e.y = e.startY;
        if (e.type == ENEMY_WALKER) e.vx = std::abs(e.vx); 
    }
    for (auto& mp : levels[currentLevel].movPlatforms) {
        mp.x = mp.startX; mp.y = mp.startY; mp.phase = 0;
    }
    activeProjectiles.clear();

    cameraX = player.x - (SCREEN_W / 2);
    cameraY = player.y - (SCREEN_H / 2);
    platformBuildCameraX = -100000.0f;
    activePlatforms.clear();
    levelCompleted = false;
    transitionAlpha = 255.0f;
    transitionState = 1; // Fade in
}

static void moveHorizontal(float amount) {
    player.x += amount;
    Rect r = playerRect();
    if (!solidRect(r)) return;
    if (amount > 0) {
        while (solidRect(playerRect())) player.x -= 0.5f;
        player.vx = 0;
    } else if (amount < 0) {
        while (solidRect(playerRect())) player.x += 0.5f;
        player.vx = 0;
    }
}

static void moveVertical(float amount) {
    player.y += amount;
    Rect r = playerRect();
    if (!solidRect(r)) {
        player.grounded = false;
        return;
    }
    if (amount > 0) {
        while (solidRect(playerRect())) player.y -= 0.5f;
        player.vy = 0;
        player.grounded = true;
    } else {
        while (solidRect(playerRect())) player.y += 0.5f;
        player.vy = 0;
    }
}

static void updateCamera() {
    float targetX = player.x - 145.0f;
    float maxCameraX = levels[currentLevel].width * TILE - SCREEN_W;
    targetX = std::max(0.0f, std::min(targetX, maxCameraX));
    cameraX += (targetX - cameraX) * 0.1f;

    float targetY = player.y - 120.0f;
    float maxCameraY = (levels[currentLevel].height * TILE) - SCREEN_H;
    targetY = std::max(0.0f, std::min(targetY, maxCameraY));
    cameraY += (targetY - cameraY) * 0.1f;
    
    if (screenShake > 0.0f) {
        cameraX += (rand() % 100 / 50.0f - 1.0f) * screenShake;
        cameraY += (rand() % 100 / 50.0f - 1.0f) * screenShake;
        screenShake *= 0.9f;
        if (screenShake < 0.1f) screenShake = 0.0f;
    }
    
    refreshNearbyPlatforms();
}

static void updatePlayer() {
    if (transitionState != 0) return;

    bool left = input.isHeld(KEY_LEFT);
    bool right = input.isHeld(KEY_RIGHT);
    bool sprint = input.isHeld(KEY_X) || input.isHeld(KEY_Y);
    bool jumpPressed = input.isPressed(KEY_A) || input.isPressed(KEY_B);

    if (jumpPressed) jumpBuffer = 6;
    else if (jumpBuffer > 0) --jumpBuffer;

    if (player.grounded) coyoteTimer = 7.0f;
    else if (coyoteTimer > 0.0f) coyoteTimer -= 1.0f;

    if (player.invincible) {
        player.invincibilityTimer--;
        if (player.invincibilityTimer <= 0) player.invincible = false;
    }

    float target = 0.0f;
    if (left) target = sprint ? -SPRINT_SPEED : -WALK_SPEED;
    else if (right) target = sprint ? SPRINT_SPEED : WALK_SPEED;

    if (target != 0.0f) {
        float acceleration = dashActive ? 0.5f : (player.grounded ? 0.35f : 0.15f);
        player.vx += (target - player.vx) * acceleration;
        player.animation++;
    } else if (!dashActive) {
        player.vx *= player.grounded ? 0.65f : 0.90f;
        if (std::abs(player.vx) < 0.04f) player.vx = 0.0f;
    }

    if (jumpBuffer > 0 && coyoteTimer > 0.0f) {
        player.vy = JUMP_VEL;
        player.grounded = false;
        coyoteTimer = 0.0f;
        jumpBuffer = 0;
        doubleJumpAvailable = true;
        spawnParticle(player.x + 6, player.y + 14, C2D_Color32(200,200,200,200), 8, 0);
    }

    player.wasGrounded = player.grounded;
    moveHorizontal(player.vx);

    player.vy += GRAVITY;
    if (player.vy > MAX_FALL) player.vy = MAX_FALL;

    player.grounded = false;
    moveVertical(player.vy);

    // Hazard collision
    for (const auto& h : levels[currentLevel].hazards) {
        if (intersects(player.x, player.y, player.w, player.h, h.x, h.y, h.w, h.h)) {
            if (!player.invincible) {
                player.lives--;
                if (player.lives <= 0) gameState = 5;
                else resetToCheckpoint();
                return;
            }
        }
    }

    updateCamera();

    // Kill plane
    if (player.y > levels[currentLevel].height * TILE + 50) {
        player.lives--;
        if (player.lives <= 0) gameState = 5;
        else resetToCheckpoint();
    }
}

static void updateEnemies() {
    Level& l = levels[currentLevel];
    for (auto& e : l.enemies) {
        if (!e.alive || !isNearCamera(e.x, 16.0f)) continue;
        
        e.timer += 1.0f;

        if (e.type == ENEMY_WALKER) {
            e.x += e.vx;
            if (e.x < e.left) { e.x = e.left; e.vx = std::abs(e.vx); }
            if (e.x > e.right) { e.x = e.right; e.vx = -std::abs(e.vx); }
        } 
        else if (e.type == ENEMY_FLYER) {
            e.x += sinf(e.timer * 0.05f) * 1.0f;
            e.y += cosf(e.timer * 0.05f) * 0.5f;
        }
        else if (e.type == ENEMY_SHOOTER) {
            if ((int)e.timer % 120 == 0) {
                Projectile proj;
                proj.x = e.x + 8;
                proj.y = e.y + 8;
                float angle = atan2f(player.y - e.y, player.x - e.x);
                proj.vx = cosf(angle) * 2.5f;
                proj.vy = sinf(angle) * 2.5f;
                proj.active = true;
                proj.life = 200;
                activeProjectiles.push_back(proj);
            }
        }

        if (intersects(player.x, player.y, player.w, player.h, e.x + 2, e.y + 3, 12, 12)) {
            if (player.vy > 1.0f && player.y + player.h < e.y + 10) {
                e.alive = false;
                player.vy = -4.5f;
                spawnParticle(e.x, e.y, C2D_Color32(200,50,50,255), 20);
                addScreenShake(2.0f);
            } else if (!player.invincible) {
                player.lives--;
                if (player.lives <= 0) gameState = 5;
                else resetToCheckpoint();
                return;
            }
        }
    }
}

static void updateEntities() {
    Level& l = levels[currentLevel];
    for (auto& c : l.coins) {
        if (c.taken || !isNearCamera(c.x, 10.0f)) continue;
        c.phase += 0.1f;
        if (intersects(player.x, player.y, player.w, player.h, c.x - 5, c.y - 5, 10, 10)) {
            c.taken = true;
            player.coins++;
            saveMgr.totalCoins++;
            spawnParticle(c.x, c.y, C2D_Color32(255,220,50,255), 12, 2);
        }
    }
    
    for (auto& mp : l.movPlatforms) {
        mp.phase += 0.02f * mp.speed;
        float oldX = mp.x;
        float oldY = mp.y;
        mp.x = mp.startX + (mp.endX - mp.startX) * (sinf(mp.phase) * 0.5f + 0.5f);
        mp.y = mp.startY + (mp.endY - mp.startY) * (sinf(mp.phase) * 0.5f + 0.5f);
        
        // Carry player
        if (player.grounded && intersects(player.x, player.y + 2, player.w, player.h, mp.x, mp.y, mp.w, mp.h)) {
            player.x += (mp.x - oldX);
            player.y += (mp.y - oldY);
        }
    }

    updateEnemies();
    
    // Update projectiles
    for (size_t i = 0; i < activeProjectiles.size();) {
        Projectile& p = activeProjectiles[i];
        p.x += p.vx;
        p.y += p.vy;
        p.life--;
        
        if (intersects(player.x, player.y, player.w, player.h, p.x-2, p.y-2, 4, 4) && !player.invincible) {
            player.lives--;
            if (player.lives <= 0) gameState = 5;
            else resetToCheckpoint();
            return;
        }
        
        if (p.life <= 0 || solidAt(p.x/TILE, p.y/TILE)) {
            spawnParticle(p.x, p.y, C2D_Color32(255,150,0,255), 5, 1);
            activeProjectiles.erase(activeProjectiles.begin() + i);
            continue;
        }
        i++;
    }

    if (intersects(player.x, player.y, player.w, player.h, l.goalX, l.goalY, 16, 32)) {
        levelCompleted = true;
        saveMgr.totalLevelsBeaten++;
        if (!l.isProcedural && currentLevel >= unlockedLevel && unlockedLevel < LEVEL_COUNT - 1)
            unlockedLevel = currentLevel + 1;
        saveMgr.checkAchievements();
        transitionState = 2; // Fade out to win screen
    }
}

static void drawBackground(const Theme& t) {
    C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, SCREEN_W, SCREEN_H, t.sky);
    
    // Parallax Layer 1 (Distant)
    float px1 = fmodf(-cameraX * 0.1f, 200.0f);
    for(int i=-1; i<4; i++) {
        rectSolid(px1 + i * 200, SCREEN_H - 100 + (cameraY*0.05f), 120, 150, t.sky2);
    }
    
    // Parallax Layer 2 (Mid)
    float px2 = fmodf(-cameraX * 0.25f, 150.0f);
    for(int i=-1; i<5; i++) {
        rect(px2 + i * 150, SCREEN_H - 80 + (cameraY*0.1f), 80, 150, t.mountain2);
    }
    
    // Parallax Layer 3 (Close)
    float px3 = fmodf(-cameraX * 0.4f, 100.0f);
    for(int i=-1; i<6; i++) {
        rect(px3 + i * 100, SCREEN_H - 50 + (cameraY*0.15f), 60, 150, t.mountain);
    }
}

static void drawTiles(const Theme& t) {
    for (const Platform& p : activePlatforms) {
        float sx = p.x - cameraX;
        float sy = p.y - cameraY;

        rect(sx, sy, p.w, p.h, t.ground);
        if (p.top) C2D_DrawRectSolid(sx, sy, 0.0f, p.w, 3.0f, t.groundTop);

        C2D_DrawRectSolid(sx, sy, 0.0f, p.w, 1.5f, C2D_Color32(0,0,0,255));
        C2D_DrawRectSolid(sx, sy + p.h - 1.5f, 0.0f, p.w, 1.5f, C2D_Color32(0,0,0,255));
        C2D_DrawRectSolid(sx, sy, 0.0f, 1.5f, p.h, C2D_Color32(0,0,0,255));
        C2D_DrawRectSolid(sx + p.w - 1.5f, sy, 0.0f, 1.5f, p.h, C2D_Color32(0,0,0,255));
    }
    
    Level& l = levels[currentLevel];
    for (int y = 0; y < l.height; ++y) {
        for (int x = 0; x < l.width; ++x) {
            if (l.map[y][x] == 'D') {
                float sx = x * TILE - cameraX;
                float sy = y * TILE - cameraY;
                if (sx > -TILE && sx < SCREEN_W + TILE) {
                    rect(sx + 4, sy + 8, 8, 8, t.decoration);
                    rect(sx + 2, sy + 12, 12, 4, t.decoration);
                }
            }
        }
    }
    
    for (const auto& mp : l.movPlatforms) {
        float sx = mp.x - cameraX;
        float sy = mp.y - cameraY;
        rect(sx, sy, mp.w, mp.h, t.groundTop);
        rect(sx+2, sy+2, mp.w-4, mp.h-4, t.ground);
    }
    
    for (const auto& h : l.hazards) {
        float sx = h.x - cameraX;
        float sy = h.y - cameraY;
        if (h.type == 0) { // Spikes
            for(int i=0; i<3; i++) {
                line(sx + i*5 + 2, sy + h.h, sx + i*5 + 4, sy, 1.5f, C2D_Color32(200,200,200,255));
            }
        }
    }
}

static void drawEntities(const Theme& t) {
    const Level& l = levels[currentLevel];
    float gx = l.goalX - cameraX;
    float gy = l.goalY - cameraY - 16;
    rect(gx + 6, gy, 3, 34, C2D_Color32(45,45,55,255));
    rect(gx + 9, gy + 1, 13, 9, t.accent);

    for (const auto& c : l.coins) {
        if (c.taken || !isNearCamera(c.x, 10.0f)) continue;
        float bob = sinf(c.phase) * 3.0f;
        float cx = c.x - cameraX;
        float cy = c.y - cameraY + bob;
        rect(cx - 5, cy - 6, 10, 12, t.coin);
        rect(cx - 3, cy - 4, 6, 8, C2D_Color32(255,240,120,255));
    }

    for (const auto& e : l.enemies) {
        if (!e.alive || !isNearCamera(e.x, 16.0f)) continue;
        float ex = e.x - cameraX;
        float ey = e.y - cameraY;
        
        if (e.type == ENEMY_WALKER) {
            rect(ex + 2, ey + 5, 12, 10, C2D_Color32(180,40,60,255)); 
            rect(ex + 4, ey + 4, 3, 3, C2D_Color32(255,255,255,255));
            rect(ex + 10, ey + 4, 3, 3, C2D_Color32(255,255,255,255));
        } else if (e.type == ENEMY_FLYER) {
            rect(ex, ey + 2, 16, 12, C2D_Color32(60,180,220,255));
            rect(ex - 4, ey + 4, 4, 4, C2D_Color32(200,200,200,255)); // wings
            rect(ex + 16, ey + 4, 4, 4, C2D_Color32(200,200,200,255));
        } else if (e.type == ENEMY_SHOOTER) {
            rect(ex + 1, ey + 1, 14, 14, C2D_Color32(200,100,50,255));
            rectSolid(ex + 5, ey + 5, 6, 6, C2D_Color32(255,0,0,255));
        }
    }
    
    for (const auto& p : activeProjectiles) {
        rectSolid(p.x - cameraX - 2, p.y - cameraY - 2, 4, 4, C2D_Color32(255, 150, 0, 255));
    }
}

static void drawPlayer(const Theme& t) {
    if (player.invincible && (frameCounter % 4 < 2)) return; // Blink

    float px = player.x - cameraX;
    float py = player.y - cameraY;
    int bob = (std::abs(player.vx) > 0.2f && player.grounded) ? ((player.animation / 5) % 2) : 0;
    
    // Trail if dashing
    if (dashActive) {
        rectSolid(px - player.vx + 2, py + 5 - bob, 10, 10, C2D_Color32(t.player & 0xFFFFFF00 | 100));
        rectSolid(px - player.vx*2 + 2, py + 5 - bob, 10, 10, C2D_Color32(t.player & 0xFFFFFF00 | 50));
    }

    rect(px + 2, py + 5 - bob, 10, 10, t.player);
    rect(px + 3, py + 1 - bob, 8, 7, t.player);
    rect(px + 4, py + 4 - bob, 2, 2, C2D_Color32(255,255,255,255));
    rect(px + 9, py + 4 - bob, 2, 2, C2D_Color32(255,255,255,255));
    rect(px + (player.vx < 0 ? 4 : 5), py + 5 - bob, 1, 1, C2D_Color32(25,25,35,255));
    rect(px + (player.vx < 0 ? 8 : 9), py + 5 - bob, 1, 1, C2D_Color32(25,25,35,255));
}

static void drawGame() {
    int themeId = levels[currentLevel].theme % themes.size();
    const Theme& t = themes[themeId];
    C2D_TargetClear(top, t.sky);
    C2D_SceneBegin(top);

    drawBackground(t);
    drawTiles(t);
    drawEntities(t);
    drawParticles();
    drawPlayer(t);

    // Dynamic HUD
    rect(0, 0, SCREEN_W, 28, C2D_Color32(20,25,35,215));
    drawOutlinedText(10, 6, 0.55f, 0.55f, C2D_Color32(255,255,255,255), "%s", levels[currentLevel].name);
    
    // Coin pulse effect
    float coinScale = 0.52f + sinf(frameCounter * 0.1f) * 0.03f;
    drawOutlinedText(190, 6, coinScale, coinScale, t.coin, "COINS %03d", player.coins);
    
    // HP Display (Hearts)
    drawOutlinedText(300, 6, 0.52f, 0.52f, C2D_Color32(255,255,255,255), "HP");
    for(int i=0; i<3; i++) {
        u32 hc = (i < player.lives) ? C2D_Color32(255,80,80,255) : C2D_Color32(80,80,80,255);
        rect(330 + i * 18, 8, 12, 12, hc);
    }
    
    // Screen transitions
    if (transitionState == 1) { // Fade in
        transitionAlpha -= 10.0f;
        if (transitionAlpha <= 0) { transitionAlpha = 0; transitionState = 0; }
        C2D_DrawRectSolid(0, 0, 0, SCREEN_W, SCREEN_H, C2D_Color32(0,0,0,(u8)transitionAlpha));
    } else if (transitionState == 2) { // Fade out
        transitionAlpha += 10.0f;
        if (transitionAlpha >= 255.0f) {
            transitionAlpha = 255.0f;
            gameState = 4; // Win state
            transitionState = 0;
        }
        C2D_DrawRectSolid(0, 0, 0, SCREEN_W, SCREEN_H, C2D_Color32(0,0,0,(u8)transitionAlpha));
    }
}

static void bottomPanel(u32 color = C2D_Color32(25,28,38,255)) {
    C2D_TargetClear(bottom, color);
    C2D_SceneBegin(bottom);
}
static void bottomEnd() {}

static void drawButton(float x, float y, float w, float h, bool selected, const char* label, u32 accent = C2D_Color32(255,225,75,255)) {
    u32 bg = selected ? C2D_Color32(70,105,190,255) : C2D_Color32(45,50,68,255);
    rect(x, y, w, h, bg);
    if (selected) rect(x, y, 4, h, accent);
    drawShadowText(x + 12, y + 9, 0.58f, 0.58f, C2D_Color32(255,255,255,255), "%s", label);
}

static int menuSelection = 0;
static void updateMenu() {
    if (input.isPressed(KEY_DOWN)) menuSelection = (menuSelection + 1) % 5;
    if (input.isPressed(KEY_UP)) menuSelection = (menuSelection + 4) % 5;

    if (input.isPressed(KEY_A)) {
        if (menuSelection == 0) { currentLevel = 0; player.lives = 3; resetPlayer(); gameState = 1; }
        else if (menuSelection == 1) gameState = 2; // Level Select
        else if (menuSelection == 2) gameState = 6; // ProcGen Menu
        else if (menuSelection == 3) gameState = 7; // Stats/Achievements
        else running = false;
    }
}

static void drawMenu() {
    bottomPanel(C2D_Color32(18,22,34,255));
    drawShadowText(36, 20, 1.35f, 1.35f, C2D_Color32(255,235,75,255), "PIXEL RUNNER");
    drawText(38, 65, 0.48f, 0.48f, C2D_Color32(205,215,235,255), "Definitive Procedural Edition");

    drawButton(35, 95, 250, 26, menuSelection == 0, "CAMPAIGN");
    drawButton(35, 125, 250, 26, menuSelection == 1, "LEVEL SELECT");
    drawButton(35, 155, 250, 26, menuSelection == 2, "ENDLESS GENERATOR");
    drawButton(35, 185, 250, 26, menuSelection == 3, "ACHIEVEMENTS");
    drawButton(35, 215, 250, 26, menuSelection == 4, "QUIT");
    bottomEnd();
}

static int procMenuCursor = 0;
static void updateProcMenu() {
    if (input.isPressed(KEY_DOWN)) procMenuCursor = (procMenuCursor + 1) % 5;
    if (input.isPressed(KEY_UP)) procMenuCursor = (procMenuCursor + 4) % 5;
    
    if (input.isPressed(KEY_LEFT) || input.isHeld(KEY_L)) {
        if (procMenuCursor == 0) currentProcConfig.chunkCount = std::max(5, currentProcConfig.chunkCount - 1);
        if (procMenuCursor == 1) currentProcConfig.difficulty = std::max(1, currentProcConfig.difficulty - 1);
        if (procMenuCursor == 2) currentProcConfig.themeId = std::max(0, currentProcConfig.themeId - 1);
        if (procMenuCursor == 3) currentProcConfig.seed--;
    }
    if (input.isPressed(KEY_RIGHT) || input.isHeld(KEY_R)) {
        if (procMenuCursor == 0) currentProcConfig.chunkCount = std::min(100, currentProcConfig.chunkCount + 1);
        if (procMenuCursor == 1) currentProcConfig.difficulty = std::min(10, currentProcConfig.difficulty + 1);
        if (procMenuCursor == 2) currentProcConfig.themeId = std::min((int)themes.size()-1, currentProcConfig.themeId + 1);
        if (procMenuCursor == 3) currentProcConfig.seed++;
    }
    
    if (input.isPressed(KEY_B)) gameState = 0;
    if (input.isPressed(KEY_A) && procMenuCursor == 4) {
        if (currentProcConfig.difficulty > saveMgr.highestProcDifficulty) {
            saveMgr.highestProcDifficulty = currentProcConfig.difficulty;
        }
        Level procLvl = generateProceduralLevel(currentProcConfig);
        if (levels.size() < LEVEL_COUNT + 1) levels.push_back(procLvl);
        else levels[levels.size()-1] = procLvl;
        
        currentLevel = levels.size() - 1;
        player.lives = 3;
        resetPlayer();
        gameState = 1;
    }
}

static void drawProcMenu() {
    bottomPanel(C2D_Color32(30,20,40,255));
    drawShadowText(20, 15, 0.85f, 0.85f, C2D_Color32(100,220,255,255), "WORLD GENERATOR");
    
    drawButton(20, 55, 280, 28, procMenuCursor == 0, "LENGTH:");
    drawText(200, 64, 0.55f, 0.55f, C2D_Color32(255,255,255,255), "%d Chunks", currentProcConfig.chunkCount);
    
    drawButton(20, 90, 280, 28, procMenuCursor == 1, "DIFFICULTY:");
    drawText(200, 99, 0.55f, 0.55f, C2D_Color32(255,255,255,255), "Level %d", currentProcConfig.difficulty);
    
    drawButton(20, 125, 280, 28, procMenuCursor == 2, "THEME:");
    drawText(200, 134, 0.55f, 0.55f, C2D_Color32(255,255,255,255), "Style %d", currentProcConfig.themeId);
    
    drawButton(20, 160, 280, 28, procMenuCursor == 3, "SEED:");
    drawText(200, 169, 0.55f, 0.55f, C2D_Color32(255,255,255,255), "%d", currentProcConfig.seed);
    
    drawButton(20, 195, 280, 32, procMenuCursor == 4, "A  GENERATE & PLAY", C2D_Color32(100,255,100,255));
    bottomEnd();
}

static int pauseSelection = 0;
static void updatePause() {
    if (input.isPressed(KEY_UP)) pauseSelection = (pauseSelection + 2) % 3;
    if (input.isPressed(KEY_DOWN)) pauseSelection = (pauseSelection + 1) % 3;
    if (input.isPressed(KEY_B)) { gameState = 1; return; }
    if (input.isPressed(KEY_A)) {
        if (pauseSelection == 0) gameState = 1;
        else if (pauseSelection == 1) { resetPlayer(); gameState = 1; }
        else gameState = 0;
    }
}
static void drawPause() {
    bottomPanel(C2D_Color32(20,23,34,255));
    drawShadowText(100, 25, 1.2f, 1.2f, C2D_Color32(255,235,75,255), "PAUSED");
    drawButton(55, 75, 210, 32, pauseSelection == 0, "RESUME");
    drawButton(55, 115, 210, 32, pauseSelection == 1, "RESTART LEVEL");
    drawButton(55, 155, 210, 32, pauseSelection == 2, "MAIN MENU");
    bottomEnd();
}

static void updateWin() {
    if (input.isPressed(KEY_A)) {
        if (levels[currentLevel].isProcedural) gameState = 6;
        else if (currentLevel < LEVEL_COUNT - 1 && currentLevel < levels.size() - 2) { // don't jump to procgen slot
            currentLevel++; resetPlayer(); gameState = 1;
        } else gameState = 0;
    }
}
static void drawWin() {
    bottomPanel(C2D_Color32(20,28,38,255));
    drawShadowText(65, 30, 1.1f, 1.1f, C2D_Color32(100,255,100,255), "LEVEL CLEAR!");
    drawText(68, 75, 0.65f, 0.65f, C2D_Color32(235,240,250,255), "%s", levels[currentLevel].name);
    
    rect(68, 105, 180, 2, C2D_Color32(100,100,100,255));
    drawText(68, 120, 0.60f, 0.60f, C2D_Color32(255,215,60,255), "Coins Collected: %d", player.coins);
    
    drawButton(70, 175, 180, 32, true, "A  CONTINUE", C2D_Color32(100,255,100,255));
    bottomEnd();
}

static void updateGameOver() {
    if (input.isPressed(KEY_A)) { player.lives = 3; resetPlayer(); gameState = 1; }
    if (input.isPressed(KEY_B)) gameState = 0;
}
static void drawGameOver() {
    bottomPanel(C2D_Color32(35,20,28,255));
    drawShadowText(70, 40, 1.1f, 1.1f, C2D_Color32(255,80,80,255), "GAME OVER");
    drawText(75, 80, 0.5f, 0.5f, C2D_Color32(200,200,200,255), "Don't give up!");
    
    drawButton(65, 125, 190, 32, true, "A  RETRY LEVEL");
    drawButton(65, 165, 190, 32, false, "B  MAIN MENU");
    bottomEnd();
}

static int levelCursor = 0;
static void updateLevelSelect() {
    if (input.isPressed(KEY_RIGHT)) levelCursor = std::min(levelCursor + 1, unlockedLevel);
    if (input.isPressed(KEY_LEFT)) levelCursor = std::max(levelCursor - 1, 0);
    if (input.isPressed(KEY_B)) gameState = 0;
    if (input.isPressed(KEY_A) && levelCursor <= unlockedLevel) {
        currentLevel = levelCursor; player.lives = 3; resetPlayer(); gameState = 1;
    }
}
static void drawLevelSelect() {
    bottomPanel(C2D_Color32(17,20,30,255));
    drawShadowText(20, 15, 0.9f, 0.9f, C2D_Color32(255,235,75,255), "CAMPAIGN SELECT");
    
    for (int i = 0; i < 3; ++i) { // Show up to 3 static levels
        float x = 20 + i * 90;
        bool unlocked = i <= unlockedLevel;
        bool sel = i == levelCursor;
        u32 bg = unlocked ? (sel ? C2D_Color32(70,105,190,255) : C2D_Color32(45,50,65,255)) : C2D_Color32(38,40,48,255);
        rect(x, 65, 80, 80, bg);
        if (sel) rect(x, 65, 80, 4, C2D_Color32(255,225,75,255));
        
        if (unlocked) {
            drawText(x + 25, 95, 0.9f, 0.9f, C2D_Color32(255,255,255,255), "%02d", i + 1);
        } else {
            drawText(x + 30, 95, 0.7f, 0.7f, C2D_Color32(100,100,100,255), "LOCKED");
        }
    }
    
    drawText(20, 205, 0.5f, 0.5f, C2D_Color32(200,200,200,255), "B to Return");
    bottomEnd();
}

static void updateStats() {
    if (input.isPressed(KEY_B)) gameState = 0;
}
static void drawStats() {
    bottomPanel(C2D_Color32(25,35,45,255));
    drawShadowText(20, 15, 0.8f, 0.8f, C2D_Color32(255,200,100,255), "PLAYER STATS & ACHIEVEMENTS");
    
    drawText(25, 55, 0.55f, 0.55f, C2D_Color32(255,255,255,255), "Total Coins: %d", saveMgr.totalCoins);
    drawText(25, 75, 0.55f, 0.55f, C2D_Color32(255,255,255,255), "Total Deaths: %d", saveMgr.totalDeaths);
    drawText(25, 95, 0.55f, 0.55f, C2D_Color32(255,255,255,255), "Levels Beaten: %d", saveMgr.totalLevelsBeaten);
    
    rect(20, 125, 280, 2, C2D_Color32(100,100,100,255));
    
    drawText(25, 135, 0.5f, 0.5f, saveMgr.achievements[0] ? C2D_Color32(255,255,0,255) : C2D_Color32(100,100,100,255), "[ %c ] 100 Coins Collected", saveMgr.achievements[0] ? 'X' : ' ');
    drawText(25, 155, 0.5f, 0.5f, saveMgr.achievements[2] ? C2D_Color32(255,255,0,255) : C2D_Color32(100,100,100,255), "[ %c ] Beat 10 Levels", saveMgr.achievements[2] ? 'X' : ' ');
    drawText(25, 175, 0.5f, 0.5f, saveMgr.achievements[4] ? C2D_Color32(255,255,0,255) : C2D_Color32(100,100,100,255), "[ %c ] Survive ProcGen Diff 5", saveMgr.achievements[4] ? 'X' : ' ');
    
    drawText(20, 210, 0.5f, 0.5f, C2D_Color32(200,200,200,255), "B to Return");
    bottomEnd();
}

static bool initGraphics() {
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    systemFont = C2D_FontLoadSystem(CFG_REGION_EUR);
    textBuf = C2D_TextBufNew(32768);
    return top != nullptr && bottom != nullptr;
}

static void shutdownGraphics() {
    C2D_TextBufDelete(textBuf);
    C2D_FontFree(systemFont);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

int main() {
    if (!initGraphics()) return 1;

    saveMgr.load();
    buildStaticLevels();
    initTemplates(); 
    gameState = 0;

    while (aptMainLoop() && running) {
        input.update();

        if (gameState == 1 && input.isPressed(KEY_START)) gameState = 3;
        else if (gameState == 3 && input.isPressed(KEY_START)) gameState = 1;

        if (gameState == 0) updateMenu();
        else if (gameState == 1) {
            updatePlayer();
            updateDash();
            updateDoubleJump();
            updateEntities();
            updateParticles();
        }
        else if (gameState == 2) updateLevelSelect();
        else if (gameState == 3) updatePause();
        else if (gameState == 4) updateWin();
        else if (gameState == 5) updateGameOver();
        else if (gameState == 6) updateProcMenu();
        else if (gameState == 7) updateStats();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(textBuf);

        if (gameState == 0) drawMenu();
        else if (gameState == 1) {
            drawGame();
            bottomPanel(C2D_Color32(22,26,37,255));
            drawShadowText(20, 15, 0.85f, 0.85f, C2D_Color32(100,220,255,255), "CONTROLS");
            
            rect(20, 50, 280, 2, C2D_Color32(100,100,100,255));
            
            drawText(20, 65, 0.55f, 0.55f, C2D_Color32(230,230,230,255), "D-Pad Left/Right : Move");
            drawText(20, 90, 0.55f, 0.55f, C2D_Color32(230,230,230,255), "A or B Button    : Jump / Double Jump");
            drawText(20, 115, 0.55f, 0.55f, C2D_Color32(230,230,230,255), "X or Y Button    : Sprint");
            drawText(20, 140, 0.55f, 0.55f, C2D_Color32(230,230,230,255), "R Shoulder       : Air Dash");
            drawText(20, 165, 0.55f, 0.55f, C2D_Color32(230,230,230,255), "START            : Pause Game");
            
            drawText(20, 210, 0.45f, 0.45f, C2D_Color32(150,150,150,255), "Watch out for moving platforms and spikes!");
            bottomEnd();
        }
        else if (gameState == 2) drawLevelSelect();
        else if (gameState == 3) drawPause();
        else if (gameState == 4) drawWin();
        else if (gameState == 5) drawGameOver();
        else if (gameState == 6) drawProcMenu();
        else if (gameState == 7) drawStats();

        C3D_FrameEnd(0);
        frameCounter++;
    }

    saveMgr.save();
    shutdownGraphics();
    return 0;
}
