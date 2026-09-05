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

enum GameMode
{
    MODE_CLASSIC,
    MODE_TIME,
    MODE_OBSTACLE
};

enum Difficulty
{
    DIFF_EASY,
    DIFF_NORMAL,
    DIFF_HARD,
    DIFF_INSANE
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
int custom_r = 80;
int custom_g = 220;
int custom_b = 120;
bool use_custom_color = false;
bool show_hud = true;
bool no_reverse = true;
bool screen_flash = true;

GridSize current_size = SIZE_MEDIUM;
int apple_count = 3;
int speed_multiplier = 1;
GameMode game_mode = MODE_CLASSIC;
Difficulty difficulty = DIFF_NORMAL;
bool obstacles_enabled = false;
bool bonus_enabled = true;
bool acceleration_enabled = true;
int lives_setting = 1;
int score_multiplier = 1;
int start_length = 3;
int time_limit = 60;
int config_page = 0;
int remaining_lives = 1;
float game_timer_ms = 0.0f;
float bonus_timer_ms = 0.0f;
float flash_timer_ms = 0.0f;
bool bonus_active = false;
Point bonus_apple = {0, 0};

std::vector<Point> snake;
std::vector<Point> apples;
std::vector<Point> obstacles;

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
Color COL_BLUE       = { 55, 150, 255};
Color COL_GREEN      = { 70, 230, 120};
Color COL_ORANGE     = { 255, 145, 45};
Color COL_PURPLE     = { 190, 100, 255};

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

Color customSnakeColor()
{
    Color c = {(u8)custom_r, (u8)custom_g, (u8)custom_b};
    return c;
}

Color accentColor()
{
    return use_custom_color ? customSnakeColor() : SNAKE_COLORS[snake_color_index].head;
}

Color snakeBodyColorA()
{
    if (use_custom_color)
    {
        Color c = customSnakeColor();
        c.r = (u8)std::min(255, (int)c.r);
        c.g = (u8)std::min(255, (int)c.g);
        c.b = (u8)std::min(255, (int)c.b);
        return c;
    }
    return SNAKE_COLORS[snake_color_index].body_a;
}

Color snakeBodyColorB()
{
    if (use_custom_color)
    {
        Color c = customSnakeColor();
        c.r = (u8)(c.r / 2 + 40);
        c.g = (u8)(c.g / 2 + 40);
        c.b = (u8)(c.b / 2 + 40);
        return c;
    }
    return SNAKE_COLORS[snake_color_index].body_b;
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

    int values[25];
    for (int i = 0; i < 25; ++i)
        values[i] = 0;

    int count = 0;
    while (count < 25 && fscanf(f, "%d", &values[count]) == 1)
        ++count;

    fclose(f);

    if (count >= 1) white_mode = values[0] != 0;
    if (count >= 2) volume = values[1];
    if (count >= 3) sfx_enabled = values[2] != 0;
    if (count >= 4) wrap_walls = values[3] != 0;
    if (count >= 5) show_grid = values[4] != 0;
    if (count >= 6) snake_color_index = values[5];
    if (count >= 7) apple_count = values[6];
    if (count >= 8) speed_multiplier = values[7];
    if (count >= 9) current_size = (GridSize)values[8];
    if (count >= 10) game_mode = (GameMode)values[9];
    if (count >= 11) difficulty = (Difficulty)values[10];
    if (count >= 12) obstacles_enabled = values[11] != 0;
    if (count >= 13) bonus_enabled = values[12] != 0;
    if (count >= 14) acceleration_enabled = values[13] != 0;
    if (count >= 15) lives_setting = values[14];
    if (count >= 16) score_multiplier = values[15];
    if (count >= 17) start_length = values[16];
    if (count >= 18) time_limit = values[17];
    if (count >= 19) custom_r = values[18];
    if (count >= 20) custom_g = values[19];
    if (count >= 21) custom_b = values[20];
    if (count >= 22) use_custom_color = values[21] != 0;
    if (count >= 23) show_hud = values[22] != 0;
    if (count >= 24) no_reverse = values[23] != 0;
    if (count >= 25) screen_flash = values[24] != 0;

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
    if (game_mode < MODE_CLASSIC || game_mode > MODE_OBSTACLE)
        game_mode = MODE_CLASSIC;
    if (difficulty < DIFF_EASY || difficulty > DIFF_INSANE)
        difficulty = DIFF_NORMAL;
    if (lives_setting < 1) lives_setting = 1;
    if (lives_setting > 5) lives_setting = 5;
    if (score_multiplier < 1) score_multiplier = 1;
    if (score_multiplier > 3) score_multiplier = 3;
    if (start_length < 3) start_length = 3;
    if (start_length > 10) start_length = 10;
    if (time_limit < 30) time_limit = 30;
    if (time_limit > 300) time_limit = 300;
    if (custom_r < 0) custom_r = 0;
    if (custom_r > 255) custom_r = 255;
    if (custom_g < 0) custom_g = 0;
    if (custom_g > 255) custom_g = 255;
    if (custom_b < 0) custom_b = 0;
    if (custom_b > 255) custom_b = 255;
}

void saveSettings()
{
    FILE* f = fopen("sdmc:/snake_settings.txt", "w");

    if (!f)
        return;

    fprintf(f, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
            white_mode ? 1 : 0,
            volume,
            sfx_enabled ? 1 : 0,
            wrap_walls ? 1 : 0,
            show_grid ? 1 : 0,
            snake_color_index,
            apple_count,
            speed_multiplier,
            (int)current_size,
            (int)game_mode,
            (int)difficulty,
            obstacles_enabled ? 1 : 0,
            bonus_enabled ? 1 : 0,
            acceleration_enabled ? 1 : 0,
            lives_setting,
            score_multiplier,
            start_length,
            time_limit,
            custom_r,
            custom_g,
            custom_b,
            use_custom_color ? 1 : 0,
            show_hud ? 1 : 0,
            no_reverse ? 1 : 0,
            screen_flash ? 1 : 0);

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
    if (no_reverse && isOpposite(requested, current_dir))
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

bool pointBlocked(const Point& p);

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

        if (!pointBlocked(p))
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

            if (!pointBlocked(p))
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

const char* modeLabel()
{
    switch (game_mode)
    {
        case MODE_CLASSIC: return "CLASSIC";
        case MODE_TIME: return "TIME";
        case MODE_OBSTACLE: return "MAZE";
        default: return "CLASSIC";
    }
}

const char* difficultyLabel()
{
    switch (difficulty)
    {
        case DIFF_EASY: return "EASY";
        case DIFF_NORMAL: return "NORMAL";
        case DIFF_HARD: return "HARD";
        case DIFF_INSANE: return "INSANE";
        default: return "NORMAL";
    }
}

const char* livesLabel()
{
    static char text[16];
    sprintf(text, "%d", lives_setting);
    return text;
}

bool pointOnObstacle(const Point& p)
{
    for (size_t i = 0; i < obstacles.size(); ++i)
        if (samePoint(obstacles[i], p))
            return true;
    return false;
}

bool pointBlocked(const Point& p)
{
    return pointOnSnake(p) || pointOnApple(p) || pointOnObstacle(p) || (bonus_active && samePoint(bonus_apple, p));
}

void generateObstacles()
{
    obstacles.clear();
    if (!obstacles_enabled && game_mode != MODE_OBSTACLE)
        return;

    int wanted = (grid_w * grid_h) / 45;
    if (difficulty == DIFF_HARD) wanted += 3;
    if (difficulty == DIFF_INSANE) wanted += 7;
    if (wanted < 6) wanted = 6;
    if (wanted > 35) wanted = 35;

    for (int i = 0; i < wanted; ++i)
    {
        Point p;
        bool good = false;
        for (int attempt = 0; attempt < 100 && !good; ++attempt)
        {
            p.x = rand() % grid_w;
            p.y = rand() % grid_h;
            good = !pointOnSnake(p) && !pointOnApple(p);
            if (good)
            {
                int cx = grid_w / 2;
                int cy = grid_h / 2;
                if (abs(p.x - cx) < 4 && abs(p.y - cy) < 3)
                    good = false;
            }
            if (good)
            {
                for (size_t j = 0; j < obstacles.size(); ++j)
                    if (samePoint(obstacles[j], p))
                        good = false;
            }
        }
        if (good)
            obstacles.push_back(p);
    }
}

bool spawnBonusApple()
{
    Point p;
    if (!findFreeCell(p))
        return false;
    bonus_apple = p;
    bonus_active = true;
    bonus_timer_ms = 9000.0f;
    return true;
}

void resetSnakeAfterLife()
{
    snake.clear();
    int startX = grid_w / 2;
    int startY = grid_h / 2;
    if (pointOnObstacle({startX, startY}) || pointOnObstacle({startX - 1, startY}))
    {
        obstacles.clear();
        generateObstacles();
    }
    snake.push_back({startX, startY});
    for (int i = 1; i < start_length; ++i)
    {
        Point p = {startX - i, startY};
        if (pointInsideGrid(p) && !pointOnObstacle(p))
            snake.push_back(p);
    }
    current_dir = DIR_RIGHT;
    queued_dir = DIR_RIGHT;
    move_accumulator = 0.0f;
}

void resetGame()
{
    calculateGrid();
    snake.clear();
    apples.clear();
    obstacles.clear();
    score = 0;
    remaining_lives = lives_setting;
    current_dir = DIR_RIGHT;
    queued_dir = DIR_RIGHT;
    move_accumulator = 0.0f;
    game_timer_ms = time_limit * 1000.0f;
    bonus_timer_ms = 0.0f;
    flash_timer_ms = 0.0f;
    bonus_active = false;

    int startX = grid_w / 2;
    int startY = grid_h / 2;
    snake.push_back({startX, startY});
    for (int i = 1; i < start_length; ++i)
        snake.push_back({startX - i, startY});

    if (game_mode == MODE_OBSTACLE)
        obstacles_enabled = true;

    generateObstacles();
    fillApples();

    if (bonus_enabled)
        spawnBonusApple();
}

float moveIntervalMs()
{
    float interval = 160.0f;

    switch (speed_multiplier)
    {
        case 0: interval = 320.0f; break;
        case 1: interval = 160.0f; break;
        case 2: interval = 80.0f; break;
        default: interval = 160.0f; break;
    }

    if (difficulty == DIFF_EASY) interval *= 1.25f;
    if (difficulty == DIFF_HARD) interval *= 0.75f;
    if (difficulty == DIFF_INSANE) interval *= 0.55f;

    if (acceleration_enabled)
    {
        interval -= score * 1.5f;
        if (interval < 42.0f) interval = 42.0f;
    }

    return interval;
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

    if (outside || pointOnObstacle(newHead))
    {
        if (remaining_lives > 1)
        {
            --remaining_lives;
            resetSnakeAfterLife();
            playTone(150.0f, 120, 0.45f);
            return true;
        }
        return false;
    }

    int appleIndex = findAppleAt(newHead);
    bool growing = appleIndex >= 0;
    bool bonusEaten = bonus_active && samePoint(bonus_apple, newHead);

    if (wouldHitSnake(newHead, growing))
    {
        if (remaining_lives > 1)
        {
            --remaining_lives;
            resetSnakeAfterLife();
            playTone(150.0f, 120, 0.45f);
            return true;
        }
        return false;
    }

    snake.insert(snake.begin(), newHead);

    if (growing)
    {
        score += score_multiplier;
        apples.erase(apples.begin() + appleIndex);
        spawnApple();
        playEatSound();
        if (screen_flash)
            flash_timer_ms = 70.0f;
        if (bonus_enabled && !bonus_active)
            spawnBonusApple();
    }
    else if (bonusEaten)
    {
        score += 5 * score_multiplier;
        bonus_active = false;
        bonus_timer_ms = 0.0f;
        playTone(1320.0f, 100, 0.7f);
        if (screen_flash)
            flash_timer_ms = 140.0f;
    }
    else
    {
        snake.pop_back();
    }

    if ((int)snake.size() >= grid_w * grid_h - (int)obstacles.size())
        return false;

    return true;
}

bool updateGame(float dtMs)
{
    if (game_paused)
        return true;

    if (flash_timer_ms > 0.0f)
    {
        flash_timer_ms -= dtMs;
        if (flash_timer_ms < 0.0f)
            flash_timer_ms = 0.0f;
    }

    if (game_mode == MODE_TIME)
    {
        game_timer_ms -= dtMs;
        if (game_timer_ms <= 0.0f)
            return false;
    }

    if (bonus_active)
    {
        bonus_timer_ms -= dtMs;
        if (bonus_timer_ms <= 0.0f)
            bonus_active = false;
    }

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

    if (game_mode == MODE_OBSTACLE && score > 0 && score % 10 == 0 && obstacles.size() < 45)
    {
        if (global_frame % 30 == 0)
        {
            Point p;
            if (findFreeCell(p))
                obstacles.push_back(p);
        }
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
Button BTN_COLOR     = {  10, 88, 300, 34, "COLOR / RGB" };
Button BTN_RESET_HS  = {  10, 132, 300, 34, "RESET HIGH SCORE" };
Button BTN_RGB_R = { 10, 45, 300, 34, "R" };
Button BTN_RGB_G = { 10, 91, 300, 34, "G" };
Button BTN_RGB_B = { 10, 137, 300, 34, "B" };
Button BTN_CUSTOM_COLOR = { 10, 181, 145, 30, "CUSTOM" };
Button BTN_RESET_COLOR = { 165, 181, 145, 30, "RESET" };
Button BTN_COLOR_BACK = { 10, 210, 140, 24, "BACK" };
Button BTN_COLOR_NEXT = { 170, 210, 140, 24, "NEXT >" };
Button BTN_HUD = { 10, 48, 145, 34, "HUD" };
Button BTN_REVERSE = { 165, 48, 145, 34, "NO REVERSE" };
Button BTN_FLASH = { 10, 92, 145, 34, "FLASH" };
Button BTN_RESET_SETTINGS = { 165, 92, 145, 34, "RESET ALL" };

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

Button BTN_CONFIG_PREV = { 10, 199, 85, 34, "<" };
Button BTN_CONFIG_NEXT = { 225, 199, 85, 34, ">" };
Button BTN_MODE        = { 10, 45, 145, 34, "MODE" };
Button BTN_DIFF        = { 165,45,145,34, "DIFFICULTY" };
Button BTN_OBSTACLE    = { 10, 88, 145, 34, "OBSTACLES" };
Button BTN_BONUS       = { 165,88,145,34, "BONUS" };
Button BTN_LIFE_DOWN   = { 10, 131, 56, 34, "-" };
Button BTN_LIFE_UP     = { 258,131,52,34, "+" };
Button BTN_ACCEL       = { 74,131,80,34, "ACCEL" };
Button BTN_MULT        = { 165,131,80,34, "MULT" };
Button BTN_LENGTH_DOWN = { 10, 45, 56, 34, "-" };
Button BTN_LENGTH_UP   = { 258,45,52,34, "+" };
Button BTN_TIME_DOWN   = { 10, 88, 56, 34, "-" };
Button BTN_TIME_UP     = { 258,88,52,34, "+" };

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

    drawBottomHeader(bottom, "OPTIONS  1/4");

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

    drawBottomHeader(bottom, "OPTIONS  2/4");

    char wrapLabel[16];
    sprintf(wrapLabel, "WRAP:%s", wrap_walls ? "ON" : "OFF");
    drawButtonLabeled(bottom, BTN_WRAP, wrapLabel, false, isTouchOn(BTN_WRAP, touchHeld, t));

    char gridLabel[16];
    sprintf(gridLabel, "GRID:%s", show_grid ? "ON" : "OFF");
    drawButtonLabeled(bottom, BTN_GRID, gridLabel, false, isTouchOn(BTN_GRID, touchHeld, t));

    char colorLabel[32];
    sprintf(colorLabel, "COLOR: %s", use_custom_color ? "CUSTOM RGB" : SNAKE_COLORS[snake_color_index].name);
    drawButtonLabeled(bottom, BTN_COLOR, colorLabel, false, isTouchOn(BTN_COLOR, touchHeld, t));

    drawButtonLabeled(bottom, BTN_RESET_HS, "RESET HIGH SCORE", false, isTouchOn(BTN_RESET_HS, touchHeld, t));

    drawButton(bottom, BTN_BACK, false, isTouchOn(BTN_BACK, touchHeld, t));
    drawButtonLabeled(bottom, BTN_SETTINGS_NAV, "NEXT >", false, isTouchOn(BTN_SETTINGS_NAV, touchHeld, t));
}

void drawSlider(u8* bottom, int y, const char* label, int value, Color c)
{
    drawString(bottom, 10, y, label, 1, BOT_WIDTH, BOT_HEIGHT, c);
    int x = 42;
    int w = 238;
    int trackY = y + 2;
    fillRect(bottom, x, trackY, w, 6, BOT_WIDTH, BOT_HEIGHT, COL_GRAY);
    int knobX = x + (value * (w - 8)) / 255;
    fillRect(bottom, x, trackY, knobX - x + 4, 6, BOT_WIDTH, BOT_HEIGHT, c);
    fillRect(bottom, knobX, y - 2, 8, 14, BOT_WIDTH, BOT_HEIGHT, c);
    char v[8];
    sprintf(v, "%d", value);
    drawString(bottom, 286, y, v, 1, BOT_WIDTH, BOT_HEIGHT, c);
}

void renderSettingsPage2(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
{
    Color fg = foregroundColor();
    Color custom = customSnakeColor();

    drawTopHeader(top, "SETTINGS", custom);

    drawCenteredText(top, TOP_WIDTH / 2, 45, "RGB COLOR MIXER", 2,
                     TOP_WIDTH, TOP_HEIGHT, custom);

    drawPanel(top, 55, 75, 290, 125, TOP_WIDTH, TOP_HEIGHT);
    fillRect(top, 90, 105, 220, 55, TOP_WIDTH, TOP_HEIGHT, custom);
    drawRect(top, 90, 105, 220, 55, TOP_WIDTH, TOP_HEIGHT, fg);

    char rgb[48];
    sprintf(rgb, "R %d   G %d   B %d", custom_r, custom_g, custom_b);
    drawCenteredText(top, TOP_WIDTH / 2, 174, rgb, 1,
                     TOP_WIDTH, TOP_HEIGHT, fg);

    drawBottomHeader(bottom, "COLOR  3/4");
    drawSlider(bottom, 48, "R", custom_r, COL_RED);
    drawSlider(bottom, 94, "G", custom_g, COL_GREEN);
    drawSlider(bottom, 140, "B", custom_b, COL_BLUE);
    drawButtonLabeled(bottom, BTN_CUSTOM_COLOR,
                      use_custom_color ? "CUSTOM: ON" : "CUSTOM: OFF",
                      use_custom_color, isTouchOn(BTN_CUSTOM_COLOR, touchHeld, t));
    drawButton(bottom, BTN_RESET_COLOR, false, isTouchOn(BTN_RESET_COLOR, touchHeld, t));
    drawButton(bottom, BTN_COLOR_BACK, false, isTouchOn(BTN_COLOR_BACK, touchHeld, t));
    drawButton(bottom, BTN_COLOR_NEXT, false, isTouchOn(BTN_COLOR_NEXT, touchHeld, t));
}

void renderSettingsPage3(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
{
    Color fg = foregroundColor();

    drawTopHeader(top, "SETTINGS", accentColor());
    drawCenteredText(top, TOP_WIDTH / 2, 48, "ADVANCED", 2,
                     TOP_WIDTH, TOP_HEIGHT, COL_CYAN);
    drawPanel(top, 45, 78, 310, 112, TOP_WIDTH, TOP_HEIGHT);

    drawString(top, 60, 96, "HUD", 1, TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 250, 96, show_hud ? "ON" : "OFF", 1, TOP_WIDTH, TOP_HEIGHT, accentColor());
    drawString(top, 60, 120, "NO REVERSE", 1, TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 250, 120, no_reverse ? "ON" : "OFF", 1, TOP_WIDTH, TOP_HEIGHT, accentColor());
    drawString(top, 60, 144, "SCREEN FLASH", 1, TOP_WIDTH, TOP_HEIGHT, fg);
    drawString(top, 250, 144, screen_flash ? "ON" : "OFF", 1, TOP_WIDTH, TOP_HEIGHT, accentColor());
    drawString(top, 60, 168, "SETTINGS SAVED", 1, TOP_WIDTH, TOP_HEIGHT, COL_GOLD);

    drawBottomHeader(bottom, "ADVANCED  4/4");
    drawButtonLabeled(bottom, BTN_HUD, show_hud ? "HUD: ON" : "HUD: OFF",
                      show_hud, isTouchOn(BTN_HUD, touchHeld, t));
    drawButtonLabeled(bottom, BTN_REVERSE, no_reverse ? "REVERSE: OFF" : "REVERSE: ON",
                      no_reverse, isTouchOn(BTN_REVERSE, touchHeld, t));
    drawButtonLabeled(bottom, BTN_FLASH, screen_flash ? "FLASH: ON" : "FLASH: OFF",
                      screen_flash, isTouchOn(BTN_FLASH, touchHeld, t));
    drawButton(bottom, BTN_RESET_SETTINGS, false, isTouchOn(BTN_RESET_SETTINGS, touchHeld, t));
    drawButton(bottom, BTN_BACK, false, isTouchOn(BTN_BACK, touchHeld, t));
    drawButtonLabeled(bottom, BTN_SETTINGS_NAV, "< HOME", false, isTouchOn(BTN_SETTINGS_NAV, touchHeld, t));
}

void updateSettingsSliders(const touchPosition& touch, bool held)
{
    if (!held || settings_page != 2)
        return;

    int values[3] = {custom_r, custom_g, custom_b};
    int ys[3] = {48, 94, 140};
    for (int i = 0; i < 3; ++i)
    {
        if (touch.py >= ys[i] - 10 && touch.py <= ys[i] + 18 &&
            touch.px >= 42 && touch.px <= 280)
        {
            int value = (touch.px - 42) * 255 / 238;
            if (value < 0) value = 0;
            if (value > 255) value = 255;
            values[i] = value;
        }
    }
    custom_r = values[0];
    custom_g = values[1];
    custom_b = values[2];
}

void renderSettings(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
{
    if (settings_page == 0)
        renderSettingsPage0(top, bottom, touchHeld, t);
    else if (settings_page == 1)
        renderSettingsPage1(top, bottom, touchHeld, t);
    else if (settings_page == 2)
        renderSettingsPage2(top, bottom, touchHeld, t);
    else
        renderSettingsPage3(top, bottom, touchHeld, t);
}

void renderConfig(u8* top, u8* bottom, bool touchHeld, const touchPosition& t)
{
    Color fg = foregroundColor();
    drawTopHeader(top, "GAME SETUP", accentColor());

    char line1[64];
    char line2[64];
    sprintf(line1, "%s  %s  %s", sizeLabel(), modeLabel(), difficultyLabel());
    sprintf(line2, "APPLES %d  SPEED %s  MULT x%d", apple_count, speedLabel(), score_multiplier);

    drawCenteredShadowedText(top, TOP_WIDTH / 2, 48, "CUSTOM GAME", 3,
                             TOP_WIDTH, TOP_HEIGHT, accentColor());
    drawCenteredText(top, TOP_WIDTH / 2, 88, line1, 1, TOP_WIDTH, TOP_HEIGHT, fg);
    drawCenteredText(top, TOP_WIDTH / 2, 106, line2, 1, TOP_WIDTH, TOP_HEIGHT, COL_GOLD);

    if (config_page == 0)
    {
        drawCenteredText(top, TOP_WIDTH / 2, 140, "BOARD & START", 2,
                         TOP_WIDTH, TOP_HEIGHT, COL_CYAN);
        char info[64];
        sprintf(info, "LENGTH %d   APPLES %d", start_length, apple_count);
        drawCenteredText(top, TOP_WIDTH / 2, 170, info, 1,
                         TOP_WIDTH, TOP_HEIGHT, fg);

        drawBottomHeader(bottom, "SETUP 1/3");
        drawString(bottom, 10, 34, "BOARD SIZE", 1, BOT_WIDTH, BOT_HEIGHT, fg);
        drawButton(bottom, BTN_SIZE_S, current_size == SIZE_SMALL, isTouchOn(BTN_SIZE_S, touchHeld, t));
        drawButton(bottom, BTN_SIZE_M, current_size == SIZE_MEDIUM, isTouchOn(BTN_SIZE_M, touchHeld, t));
        drawButton(bottom, BTN_SIZE_L, current_size == SIZE_LARGE, isTouchOn(BTN_SIZE_L, touchHeld, t));

        drawString(bottom, 10, 91, "APPLES", 1, BOT_WIDTH, BOT_HEIGHT, fg);
        drawButton(bottom, BTN_APPLE_1, apple_count == 1, isTouchOn(BTN_APPLE_1, touchHeld, t));
        drawButton(bottom, BTN_APPLE_3, apple_count == 3, isTouchOn(BTN_APPLE_3, touchHeld, t));
        drawButton(bottom, BTN_APPLE_5, apple_count == 5, isTouchOn(BTN_APPLE_5, touchHeld, t));
        drawButton(bottom, BTN_APPLE_M, false, isTouchOn(BTN_APPLE_M, touchHeld, t));
        drawButton(bottom, BTN_APPLE_P, false, isTouchOn(BTN_APPLE_P, touchHeld, t));

        drawString(bottom, 10, 141, "LENGTH", 1, BOT_WIDTH, BOT_HEIGHT, fg);
        drawButton(bottom, BTN_LENGTH_DOWN, false, isTouchOn(BTN_LENGTH_DOWN, touchHeld, t));
        char lengthText[16];
        sprintf(lengthText, "%d", start_length);
        drawCenteredText(bottom, 160, 140, lengthText, 2, BOT_WIDTH, BOT_HEIGHT, COL_GOLD);
        drawButton(bottom, BTN_LENGTH_UP, false, isTouchOn(BTN_LENGTH_UP, touchHeld, t));

        drawButton(bottom, BTN_CONFIG_NEXT, false, isTouchOn(BTN_CONFIG_NEXT, touchHeld, t));
    }
    else if (config_page == 1)
    {
        drawCenteredText(top, TOP_WIDTH / 2, 140, "SPEED & DIFFICULTY", 2,
                         TOP_WIDTH, TOP_HEIGHT, COL_CYAN);
        char info[64];
        sprintf(info, "%s  ACCEL:%s", difficultyLabel(), acceleration_enabled ? "ON" : "OFF");
        drawCenteredText(top, TOP_WIDTH / 2, 170, info, 1,
                         TOP_WIDTH, TOP_HEIGHT, fg);

        drawBottomHeader(bottom, "SETUP 2/3");
        drawString(bottom, 10, 34, "SPEED", 1, BOT_WIDTH, BOT_HEIGHT, fg);
        drawButton(bottom, BTN_SPEED_05, speed_multiplier == 0, isTouchOn(BTN_SPEED_05, touchHeld, t));
        drawButton(bottom, BTN_SPEED_1, speed_multiplier == 1, isTouchOn(BTN_SPEED_1, touchHeld, t));
        drawButton(bottom, BTN_SPEED_2, speed_multiplier == 2, isTouchOn(BTN_SPEED_2, touchHeld, t));

        drawButtonLabeled(bottom, BTN_MODE, difficultyLabel(), false, false);
        drawButtonLabeled(bottom, BTN_DIFF, "NEXT DIFF", false, isTouchOn(BTN_DIFF, touchHeld, t));

        drawString(bottom, 10, 91, "DIFFICULTY", 1, BOT_WIDTH, BOT_HEIGHT, fg);
        char diffText[24];
        sprintf(diffText, "%s", difficultyLabel());
        drawCenteredText(bottom, 232, 94, diffText, 1, BOT_WIDTH, BOT_HEIGHT, COL_GOLD);

        drawButtonLabeled(bottom, BTN_ACCEL, acceleration_enabled ? "ACCEL ON" : "ACCEL OFF",
                          acceleration_enabled, isTouchOn(BTN_ACCEL, touchHeld, t));

        drawString(bottom, 10, 141, "MULTIPLIER", 1, BOT_WIDTH, BOT_HEIGHT, fg);
        char multText[16];
        sprintf(multText, "x%d", score_multiplier);
        drawCenteredText(bottom, 232, 141, multText, 2, BOT_WIDTH, BOT_HEIGHT, COL_GOLD);

        drawButton(bottom, BTN_CONFIG_PREV, false, isTouchOn(BTN_CONFIG_PREV, touchHeld, t));
        drawButton(bottom, BTN_CONFIG_NEXT, false, isTouchOn(BTN_CONFIG_NEXT, touchHeld, t));
    }
    else
    {
        drawCenteredText(top, TOP_WIDTH / 2, 140, "RULES & MODES", 2,
                         TOP_WIDTH, TOP_HEIGHT, COL_CYAN);
        char info[96];
        sprintf(info, "%s  WALLS:%s  OBS:%s", modeLabel(), wrap_walls ? "WRAP" : "SOLID",
                obstacles_enabled ? "ON" : "OFF");
        drawCenteredText(top, TOP_WIDTH / 2, 170, info, 1,
                         TOP_WIDTH, TOP_HEIGHT, fg);

        drawBottomHeader(bottom, "SETUP 3/3");
        char modeText[32];
        sprintf(modeText, "MODE: %s", modeLabel());
        drawButtonLabeled(bottom, BTN_MODE, modeText, false, isTouchOn(BTN_MODE, touchHeld, t));

        char obsText[32];
        sprintf(obsText, "OBSTACLES:%s", obstacles_enabled ? "ON" : "OFF");
        drawButtonLabeled(bottom, BTN_OBSTACLE, obsText, obstacles_enabled, isTouchOn(BTN_OBSTACLE, touchHeld, t));

        char bonusText[32];
        sprintf(bonusText, "BONUS:%s", bonus_enabled ? "ON" : "OFF");
        drawButtonLabeled(bottom, BTN_BONUS, bonusText, bonus_enabled, isTouchOn(BTN_BONUS, touchHeld, t));

        char lifeText[32];
        sprintf(lifeText, "LIVES %d", lives_setting);
        drawButtonLabeled(bottom, BTN_ACCEL, lifeText, false, false);
        drawButton(bottom, BTN_LIFE_DOWN, false, isTouchOn(BTN_LIFE_DOWN, touchHeld, t));
        drawButton(bottom, BTN_LIFE_UP, false, isTouchOn(BTN_LIFE_UP, touchHeld, t));

        char wallText[32];
        sprintf(wallText, "WALLS:%s", wrap_walls ? "WRAP" : "SOLID");
        drawButtonLabeled(bottom, BTN_MULT, wallText, wrap_walls, isTouchOn(BTN_MULT, touchHeld, t));

        if (game_mode == MODE_TIME)
        {
            char timerText[32];
            sprintf(timerText, "TIME %ds", time_limit);
            drawButtonLabeled(bottom, BTN_GRID, timerText, false, false);
        }

        drawButton(bottom, BTN_CONFIG_PREV, false, isTouchOn(BTN_CONFIG_PREV, touchHeld, t));
        drawButton(bottom, BTN_PLAY, false, isTouchOn(BTN_PLAY, touchHeld, t));
    }
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

void drawObstacle(u8* top, const Point& p)
{
    int x = offset_x + p.x * cell_size;
    int y = offset_y + p.y * cell_size;
    fillRect(top, x + 1, y + 1, cell_size - 2, cell_size - 2,
             TOP_WIDTH, TOP_HEIGHT, COL_GRAY);
    drawRect(top, x, y, cell_size, cell_size,
             TOP_WIDTH, TOP_HEIGHT, COL_WHITE);
}

void drawBonusApple(u8* top)
{
    if (!bonus_active)
        return;
    int x = offset_x + bonus_apple.x * cell_size;
    int y = offset_y + bonus_apple.y * cell_size;
    fillRect(top, x + 1, y + 1, cell_size - 2, cell_size - 2,
             TOP_WIDTH, TOP_HEIGHT, COL_GOLD);
    drawRect(top, x, y, cell_size, cell_size,
             TOP_WIDTH, TOP_HEIGHT, COL_WHITE);
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

    Color c;

    if (index == 0)
        c = accentColor();
    else if (index % 2 == 0)
        c = snakeBodyColorA();
    else
        c = snakeBodyColorB();

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

    for (size_t i = 0; i < obstacles.size(); ++i)
        drawObstacle(top, obstacles[i]);

    for (size_t i = 0; i < apples.size(); ++i)
        drawApple(top, apples[i]);

    drawBonusApple(top);

    for (size_t i = snake.size(); i > 0; --i)
        drawSnakeSegment(top, snake[i - 1], i - 1);

    if (screen_flash && flash_timer_ms > 0.0f)
    {
        drawRect(top, 1, 1, TOP_WIDTH - 2, TOP_HEIGHT - 2,
                 TOP_WIDTH, TOP_HEIGHT, accentColor());
        drawRect(top, 3, 3, TOP_WIDTH - 6, TOP_HEIGHT - 6,
                 TOP_WIDTH, TOP_HEIGHT, accentColor());
    }

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

    if (show_hud)
    {
        char scoreText[64];
        sprintf(scoreText, "SCORE %d BEST %d LIVES %d", score, highscore, remaining_lives);
        drawCenteredText(bottom, BOT_WIDTH / 2, 31,
                         scoreText, 1,
                         BOT_WIDTH, BOT_HEIGHT,
                         COL_GOLD);
    }

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
        config_page = 0;
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

void resetAllSettings()
{
    white_mode = false;
    sfx_enabled = true;
    wrap_walls = false;
    show_grid = true;
    volume = 50;
    snake_color_index = 0;
    current_size = SIZE_MEDIUM;
    apple_count = 3;
    speed_multiplier = 1;
    game_mode = MODE_CLASSIC;
    difficulty = DIFF_NORMAL;
    obstacles_enabled = false;
    bonus_enabled = true;
    acceleration_enabled = true;
    lives_setting = 1;
    score_multiplier = 1;
    start_length = 3;
    time_limit = 60;
    custom_r = 80;
    custom_g = 220;
    custom_b = 120;
    use_custom_color = false;
    show_hud = true;
    no_reverse = true;
    screen_flash = true;
    applyVolume();
    saveSettings();
}

void handleSettingsInput(u32 kDown, const touchPosition& touch, GameState& state)
{
    if (!(kDown & KEY_TOUCH))
        return;

    if ((settings_page == 2 && BTN_COLOR_BACK.isClicked(touch)) ||
        (settings_page != 2 && BTN_BACK.isClicked(touch)))
    {
        state = STATE_MAIN_MENU;
        return;
    }

    if ((settings_page == 2 && BTN_COLOR_NEXT.isClicked(touch)) ||
        (settings_page != 2 && BTN_SETTINGS_NAV.isClicked(touch)))
    {
        settings_page = (settings_page + 1) % 4;
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
    else if (settings_page == 1)
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
            settings_page = 2;
            return;
        }

        if (BTN_RESET_HS.isClicked(touch))
        {
            highscore = 0;
            saveHighscore();
            return;
        }
    }
    else if (settings_page == 2)
    {
        if (BTN_CUSTOM_COLOR.isClicked(touch))
        {
            use_custom_color = !use_custom_color;
            saveSettings();
            return;
        }

        if (BTN_RESET_COLOR.isClicked(touch))
        {
            custom_r = 80;
            custom_g = 220;
            custom_b = 120;
            saveSettings();
            return;
        }
    }
    else
    {
        if (BTN_HUD.isClicked(touch))
        {
            show_hud = !show_hud;
            saveSettings();
            return;
        }

        if (BTN_REVERSE.isClicked(touch))
        {
            no_reverse = !no_reverse;
            saveSettings();
            return;
        }

        if (BTN_FLASH.isClicked(touch))
        {
            screen_flash = !screen_flash;
            saveSettings();
            return;
        }

        if (BTN_RESET_SETTINGS.isClicked(touch))
        {
            resetAllSettings();
            return;
        }
    }
}

void handleConfigInput(u32 kDown, const touchPosition& touch, GameState& state)
{
    if (!(kDown & KEY_TOUCH))
        return;

    if (config_page == 0)
    {
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
        if (BTN_LENGTH_DOWN.isClicked(touch))
        {
            --start_length;
            if (start_length < 3) start_length = 3;
            saveSettings();
            return;
        }
        if (BTN_LENGTH_UP.isClicked(touch))
        {
            ++start_length;
            if (start_length > 10) start_length = 10;
            saveSettings();
            return;
        }
        if (BTN_CONFIG_NEXT.isClicked(touch))
        {
            config_page = 1;
            return;
        }
    }
    else if (config_page == 1)
    {
        if (BTN_CONFIG_PREV.isClicked(touch))
        {
            config_page = 0;
            return;
        }
        if (BTN_CONFIG_NEXT.isClicked(touch))
        {
            config_page = 2;
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
        if (BTN_DIFF.isClicked(touch))
        {
            difficulty = (Difficulty)((difficulty + 1) % 4);
            saveSettings();
            return;
        }
        if (BTN_ACCEL.isClicked(touch))
        {
            acceleration_enabled = !acceleration_enabled;
            saveSettings();
            return;
        }
    }
    else
    {
        if (BTN_CONFIG_PREV.isClicked(touch))
        {
            config_page = 1;
            return;
        }
        if (BTN_MODE.isClicked(touch))
        {
            game_mode = (GameMode)((game_mode + 1) % 3);
            saveSettings();
            return;
        }
        if (BTN_OBSTACLE.isClicked(touch))
        {
            obstacles_enabled = !obstacles_enabled;
            saveSettings();
            return;
        }
        if (BTN_BONUS.isClicked(touch))
        {
            bonus_enabled = !bonus_enabled;
            saveSettings();
            return;
        }
        if (BTN_LIFE_DOWN.isClicked(touch))
        {
            --lives_setting;
            if (lives_setting < 1) lives_setting = 1;
            saveSettings();
            return;
        }
        if (BTN_LIFE_UP.isClicked(touch))
        {
            ++lives_setting;
            if (lives_setting > 5) lives_setting = 5;
            saveSettings();
            return;
        }
        if (BTN_MULT.isClicked(touch))
        {
            wrap_walls = !wrap_walls;
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

        if (state == STATE_SETTINGS && touchHeld)
        {
            int oldR = custom_r;
            int oldG = custom_g;
            int oldB = custom_b;
            updateSettingsSliders(touchNow, true);
            if (oldR != custom_r || oldG != custom_g || oldB != custom_b)
                saveSettings();
        }

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
