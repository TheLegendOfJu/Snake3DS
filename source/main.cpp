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
bool needsRedraw = true; 

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

void drawUI() {
    const char* bg = darkMode ? "\x1b[37;40m" : "\x1b[30;47m";
    const char* activeBg = darkMode ? "\x1b[30;47m" : "\x1b[37;40m";

    // --- TOP SCREEN ---
    consoleSelect(&topScreen);
    printf("%s\x1b[2J", bg); 
    
    // 30x30 Riesiges Raster (1:1 Seitenverhältnis, zentriert)
    for(int r = 1; r <= 29; r++) {
        for(int c = 11; c <= 39; c++) {
            if (r == 10 || r == 20) {
                if (c == 20 || c == 30) printf("\x1b[%d;%dH+", r, c);
                else printf("\x1b[%d;%dH-", r, c);
            } else {
                if (c == 20 || c == 30) printf("\x1b[%d;%dH|", r, c);
            }
        }
    }

    // Riesige ASCII X und O
    for(int i = 0; i < 9; i++) {
        if (board[i] == NONE) continue;
        int row = 1 + (i / 3) * 10;
        int col = 11 + (i % 3) * 10;
        
        if (board[i] == PLAYER_X) {
            printf("\x1b[%d;%dHXX     XX", row+1, col);
            printf("\x1b[%d;%dH XX   XX ", row+2, col);
            printf("\x1b[%d;%dH  XX XX  ", row+3, col);
            printf("\x1b[%d;%dH   XXX   ", row+4, col);
            printf("\x1b[%d;%dH  XX XX  ", row+5, col);
            printf("\x1b[%d;%dH XX   XX ", row+6, col);
            printf("\x1b[%d;%dHXX     XX", row+7, col);
        } else {
            printf("\x1b[%d;%dH  OOOOO  ", row+1, col);
            printf("\x1b[%d;%dH OO   OO ", row+2, col);
            printf("\x1b[%d;%dHOO     OO", row+3, col);
            printf("\x1b[%d;%dHOO     OO", row+4, col);
            printf("\x1b[%d;%dHOO     OO", row+5, col);
            printf("\x1b[%d;%dH OO   OO ", row+6, col);
            printf("\x1b[%d;%dH  OOOOO  ", row+7, col);
        }
    }

    // --- BOTTOM SCREEN ---
    consoleSelect(&bottomScreen);
    printf("%s\x1b[2J", bg);

    // 5 Modi-Tabs (Zeile 1-2, volle Breite)
    const char* mNames[5] = {"   2P   ", "  Easy  ", " Medium ", "  Hard  ", " Imposs "};
    for (int i = 0; i < 5; i++) {
        bool isSel = (currentMode == i);
        printf("\x1b[1;%dH%s%s%s", i * 8 + 1, isSel ? activeBg : bg, mNames[i], bg);
        printf("\x1b[2;%dH%s        %s", i * 8 + 1, isSel ? activeBg : bg, bg); 
    }

    if (state == MENU_2P_SELECT) {
        printf("\x1b[12;8H+---------+    +---------+");
        printf("\x1b[13;8H| S1: X   |    | S1: O   |");
        printf("\x1b[14;8H+---------+    +---------+");
        printf("\x1b[17;12H(Bitte oben waehlen)");
    } else {
        // Kleineres Touch-Gitter unten
        for(int r = 5; r <= 21; r++) {
            for(int c = 9; c <= 31; c++) {
                if (r == 10 || r == 16) {
                    if (c == 16 || c == 24) printf("\x1b[%d;%dH+", r, c);
                    else printf("\x1b[%d;%dH-", r, c);
                } else {
                    if (c == 16 || c == 24) printf("\x1b[%d;%dH|", r, c);
                }
            }
        }
        for(int i = 0; i < 9; i++) {
            if (board[i] == NONE) continue;
            int row = 5 + (i / 3) * 6;
            int col = 9 + (i % 3) * 8;
            printf("\x1b[%d;%dH%c", row + 2, col + 3, board[i] == PLAYER_X ? 'X' : 'O'); 
        }
    }

    // UI Texte unten
    if (state == GAMEOVER) {
        Player w = checkWin();
        if (w == NONE) printf("\x1b[25;13H UNENTSCHIEDEN! ");
        else printf("\x1b[25;14H GEWINNER: %c ", w == PLAYER_X ? 'X' : 'O');
        printf("\x1b[27;14H [ Neustart ] ");
    } else if (state == PLAYING) {
        printf("\x1b[25;15H AM ZUG: %c ", currentPlayer == PLAYER_X ? 'X' : 'O');
    }
    printf("\x1b[29;8H[ T: Dark/White Toggle (hier tippen) ]");
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

            // 1. Modi-Tabs (obere 30 Pixel, verteilt in 5 Zonen à 64 Pixel)
            if (touch.py <= 30) {
                int selectedMode = touch.px / 64; 
                if (selectedMode >= 0 && selectedMode < 5) {
                    currentMode = (Mode)selectedMode;
                    humanPlayer = PLAYER_X;
                    resetBoard();
                }
            }
            // 2. Theme Toggle (Ganz unten)
            else if (touch.py > 200) {
                darkMode = !darkMode;
                needsRedraw = true;
            }
            // 3. 2-Player X/O Auswahl
            else if (state == MENU_2P_SELECT) {
                if (touch.py > 80 && touch.py < 160) {
                    humanPlayer = (touch.px < 160) ? PLAYER_X : PLAYER_O;
                    state = PLAYING;
                    needsRedraw = true;
                }
            }
            // 4. Spielfeld Touch auf dem 3x3 Raster
            else if (state == PLAYING) {
                if (currentMode == MODE_2P || currentPlayer == humanPlayer) {
                    if (touch.px >= 72 && touch.px <= 256 && touch.py >= 40 && touch.py <= 176) {
                        int col = (touch.px - 72) / 61;
                        int row = (touch.py - 40) / 45;
                        if (col > 2) col = 2;
                        if (row > 2) row = 2;
                        int idx = row * 3 + col;

                        if (idx >= 0 && idx < 9 && board[idx] == NONE) {
                            board[idx] = currentPlayer;
                            needsRedraw = true;

                            if (checkWin() != NONE || isDraw()) {
                                state = GAMEOVER;
                            } else {
                                currentPlayer = (currentPlayer == PLAYER_X) ? PLAYER_O : PLAYER_X;
                                if (currentMode != MODE_2P) aiMove();
                            }
                        }
                    }
                }
            } 
            else if (state == GAMEOVER) {
                resetBoard(); // Überall Tippen startet neu
            }
        }

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
