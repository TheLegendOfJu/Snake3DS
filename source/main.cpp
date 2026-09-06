#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <vector>

enum Mode { MODE_2P, MODE_EASY, MODE_MED, MODE_HARD, MODE_IMP };
enum Player { NONE, PLAYER_X, PLAYER_O };
enum GameState { MENU_2P_SELECT, PLAYING, GAMEOVER };

PrintConsole topScreen, bottomScreen;
Player board[9];
Mode currentMode = MODE_2P;
Player currentPlayer = PLAYER_X;
Player humanPlayer = PLAYER_X;
GameState state = PLAYING;
bool darkMode = true;
bool needsRedraw = true; // Verhindert Bildschirm-Flackern!

void resetBoard() {
    for (int i = 0; i < 9; i++) board[i] = NONE;
    currentPlayer = PLAYER_X;
    if (currentMode == MODE_2P) state = MENU_2P_SELECT;
    else state = PLAYING;
    needsRedraw = true;
}

Player checkWin() {
    int wins[8][3] = {{0,1,2}, {3,4,5}, {6,7,8}, {0,3,6}, {1,4,7}, {2,5,8}, {0,4,8}, {2,4,6}};
    for (int i = 0; i < 8; i++) {
        if (board[wins[i][0]] != NONE &&
            board[wins[i][0]] == board[wins[i][1]] &&
            board[wins[i][1]] == board[wins[i][2]])
            return board[wins[i][0]];
    }
    return NONE;
}

bool isDraw() {
    for (int i = 0; i < 9; i++) if (board[i] == NONE) return false;
    return true;
}

int minimax(Player b[9], int depth, bool isMax, Player ai, Player human) {
    Player winner = checkWin();
    if (winner == ai) return 10 - depth;
    if (winner == human) return -10 + depth;
    if (isDraw()) return 0;

    if (isMax) {
        int best = -1000;
        for (int i = 0; i < 9; i++) {
            if (b[i] == NONE) {
                b[i] = ai;
                best = std::max(best, minimax(b, depth + 1, !isMax, ai, human));
                b[i] = NONE;
            }
        }
        return best;
    } else {
        int best = 1000;
        for (int i = 0; i < 9; i++) {
            if (b[i] == NONE) {
                b[i] = human;
                best = std::min(best, minimax(b, depth + 1, !isMax, ai, human));
                b[i] = NONE;
            }
        }
        return best;
    }
}

void aiMove() {
    if (state != PLAYING || currentMode == MODE_2P) return;
    
    Player ai = (humanPlayer == PLAYER_X) ? PLAYER_O : PLAYER_X;
    std::vector<int> available;
    for (int i = 0; i < 9; i++) if (board[i] == NONE) available.push_back(i);
    if (available.empty()) return;

    int move = -1;
    int rng = rand() % 100;
    bool useMinimax = false;

    if (currentMode == MODE_IMP) useMinimax = true;
    else if (currentMode == MODE_HARD && rng < 80) useMinimax = true;
    else if (currentMode == MODE_MED && rng < 40) useMinimax = true;

    if (useMinimax) {
        int bestVal = -1000;
        for (int i = 0; i < 9; i++) {
            if (board[i] == NONE) {
                board[i] = ai;
                int moveVal = minimax(board, 0, false, ai, humanPlayer);
                board[i] = NONE;
                if (moveVal > bestVal) { bestVal = moveVal; move = i; }
            }
        }
    } 
    if (move == -1) {
        move = available[rand() % available.size()];
    }

    board[move] = currentPlayer;
    if (checkWin() != NONE || isDraw()) state = GAMEOVER;
    else currentPlayer = (currentPlayer == PLAYER_X) ? PLAYER_O : PLAYER_X;
    
    needsRedraw = true;
}

