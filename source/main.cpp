#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <ctime>

// Bildschirm-Dimensionen
#define TOP_WIDTH 400
#define TOP_HEIGHT 240
#define BOT_WIDTH 320
#define BOT_HEIGHT 240

// Spiel-Zustände
enum GameState {
    STATE_MAIN_MENU,
    STATE_SETTINGS,
    STATE_CONFIG,
    STATE_PLAYING,
    STATE_GAME_OVER
};

// Richtungen für die Schlange
enum Direction {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
};

// Struktur für Farben (RGB)
struct Color {
    u8 r, g, b;
    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b;
    }
};

// Kompakter 8x8 Font für ASCII (32 bis 127), gespeichert als 64-Bit Integers
// Jedes Bit repräsentiert einen Pixel (8 Zeilen x 8 Spalten)
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

// Zeichnet einen einzelnen Pixel in den Framebuffer
// Die Displays des 3DS sind physisch im Hochformat eingebaut (rotated um 90 Grad)
void drawPixel(u8* fb, int x, int y, int screen_width, int screen_height, Color c) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height) return;
    int index = ((x * screen_height) + (screen_height - 1 - y)) * 3;
    fb[index] = c.b;
    fb[index + 1] = c.g;
    fb[index + 2] = c.r;
}

// Zeichnet ein gefülltes Rechteck
void fillRect(u8* fb, int x, int y, int w, int h, int screen_width, int screen_height, Color c) {
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            drawPixel(fb, x + i, y + j, screen_width, screen_height, c);
        }
    }
}

// Zeichnet den Umriss eines Rechtecks
void drawRect(u8* fb, int x, int y, int w, int h, int screen_width, int screen_height, Color c) {
    for (int i = 0; i < w; i++) {
        drawPixel(fb, x + i, y, screen_width, screen_height, c);
        drawPixel(fb, x + i, y + h - 1, screen_width, screen_height, c);
    }
    for (int j = 0; j < h; j++) {
        drawPixel(fb, x, y + j, screen_width, screen_height, c);
        drawPixel(fb, x + w - 1, y + j, screen_width, screen_height, c);
    }
}

// Zeichnet ein einzelnes Zeichen skaliert
void drawChar(u8* fb, int x, int y, char c, int scale, int screen_width, int screen_height, Color fg) {
    if (c < 32 || c > 127) return;
    uint64_t bitmap = font8x8[c - 32];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            // Lese das Bit von links nach rechts
            if (bitmap & (1ULL << (63 - (row * 8 + col)))) {
                fillRect(fb, x + col * scale, y + row * scale, scale, scale, screen_width, screen_height, fg);
            }
        }
    }
}

// Zeichnet einen String
void drawString(u8* fb, int x, int y, const char* str, int scale, int screen_width, int screen_height, Color fg) {
    int curr_x = x;
    while (*str) {
        drawChar(fb, curr_x, y, *str, scale, screen_width, screen_height, fg);
        curr_x += 8 * scale; // Bewege X um die Breite des Zeichens
        str++;
    }
}

// Leert den Bildschirm
void clearScreen(u8* fb, int screen_width, int screen_height, Color c) {
    for(int i=0; i<screen_width*screen_height*3; i+=3) {
        fb[i] = c.b; fb[i+1] = c.g; fb[i+2] = c.r;
    }
}

// Touch-Button Klasse
struct Button {
    int x, y, w, h;
    const char* text;
    bool isClicked(touchPosition touch) {
        return (touch.px >= x && touch.px <= x + w &&
                touch.py >= y && touch.py <= y + h);
    }
    void draw(u8* fb, Color fg, Color bg, Color border) {
        fillRect(fb, x, y, w, h, BOT_WIDTH, BOT_HEIGHT, bg);
        drawRect(fb, x, y, w, h, BOT_WIDTH, BOT_HEIGHT, border);
        
        // Zentriere Text grob
        int text_w = strlen(text) * 8 * 2;
        int text_x = x + (w - text_w) / 2;
        int text_y = y + (h - 16) / 2;
        drawString(fb, text_x, text_y, text, 2, BOT_WIDTH, BOT_HEIGHT, fg);
    }
};

// Globale Einstellungen
bool white_mode = false;
int highscore = 0;
int volume = 50; // In Prozent (0-100)

