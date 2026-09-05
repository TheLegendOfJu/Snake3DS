#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <ctime>
#include <math.h>
#include <algorithm>

#define TOP_WIDTH       400
#define TOP_HEIGHT      240
#define BOT_WIDTH       320
#define BOT_HEIGHT      240

#define CELL_SIZE       10
#define MAX_APPLES      50

#define AUDIO_SAMPLE_RATE  22050
#define AUDIO_MAX_SAMPLES  (AUDIO_SAMPLE_RATE / 2)

#define SNAKE_TAU 6.28318530718f

enum GameState
{
    STATE_MAIN_MENU,
    STATE_SETTINGS,
    STATE_CONFIG,
    STATE_PLAYING,
    STATE_GAME_OVER
};

enum Direction
{
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
};

enum GridSize
{
    SIZE_SMALL,
    SIZE_MEDIUM,
    SIZE_LARGE
};

struct Color
{
    u8 r;
    u8 g;
    u8 b;
};

struct Point
{
    int x;
    int y;
};

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

bool white_mode = false;
bool sfx_enabled = true;
bool wrap_walls = false;
bool show_grid = true;
int highscore = 0;
int volume = 50;
int snake_color_index = 0;
int settings_page = 0;

GridSize current_size = SIZE_MEDIUM;
int apple_count = 3;
int speed_multiplier = 1;

std::vector<Point> snake;
std::vector<Point> apples;

Direction current_dir = DIR_RIGHT;
Direction queued_dir = DIR_RIGHT;

int score = 0;
int grid_w = 30;
int grid_h = 18;
int cell_size = CELL_SIZE;
int offset_x = 0;
int offset_y = 0;

unsigned int global_frame = 0;
unsigned int last_tick_ms = 0;
float move_accumulator = 0.0f;

bool game_paused = false;
bool exit_requested = false;

Color COL_BLACK      = {  8,   8,  12};
Color COL_WHITE      = {248, 248, 248};
Color COL_PANEL      = { 25,  28,  38};
Color COL_BORDER     = { 90, 100, 125};
Color COL_RED        = {240,  60,  65};
Color COL_GOLD       = {255, 205,  45};
Color COL_GRAY       = { 90,  95, 110};
Color COL_SHADOW     = {  0,   0,   0};
Color COL_CYAN       = { 50, 220, 230};

struct SnakeColorSet
{
    Color head;
    Color body_a;
    Color body_b;
    const char* name;
};

const int SNAKE_COLOR_COUNT = 4;
SnakeColorSet SNAKE_COLORS[SNAKE_COLOR_COUNT] = {
    { {150, 255, 175}, { 55, 220, 105}, { 20, 125,  60}, "GREEN"  },
    { {150, 200, 255}, { 70, 140, 255}, { 30,  80, 180}, "BLUE"   },
    { {230, 170, 255}, {170,  90, 230}, {110,  40, 160}, "PURPLE" },
    { {255, 200, 140}, {255, 140,  40}, {180,  90,  10}, "ORANGE" }
};

Color accentColor()
{
    return SNAKE_COLORS[snake_color_index].head;
}

s16* audio_buffer[2] = { NULL, NULL };
ndspWaveBuf audio_wavebuf[2];
int audio_active_buffer = 0;
bool audio_ready = false;

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