// Globales UI Rendering ohne Clear-Screen Flackern
void drawUI() {
    const char* bg = darkMode ? "\x1b[37;40m" : "\x1b[30;47m";
    const char* activeBg = darkMode ? "\x1b[30;47m" : "\x1b[37;40m";

    // --- TOP SCREEN ---
    consoleSelect(&topScreen);
    printf("%s\x1b[1;1H", bg);
    printf("\x1b[2J"); // Nur bei Render-Call leeren
    printf("\x1b[2;11H=== TIC TAC TOE ===");
    
    if (state == GAMEOVER) {
        Player w = checkWin();
        if (w == NONE) printf("\x1b[4;11H  UNENTSCHIEDEN!   ");
        else printf("\x1b[4;11H GEWINNER: %c       ", w == PLAYER_X ? 'X' : 'O');
    } else if (state == MENU_2P_SELECT) {
        printf("\x1b[4;7H[2P] Waehle X oder O unten!");
    } else {
        printf("\x1b[4;11H AM ZUG: %c         ", currentPlayer == PLAYER_X ? 'X' : 'O');
    }

    // Spielfeld Oben
    for (int i = 0; i < 9; i++) {
        int col = 11 + (i % 3) * 6;
        int row = 8 + (i / 3) * 3;
        char c = (board[i] == NONE) ? ' ' : (board[i] == PLAYER_X ? 'X' : 'O');
        printf("\x1b[%d;%dH+---+", row - 1, col);
        printf("\x1b[%d;%dH| %c |", row, col, c);
        printf("\x1b[%d;%dH+---+", row + 1, col);
    }

    // --- BOTTOM SCREEN ---
    consoleSelect(&bottomScreen);
    printf("%s\x1b[1;1H", bg);
    printf("\x1b[2J");

    // 5 Modi-Buttons ganz oben nebeneinander (Zeile 1 bis 3)
    const char* mNames[5] = {" 2P ", "Easy", "Med ", "Hard", "Imp "};
    for (int i = 0; i < 5; i++) {
        bool isSel = (currentMode == i);
        printf("\x1b[1;%dH%s%s%s", i * 8 + 1, isSel ? activeBg : bg, mNames[i], bg);
    }

    // 2-Player Sonderauswahl Menu
    if (state == MENU_2P_SELECT) {
        printf("\x1b[6;5HWer beginnt?");
        printf("\x1b[8;5H[ S2: X ]   [ S2: O ]");
    } else {
        // Touch-Spielfeld Unten (Größer für leichte Bedienung)
        for (int i = 0; i < 9; i++) {
            int col = 7 + (i % 3) * 8;
            int row = 6 + (i / 3) * 4;
            char c = (board[i] == NONE) ? ' ' : (board[i] == PLAYER_X ? 'X' : 'O');
            printf("\x1b[%d;%dH+-----+", row - 1, col);
            printf("\x1b[%d;%dH|  %c  |", row, col, c);
            printf("\x1b[%d;%dH+-----+", row + 1, col);
        }
    }

    // Footer Buttons
    printf("\x1b[21;2H[ Mode: %s ]", mNames[currentMode]);
    printf("\x1b[23;2H[ T: Dark/White Toggle ]");
    if (state == GAMEOVER) printf("\x1b[23;26H[ Neustart ]");
}

int main(int argc, char **argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);
    srand(time(NULL));

    resetBoard();

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break;

        if (kDown & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);

            // 1. Modi-Auswahl Oben (Y: 0-30 px, X verteilt von 0 bis 320 px)
            if (touch.py <= 30) {
                int selectedMode = touch.px / 64; // 320 / 5 = 64px pro Button
                if (selectedMode >= 0 && selectedMode < 5) {
                    currentMode = (Mode)selectedMode;
                    humanPlayer = PLAYER_X;
                    resetBoard();
                }
            }

            // 2. Theme Toggle (Unten Links)
            else if (touch.py > 160 && touch.px < 150) {
                darkMode = !darkMode;
                needsRedraw = true;
            }

            // 3. 2-Player X/O Modus Wahl
            else if (state == MENU_2P_SELECT) {
                if (touch.py > 40 && touch.py < 80) {
                    if (touch.px < 160) humanPlayer = PLAYER_X;
                    else humanPlayer = PLAYER_O;
                    state = PLAYING;
                    needsRedraw = true;
                }
            }

            // 4. Spielfeld Touch (Raster 3x3 im Bereich X: 45-255, Y: 40-160)
            else if (state == PLAYING) {
                if (currentMode == MODE_2P || currentPlayer == humanPlayer) {
                    if (touch.px >= 45 && touch.px <= 255 && touch.py >= 40 && touch.py <= 160) {
                        int col = (touch.px - 45) / 70;
                        int row = (touch.py - 40) / 40;
                        int idx = row * 3 + col;

                        if (idx >= 0 && idx < 9 && board[idx] == NONE) {
                            board[idx] = currentPlayer;
                            needsRedraw = true;

                            if (checkWin() != NONE || isDraw()) {
                                state = GAMEOVER;
                            } else {
                                currentPlayer = (currentPlayer == PLAYER_X) ? PLAYER_O : PLAYER_X;
                                if (currentMode != MODE_2P) {
                                    aiMove();
                                }
                            }
                        }
                    }
                }
            } 
            else if (state == GAMEOVER) {
                resetBoard();
            }
        }

        // Zeichne NUR neu, wenn eine Eingabe/Aktion passiert ist (Fix gegen Flackern)
        if (needsRedraw) {
            drawUI();
            needsRedraw = false;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
