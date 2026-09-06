#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <vector>

// --- Enums & Status ---
enum Mode { MODE_2P, MODE_EASY, MODE_MED, MODE_HARD, MODE_IMP };
enum Player { NONE, PLAYER_X, PLAYER_O };
enum GameState { MENU, PLAYING, GAMEOVER };

PrintConsole topScreen, bottomScreen;
Player board[9];
Mode currentMode = MODE_2P;
Player currentPlayer = PLAYER_X;
Player humanPlayer = PLAYER_X;
GameState state = MENU;
bool darkMode = true;

// --- Hilfsfunktionen ---
void resetBoard() {
    for (int i = 0; i < 9; i++) board[i] = NONE;
    currentPlayer = PLAYER_X;
    state = PLAYING;
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

// --- AI: Minimax Algorithmus ---
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

    // Wahrscheinlichkeiten für Random-Moves vs. perfekte Minimax-Moves
    bool useMinimax = false;
    if (currentMode == MODE_IMP) useMinimax = true;
    else if (currentMode == MODE_HARD && rng < 80) useMinimax = true;
    else if (currentMode == MODE_MED && rng < 50) useMinimax = true;

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
}

// --- Rendering ---
void drawBoard() {
    printf("\x1b[2J"); // Clear Screen
    if (darkMode) printf("\x1b[37;40m"); else printf("\x1b[30;47m");

    printf("\x1b[2;10H=== TIC TAC TOE ===\n\n");
    if (state == GAMEOVER) {
        Player w = checkWin();
        if (w == NONE) printf("\x1b[4;15H UNENTSCHIEDEN! ");
        else printf("\x1b[4;15H GEWINNER: %c ", w == PLAYER_X ? 'X' : 'O');
    } else {
        printf("\x1b[4;15H AM ZUG: %c ", currentPlayer == PLAYER_X ? 'X' : 'O');
    }

    // Spielfeld rendern
    for (int i = 0; i < 9; i++) {
        int x = 15 + (i % 3) * 4;
        int y = 8 + (i / 3) * 2;
        char c = (board[i] == NONE) ? '.' : (board[i] == PLAYER_X ? 'X' : 'O');
        printf("\x1b[%d;%dH %c ", y, x, c);
    }
}

void drawBottomUI() {
    consoleSelect(&bottomScreen);
    drawBoard(); // Board auch unten rendern

    // Buttons rendern
    printf("\x1b[16;2H [1] 2-Player (X/O Wahl in Menu)");
    printf("\x1b[18;2H [2] Easy   [3] Medium");
    printf("\x1b[20;2H [4] Hard   [5] Impossible");
    printf("\x1b[23;2H [T] Dark/White Toggle");
    
    printf("\x1b[26;2H MODUS: %d  |  SPIELER: %c", currentMode, humanPlayer == PLAYER_X ? 'X' : 'O');
}

void drawTopUI() {
    consoleSelect(&topScreen);
    drawBoard();
}

// --- Main Loop ---
int main(int argc, char **argv) {
    gfxInitDefault();
    consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);
    srand(time(NULL));

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_START) break; // Exit game

        // Touch Input Logik (Bottom Screen)
        if (kDown & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);

            // Toggle Theme (Grobe Touch-Zone unten links)
            if (touch.py > 180 && touch.py < 200 && touch.px < 100) darkMode = !darkMode;

            // Modi-Auswahl
            if (touch.py > 120 && touch.py < 140) {
                if (touch.px < 160) { currentMode = MODE_2P; humanPlayer = PLAYER_X; resetBoard(); }
                else { currentMode = MODE_2P; humanPlayer = PLAYER_O; resetBoard(); } // O als 1. Spieler
            }
            if (touch.py > 140 && touch.py < 160) {
                if (touch.px < 100) { currentMode = MODE_EASY; resetBoard(); }
                else if (touch.px > 100) { currentMode = MODE_MED; resetBoard(); }
            }
            if (touch.py > 160 && touch.py < 180) {
                if (touch.px < 100) { currentMode = MODE_HARD; resetBoard(); }
                else if (touch.px > 100) { currentMode = MODE_IMP; resetBoard(); }
            }

            // Spielfeld-Eingabe (3x3 Raster, ca. 120x60 Pixel in der Mitte)
            if (state == PLAYING) {
                if (currentMode == MODE_2P || currentPlayer == humanPlayer) {
                    if (touch.px > 110 && touch.px < 210 && touch.py > 50 && touch.py < 110) {
                        int col = (touch.px - 110) / 33;
                        int row = (touch.py - 50) / 20;
                        int idx = row * 3 + col;
                        if (idx >= 0 && idx < 9 && board[idx] == NONE) {
                            board[idx] = currentPlayer;
                            if (checkWin() != NONE || isDraw()) state = GAMEOVER;
                            else {
                                currentPlayer = (currentPlayer == PLAYER_X) ? PLAYER_O : PLAYER_X;
                                if (currentMode != MODE_2P) aiMove();
                            }
                        }
                    }
                }
            } else if (state == GAMEOVER) {
                resetBoard(); // Neustart bei Tap nach GameOver
            }
        }

        drawTopUI();
        drawBottomUI();
        
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