Color panelPressedColor()
{
    return white_mode ? Color{150, 175, 195} : Color{85, 100, 130};
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

void drawButtonLabeled(u8* fb, const Button& b, const char* label, bool selected, bool pressed)
{
    Color fg = foregroundColor();
    Color bg = panelColor();

    if (pressed)
        bg = panelPressedColor();
    else if (selected)
        bg = panelSelectedColor();

    fillRect(fb, b.x + 2, b.y + 2, b.w, b.h,
             BOT_WIDTH, BOT_HEIGHT, COL_SHADOW);
    fillRect(fb, b.x, b.y, b.w, b.h,
             BOT_WIDTH, BOT_HEIGHT, bg);
    drawRect(fb, b.x, b.y, b.w, b.h,
             BOT_WIDTH, BOT_HEIGHT, (selected || pressed) ? COL_CYAN : borderColor());

    int tw = textWidth(label, 2);
    int tx = b.x + (b.w - tw) / 2;
    int ty = b.y + (b.h - 16) / 2;

    if (tx < b.x + 2) tx = b.x + 2;
    drawString(fb, tx, ty, label, 2,
               BOT_WIDTH, BOT_HEIGHT, fg);
}

void drawButton(u8* fb, const Button& b, bool selected, bool pressed)
{
    drawButtonLabeled(fb, b, b.text, selected, pressed);
}

bool isTouchOn(const Button& b, bool touchHeld, const touchPosition& t)
{
    return touchHeld && b.contains(t.px, t.py);
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

Color getRainbow(unsigned int frame)
{
    double t = frame * 0.045;
    Color c;
    c.r = (u8)(sin(t) * 127.0 + 128.0);
    c.g = (u8)(sin(t + 2.094) * 127.0 + 128.0);
    c.b = (u8)(sin(t + 4.188) * 127.0 + 128.0);
    return c;
}

void initAudio(bool ndspOk)
{
    audio_ready = false;

    if (!ndspOk)
        return;

    audio_buffer[0] = (s16*)linearAlloc(AUDIO_MAX_SAMPLES * sizeof(s16));
    audio_buffer[1] = (s16*)linearAlloc(AUDIO_MAX_SAMPLES * sizeof(s16));

    if (!audio_buffer[0] || !audio_buffer[1])
        return;

    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, AUDIO_SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    audio_ready = true;
}

void exitAudio()
{
    if (audio_buffer[0]) linearFree(audio_buffer[0]);
    if (audio_buffer[1]) linearFree(audio_buffer[1]);
    audio_buffer[0] = NULL;
    audio_buffer[1] = NULL;
}

void playTone(float frequency, int durationMs, float volumeScale)
{
    if (!audio_ready || !sfx_enabled)
        return;

    int samples = (AUDIO_SAMPLE_RATE * durationMs) / 1000;
    if (samples > AUDIO_MAX_SAMPLES) samples = AUDIO_MAX_SAMPLES;
    if (samples <= 0) return;

    int bufIndex = audio_active_buffer;
    audio_active_buffer = (audio_active_buffer + 1) % 2;

    s16* buf = audio_buffer[bufIndex];
    if (!buf) return;

    float amp = 9000.0f * volumeScale;

    for (int i = 0; i < samples; ++i)
    {
        float t = (float)i / (float)AUDIO_SAMPLE_RATE;
        float envelope = 1.0f - ((float)i / (float)samples);
        buf[i] = (s16)(sinf(SNAKE_TAU * frequency * t) * amp * envelope);
    }

    DSP_FlushDataCache(buf, samples * sizeof(s16));

    ndspWaveBuf* wb = &audio_wavebuf[bufIndex];
    memset(wb, 0, sizeof(ndspWaveBuf));
    wb->data_vaddr = buf;
    wb->nsamples = samples;
    wb->looping = false;

    ndspChnWaveBufAdd(0, wb);
}

void playEatSound()
{
    playTone(880.0f, 70, 0.55f);
}

void playGameOverSound()
{
    playTone(200.0f, 300, 0.7f);
}

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

void loadSettings()
{
    FILE* f = fopen("sdmc:/snake_settings.txt", "r");

    if (!f)
        return;

    int wm = 0, vol = 50, sfx = 1, wrap = 0, grid = 1, color = 0;
    int apples = 3, speed = 1, size = 1;

    fscanf(f, "%d %d %d %d %d %d %d %d %d",
           &wm, &vol, &sfx, &wrap, &grid, &color, &apples, &speed, &size);

    fclose(f);

    white_mode = wm != 0;
    volume = vol;
    sfx_enabled = sfx != 0;
    wrap_walls = wrap != 0;
    show_grid = grid != 0;
    snake_color_index = color;
    apple_count = apples;
    speed_multiplier = speed;
    current_size = (GridSize)size;

    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (apple_count < 1) apple_count = 1;
    if (apple_count > MAX_APPLES) apple_count = MAX_APPLES;
    if (snake_color_index < 0 || snake_color_index >= SNAKE_COLOR_COUNT)
        snake_color_index = 0;
    if (speed_multiplier < 0 || speed_multiplier > 2)
        speed_multiplier = 1;
    if (current_size < SIZE_SMALL || current_size > SIZE_LARGE)
        current_size = SIZE_MEDIUM;
}

void saveSettings()
{
    FILE* f = fopen("sdmc:/snake_settings.txt", "w");

    if (!f)
        return;

    fprintf(f, "%d %d %d %d %d %d %d %d %d\n",
            white_mode ? 1 : 0, volume, sfx_enabled ? 1 : 0,
            wrap_walls ? 1 : 0, show_grid ? 1 : 0, snake_color_index,
            apple_count, speed_multiplier, (int)current_size);

    fclose(f);
}

void applyVolume()
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    ndspSetMasterVol(volume / 100.0f);
}

void calculateGrid()
{
    switch (current_size)
    {
        case SIZE_SMALL:
            grid_w = 20;
            grid_h = 12;
            break;

        case SIZE_MEDIUM:
            grid_w = 30;
            grid_h = 18;
            break;

        case SIZE_LARGE:
            grid_w = 40;
            grid_h = 24;
            break;
    }

    cell_size = CELL_SIZE;
    offset_x = (TOP_WIDTH - grid_w * cell_size) / 2;
    offset_y = (TOP_HEIGHT - grid_h * cell_size) / 2;
}

bool isOpposite(Direction a, Direction b)
{
    if (a == DIR_UP && b == DIR_DOWN) return true;
    if (a == DIR_DOWN && b == DIR_UP) return true;
    if (a == DIR_LEFT && b == DIR_RIGHT) return true;
    if (a == DIR_RIGHT && b == DIR_LEFT) return true;
    return false;
}

void requestDirection(Direction requested)
{
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
        if (samePoint(apples[i], p))
            return true;
    }

    return false;
}

