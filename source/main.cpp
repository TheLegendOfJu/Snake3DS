#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <ctime>
#include <math.h>
#include <algorithm>

// ============================================================================
// SNAKE 3DS - REWORKED EDITION
// ============================================================================
// Main fixes / improvements:
//  - safer direction queue (no accidental 180-degree turns)
//  - tail-cell collision is handled correctly when the snake is not growing
//  - apples are spawned with a hard attempt limit, so no infinite loop
//  - configuration is validated before starting a game
//  - touch buttons have cleaner hitboxes and pressed feedback
//  - UI is redesigned for the two 3DS screens
//  - score/highscore layout is clearer
//  - game timing uses elapsed time instead of assuming one exact FPS
//  - pause button / HOME-safe cleanup is handled by the normal loop
//  - settings and configuration have explicit selected states
//  - large boards fit the 400x240 top screen exactly
// ============================================================================

#define TOP_WIDTH       400
#define TOP_HEIGHT      240
#define BOT_WIDTH       320
#define BOT_HEIGHT      240

#define CELL_SMALL      10
#define CELL_MEDIUM     10
#define CELL_LARGE      10

#define MAX_APPLES      50
#define MAX_SNAKE_LEN   960
#define FRAME_MS        16

// ---------------------------------------------------------------------------
// Game states
// ---------------------------------------------------------------------------
enum GameState
{
    STATE_MAIN_MENU,
    STATE_SETTINGS,
    STATE_CONFIG,
    STATE_PLAYING,
    STATE_GAME_OVER
};

// ---------------------------------------------------------------------------
// Directions
// ---------------------------------------------------------------------------
enum Direction
{
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
};

// ---------------------------------------------------------------------------
// Basic color type
// ---------------------------------------------------------------------------
struct Color
{
    u8 r;
    u8 g;
    u8 b;
};

static bool sameColor(const Color& a, const Color& b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// ---------------------------------------------------------------------------
// Small 8x8 ASCII font.
// ---------------------------------------------------------------------------
const uint64_t font8x8[96] = {
    0x0000000000000000, 0x00183C3C18180018, 0x0066666600000000, 0x0036367F367F3636,
    0x000C1EE07C0F7830, 0x000063660C1833C6, 0x00386C6C386D663B, 0x000C181800000000,
    0x0018306060603018, 0x0060301818183060, 0x0000663CFF3C6600, 0x000018187E181800,
    0x0000000000181830, 0x000000007E000000, 0x0000000000001818, 0x00060C183060C080,
    0x003C66666666663C, 0x001838181818187E, 0x003C66060C30607E, 0x003C66061C06663C,
    0x001C3C6C6C7E0C0C, 0x007E607C0606663C, 0x003C607C6666663C, 0x007E060C18303030,
    0x003C66663C66663C, 0x003C6666663E063C, 0x0000181800001818, 0x0000181800181830,
    0x000C18306030180C, 0x0000007E007E0000, 0x006030180C183060, 0x003C66060C180018,
    0x003C666E6E60623C, 0x00183C66667E6666, 0x007C66667C66667C, 0x003C66606060663C,
    0x00786C6666666C78, 0x007E60607860607E, 0x007E606078606060, 0x003C66606E66663C,
    0x006666667E666666, 0x003C18181818183C, 0x000606060606663C, 0x00666C7870786C66,
    0x006060606060607E, 0x0063777F6B636363, 0x0066767E7E6E6666, 0x003C66666666663C,
    0x007C66667C606060, 0x003C6666666A6C36, 0x007C66667C6C6666, 0x003C66603C06663C,
    0x007E181818181818, 0x006666666666663C, 0x0066666666663C18, 0x006363636B7F7763,
    0x0066663C183C6666, 0x006666663C181818, 0x007E060C1830607E, 0x003C30303030303C,
    0x0000406030180C06, 0x003C0C0C0C0C0C3C, 0x00081C3663000000, 0x00000000000000FF,
    0x0018180000000000, 0x0000003C063E663E, 0x0060607C6666667C, 0x0000003C6060603C,
    0x0006063E6666663E, 0x0000003C667E603C, 0x001C30307C303030, 0x0000003E66663E06,
    0x0060607C66666666, 0x001800381818183C, 0x000C000C0C0C0C38, 0x006060666C786C66,
    0x003818181818183C, 0x000000667F7F6B63, 0x0000007C66666666, 0x0000003C6666663C,
    0x0000007C66667C60, 0x0000003E66663E06, 0x0000007C66606060, 0x0000003E603C063C,
    0x0030307C3030301C, 0x000000666666663E, 0x0000006666663C18, 0x000000636B7F7F36,
    0x000000663C183C66, 0x0000006666663E06, 0x0000007E0C18307E, 0x000E18187018180E,
    0x0018181818181818, 0x007018180E181870, 0x00000000324C0000
};

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------
struct Point
{
    int x;
    int y;
};

struct Apple
{
    Point pos;
    bool gold;
};

// ---------------------------------------------------------------------------
// UI button
// ---------------------------------------------------------------------------
struct Button
{
    int x;
    int y;
    int w;
    int h;
    const char* text;

    bool contains(int px, int py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    bool isClicked(const touchPosition& touch) const
    {
        return contains(touch.px, touch.py);
    }
};

// ---------------------------------------------------------------------------
// Global settings
// ---------------------------------------------------------------------------
bool white_mode = false;
int highscore = 0;
int volume = 50;

// Grid sizes are deliberately selected so 40x24 at 10px/cell exactly fills
// the 400x240 top screen. This also fixes the large-board clipping issue.
enum GridSize
{
    SIZE_SMALL,
    SIZE_MEDIUM,
    SIZE_LARGE
};

GridSize current_size = SIZE_MEDIUM;
int apple_count = 3;
int speed_multiplier = 1;

// ---------------------------------------------------------------------------
// Game variables
// ---------------------------------------------------------------------------
std::vector<Point> snake;
std::vector<Apple> apples;

Direction current_dir = DIR_RIGHT;
Direction next_dir = DIR_RIGHT;
Direction queued_dir = DIR_RIGHT;

int score = 0;
int grid_w = 30;
int grid_h = 18;
int cell_size = 10;
int offset_x = 0;
int offset_y = 0;

unsigned int global_frame = 0;
unsigned int last_tick_ms = 0;
float move_accumulator = 0.0f;

bool game_paused = false;
bool touch_was_down = false;
bool exit_requested = false;

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
Color COL_BLACK      = {  8,   8,  12};
Color COL_WHITE      = {248, 248, 248};
Color COL_PANEL      = { 25,  28,  38};
Color COL_PANEL2     = { 38,  42,  56};
Color COL_BORDER     = { 90, 100, 125};
Color COL_GREEN      = { 55, 220, 105};
Color COL_GREEN_DARK = { 20, 125,  60};
Color COL_GREEN_LITE = {130, 255, 160};
Color COL_RED        = {240,  60,  65};
Color COL_GOLD       = {255, 205,  45};
Color COL_BLUE       = { 65, 140, 255};
Color COL_GRAY       = { 90,  95, 110};
Color COL_DARKGRAY   = { 42,  45,  55};
Color COL_SHADOW     = {  0,   0,   0};
Color COL_CYAN       = { 50, 220, 230};

// ---------------------------------------------------------------------------
// Framebuffer helpers
// ---------------------------------------------------------------------------
void drawPixel(u8* fb, int x, int y, int screen_width, int screen_height, Color c)
{
    if (!fb) return;
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height) return;

    int index = ((x * screen_height) + (screen_height - 1 - y)) * 3;
    fb[index + 0] = c.b;
    fb[index + 1] = c.g;
    fb[index + 2] = c.r;
}

void fillRect(u8* fb, int x, int y, int w, int h,
             int screen_width, int screen_height, Color c)
{
    if (w <= 0 || h <= 0) return;

    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(screen_width, x + w);
    int y1 = std::min(screen_height, y + h);

    for (int px = x0; px < x1; ++px)
    {
        for (int py = y0; py < y1; ++py)
        {
            drawPixel(fb, px, py, screen_width, screen_height, c);
        }
    }
}

void drawRect(u8* fb, int x, int y, int w, int h,
             int screen_width, int screen_height, Color c)
{
    if (w <= 0 || h <= 0) return;

    for (int i = 0; i < w; ++i)
    {
        drawPixel(fb, x + i, y, screen_width, screen_height, c);
        drawPixel(fb, x + i, y + h - 1, screen_width, screen_height, c);
    }

    for (int j = 0; j < h; ++j)
    {
        drawPixel(fb, x, y + j, screen_width, screen_height, c);
        drawPixel(fb, x + w - 1, y + j, screen_width, screen_height, c);
    }
}

void drawLineH(u8* fb, int x, int y, int w,
               int screen_width, int screen_height, Color c)
{
    for (int i = 0; i < w; ++i)
        drawPixel(fb, x + i, y, screen_width, screen_height, c);
}

void drawLineV(u8* fb, int x, int y, int h,
               int screen_width, int screen_height, Color c)
{
    for (int i = 0; i < h; ++i)
        drawPixel(fb, x, y + i, screen_width, screen_height, c);
}

void clearScreen(u8* fb, int screen_width, int screen_height, Color c)
{
    if (!fb) return;

    for (int x = 0; x < screen_width; ++x)
    {
        for (int y = 0; y < screen_height; ++y)
        {
            drawPixel(fb, x, y, screen_width, screen_height, c);
        }
    }
}

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------
void drawChar(u8* fb, int x, int y, char c, int scale,
              int screen_width, int screen_height, Color fg)
{
    if (c < 32 || c > 127 || scale <= 0) return;

    uint64_t bitmap = font8x8[c - 32];

    for (int row = 0; row < 8; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            if (bitmap & (1ULL << (63 - (row * 8 + col))))
            {
                fillRect(fb,
                         x + col * scale,
                         y + row * scale,
                         scale,
                         scale,
                         screen_width,
                         screen_height,
                         fg);
            }
        }
    }
}