// Spielfeld Optionen
enum GridSize { SIZE_SMALL, SIZE_MEDIUM, SIZE_LARGE };
GridSize current_size = SIZE_MEDIUM;
int apple_count = 3;
int speed_multiplier = 1; // 0 = 0.5x, 1 = 1.0x, 2 = 2.0x

// In-Game Variablen
struct Point { int x, y; };
std::vector<Point> snake;
std::vector<Point> apples;
Direction current_dir = DIR_RIGHT;
Direction next_dir = DIR_RIGHT;
int score = 0;
int frame_count = 0;

// Felddimensionen im Spiel (Tile basiert, 10x10 Pixel)
int grid_w, grid_h, offset_x, offset_y;

void loadHighscore() {
    FILE* f = fopen("sdmc:/snake_highscore.txt", "r");
    if (f) {
        fscanf(f, "%d", &highscore);
        fclose(f);
    }
}

void saveHighscore() {
    FILE* f = fopen("sdmc:/snake_highscore.txt", "w");
    if (f) {
        fprintf(f, "%d", highscore);
        fclose(f);
    }
}

void applyVolume() {
    // libctru NDSP Nutzung für simple Audio-Regelung
    // Da wir keine Musik abspielen, limitieren wir uns auf ndspSetMasterVol
    float volFloat = volume / 100.0f;
    ndspSetMasterVol(volFloat);
}

void spawnApple() {
    Point a;
    bool valid = false;
    while (!valid) {
        a.x = rand() % grid_w;
        a.y = rand() % grid_h;
        valid = true;
        // Kollision mit der Schlange prüfen
        for (const auto& segment : snake) {
            if (segment.x == a.x && segment.y == a.y) {
                valid = false; break;
            }
        }
        // Kollision mit anderen Äpfeln prüfen
        for (const auto& ap : apples) {
             if(ap.x == a.x && ap.y == a.y) {
                 valid = false; break;
             }
        }
    }
    apples.push_back(a);
}

void resetGame() {
    snake.clear();
    apples.clear();
    score = 0;
    current_dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    frame_count = 0;

    // Grid basierend auf Einstellung
    if (current_size == SIZE_SMALL) {
        grid_w = 20; grid_h = 12;
    } else if (current_size == SIZE_MEDIUM) {
        grid_w = 30; grid_h = 18;
    } else {
        grid_w = 40; grid_h = 24;
    }
    
    // Zentrieren auf dem 400x240 Top Screen (Tiles = 10x10 px)
    offset_x = (TOP_WIDTH - (grid_w * 10)) / 2;
    offset_y = (TOP_HEIGHT - (grid_h * 10)) / 2;

    // Startposition der Schlange
    snake.push_back({grid_w / 2, grid_h / 2});
    snake.push_back({grid_w / 2 - 1, grid_h / 2});
    snake.push_back({grid_w / 2 - 2, grid_h / 2});

    for (int i = 0; i < apple_count; i++) {
        spawnApple();
    }
}