bool findFreeCell(Point& result)
{
    const int totalCells = grid_w * grid_h;

    if (totalCells <= 0)
        return false;

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

    apples.push_back(freeCell);
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

void resetGame()
{
    calculateGrid();

    snake.clear();
    apples.clear();

    score = 0;
    current_dir = DIR_RIGHT;
    queued_dir = DIR_RIGHT;

    move_accumulator = 0.0f;
    last_tick_ms = osGetTime();
    game_paused = false;

    int startX = grid_w / 2;
    int startY = grid_h / 2;

    snake.push_back({startX, startY});
    snake.push_back({startX - 1, startY});
    snake.push_back({startX - 2, startY});

    fillApples();
}

float moveIntervalMs()
{
    switch (speed_multiplier)
    {
        case 0: return 320.0f;
        case 1: return 160.0f;
        case 2: return 80.0f;
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

    if (wrap_walls)
    {
        result.x = (result.x + grid_w) % grid_w;
        result.y = (result.y + grid_h) % grid_h;
    }

    return result;
}

int findAppleAt(const Point& p)
{
    for (size_t i = 0; i < apples.size(); ++i)
    {
        if (samePoint(apples[i], p))
            return (int)i;
    }

    return -1;
}

bool wouldHitSnake(const Point& newHead, bool growing)
{
    if (snake.empty())
        return false;

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
        score += 1;
        apples.erase(apples.begin() + appleIndex);
        spawnApple();
        playEatSound();
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

    if (circle.dy > 60)
        requestDirection(DIR_UP);
    else if (circle.dy < -60)
        requestDirection(DIR_DOWN);
    else if (circle.dx < -60)
        requestDirection(DIR_LEFT);
    else if (circle.dx > 60)
        requestDirection(DIR_RIGHT);
}

Button BTN_START    = { 55,  45, 210, 42, "START" };
Button BTN_SETTINGS = { 55,  98, 210, 42, "SETTINGS" };
Button BTN_QUIT     = { 55, 151, 210, 42, "QUIT" };

Button BTN_BACK        = {  10, 198, 140, 32, "BACK" };
Button BTN_SETTINGS_NAV = { 170, 198, 140, 32, "NEXT >" };

Button BTN_THEME    = { 10,  44, 300, 34, "THEME" };
Button BTN_SFX      = { 10,  88, 300, 34, "SFX" };
Button BTN_VOL_DOWN = { 10, 132,  60, 34, "-" };
Button BTN_VOL_UP   = { 250, 132,  60, 34, "+" };

Button BTN_WRAP      = {  10, 44, 145, 34, "WRAP" };
Button BTN_GRID      = { 165, 44, 145, 34, "GRID" };
Button BTN_COLOR     = {  10, 88, 300, 34, "COLOR" };
Button BTN_RESET_HS  = {  10, 132, 300, 34, "RESET HIGH SCORE" };

Button BTN_SIZE_S   = { 10,  54, 92, 34, "SMALL" };
Button BTN_SIZE_M   = { 114, 54, 92, 34, "MEDIUM" };
Button BTN_SIZE_L   = { 218, 54, 92, 34, "LARGE" };

Button BTN_APPLE_1  = { 10, 105, 56, 34, "1" };
Button BTN_APPLE_3  = { 74, 105, 56, 34, "3" };
Button BTN_APPLE_5  = { 138,105, 56, 34, "5" };
Button BTN_APPLE_M  = { 204,105, 48, 34, "-" };
Button BTN_APPLE_P  = { 262,105, 48, 34, "+" };

Button BTN_SPEED_05 = { 10, 155, 92, 34, "0.5X" };
Button BTN_SPEED_1  = { 114,155, 92, 34, "1.0X" };
Button BTN_SPEED_2  = { 218,155, 92, 34, "2.0X" };
Button BTN_PLAY     = { 108, 199,104, 34, "PLAY" };

Button BTN_UP       = { 130,  48, 60, 40, "^" };
Button BTN_DOWN     = { 130, 132, 60, 40, "V" };
Button BTN_LEFT     = {  50,  90, 60, 40, "<" };
Button BTN_RIGHT    = { 210,  90, 60, 40, ">" };
Button BTN_PAUSE    = { 110, 178,100, 28, "PAUSE" };

Button BTN_RETRY    = { 55,  65, 210, 42, "RETRY" };
Button BTN_MENU     = { 55, 120, 210, 42, "MAIN MENU" };

void renderMainMenu(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
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

    drawButton(bottom, BTN_START, false, isTouchOn(BTN_START, touchHeld, t));
    drawButton(bottom, BTN_SETTINGS, false, isTouchOn(BTN_SETTINGS, touchHeld, t));
    drawButton(bottom, BTN_QUIT, false, isTouchOn(BTN_QUIT, touchHeld, t));
}

void renderSettingsPage0(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
{
    Color fg = foregroundColor();

    drawTopHeader(top, "SETTINGS", COL_CYAN);

    drawCenteredText(top, TOP_WIDTH / 2, 52,
                     "DISPLAY & SOUND", 2,
                     TOP_WIDTH, TOP_HEIGHT, COL_CYAN);

    drawPanel(top, 45, 82, 310, 118, TOP_WIDTH, TOP_HEIGHT);

    drawString(top, 60, 98, "D-PAD", 1,
               TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 220, 98, "MOVE", 2,
               TOP_WIDTH, TOP_HEIGHT, accentColor());

    drawString(top, 60, 130, "CIRCLE PAD", 1,
               TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 220, 130, "MOVE", 2,
               TOP_WIDTH, TOP_HEIGHT, accentColor());

    drawString(top, 60, 162, "TOUCH", 1,
               TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 220, 162, "ARROWS", 2,
               TOP_WIDTH, TOP_HEIGHT, accentColor());

    drawBottomHeader(bottom, "OPTIONS  1/2");

    char themeLabel[24];
    sprintf(themeLabel, "THEME: %s", white_mode ? "LIGHT" : "DARK");
    drawButtonLabeled(bottom, BTN_THEME, themeLabel, false, isTouchOn(BTN_THEME, touchHeld, t));

    char sfxLabel[24];
    sprintf(sfxLabel, "SFX: %s", sfx_enabled ? "ON" : "OFF");
    drawButtonLabeled(bottom, BTN_SFX, sfxLabel, false, isTouchOn(BTN_SFX, touchHeld, t));

    drawButton(bottom, BTN_VOL_DOWN, false, isTouchOn(BTN_VOL_DOWN, touchHeld, t));
    drawButton(bottom, BTN_VOL_UP, false, isTouchOn(BTN_VOL_UP, touchHeld, t));

    char volumeText[16];
    sprintf(volumeText, "%d%%", volume);
    drawCenteredText(bottom, 160, 141,
                     volumeText, 2,
                     BOT_WIDTH, BOT_HEIGHT,
                     COL_GOLD);

    drawButton(bottom, BTN_BACK, false, isTouchOn(BTN_BACK, touchHeld, t));
    drawButtonLabeled(bottom, BTN_SETTINGS_NAV, "NEXT >", false, isTouchOn(BTN_SETTINGS_NAV, touchHeld, t));
}

void renderSettingsPage1(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
{
    Color fg = foregroundColor();

    drawTopHeader(top, "SETTINGS", COL_CYAN);

    drawCenteredText(top, TOP_WIDTH / 2, 52,
                     "GAMEPLAY", 2,
                     TOP_WIDTH, TOP_HEIGHT, COL_CYAN);

    drawPanel(top, 45, 82, 310, 118, TOP_WIDTH, TOP_HEIGHT);

    drawString(top, 60, 98, "WRAP WALLS", 1,
               TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 250, 98, wrap_walls ? "ON" : "OFF", 1,
               TOP_WIDTH, TOP_HEIGHT, accentColor());

    drawString(top, 60, 118, "SHOW GRID", 1,
               TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 250, 118, show_grid ? "ON" : "OFF", 1,
               TOP_WIDTH, TOP_HEIGHT, accentColor());

    drawString(top, 60, 138, "SNAKE COLOR", 1,
               TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 250, 138, SNAKE_COLORS[snake_color_index].name, 1,
               TOP_WIDTH, TOP_HEIGHT, accentColor());

    drawString(top, 60, 168, "HIGH SCORE", 1,
               TOP_WIDTH, TOP_HEIGHT, fg);
    char hsTxt[16];
    sprintf(hsTxt, "%d", highscore);
    drawString(top, 250, 168, hsTxt, 1,
               TOP_WIDTH, TOP_HEIGHT, COL_GOLD);

    drawBottomHeader(bottom, "OPTIONS  2/2");

    char wrapLabel[16];
    sprintf(wrapLabel, "WRAP:%s", wrap_walls ? "ON" : "OFF");
    drawButtonLabeled(bottom, BTN_WRAP, wrapLabel, false, isTouchOn(BTN_WRAP, touchHeld, t));

    char gridLabel[16];
    sprintf(gridLabel, "GRID:%s", show_grid ? "ON" : "OFF");
    drawButtonLabeled(bottom, BTN_GRID, gridLabel, false, isTouchOn(BTN_GRID, touchHeld, t));

    char colorLabel[32];
    sprintf(colorLabel, "COLOR: %s", SNAKE_COLORS[snake_color_index].name);
    drawButtonLabeled(bottom, BTN_COLOR, colorLabel, false, isTouchOn(BTN_COLOR, touchHeld, t));

    drawButtonLabeled(bottom, BTN_RESET_HS, "RESET HIGH SCORE", false, isTouchOn(BTN_RESET_HS, touchHeld, t));

    drawButton(bottom, BTN_BACK, false, isTouchOn(BTN_BACK, touchHeld, t));
    drawButtonLabeled(bottom, BTN_SETTINGS_NAV, "< PREV", false, isTouchOn(BTN_SETTINGS_NAV, touchHeld, t));
}

void renderSettings(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
{
    if (settings_page == 0)
        renderSettingsPage0(top, bottom, touchHeld, t);
    else
        renderSettingsPage1(top, bottom, touchHeld, t);
}

void renderConfig(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
{
    Color fg = foregroundColor();

    drawTopHeader(top, "GAME SETUP", SNAKE_COLORS[snake_color_index].body_a);

    drawCenteredShadowedText(top, TOP_WIDTH / 2, 62,
                             "READY?", 3,
                             TOP_WIDTH, TOP_HEIGHT,
                             SNAKE_COLORS[snake_color_index].body_a);

    char info[64];
    sprintf(info, "%s  |  %d APPLES  |  %s",
            sizeLabel(), apple_count, speedLabel());

    drawCenteredText(top, TOP_WIDTH / 2, 112,
                     info, 1,
                     TOP_WIDTH, TOP_HEIGHT,
                     fg);

    drawCenteredText(top, TOP_WIDTH / 2, 135,
                     wrap_walls ? "WRAP WALLS: ON" : "WRAP WALLS: OFF", 1,
                     TOP_WIDTH, TOP_HEIGHT,
                     COL_GRAY);

    drawCenteredText(top, TOP_WIDTH / 2, 155,
                     "CHOOSE YOUR SETTINGS BELOW", 1,
                     TOP_WIDTH, TOP_HEIGHT,
                     COL_GRAY);

    drawBottomHeader(bottom, "CONFIGURE");

    drawString(bottom, 10, 34, "BOARD SIZE", 1,
               BOT_WIDTH, BOT_HEIGHT, fg);

    drawButton(bottom, BTN_SIZE_S, current_size == SIZE_SMALL, isTouchOn(BTN_SIZE_S, touchHeld, t));
    drawButton(bottom, BTN_SIZE_M, current_size == SIZE_MEDIUM, isTouchOn(BTN_SIZE_M, touchHeld, t));
    drawButton(bottom, BTN_SIZE_L, current_size == SIZE_LARGE, isTouchOn(BTN_SIZE_L, touchHeld, t));

    drawString(bottom, 10, 91, "APPLES", 1,
               BOT_WIDTH, BOT_HEIGHT, fg);

    drawButton(bottom, BTN_APPLE_1, apple_count == 1, isTouchOn(BTN_APPLE_1, touchHeld, t));
    drawButton(bottom, BTN_APPLE_3, apple_count == 3, isTouchOn(BTN_APPLE_3, touchHeld, t));
    drawButton(bottom, BTN_APPLE_5, apple_count == 5, isTouchOn(BTN_APPLE_5, touchHeld, t));
    drawButton(bottom, BTN_APPLE_M, false, isTouchOn(BTN_APPLE_M, touchHeld, t));
    drawButton(bottom, BTN_APPLE_P, false, isTouchOn(BTN_APPLE_P, touchHeld, t));

    char applesText[16];
    sprintf(applesText, "%d", apple_count);
    drawCenteredText(bottom, 232, 116,
                     applesText, 2,
                     BOT_WIDTH, BOT_HEIGHT,
                     COL_GOLD);

    drawString(bottom, 10, 141, "SPEED", 1,
               BOT_WIDTH, BOT_HEIGHT, fg);

    drawButton(bottom, BTN_SPEED_05, speed_multiplier == 0, isTouchOn(BTN_SPEED_05, touchHeld, t));
    drawButton(bottom, BTN_SPEED_1, speed_multiplier == 1, isTouchOn(BTN_SPEED_1, touchHeld, t));
    drawButton(bottom, BTN_SPEED_2, speed_multiplier == 2, isTouchOn(BTN_SPEED_2, touchHeld, t));

    drawButton(bottom, BTN_PLAY, false, isTouchOn(BTN_PLAY, touchHeld, t));
}

void drawBoardBackground(u8* top)
{
    Color board = white_mode ? Color{238, 238, 242} : Color{12, 16, 22};
    fillRect(top, offset_x, offset_y,
             grid_w * cell_size,
             grid_h * cell_size,
             TOP_WIDTH, TOP_HEIGHT, board);

    if (show_grid)
    {
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
    }

    drawRect(top,
             offset_x,
             offset_y,
             grid_w * cell_size,
             grid_h * cell_size,
             TOP_WIDTH, TOP_HEIGHT,
             COL_CYAN);
}

void drawApple(u8* top, const Point& apple)
{
    int x = offset_x + apple.x * cell_size;
    int y = offset_y + apple.y * cell_size;

    fillRect(top, x + 2, y + 2,
             cell_size - 4, cell_size - 4,
             TOP_WIDTH, TOP_HEIGHT, COL_RED);

    drawRect(top, x + 1, y + 1,
             cell_size - 2, cell_size - 2,
             TOP_WIDTH, TOP_HEIGHT, COL_BLACK);

    if (cell_size >= 8)
    {
        fillRect(top, x + 3, y + 3, 2, 2,
                 TOP_WIDTH, TOP_HEIGHT, COL_WHITE);
    }
}

void drawSnakeSegment(u8* top, const Point& p, size_t index)
{
    int x = offset_x + p.x * cell_size;
    int y = offset_y + p.y * cell_size;

    SnakeColorSet& pal = SNAKE_COLORS[snake_color_index];
    Color c;

    if (index == 0)
        c = pal.head;
    else if (index % 2 == 0)
        c = pal.body_a;
    else
        c = pal.body_b;

    fillRect(top, x + 1, y + 1,
             cell_size - 2, cell_size - 2,
             TOP_WIDTH, TOP_HEIGHT, c);

    drawRect(top, x, y,
             cell_size, cell_size,
             TOP_WIDTH, TOP_HEIGHT, COL_BLACK);

    if (index == 0 && cell_size >= 10)
    {
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

void renderGame(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
{
    drawBoardBackground(top);

    for (size_t i = 0; i < apples.size(); ++i)
        drawApple(top, apples[i]);

    for (size_t i = snake.size(); i > 0; --i)
        drawSnakeSegment(top, snake[i - 1], i - 1);

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

    char scoreText[64];
    sprintf(scoreText, "SCORE %d   BEST %d", score, highscore);
    drawCenteredText(bottom, BOT_WIDTH / 2, 31,
                     scoreText, 1,
                     BOT_WIDTH, BOT_HEIGHT,
                     COL_GOLD);

    drawButton(bottom, BTN_UP, false, isTouchOn(BTN_UP, touchHeld, t));
    drawButton(bottom, BTN_DOWN, false, isTouchOn(BTN_DOWN, touchHeld, t));
    drawButton(bottom, BTN_LEFT, false, isTouchOn(BTN_LEFT, touchHeld, t));
    drawButton(bottom, BTN_RIGHT, false, isTouchOn(BTN_RIGHT, touchHeld, t));

    drawButtonLabeled(bottom, BTN_PAUSE, game_paused ? "RESUME" : "PAUSE",
                      false, isTouchOn(BTN_PAUSE, touchHeld, t));

    drawCenteredText(bottom, 160, 212,
                     game_paused ? "PAUSED" : "PLAYING", 1,
                     BOT_WIDTH, BOT_HEIGHT,
                     game_paused ? COL_GOLD : accentColor());
}

void renderGameOver(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
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
    drawButton(bottom, BTN_RETRY, false, isTouchOn(BTN_RETRY, touchHeld, t));
    drawButton(bottom, BTN_MENU, false, isTouchOn(BTN_MENU, touchHeld, t));
}

void handleMainMenuInput(u32 kDown, const touchPosition& touch, GameState& state)
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
        settings_page = 0;
        state = STATE_SETTINGS;
        return;
    }

    if (BTN_QUIT.isClicked(touch))
    {
        exit_requested = true;
        return;
    }
}

void handleSettingsInput(u32 kDown, const touchPosition& touch, GameState& state)
{
    if (!(kDown & KEY_TOUCH))
        return;

    if (BTN_BACK.isClicked(touch))
    {
        state = STATE_MAIN_MENU;
        return;
    }

    if (BTN_SETTINGS_NAV.isClicked(touch))
    {
        settings_page = (settings_page == 0) ? 1 : 0;
        return;
    }

    if (settings_page == 0)
    {
        if (BTN_THEME.isClicked(touch))
        {
            white_mode = !white_mode;
            saveSettings();
            return;
        }

        if (BTN_SFX.isClicked(touch))
        {
            sfx_enabled = !sfx_enabled;
            saveSettings();
            return;
        }

        if (BTN_VOL_DOWN.isClicked(touch))
        {
            volume -= 10;
            if (volume < 0) volume = 0;
            applyVolume();
            saveSettings();
            return;
        }

        if (BTN_VOL_UP.isClicked(touch))
        {
            volume += 10;
            if (volume > 100) volume = 100;
            applyVolume();
            saveSettings();
            return;
        }
    }
    else
    {
        if (BTN_WRAP.isClicked(touch))
        {
            wrap_walls = !wrap_walls;
            saveSettings();
            return;
        }

        if (BTN_GRID.isClicked(touch))
        {
            show_grid = !show_grid;
            saveSettings();
            return;
        }

        if (BTN_COLOR.isClicked(touch))
        {
            snake_color_index = (snake_color_index + 1) % SNAKE_COLOR_COUNT;
            saveSettings();
            return;
        }

        if (BTN_RESET_HS.isClicked(touch))
        {
            highscore = 0;
            saveHighscore();
            return;
        }
    }
}

void handleConfigInput(u32 kDown, const touchPosition& touch, GameState& state)
{
    if (!(kDown & KEY_TOUCH))
        return;

    if (BTN_SIZE_S.isClicked(touch))
    {
        current_size = SIZE_SMALL;
        saveSettings();
        return;
    }

    if (BTN_SIZE_M.isClicked(touch))
    {
        current_size = SIZE_MEDIUM;
        saveSettings();
        return;
    }

    if (BTN_SIZE_L.isClicked(touch))
    {
        current_size = SIZE_LARGE;
        saveSettings();
        return;
    }

    if (BTN_APPLE_1.isClicked(touch))
    {
        apple_count = 1;
        saveSettings();
        return;
    }

    if (BTN_APPLE_3.isClicked(touch))
    {
        apple_count = 3;
        saveSettings();
        return;
    }

    if (BTN_APPLE_5.isClicked(touch))
    {
        apple_count = 5;
        saveSettings();
        return;
    }

    if (BTN_APPLE_M.isClicked(touch))
    {
        --apple_count;
        if (apple_count < 1) apple_count = 1;
        saveSettings();
        return;
    }

    if (BTN_APPLE_P.isClicked(touch))
    {
        ++apple_count;
        if (apple_count > MAX_APPLES) apple_count = MAX_APPLES;
        saveSettings();
        return;
    }

    if (BTN_SPEED_05.isClicked(touch))
    {
        speed_multiplier = 0;
        saveSettings();
        return;
    }

    if (BTN_SPEED_1.isClicked(touch))
    {
        speed_multiplier = 1;
        saveSettings();
        return;
    }

    if (BTN_SPEED_2.isClicked(touch))
    {
        speed_multiplier = 2;
        saveSettings();
        return;
    }

    if (BTN_PLAY.isClicked(touch))
    {
        resetGame();
        state = STATE_PLAYING;
        return;
    }
}

void handlePlayingInput(u32 kDown, const touchPosition& touch, bool hasTouch)
{
    if (kDown & KEY_START)
    {
        game_paused = !game_paused;
        return;
    }

    if (!hasTouch)
        return;

    if (BTN_PAUSE.isClicked(touch))
    {
        game_paused = !game_paused;
        return;
    }

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

void handleGameOverInput(u32 kDown, const touchPosition& touch, GameState& state)
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

void checkHighscore()
{
    if (score > highscore)
    {
        highscore = score;
        saveHighscore();
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    gfxInitDefault();
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);

    Result ndspRes = ndspInit();

    loadHighscore();
    loadSettings();
    applyVolume();
    initAudio(ndspRes == 0);

    srand((unsigned int)(time(NULL) ^ osGetTime()));

    GameState state = STATE_MAIN_MENU;
    global_frame = 0;
    last_tick_ms = osGetTime();

    while (aptMainLoop())
    {
        hidScanInput();

        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();

        circlePosition circle;
        hidCircleRead(&circle);

        touchPosition touchDown;
        bool hasTouchDown = (kDown & KEY_TOUCH) != 0;
        if (hasTouchDown)
            hidTouchRead(&touchDown);

        touchPosition touchNow;
        bool touchHeld = (kHeld & KEY_TOUCH) != 0;
        if (touchHeld)
            hidTouchRead(&touchNow);
        else
        {
            touchNow.px = 0;
            touchNow.py = 0;
        }

        ++global_frame;

        if (state == STATE_MAIN_MENU)
        {
            handleMainMenuInput(kDown, touchDown, state);
        }
        else if (state == STATE_SETTINGS)
        {
            handleSettingsInput(kDown, touchDown, state);
        }
        else if (state == STATE_CONFIG)
        {
            handleConfigInput(kDown, touchDown, state);
        }
        else if (state == STATE_PLAYING)
        {
            readPhysicalDirection(kDown, circle);
            handlePlayingInput(kDown, touchDown, hasTouchDown);
        }
        else if (state == STATE_GAME_OVER)
        {
            handleGameOverInput(kDown, touchDown, state);
        }

        if (exit_requested)
            break;

        unsigned int now = osGetTime();
        unsigned int elapsed = now - last_tick_ms;
        last_tick_ms = now;

        if (elapsed > 250)
            elapsed = 250;

        if (state == STATE_PLAYING && !game_paused)
        {
            if (!updateGame((float)elapsed))
            {
                playGameOverSound();
                checkHighscore();
                state = STATE_GAME_OVER;
            }
        }

        u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
        u8* bot_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

        clearScreen(top_fb, TOP_WIDTH, TOP_HEIGHT, backgroundColor());
        clearScreen(bot_fb, BOT_WIDTH, BOT_HEIGHT, backgroundColor());

        switch (state)
        {
            case STATE_MAIN_MENU:
                renderMainMenu(top_fb, bot_fb, touchHeld, touchNow);
                break;

            case STATE_SETTINGS:
                renderSettings(top_fb, bot_fb, touchHeld, touchNow);
                break;

            case STATE_CONFIG:
                renderConfig(top_fb, bot_fb, touchHeld, touchNow);
                break;

            case STATE_PLAYING:
                renderGame(top_fb, bot_fb, touchHeld, touchNow);
                break;

            case STATE_GAME_OVER:
                renderGameOver(top_fb, bot_fb, touchHeld, touchNow);
                break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    checkHighscore();
    exitAudio();

    if (ndspRes == 0)
        ndspExit();

    gfxExit();
    return 0;
}