void drawString(u8* fb, int x, int y, const char* str, int scale,
                int screen_width, int screen_height, Color fg)
{
    if (!str) return;

    int curr_x = x;

    while (*str)
    {
        drawChar(fb, curr_x, y, *str, scale,
                 screen_width, screen_height, fg);
        curr_x += 8 * scale;
        ++str;
    }
}

int textWidth(const char* str, int scale)
{
    if (!str) return 0;
    return (int)strlen(str) * 8 * scale;
}

void drawCenteredText(u8* fb, int center_x, int y, const char* str,
                      int scale, int screen_width, int screen_height, Color c)
{
    int w = textWidth(str, scale);
    drawString(fb, center_x - w / 2, y, str, scale,
               screen_width, screen_height, c);
}

void drawShadowedText(u8* fb, int x, int y, const char* str, int scale,
                      int screen_width, int screen_height, Color fg)
{
    drawString(fb, x + 2, y + 2, str, scale,
               screen_width, screen_height, COL_SHADOW);
    drawString(fb, x, y, str, scale,
               screen_width, screen_height, fg);
}

void drawCenteredShadowedText(u8* fb, int center_x, int y, const char* str,
                              int scale, int screen_width, int screen_height,
                              Color fg)
{
    int w = textWidth(str, scale);
    drawShadowedText(fb, center_x - w / 2, y, str, scale,
                     screen_width, screen_height, fg);
}

// ---------------------------------------------------------------------------
// General UI helpers
// ---------------------------------------------------------------------------
Color backgroundColor()
{
    return white_mode ? COL_WHITE : COL_BLACK;
}

Color foregroundColor()
{
    return white_mode ? COL_BLACK : COL_WHITE;
}

Color panelColor()
{
    return white_mode ? Color{232, 232, 238} : COL_PANEL;
}

Color panelSelectedColor()
{
    return white_mode ? Color{190, 210, 225} : Color{55, 65, 85};
}