int main(int argc, char **argv) {
    // Initialisiere die Grafik
    gfxInitDefault();
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);
    
    // Audio initialisieren, auch wenn nur Volume angepasst wird
    Result ndspRes = ndspInit();
    
    loadHighscore();
    applyVolume();
    srand(time(NULL));

    GameState state = STATE_MAIN_MENU;
    
    // Farben Definitionen
    Color col_black = {0, 0, 0};
    Color col_white = {255, 255, 255};
    Color col_green = {0, 255, 0};
    Color col_red   = {255, 0, 0};
    Color col_gray  = {100, 100, 100};
    Color col_darkgray = {40, 40, 40};

    // UI Buttons
    Button btn_start = {60, 40, 200, 40, "Start"};
    Button btn_settings = {60, 100, 200, 40, "Settings"};
    Button btn_quit = {60, 160, 200, 40, "Quit"};

    // Game Over Buttons
    Button btn_retry = {60, 60, 200, 40, "Retry"};
    Button btn_menu = {60, 140, 200, 40, "Main Menu"};

    // Settings Buttons
    Button btn_white_mode = {60, 140, 200, 40, "White Mode"};
    Button btn_vol_down = {60, 190, 50, 30, "-"};
    Button btn_vol_up = {210, 190, 50, 30, "+"};
    Button btn_back = {10, 10, 80, 30, "Back"};

    // Config Buttons
    Button btn_sz_s = {20, 20, 80, 30, "Small"};
    Button btn_sz_m = {110, 20, 80, 30, "Medium"};
    Button btn_sz_l = {200, 20, 80, 30, "Large"};

    Button btn_ap_1 = {20, 70, 50, 30, "1"};
    Button btn_ap_3 = {80, 70, 50, 30, "3"};
    Button btn_ap_5 = {140, 70, 50, 30, "5"};
    Button btn_ap_c_m = {200, 70, 30, 30, "-"};
    Button btn_ap_c_p = {260, 70, 30, 30, "+"};

    Button btn_sp_05 = {20, 120, 80, 30, "0.5x"};
    Button btn_sp_1 = {110, 120, 80, 30, "1.0x"};
    Button btn_sp_2 = {200, 120, 80, 30, "2.0x"};
    
    Button btn_play = {110, 180, 100, 40, "PLAY"};

    // Touch-Steuerung In-Game
    Button btn_up = {130, 30, 60, 60, "^"};
    Button btn_down = {130, 150, 60, 60, "v"};
    Button btn_left = {50, 90, 60, 60, "<"};
    Button btn_right = {210, 90, 60, 60, ">"};

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        touchPosition touch;
        
        if (kDown & KEY_TOUCH) {
            hidTouchRead(&touch);
        }

        // Hardware-Tasten (D-Pad, Circle Pad) Steuerung
        if (state == STATE_PLAYING) {
            circlePosition cpos;
            hidCircleRead(&cpos);
            if ((kHeld & KEY_DUP) || cpos.dy > 50) { if (current_dir != DIR_DOWN) next_dir = DIR_UP; }
            else if ((kHeld & KEY_DDOWN) || cpos.dy < -50) { if (current_dir != DIR_UP) next_dir = DIR_DOWN; }
            else if ((kHeld & KEY_DLEFT) || cpos.dx < -50) { if (current_dir != DIR_RIGHT) next_dir = DIR_LEFT; }
            else if ((kHeld & KEY_DRIGHT) || cpos.dx > 50) { if (current_dir != DIR_LEFT) next_dir = DIR_RIGHT; }
        }

        if (state == STATE_MAIN_MENU) {
            if (kDown & KEY_TOUCH) {
                if (btn_start.isClicked(touch)) state = STATE_CONFIG;
                if (btn_settings.isClicked(touch)) state = STATE_SETTINGS;
                if (btn_quit.isClicked(touch)) break; // Beendet das Spiel
            }
        } 
        else if (state == STATE_SETTINGS) {
            if (kDown & KEY_TOUCH) {
                if (btn_back.isClicked(touch)) state = STATE_MAIN_MENU;
                if (btn_white_mode.isClicked(touch)) white_mode = !white_mode;
                if (btn_vol_down.isClicked(touch) && volume > 0) { volume -= 10; applyVolume(); }
                if (btn_vol_up.isClicked(touch) && volume < 100) { volume += 10; applyVolume(); }
            }
        }
        else if (state == STATE_CONFIG) {
            if (kDown & KEY_TOUCH) {
                if (btn_back.isClicked(touch)) state = STATE_MAIN_MENU;
                if (btn_sz_s.isClicked(touch)) current_size = SIZE_SMALL;
                if (btn_sz_m.isClicked(touch)) current_size = SIZE_MEDIUM;
                if (btn_sz_l.isClicked(touch)) current_size = SIZE_LARGE;
                
                if (btn_ap_1.isClicked(touch)) apple_count = 1;
                if (btn_ap_3.isClicked(touch)) apple_count = 3;
                if (btn_ap_5.isClicked(touch)) apple_count = 5;
                if (btn_ap_c_m.isClicked(touch)) { apple_count--; if(apple_count < 1) apple_count = 1; }
                if (btn_ap_c_p.isClicked(touch)) { apple_count++; if(apple_count > 50) apple_count = 50; }
                
                if (btn_sp_05.isClicked(touch)) speed_multiplier = 0;
                if (btn_sp_1.isClicked(touch)) speed_multiplier = 1;
                if (btn_sp_2.isClicked(touch)) speed_multiplier = 2;

                if (btn_play.isClicked(touch)) {
                    resetGame();
                    state = STATE_PLAYING;
                }
            }
        }
        else if (state == STATE_PLAYING) {
            if (kDown & KEY_TOUCH) {
                if (btn_up.isClicked(touch) && current_dir != DIR_DOWN) next_dir = DIR_UP;
                if (btn_down.isClicked(touch) && current_dir != DIR_UP) next_dir = DIR_DOWN;
                if (btn_left.isClicked(touch) && current_dir != DIR_RIGHT) next_dir = DIR_LEFT;
                if (btn_right.isClicked(touch) && current_dir != DIR_LEFT) next_dir = DIR_RIGHT;
            }

            // Geschwindigkeit regulieren (0.5x=20 frames, 1x=10 frames, 2x=5 frames)
            int move_delay = (speed_multiplier == 0) ? 20 : ((speed_multiplier == 1) ? 10 : 5);
            frame_count++;

            if (frame_count >= move_delay) {
                frame_count = 0;
                current_dir = next_dir;
                
                Point head = snake.front();
                Point new_head = head;
                
                if (current_dir == DIR_UP) new_head.y--;
                if (current_dir == DIR_DOWN) new_head.y++;
                if (current_dir == DIR_LEFT) new_head.x--;
                if (current_dir == DIR_RIGHT) new_head.x++;

                // Kollisionen prüfen
                bool dead = false;
                if (new_head.x < 0 || new_head.x >= grid_w || new_head.y < 0 || new_head.y >= grid_h) dead = true;
                for (const auto& segment : snake) {
                    if (segment.x == new_head.x && segment.y == new_head.y) dead = true;
                }

                if (dead) {
                    if (score > highscore) {
                        highscore = score;
                        saveHighscore();
                    }
                    state = STATE_GAME_OVER;
                } else {
                    snake.insert(snake.begin(), new_head);
                    
                    // Apfel gefressen?
                    bool ate = false;
                    for (size_t i = 0; i < apples.size(); i++) {
                        if (apples[i].x == new_head.x && apples[i].y == new_head.y) {
                            ate = true;
                            apples.erase(apples.begin() + i);
                            score += 10;
                            spawnApple();
                            break;
                        }
                    }
                    if (!ate) {
                        snake.pop_back(); // Kein Apfel -> Schwanz nachziehen
                    }
                }
            }
        }
        else if (state == STATE_GAME_OVER) {
            if (kDown & KEY_TOUCH) {
                if (btn_retry.isClicked(touch)) {
                    resetGame();
                    state = STATE_PLAYING;
                }
                if (btn_menu.isClicked(touch)) {
                    state = STATE_MAIN_MENU;
                }
            }
        }

        u8* top_fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
        u8* bot_fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

        Color bg = white_mode ? col_white : col_black;
        Color fg = white_mode ? col_black : col_white;
        Color btn_bg = white_mode ? col_white : col_darkgray;

        clearScreen(top_fb, TOP_WIDTH, TOP_HEIGHT, bg);
        clearScreen(bot_fb, BOT_WIDTH, BOT_HEIGHT, bg);

        if (state == STATE_MAIN_MENU) {
            // Top Screen Menu
            drawString(top_fb, 140, 80, "S N A K E", 3, TOP_WIDTH, TOP_HEIGHT, fg);
            char hs_text[32];
            sprintf(hs_text, "Highscore: %d", highscore);
            drawString(top_fb, 140, 10, hs_text, 2, TOP_WIDTH, TOP_HEIGHT, fg);
            
            // Bottom Screen Menu
            btn_start.draw(bot_fb, fg, btn_bg, fg);
            btn_settings.draw(bot_fb, fg, btn_bg, fg);
            btn_quit.draw(bot_fb, fg, btn_bg, fg);
        }
        else if (state == STATE_SETTINGS) {
            // Top Screen (Controls)
            drawString(top_fb, 20, 20, "CONTROLS:", 2, TOP_WIDTH, TOP_HEIGHT, fg);
            drawString(top_fb, 20, 60, "- Touch Arrows on bottom screen", 2, TOP_WIDTH, TOP_HEIGHT, fg);
            drawString(top_fb, 20, 100, "- D-Pad", 2, TOP_WIDTH, TOP_HEIGHT, fg);
            drawString(top_fb, 20, 140, "- Circle Pad", 2, TOP_WIDTH, TOP_HEIGHT, fg);
            
            // Bottom Screen
            btn_back.draw(bot_fb, fg, btn_bg, fg);
            btn_white_mode.draw(bot_fb, fg, btn_bg, fg);
            
            char vol_text[32];
            sprintf(vol_text, "Volume: %d%%", volume);
            drawString(bot_fb, 115, 200, vol_text, 2, BOT_WIDTH, BOT_HEIGHT, fg);
            btn_vol_down.draw(bot_fb, fg, btn_bg, fg);
            btn_vol_up.draw(bot_fb, fg, btn_bg, fg);
        }
        else if (state == STATE_CONFIG) {
            // Top Screen (Information)
            drawString(top_fb, 120, 100, "Setup Game", 3, TOP_WIDTH, TOP_HEIGHT, fg);
            
            // Bottom Screen
            btn_back.draw(bot_fb, fg, btn_bg, fg);
            
            drawString(bot_fb, 20, 5, "Size:", 1, BOT_WIDTH, BOT_HEIGHT, fg);
            btn_sz_s.draw(bot_fb, fg, (current_size==SIZE_SMALL)?col_gray:btn_bg, fg);
            btn_sz_m.draw(bot_fb, fg, (current_size==SIZE_MEDIUM)?col_gray:btn_bg, fg);
            btn_sz_l.draw(bot_fb, fg, (current_size==SIZE_LARGE)?col_gray:btn_bg, fg);

            drawString(bot_fb, 20, 55, "Apples:", 1, BOT_WIDTH, BOT_HEIGHT, fg);
            btn_ap_1.draw(bot_fb, fg, (apple_count==1)?col_gray:btn_bg, fg);
            btn_ap_3.draw(bot_fb, fg, (apple_count==3)?col_gray:btn_bg, fg);
            btn_ap_5.draw(bot_fb, fg, (apple_count==5)?col_gray:btn_bg, fg);
            btn_ap_c_m.draw(bot_fb, fg, btn_bg, fg);
            char ap_text[16]; sprintf(ap_text, "%d", apple_count);
            drawString(bot_fb, 235, 78, ap_text, 2, BOT_WIDTH, BOT_HEIGHT, fg);
            btn_ap_c_p.draw(bot_fb, fg, btn_bg, fg);

            drawString(bot_fb, 20, 105, "Speed:", 1, BOT_WIDTH, BOT_HEIGHT, fg);
            btn_sp_05.draw(bot_fb, fg, (speed_multiplier==0)?col_gray:btn_bg, fg);
            btn_sp_1.draw(bot_fb, fg, (speed_multiplier==1)?col_gray:btn_bg, fg);
            btn_sp_2.draw(bot_fb, fg, (speed_multiplier==2)?col_gray:btn_bg, fg);

            btn_play.draw(bot_fb, fg, btn_bg, fg);
        }
        else if (state == STATE_PLAYING) {
            // Top Screen (Game)
            char sc_text[32];
            sprintf(sc_text, "Score: %d  High: %d", score, highscore);
            drawString(top_fb, 10, 5, sc_text, 1, TOP_WIDTH, TOP_HEIGHT, fg);

            // Spielfeld-Rand
            drawRect(top_fb, offset_x-1, offset_y-1, grid_w*10+2, grid_h*10+2, TOP_WIDTH, TOP_HEIGHT, fg);
            
            // Äpfel
            for (const auto& a : apples) {
                fillRect(top_fb, offset_x + a.x*10, offset_y + a.y*10, 10, 10, TOP_WIDTH, TOP_HEIGHT, col_red);
            }
            // Schlange
            for (const auto& s : snake) {
                fillRect(top_fb, offset_x + s.x*10, offset_y + s.y*10, 10, 10, TOP_WIDTH, TOP_HEIGHT, col_green);
                drawRect(top_fb, offset_x + s.x*10, offset_y + s.y*10, 10, 10, TOP_WIDTH, TOP_HEIGHT, col_black);
            }

            // Bottom Screen (Controls)
            btn_up.draw(bot_fb, fg, btn_bg, fg);
            btn_down.draw(bot_fb, fg, btn_bg, fg);
            btn_left.draw(bot_fb, fg, btn_bg, fg);
            btn_right.draw(bot_fb, fg, btn_bg, fg);
        }
        else if (state == STATE_GAME_OVER) {
            // Top Screen
            drawString(top_fb, 110, 80, "GAME OVER", 4, TOP_WIDTH, TOP_HEIGHT, fg);
            char go_sc[32];
            sprintf(go_sc, "Score: %d", score);
            drawString(top_fb, 150, 140, go_sc, 2, TOP_WIDTH, TOP_HEIGHT, fg);

            // Bottom Screen
            btn_retry.draw(bot_fb, fg, btn_bg, fg);
            btn_menu.draw(bot_fb, fg, btn_bg, fg);
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    // Aufräumen und Verlassen
    if (ndspRes == 0) ndspExit();
    gfxExit();
    return 0;
}
