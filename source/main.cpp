#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Bildschirm-Dimensionen
#define SCREEN_WIDTH 40
#define SCREEN_HEIGHT 28

// Struktur für Upgrades
typedef struct {
    char name[20];
    double cost;
    double cps; // Cookies pro Sekunde
    int count;
} Upgrade;

// Spiel-Zustand
static double cookies = 0;
static double totalCookiesEarned = 0;
static double cookiesPerSecond = 0;
static double cookiesPerClick = 1;

static Upgrade upgrades[2];
static u64 lastTickTime = 0;

// Farb-Definitionen mittels ANSI Escape Codes
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_WHITE   "\x1b[37m"
#define COLOR_MAGENTA "\x1b[35m"

// Bildschirm leeren
void clearScreen() {
    printf("\x1b[2J");
}

// CPS und Gesamtproduktion neu berechnen
void recalculateCPS() {
    cookiesPerSecond = 0;
    for (int i = 0; i < 2; i++) {
        cookiesPerSecond += upgrades[i].count * upgrades[i].cps;
    }
}

// Spielstand auf die SD-Karte schreiben
void saveGame() {
    FILE *f = fopen("/cookie_save.dat", "w");
    if (f) {
        fprintf(f, "%f\n%f\n%d\n%d\n%f\n%f\n", 
            cookies, 
            totalCookiesEarned, 
            upgrades[0].count, 
            upgrades[1].count,
            upgrades[0].cost,
            upgrades[1].cost
        );
        fclose(f);
    }
}

// Spielstand beim Starten wieder einlesen
void loadGame() {
    FILE *f = fopen("/cookie_save.dat", "r");
    if (f) {
        fscanf(f, "%lf\n%lf\n%d\n%d\n%lf\n%lf\n", 
            &cookies, 
            &totalCookiesEarned, 
            &upgrades[0].count, 
            &upgrades[1].count,
            &upgrades[0].cost,
            &upgrades[1].cost
        );
        fclose(f);
        recalculateCPS(); // CPS an geladene Upgrades anpassen
    }
}

// Initialisierung
void initGame() {
    cookies = 0;
    totalCookiesEarned = 0;
    cookiesPerSecond = 0;
    cookiesPerClick = 1;

    // Upgrade 0: Cursor
    strcpy(upgrades[0].name, "Cursor");
    upgrades[0].cost = 15;
    upgrades[0].cps = 0.5;
    upgrades[0].count = 0;

    // Upgrade 1: Oma
    strcpy(upgrades[1].name, "Oma");
    upgrades[1].cost = 100;
    upgrades[1].cps = 4.0;
    upgrades[1].count = 0;

    // Vorhandenen Spielstand laden (falls vorhanden)
    loadGame();

    lastTickTime = osGetTime();
}

// Spiellogik (Idle-Passiv-Einkommen pro Sekunde)
void updateGame() {
    u64 currentTime = osGetTime();
    double elapsedSeconds = (currentTime - lastTickTime) / 1000.0;

    if (elapsedSeconds >= 0.1) { // Alle 100ms aktualisieren für flüssigen Zuwachs
        double earned = cookiesPerSecond * elapsedSeconds;
        cookies += earned;
        totalCookiesEarned += earned;
        lastTickTime = currentTime;
    }
}

// UI und Spieloberfläche zeichnen
void drawGame() {
    clearScreen();

    // Oberer Rahmen
    printf(COLOR_CYAN);
    printf("\x1b[1;1H┌────────────────────────────────────────┐");
    printf("\x1b[2;12H=== 3DS COOKIE CLICKER ===");
    printf("\x1b[3;1H├────────────────────────────────────────┤");

    // Cookie-Zähler
    printf(COLOR_YELLOW);
    printf("\x1b[5;4H🍪 Cookies: %.1f", cookies);
    printf(COLOR_WHITE);
    printf("\x1b[6;4H⚡ Pro Sekunde (CPS): %.1f", cookiesPerSecond);
    printf("\x1b[7;4H👆 Pro Klick: %.1f", cookiesPerClick);

    // Trenner
    printf(COLOR_CYAN);
    printf("\x1b[9;1H├────────────────────────────────────────┤");
    printf("\x1b[10;4H" COLOR_MAGENTA "--- UPGRADES ---" COLOR_WHITE);

    // Upgrade-Liste anzeigen
    for (int i = 0; i < 2; i++) {
        int row = 12 + (i * 3);
        if (cookies >= upgrades[i].cost) {
            printf(COLOR_GREEN);
        } else {
            printf(COLOR_RED);
        }
        printf("\x1b[%d;4H[%d] %s (Anzahl: %d)", row, i + 1, upgrades[i].name, upgrades[i].count);
        printf(COLOR_WHITE);
        printf("\x1b[%d;6HPreis: %.0f C | CPS: +%.1f", row + 1, upgrades[i].cost, upgrades[i].cps);
    }

    // Steuerungs-Hinweise am Boden
    printf(COLOR_CYAN);
    printf("\x1b[21;1H├────────────────────────────────────────┤");
    printf(COLOR_WHITE);
    printf("\x1b[22;4H[ A ] Riesigen Cookie backen (+1)");
    printf("\x1b[23;4H[ X / Y ] Upgrade 1 (Cursor) / 2 (Oma)");
    printf("\x1b[24;4H[ SELECT ] Spielstand jetzt speichern");
    printf("\x1b[25;4H[ START ] Speichern & Beenden");
    printf("\x1b[26;1H└────────────────────────────────────────┘");
    printf(COLOR_RESET);
}

int main(int argc, char **argv) {
    // Grafik und Textkonsole initialisieren
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    initGame();

    // Hauptschleife
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        // SELECT-Taste: Manuell speichern
        if (kDown & KEY_SELECT) {
            saveGame();
        }

        // START-Taste: Speichern und Spiel beenden
        if (kDown & KEY_START) {
            saveGame();
            break;
        }

        // A-Taste: Manuell Cookies klicken
        if (kDown & KEY_A) {
            cookies += cookiesPerClick;
            totalCookiesEarned += cookiesPerClick;
        }

        // X-Taste: Upgrade 0 (Cursor) kaufen
        if (kDown & KEY_X) {
            if (cookies >= upgrades[0].cost) {
                cookies -= upgrades[0].cost;
                upgrades[0].count++;
                upgrades[0].cost *= 1.15; // Typischer Cookie-Clicker Preisanstieg (15%)
                recalculateCPS();
            }
        }

        // Y-Taste: Upgrade 1 (Oma) kaufen
        if (kDown & KEY_Y) {
            if (cookies >= upgrades[1].cost) {
                cookies -= upgrades[1].cost;
                upgrades[1].count++;
                upgrades[1].cost *= 1.15;
                recalculateCPS();
            }
        }

        // Idle-Einkommen berechnen
        updateGame();

        // UI rendern
        drawGame();

        // Frame synchronisieren
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}