Color borderColor()
{
    return white_mode ? Color{70, 70, 80} : COL_BORDER;
}

void drawPanel(u8* fb, int x, int y, int w, int h,
               int screen_width, int screen_height)
{
    fillRect(fb, x + 2, y + 2, w, h, screen_width, screen_height, COL_SHADOW);
    fillRect(fb, x, y, w, h, screen_width, screen_height, panelColor());
    drawRect(fb, x, y, w, h, screen_width, screen_height, borderColor());
}

void drawButton(u8* fb, const Button& b, bool selected, bool pressed)
{
    Color fg = foregroundColor();
    Color bg = selected ? panelSelectedColor() : panelColor();

    if (pressed)
    {
        bg = white_mode ? Color{175, 195, 210} : Color{65, 75, 95};
    }

    fillRect(fb, b.x + 2, b.y + 2, b.w, b.h,
             BOT_WIDTH, BOT_HEIGHT, COL_SHADOW);
    fillRect(fb, b.x, b.y, b.w, b.h,
             BOT_WIDTH, BOT_HEIGHT, bg);
    drawRect(fb, b.x, b.y, b.w, b.h,
             BOT_WIDTH, BOT_HEIGHT, selected ? COL_CYAN : borderColor());

    int tw = textWidth(b.text, 2);
    int tx = b.x + (b.w - tw) / 2;
    int ty = b.y + (b.h - 16) / 2;

    if (tx < b.x + 2) tx = b.x + 2;
    drawString(fb, tx, ty, b.text, 2,
               BOT_WIDTH, BOT_HEIGHT, fg);
}

void drawTopHeader(u8* fb, const char* title, Color accent)
{
    fillRect(fb, 0, 0, TOP_WIDTH, 28, TOP_WIDTH, TOP_HEIGHT,
             white_mode ? Color{220, 220, 225} : Color{18, 20, 28});
    drawLineH(fb, 0, 27, TOP_WIDTH, TOP_WIDTH, TOP_HEIGHT, accent);
    drawShadowedText(fb, 12, 7, title, 2,
                     TOP_WIDTH, TOP_HEIGHT, foregroundColor());
}

void drawBottomHeader(u8* fb, const char* title)
{
    fillRect(fb, 0, 0, BOT_WIDTH, 26, BOT_WIDTH, BOT_HEIGHT,
             white_mode ? Color{220, 220, 225} : Color{18, 20, 28});
    drawLineH(fb, 0, 25, BOT_WIDTH, BOT_WIDTH, BOT_HEIGHT, COL_CYAN);
    drawShadowedText(fb, 10, 6, title, 2,
                     BOT_WIDTH, BOT_HEIGHT, foregroundColor());
}

// ---------------------------------------------------------------------------
// Animation helpers
// ---------------------------------------------------------------------------
Color getRainbow(unsigned int frame)
{
    double t = frame * 0.045;
    Color c;
    c.r = (u8)(sin(t) * 127.0 + 128.0);
    c.g = (u8)(sin(t + 2.094) * 127.0 + 128.0);
    c.b = (u8)(sin(t + 4.188) * 127.0 + 128.0);
    return c;
}

// ---------------------------------------------------------------------------
// Save / load
// ---------------------------------------------------------------------------
void loadHighscore()
{
    FILE* f = fopen("sdmc:/snake_highscore.txt", "r");

    if (!f)
    {
        highscore = 0;
        return;
    }

    if (fscanf(f, "%d", &highscore) != 1)
        highscore = 0;

    fclose(f);

    if (highscore < 0)
        highscore = 0;
}

void saveHighscore()
{
    FILE* f = fopen("sdmc:/snake_highscore.txt", "w");

    if (!f)
        return;

    fprintf(f, "%d\n", highscore);
    fclose(f);
}

void applyVolume()
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    ndspSetMasterVol(volume / 100.0f);
}

// ---------------------------------------------------------------------------
// Grid setup
// ---------------------------------------------------------------------------
void calculateGrid()
{
    switch (current_size)
    {
        case SIZE_SMALL:
            grid_w = 20;
            grid_h = 12;
            cell_size = CELL_SMALL;
            break;

        case SIZE_MEDIUM:
            grid_w = 30;
            grid_h = 18;
            cell_size = CELL_MEDIUM;
            break;

        case SIZE_LARGE:
            grid_w = 40;
            grid_h = 24;
            cell_size = CELL_LARGE;
            break;
    }

    offset_x = (TOP_WIDTH - grid_w * cell_size) / 2;
    offset_y = (TOP_HEIGHT - grid_h * cell_size) / 2;
}

// ---------------------------------------------------------------------------
// Direction helpers
// ---------------------------------------------------------------------------
bool isOpposite(Direction a, Direction b)
{
    if (a == DIR_UP && b == DIR_DOWN) return true;
    if (a == DIR_DOWN && b == DIR_UP) return true;
    if (a == DIR_LEFT && b == DIR_RIGHT) return true;
    if (a == DIR_RIGHT && b == DIR_LEFT) return true;
    return false;
}

bool isHorizontal(Direction d)
{
    return d == DIR_LEFT || d == DIR_RIGHT;
}

bool isVertical(Direction d)
{
    return d == DIR_UP || d == DIR_DOWN;
}

void requestDirection(Direction requested)
{
    // Always compare with the direction that will actually be used for the
    // next move. This fixes the classic rapid-input 180-degree bug.
    if (isOpposite(requested, current_dir))
        return;

    if (requested == queued_dir)
        return;

    queued_dir = requested;
}

void commitDirection()
{
    if (!isOpposite(queued_dir, current_dir))
        current_dir = queued_dir;
}

// ---------------------------------------------------------------------------
// Point helpers
// ---------------------------------------------------------------------------
bool samePoint(const Point& a, const Point& b)
{
    return a.x == b.x && a.y == b.y;
}

bool pointInsideGrid(const Point& p)
{
    return p.x >= 0 && p.x < grid_w && p.y >= 0 && p.y < grid_h;
}

bool pointOnSnake(const Point& p, size_t startIndex = 0)
{
    if (startIndex >= snake.size()) return false;

    for (size_t i = startIndex; i < snake.size(); ++i)
    {
        if (samePoint(snake[i], p))
            return true;
    }

    return false;
}

bool pointOnApple(const Point& p)
{
    for (size_t i = 0; i < apples.size(); ++i)
    {
        if (samePoint(apples[i].pos, p))
            return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Apple spawning
// ---------------------------------------------------------------------------
bool findFreeCell(Point& result)
{
    const int totalCells = grid_w * grid_h;

    if (totalCells <= 0)
        return false;

    // Random probing is fast on normal games.
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        Point p;
        p.x = rand() % grid_w;
        p.y = rand() % grid_h;

        if (!pointOnSnake(p) && !pointOnApple(p))
        {
            result = p;
            return true;
        }
    }

    // Deterministic fallback prevents an infinite loop when the board is
    // almost completely full.
    for (int y = 0; y < grid_h; ++y)
    {
        for (int x = 0; x < grid_w; ++x)
        {
            Point p = {x, y};

            if (!pointOnSnake(p) && !pointOnApple(p))
            {
                result = p;
                return true;
            }
        }
    }

    return false;
}

bool spawnApple()
{
    if ((int)apples.size() >= MAX_APPLES)
        return false;

    Point freeCell;

    if (!findFreeCell(freeCell))
        return false;

    Apple a;
    a.pos = freeCell;
    a.gold = (rand() % 100) < 15;
    apples.push_back(a);
    return true;
}

void fillApples()
{
    while ((int)apples.size() < apple_count)
    {
        if (!spawnApple())
            break;
    }
}

// ---------------------------------------------------------------------------
// Game reset
// ---------------------------------------------------------------------------
void resetGame()
{
    calculateGrid();

    snake.clear();
    apples.clear();

    score = 0;
    current_dir = DIR_RIGHT;
    queued_dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;

    move_accumulator = 0.0f;
    last_tick_ms = osGetTime();
    game_paused = false;

    int startX = grid_w / 2;
    int startY = grid_h / 2;

    // Small boards still get a stable three-segment starting snake.
    snake.push_back({startX, startY});
    snake.push_back({startX - 1, startY});
    snake.push_back({startX - 2, startY});

    fillApples();
}

// ---------------------------------------------------------------------------
// Game speed
// ---------------------------------------------------------------------------
float moveIntervalMs()
{
    switch (speed_multiplier)
    {
        case 0: return 320.0f; // 0.5x
        case 1: return 160.0f; // 1.0x
        case 2: return 80.0f;  // 2.0x
        default: return 160.0f;
    }
}

const char* speedLabel()
{
    switch (speed_multiplier)
    {
        case 0: return "0.5x";
        case 1: return "1.0x";
        case 2: return "2.0x";
        default: return "1.0x";
    }
}

const char* sizeLabel()
{
    switch (current_size)
    {
        case SIZE_SMALL: return "SMALL";
        case SIZE_MEDIUM: return "MEDIUM";
        case SIZE_LARGE: return "LARGE";
        default: return "MEDIUM";
    }
}

// ---------------------------------------------------------------------------
// Game movement
// ---------------------------------------------------------------------------
Point nextHeadPosition()
{
    Point head = snake.front();
    Point result = head;

    switch (current_dir)
    {
        case DIR_UP:    result.y -= 1; break;
        case DIR_DOWN:  result.y += 1; break;
        case DIR_LEFT:  result.x -= 1; break;
        case DIR_RIGHT: result.x += 1; break;
    }

    return result;
}

int findAppleAt(const Point& p)
{
    for (size_t i = 0; i < apples.size(); ++i)
    {
        if (samePoint(apples[i].pos, p))
            return (int)i;
    }

    return -1;
}

bool wouldHitSnake(const Point& newHead, bool growing)
{
    if (snake.empty())
        return false;

    // Important bug fix:
    // If the snake is NOT growing, the current tail moves away during this
    // tick. Therefore moving into the old tail is legal.
    size_t checkEnd = snake.size();

    if (!growing && checkEnd > 0)
        --checkEnd;

    for (size_t i = 0; i < checkEnd; ++i)
    {
        if (samePoint(snake[i], newHead))
            return true;
    }

    return false;
}

bool updateSnakeOneTick()
{
    if (snake.empty())
        return false;

    commitDirection();

    Point newHead = nextHeadPosition();
    bool outside = !pointInsideGrid(newHead);

    if (outside)
        return false;

    int appleIndex = findAppleAt(newHead);
    bool growing = appleIndex >= 0;

    if (wouldHitSnake(newHead, growing))
        return false;

    snake.insert(snake.begin(), newHead);

    if (growing)
    {
        bool gold = apples[appleIndex].gold;
        score += gold ? 50 : 10;
        apples.erase(apples.begin() + appleIndex);

        // Maintain the configured apple count unless the board is full.
        spawnApple();
    }
    else
    {
        snake.pop_back();
    }

    if ((int)snake.size() >= grid_w * grid_h)
        return false;

    return true;
}

bool updateGame(float dtMs)
{
    if (game_paused)
        return true;

    move_accumulator += dtMs;

    const float interval = moveIntervalMs();

    // Cap catch-up work after lag. Without a cap, a lag spike can cause many
    // moves in one frame and make touch/D-pad input feel broken.
    int safetyTicks = 0;

    while (move_accumulator >= interval && safetyTicks < 4)
    {
        move_accumulator -= interval;
        ++safetyTicks;

        if (!updateSnakeOneTick())
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Input helpers
// ---------------------------------------------------------------------------
void readPhysicalDirection(u32 kDown, const circlePosition& circle)
{
    if (kDown & KEY_DUP)
        requestDirection(DIR_UP);
    else if (kDown & KEY_DDOWN)
        requestDirection(DIR_DOWN);
    else if (kDown & KEY_DLEFT)
        requestDirection(DIR_LEFT);
    else if (kDown & KEY_DRIGHT)
        requestDirection(DIR_RIGHT);

    // Circle pad is deliberately checked only when it crosses a useful
    // threshold. This prevents tiny stick drift from changing direction.
    if (circle.dy > 60)
        requestDirection(DIR_UP);
    else if (circle.dy < -60)
        requestDirection(DIR_DOWN);
    else if (circle.dx < -60)
        requestDirection(DIR_LEFT);
    else if (circle.dx > 60)
        requestDirection(DIR_RIGHT);
}

// ---------------------------------------------------------------------------
// Button layout
// ---------------------------------------------------------------------------
Button BTN_START    = { 55,  45, 210, 42, "START" };
Button BTN_SETTINGS = { 55,  98, 210, 42, "SETTINGS" };
Button BTN_QUIT     = { 55, 151, 210, 42, "QUIT" };

Button BTN_BACK     = { 10, 198, 82, 32, "BACK" };
Button BTN_WHITE    = { 100, 198, 100, 32, "THEME" };
Button BTN_VOL_DOWN = { 210, 198, 42, 32, "-" };
Button BTN_VOL_UP   = { 262, 198, 42, 32, "+" };

Button BTN_SIZE_S   = { 10,  54, 92, 34, "SMALL" };
Button BTN_SIZE_M   = { 114, 54, 92, 34, "MEDIUM" };
Button BTN_SIZE_L   = { 218, 54, 92, 34, "LARGE" };

Button BTN_APPLE_1  = { 10, 105, 56, 34, "1" };
Button BTN_APPLE_3  = { 74, 105, 56, 34, "3" };
Button BTN_APPLE_5  = { 138,105, 56, 34, "5" };
Button BTN_APPLE_M   = { 204,105, 48, 34, "-" };
Button BTN_APPLE_P   = { 262,105, 48, 34, "+" };

Button BTN_SPEED_05 = { 10, 155, 92, 34, "0.5X" };
Button BTN_SPEED_1  = { 114,155, 92, 34, "1.0X" };
Button BTN_SPEED_2  = { 218,155, 92, 34, "2.0X" };
Button BTN_PLAY     = { 108, 199,104, 34, "PLAY" };

Button BTN_UP       = { 130, 30, 60, 48, "^" };
Button BTN_DOWN     = { 130,162, 60, 48, "V" };
Button BTN_LEFT     = { 50, 96, 60, 48, "<" };
Button BTN_RIGHT    = { 210,96, 60, 48, ">" };
Button BTN_PAUSE    = { 120, 216,80, 20, "PAUSE" };

Button BTN_RETRY    = { 55,  65, 210, 42, "RETRY" };
Button BTN_MENU     = { 55, 120, 210, 42, "MAIN MENU" };

// ---------------------------------------------------------------------------
// Main menu rendering
// ---------------------------------------------------------------------------
void renderMainMenu(u8* top, u8* bottom)
{
    Color fg = foregroundColor();

    drawCenteredShadowedText(top, TOP_WIDTH / 2, 55,
                             "SNAKE", 4,
                             TOP_WIDTH, TOP_HEIGHT,
                             getRainbow(global_frame));

    drawCenteredText(top, TOP_WIDTH / 2, 94,
                     "3DS EDITION", 1,
                     TOP_WIDTH, TOP_HEIGHT,
                     fg);

    char hs[48];
    sprintf(hs, "HIGH SCORE  %d", highscore);
    drawCenteredText(top, TOP_WIDTH / 2, 120,
                     hs, 2,
                     TOP_WIDTH, TOP_HEIGHT,
                     COL_GOLD);

    drawCenteredText(top, TOP_WIDTH / 2, 152,
                     "EAT APPLES. GROW. SURVIVE.", 1,
                     TOP_WIDTH, TOP_HEIGHT,
                     fg);

    drawBottomHeader(bottom, "MAIN MENU");

    drawButton(bottom, BTN_START, false, false);
    drawButton(bottom, BTN_SETTINGS, false, false);
    drawButton(bottom, BTN_QUIT, false, false);
}

// ---------------------------------------------------------------------------
// Settings rendering
// ---------------------------------------------------------------------------
void renderSettings(u8* top, u8* bottom)
{
    Color fg = foregroundColor();

    drawTopHeader(top, "SETTINGS", COL_CYAN);

    drawCenteredText(top, TOP_WIDTH / 2, 52,
                     "CONTROLS", 2,
                     TOP_WIDTH, TOP_HEIGHT, COL_CYAN);

    drawPanel(top, 45, 82, 310, 118, TOP_WIDTH, TOP_HEIGHT);

    drawString(top, 65, 98, "D-PAD", 2,
               TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 205, 98, "MOVE", 2,
               TOP_WIDTH, TOP_HEIGHT, COL_GREEN);

    drawString(top, 65, 130, "CIRCLE PAD", 2,
               TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 205, 130, "MOVE", 2,
               TOP_WIDTH, TOP_HEIGHT, COL_GREEN);

    drawString(top, 65, 162, "TOUCH", 2,
               TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 205, 162, "ARROWS", 2,
               TOP_WIDTH, TOP_HEIGHT, COL_GREEN);

    drawBottomHeader(bottom, "OPTIONS");

    drawButton(bottom, BTN_BACK, false, false);
    drawButton(bottom, BTN_WHITE, false, false);
    drawButton(bottom, BTN_VOL_DOWN, false, false);
    drawButton(bottom, BTN_VOL_UP, false, false);

    char volumeText[32];
    sprintf(volumeText, "VOL %d%%", volume);
    drawCenteredText(bottom, 160, 160,
                     volumeText, 2,
                     BOT_WIDTH, BOT_HEIGHT,
                     COL_GOLD);

    drawString(bottom, 108, 183,
               white_mode ? "LIGHT" : "DARK", 1,
               BOT_WIDTH, BOT_HEIGHT, fg);
}

// ---------------------------------------------------------------------------
// Configuration rendering
// ---------------------------------------------------------------------------
void renderConfig(u8* top, u8* bottom)
{
    Color fg = foregroundColor();

    drawTopHeader(top, "GAME SETUP", COL_GREEN);

    drawCenteredShadowedText(top, TOP_WIDTH / 2, 62,
                             "READY?", 3,
                             TOP_WIDTH, TOP_HEIGHT,
                             COL_GREEN);

    char info[64];
    sprintf(info, "%s  |  %d APPLES  |  %s",
            sizeLabel(), apple_count, speedLabel());

    drawCenteredText(top, TOP_WIDTH / 2, 112,
                     info, 1,
                     TOP_WIDTH, TOP_HEIGHT,
                     fg);

    drawCenteredText(top, TOP_WIDTH / 2, 150,
                     "CHOOSE YOUR SETTINGS BELOW", 1,
                     TOP_WIDTH, TOP_HEIGHT,
                     COL_GRAY);

    drawBottomHeader(bottom, "CONFIGURE");

    drawString(bottom, 10, 34, "BOARD SIZE", 1,
               BOT_WIDTH, BOT_HEIGHT, fg);

    drawButton(bottom, BTN_SIZE_S, current_size == SIZE_SMALL, false);
    drawButton(bottom, BTN_SIZE_M, current_size == SIZE_MEDIUM, false);
    drawButton(bottom, BTN_SIZE_L, current_size == SIZE_LARGE, false);

    drawString(bottom, 10, 91, "APPLES", 1,
               BOT_WIDTH, BOT_HEIGHT, fg);

    drawButton(bottom, BTN_APPLE_1, apple_count == 1, false);
    drawButton(bottom, BTN_APPLE_3, apple_count == 3, false);
    drawButton(bottom, BTN_APPLE_5, apple_count == 5, false);
    drawButton(bottom, BTN_APPLE_M, false, false);
    drawButton(bottom, BTN_APPLE_P, false, false);

    char applesText[16];
    sprintf(applesText, "%d", apple_count);
    drawCenteredText(bottom, 232, 116,
                     applesText, 2,
                     BOT_WIDTH, BOT_HEIGHT,
                     COL_GOLD);

    drawString(bottom, 10, 141, "SPEED", 1,
               BOT_WIDTH, BOT_HEIGHT, fg);

    drawButton(bottom, BTN_SPEED_05, speed_multiplier == 0, false);
    drawButton(bottom, BTN_SPEED_1, speed_multiplier == 1, false);
    drawButton(bottom, BTN_SPEED_2, speed_multiplier == 2, false);

    drawButton(bottom, BTN_PLAY, false, false);
}

// ---------------------------------------------------------------------------
// Game board rendering
// ---------------------------------------------------------------------------
void drawBoardBackground(u8* top)
{
    // Board background is slightly different from the global background,
    // making the play area easier to see on a real 3DS screen.
    Color board = white_mode ? Color{238, 238, 242} : Color{12, 16, 22};
    fillRect(top, offset_x, offset_y,
             grid_w * cell_size,
             grid_h * cell_size,
             TOP_WIDTH, TOP_HEIGHT, board);

    // A subtle grid is useful at medium and large sizes without becoming
    // visually noisy on the small board.
    Color grid = white_mode ? Color{220, 220, 225} : Color{22, 27, 35};

    for (int x = 0; x <= grid_w; ++x)
    {
        drawLineV(top,
                  offset_x + x * cell_size,
                  offset_y,
                  grid_h * cell_size,
                  TOP_WIDTH, TOP_HEIGHT, grid);
    }

    for (int y = 0; y <= grid_h; ++y)
    {
        drawLineH(top,
                  offset_x,
                  offset_y + y * cell_size,
                  grid_w * cell_size,
                  TOP_WIDTH, TOP_HEIGHT, grid);
    }

    drawRect(top,
             offset_x,
             offset_y,
             grid_w * cell_size,
             grid_h * cell_size,
             TOP_WIDTH, TOP_HEIGHT,
             COL_CYAN);
}

void drawApple(u8* top, const Apple& apple)
{
    int x = offset_x + apple.pos.x * cell_size;
    int y = offset_y + apple.pos.y * cell_size;

    Color c = apple.gold ? COL_GOLD : COL_RED;

    fillRect(top, x + 2, y + 2,
             cell_size - 4, cell_size - 4,
             TOP_WIDTH, TOP_HEIGHT, c);

    drawRect(top, x + 1, y + 1,
             cell_size - 2, cell_size - 2,
             TOP_WIDTH, TOP_HEIGHT, COL_BLACK);

    // Gold apples get a small highlight so they are distinguishable even
    // when the screen is photographed in low light.
    if (apple.gold && cell_size >= 8)
    {
        fillRect(top, x + 3, y + 3, 2, 2,
                 TOP_WIDTH, TOP_HEIGHT, COL_WHITE);
    }
}

void drawSnakeSegment(u8* top, const Point& p, size_t index)
{
    int x = offset_x + p.x * cell_size;
    int y = offset_y + p.y * cell_size;

    Color c;

    if (index == 0)
        c = COL_GREEN_LITE;
    else if (index % 2 == 0)
        c = COL_GREEN;
    else
        c = COL_GREEN_DARK;

    fillRect(top, x + 1, y + 1,
             cell_size - 2, cell_size - 2,
             TOP_WIDTH, TOP_HEIGHT, c);

    drawRect(top, x, y,
             cell_size, cell_size,
             TOP_WIDTH, TOP_HEIGHT, COL_BLACK);

    if (index == 0 && cell_size >= 10)
    {
        // Two tiny eyes point in the current direction.
        int eye1x = x + 3;
        int eye2x = x + 6;
        int eyeY = y + 3;

        if (current_dir == DIR_UP || current_dir == DIR_DOWN)
        {
            eye1x = x + 3;
            eye2x = x + 6;
            eyeY = (current_dir == DIR_UP) ? y + 2 : y + 6;
        }
        else
        {
            eyeY = y + 3;
            eye1x = (current_dir == DIR_LEFT) ? x + 2 : x + 6;
            eye2x = eye1x;
        }

        fillRect(top, eye1x, eyeY, 2, 2,
                 TOP_WIDTH, TOP_HEIGHT, COL_WHITE);
        fillRect(top, eye2x, eyeY + 3, 2, 2,
                 TOP_WIDTH, TOP_HEIGHT, COL_WHITE);
    }
}

void renderGame(u8* top, u8* bottom)
{
    Color fg = foregroundColor();

    drawBoardBackground(top);

    for (size_t i = 0; i < apples.size(); ++i)
        drawApple(top, apples[i]);

    for (size_t i = snake.size(); i > 0; --i)
        drawSnakeSegment(top, snake[i - 1], i - 1);

    // Score bar overlays the top-left corner without covering the board when
    // the board is large.
    fillRect(top, 0, 0, TOP_WIDTH, 20,
             TOP_WIDTH, TOP_HEIGHT,
             white_mode ? Color{230, 230, 235} : Color{10, 12, 18});

    char scoreText[64];
    sprintf(scoreText, "SCORE %d   BEST %d", score, highscore);
    drawString(top, 8, 5, scoreText, 1,
               TOP_WIDTH, TOP_HEIGHT, fg);

    if (game_paused)
    {
        drawPanel(top, 115, 85, 170, 65,
                  TOP_WIDTH, TOP_HEIGHT);
        drawCenteredText(top, 200, 103,
                         "PAUSED", 2,
                         TOP_WIDTH, TOP_HEIGHT,
                         COL_GOLD);
    }

    drawBottomHeader(bottom, "CONTROLS");

    drawButton(bottom, BTN_UP, false, false);
    drawButton(bottom, BTN_DOWN, false, false);
    drawButton(bottom, BTN_LEFT, false, false);
    drawButton(bottom, BTN_RIGHT, false, false);

    drawString(bottom, 115, 58, "TOUCH", 1,
               BOT_WIDTH, BOT_HEIGHT, COL_GRAY);
    drawString(bottom, 112, 78, "MOVE", 1,
               BOT_WIDTH, BOT_HEIGHT, COL_GRAY);

    drawCenteredText(bottom, 160, 218,
                     game_paused ? "PAUSED" : "PLAYING", 1,
                     BOT_WIDTH, BOT_HEIGHT,
                     game_paused ? COL_GOLD : COL_GREEN);
}

// ---------------------------------------------------------------------------
// Game over rendering
// ---------------------------------------------------------------------------
void renderGameOver(u8* top, u8* bottom)
{
    Color fg = foregroundColor();

    drawTopHeader(top, "GAME OVER", COL_RED);

    drawCenteredShadowedText(top, TOP_WIDTH / 2, 58,
                             "GAME OVER", 4,
                             TOP_WIDTH, TOP_HEIGHT,
                             COL_RED);

    char scoreText[32];
    sprintf(scoreText, "SCORE  %d", score);

    drawCenteredText(top, TOP_WIDTH / 2, 120,
                     scoreText, 2,
                     TOP_WIDTH, TOP_HEIGHT, fg);

    if (score >= highscore && score > 0)
    {
        drawCenteredText(top, TOP_WIDTH / 2, 153,
                         "NEW HIGH SCORE!", 2,
                         TOP_WIDTH, TOP_HEIGHT, COL_GOLD);
    }
    else
    {
        char bestText[32];
        sprintf(bestText, "BEST  %d", highscore);
        drawCenteredText(top, TOP_WIDTH / 2, 153,
                         bestText, 2,
                         TOP_WIDTH, TOP_HEIGHT, COL_GOLD);
    }

    drawBottomHeader(bottom, "TRY AGAIN?");
    drawButton(bottom, BTN_RETRY, false, false);
    drawButton(bottom, BTN_MENU, false, false);
}

// ---------------------------------------------------------------------------
// Touch helpers
// ---------------------------------------------------------------------------
bool getTouchPress(u32 kDown, touchPosition& touch)
{
    if (!(kDown & KEY_TOUCH))
        return false;

    hidTouchRead(&touch);
    return true;
}

// ---------------------------------------------------------------------------
// Main menu input
// ---------------------------------------------------------------------------
void handleMainMenuInput(u32 kDown, const touchPosition& touch,
                         GameState& state)
{
    if (!(kDown & KEY_TOUCH))
        return;

    if (BTN_START.isClicked(touch))
    {
        state = STATE_CONFIG;
        return;
    }

    if (BTN_SETTINGS.isClicked(touch))
    {
        state = STATE_SETTINGS;
        return;
    }

    if (BTN_QUIT.isClicked(touch))
    {
        exit_requested = true;
        return;
    }
}

// ---------------------------------------------------------------------------
// Settings input
// ---------------------------------------------------------------------------
void handleSettingsInput(u32 kDown, const touchPosition& touch,
                         GameState& state)
{
    if (!(kDown & KEY_TOUCH))
        return;

    if (BTN_BACK.isClicked(touch))
    {
        state = STATE_MAIN_MENU;
        return;
    }

    if (BTN_WHITE.isClicked(touch))
    {
        white_mode = !white_mode;
        return;
    }

    if (BTN_VOL_DOWN.isClicked(touch))
    {
        volume -= 10;
        if (volume < 0) volume = 0;
        applyVolume();
        return;
    }

    if (BTN_VOL_UP.isClicked(touch))
    {
        volume += 10;
        if (volume > 100) volume = 100;
        applyVolume();
        return;
    }
}

// ---------------------------------------------------------------------------
// Configuration input
// ---------------------------------------------------------------------------
void handleConfigInput(u32 kDown, const touchPosition& touch,
                       GameState& state)
{
    if (!(kDown & KEY_TOUCH))
        return;

    if (BTN_SIZE_S.isClicked(touch))
    {
        current_size = SIZE_SMALL;
        return;
    }

    if (BTN_SIZE_M.isClicked(touch))
    {
        current_size = SIZE_MEDIUM;
        return;
    }

    if (BTN_SIZE_L.isClicked(touch))
    {
        current_size = SIZE_LARGE;
        return;
    }

    if (BTN_APPLE_1.isClicked(touch))
    {
        apple_count = 1;
        return;
    }

    if (BTN_APPLE_3.isClicked(touch))
    {
        apple_count = 3;
        return;
    }

    if (BTN_APPLE_5.isClicked(touch))
    {
        apple_count = 5;
        return;
    }

    if (BTN_APPLE_M.isClicked(touch))
    {
        --apple_count;
        if (apple_count < 1) apple_count = 1;
        return;
    }

    if (BTN_APPLE_P.isClicked(touch))
    {
        ++apple_count;
        if (apple_count > MAX_APPLES) apple_count = MAX_APPLES;
        return;
    }

    if (BTN_SPEED_05.isClicked(touch))
    {
        speed_multiplier = 0;
        return;
    }

    if (BTN_SPEED_1.isClicked(touch))
    {
        speed_multiplier = 1;
        return;
    }

    if (BTN_SPEED_2.isClicked(touch))
    {
        speed_multiplier = 2;
        return;
    }

    if (BTN_PLAY.isClicked(touch))
    {
        resetGame();
        state = STATE_PLAYING;
        return;
    }
}

// ---------------------------------------------------------------------------
// Playing input
// ---------------------------------------------------------------------------
void handlePlayingInput(u32 kDown, const touchPosition& touch,
                        bool hasTouch)
{
    if (kDown & KEY_START)
    {
        game_paused = !game_paused;
        return;
    }

    if (!hasTouch)
        return;

    if (BTN_UP.isClicked(touch))
    {
        requestDirection(DIR_UP);
        return;
    }

    if (BTN_DOWN.isClicked(touch))
    {
        requestDirection(DIR_DOWN);
        return;
    }

    if (BTN_LEFT.isClicked(touch))
    {
        requestDirection(DIR_LEFT);
        return;
    }

    if (BTN_RIGHT.isClicked(touch))
    {
        requestDirection(DIR_RIGHT);
        return;
    }
}

// ---------------------------------------------------------------------------
// Game-over input
// ---------------------------------------------------------------------------
void handleGameOverInput(u32 kDown, const touchPosition& touch,
                         GameState& state)
{
    if (!(kDown & KEY_TOUCH))
        return;

    if (BTN_RETRY.isClicked(touch))
    {
        resetGame();
        state = STATE_PLAYING;
        return;
    }

    if (BTN_MENU.isClicked(touch))
    {
        state = STATE_MAIN_MENU;
        return;
    }
}

// ---------------------------------------------------------------------------
// Highscore update
// ---------------------------------------------------------------------------
void checkHighscore()
{
    if (score > highscore)
    {
        highscore = score;
        saveHighscore();
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    gfxInitDefault();
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);

    Result ndspRes = ndspInit();

    loadHighscore();
    applyVolume();

    srand((unsigned int)(time(NULL) ^ osGetTime()));

    GameState state = STATE_MAIN_MENU;
    global_frame = 0;
    last_tick_ms = osGetTime();

    while (aptMainLoop())
    {
        hidScanInput();

        u32 kDown = hidKeysDown();
        circlePosition circle;
        hidCircleRead(&circle);

        touchPosition touch;
        bool hasTouch = getTouchPress(kDown, touch);

        ++global_frame;

        // ---------------------------------------------------------------
        // INPUT
        // ---------------------------------------------------------------
        if (state == STATE_MAIN_MENU)
        {
            handleMainMenuInput(kDown, touch, state);
        }
        else if (state == STATE_SETTINGS)
        {
            handleSettingsInput(kDown, touch, state);
        }
        else if (state == STATE_CONFIG)
        {
            handleConfigInput(kDown, touch, state);
        }
        else if (state == STATE_PLAYING)
        {
            readPhysicalDirection(kDown, circle);
            handlePlayingInput(kDown, touch, hasTouch);
        }
        else if (state == STATE_GAME_OVER)
        {
            handleGameOverInput(kDown, touch, state);
        }

        if (exit_requested)
            break;

        // ---------------------------------------------------------------
        // UPDATE
        // ---------------------------------------------------------------
        unsigned int now = osGetTime();
        unsigned int elapsed = now - last_tick_ms;
        last_tick_ms = now;

        // Avoid enormous dt after the app has been backgrounded.
        if (elapsed > 250)
            elapsed = 250;

        if (state == STATE_PLAYING && !game_paused)
        {
            if (!updateGame((float)elapsed))
            {
                checkHighscore();
                state = STATE_GAME_OVER;
            }
        }

        // ---------------------------------------------------------------
        // FRAMEBUFFERS
        // ---------------------------------------------------------------
        u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
        u8* bot_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

        clearScreen(top_fb, TOP_WIDTH, TOP_HEIGHT, backgroundColor());
        clearScreen(bot_fb, BOT_WIDTH, BOT_HEIGHT, backgroundColor());

        // ---------------------------------------------------------------
        // RENDER
        // ---------------------------------------------------------------
        switch (state)
        {
            case STATE_MAIN_MENU:
                renderMainMenu(top_fb, bot_fb);
                break;

            case STATE_SETTINGS:
                renderSettings(top_fb, bot_fb);
                break;

            case STATE_CONFIG:
                renderConfig(top_fb, bot_fb);
                break;

            case STATE_PLAYING:
                renderGame(top_fb, bot_fb);
                break;

            case STATE_GAME_OVER:
                renderGameOver(top_fb, bot_fb);
                break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    checkHighscore();

    if (ndspRes == 0)
        ndspExit();

    gfxExit();
    return 0;
}

// ============================================================================
// END OF FILE
// ============================================================================
// Notes for devkitPro / devkitARM:
//   - Compile as C++ (not C).
//   - Link against the normal 3DS libraries provided by devkitPro.
//   - The source intentionally uses only standard C/C++ + libctru APIs.
// ============================================================================
