// =============================================================================
// 3DS FTP-Server mit Touch-GUI – Alles-in-einer-Datei Version
// =============================================================================
// Benötigt: devkitARM + libctru + citro2d + citro3d
// Build: siehe Makefile (make)
// =============================================================================

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <malloc.h>

// =============================================================================
// Settings – Laden/Speichern der Konfiguration auf der SD-Karte
// =============================================================================

struct Settings {
    uint16_t port = 5000;
    std::string username; // leer = kein Login nötig
    std::string password;

    static Settings load();
    void save() const;
};

static const char* kSettingsDir  = "sdmc:/3ds/ftpserver";
static const char* kSettingsFile = "sdmc:/3ds/ftpserver/settings.cfg";

Settings Settings::load() {
    Settings s;
    std::ifstream in(kSettingsFile);
    if (!in.is_open()) return s;

    std::string line;
    if (std::getline(in, line) && !line.empty()) {
        // std::stoi wirft bei Fehlern Exceptions, die im Build deaktiviert sind
        // (-fno-exceptions) -> stattdessen exception-frei mit strtol parsen.
        char* endPtr = nullptr;
        long value = strtol(line.c_str(), &endPtr, 10);
        if (endPtr != line.c_str() && value > 0 && value <= 65535) {
            s.port = static_cast<uint16_t>(value);
        }
    }
    if (std::getline(in, line)) s.username = line;
    if (std::getline(in, line)) s.password = line;
    return s;
}

void Settings::save() const {
    mkdir("sdmc:/3ds", 0777);
    mkdir(kSettingsDir, 0777);

    std::ofstream out(kSettingsFile, std::ios::trunc);
    if (!out.is_open()) return;

    out << port << "\n" << username << "\n" << password << "\n";
}

// =============================================================================
// FtpServer – FTP-Server-Logik, läuft in eigenem Thread (BSD-Sockets über soc)
// =============================================================================

struct TransferInfo {
    bool        active     = false;
    bool        isUpload   = false;
    std::string path;
    uint64_t    bytesDone  = 0;
    uint64_t    bytesTotal = 0;
    double      speedKiBs  = 0.0;
};

class FtpServer {
public:
    FtpServer() {}
    ~FtpServer() { stop(); }

    bool start(uint16_t port) {
        if (running_) return true;
        port_ = port;

        listenSock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSock_ < 0) return false;

        int yes = 1;
        setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (bind(listenSock_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(listenSock_); listenSock_ = -1; return false;
        }
        if (listen(listenSock_, 1) < 0) {
            close(listenSock_); listenSock_ = -1; return false;
        }

        running_ = true;
        thread_ = std::thread(&FtpServer::serverThreadFunc, this);
        return true;
    }

    void stop() {
        if (!running_) return;
        running_ = false;

        if (listenSock_ >= 0) { close(listenSock_); listenSock_ = -1; }
        if (thread_.joinable()) thread_.join();

        clearTransfer();
        clientConnected_ = false;
    }

    bool isRunning() const { return running_; }
    bool hasClient() const { return clientConnected_; }

    void setCredentials(const std::string& user, const std::string& pass) {
        std::lock_guard<std::mutex> lock(credMutex_);
        user_ = user;
        pass_ = pass;
    }

    TransferInfo getTransferInfo() {
        std::lock_guard<std::mutex> lock(transferMutex_);
        return transferInfo_;
    }

private:
    // -------------------------------------------------------------------
    bool checkLogin(const std::string& user, const std::string& pass) {
        std::lock_guard<std::mutex> lock(credMutex_);
        if (user_.empty() && pass_.empty()) return true;
        return user == user_ && pass == pass_;
    }

    static bool recvLine(int sock, std::string& out) {
        out.clear();
        char c;
        while (true) {
            int n = recv(sock, &c, 1, 0);
            if (n <= 0) return false;
            if (c == '\n') {
                if (!out.empty() && out.back() == '\r') out.pop_back();
                return true;
            }
            out.push_back(c);
            if (out.size() > 4096) return false;
        }
    }

    static void sendLine(int sock, const std::string& msg) {
        send(sock, msg.c_str(), msg.size(), 0);
    }

    static std::string formatUnixLikeEntry(const std::string& name, bool isDir, long size) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%crwxrwxrwx 1 3ds 3ds %10ld Jan 01 00:00 %s\r\n",
                 isDir ? 'd' : '-', size, name.c_str());
        return buf;
    }

    std::string normalizePath(const std::string& base, const std::string& path) const {
        std::string full;
        if (!path.empty() && path[0] == '/') full = path;
        else full = base + (base.empty() || base.back() == '/' ? "" : "/") + path;

        std::vector<std::string> parts;
        std::stringstream ss(full);
        std::string seg;
        while (std::getline(ss, seg, '/')) {
            if (seg.empty() || seg == ".") continue;
            if (seg == "..") { if (!parts.empty()) parts.pop_back(); }
            else parts.push_back(seg);
        }

        std::string result = "/";
        for (size_t i = 0; i < parts.size(); ++i) {
            result += parts[i];
            if (i + 1 < parts.size()) result += "/";
        }
        return result;
    }

    std::string toRealPath(const std::string& ftpPath) const {
        if (ftpPath == "/") return "sdmc:";
        return "sdmc:" + ftpPath;
    }

    int openPasvListener(std::string& replyOut) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0) return -1;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = 0;

        if (bind(s, (sockaddr*)&addr, sizeof(addr)) < 0) { close(s); return -1; }
        if (listen(s, 1) < 0) { close(s); return -1; }

        socklen_t len = sizeof(addr);
        getsockname(s, (sockaddr*)&addr, &len);
        uint16_t p = ntohs(addr.sin_port);

        uint32_t ip = gethostid();
        uint8_t* b = reinterpret_cast<uint8_t*>(&ip);

        char buf[128];
        snprintf(buf, sizeof(buf), "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
                 b[0], b[1], b[2], b[3], (p >> 8) & 0xFF, p & 0xFF);
        replyOut = buf;
        return s;
    }

    int acceptDataConnection(int pasvListenSock, int timeoutSeconds = 15) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(pasvListenSock, &fds);
        timeval tv{ timeoutSeconds, 0 };

        int r = select(pasvListenSock + 1, &fds, nullptr, nullptr, &tv);
        if (r <= 0) { close(pasvListenSock); return -1; }

        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        int dataSock = accept(pasvListenSock, (sockaddr*)&addr, &len);
        close(pasvListenSock);
        return dataSock;
    }

    void setTransfer(bool active, bool upload, const std::string& path, uint64_t total) {
        std::lock_guard<std::mutex> lock(transferMutex_);
        transferInfo_.active = active;
        transferInfo_.isUpload = upload;
        transferInfo_.path = path;
        transferInfo_.bytesDone = 0;
        transferInfo_.bytesTotal = total;
        transferInfo_.speedKiBs = 0.0;
    }

    void updateTransferProgress(uint64_t done) {
        static u64 lastTick = 0;
        u64 now = osGetTime();

        std::lock_guard<std::mutex> lock(transferMutex_);
        if (lastTick == 0) lastTick = now;
        uint64_t deltaBytes = done - transferInfo_.bytesDone;
        u64 deltaMs = now - lastTick;
        if (deltaMs >= 200) {
            double seconds = deltaMs / 1000.0;
            if (seconds > 0) transferInfo_.speedKiBs = (deltaBytes / 1024.0) / seconds;
            lastTick = now;
        }
        transferInfo_.bytesDone = done;
    }

    void clearTransfer() {
        std::lock_guard<std::mutex> lock(transferMutex_);
        transferInfo_ = TransferInfo{};
    }

    void serverThreadFunc() {
        while (running_) {
            sockaddr_in clientAddr{};
            socklen_t len = sizeof(clientAddr);
            int clientSock = accept(listenSock_, (sockaddr*)&clientAddr, &len);
            if (clientSock < 0) break;

            clientConnected_ = true;
            handleClient(clientSock);
            clientConnected_ = false;
            close(clientSock);
            clearTransfer();
        }
    }

    void handleClient(int clientSock) {
        std::string cwd = "/";
        bool loggedIn = false;
        std::string pendingUser;
        std::string renameFromPath;
        int pasvListenSock = -1;

        sendLine(clientSock, "220 3DS FTP Server bereit\r\n");

        std::string line;
        while (running_ && recvLine(clientSock, line)) {
            if (line.empty()) continue;

            std::string verb, arg;
            size_t sp = line.find(' ');
            if (sp == std::string::npos) verb = line;
            else { verb = line.substr(0, sp); arg = line.substr(sp + 1); }
            for (auto& c : verb) c = toupper(c);

            if (verb == "USER") {
                pendingUser = arg;
                sendLine(clientSock, "331 Passwort erforderlich\r\n");
            } else if (verb == "PASS") {
                if (checkLogin(pendingUser, arg)) {
                    loggedIn = true;
                    sendLine(clientSock, "230 Login erfolgreich\r\n");
                } else {
                    sendLine(clientSock, "530 Login inkorrekt\r\n");
                }
            } else if (verb == "SYST") {
                sendLine(clientSock, "215 UNIX Type: L8\r\n");
            } else if (verb == "FEAT") {
                sendLine(clientSock, "211-Features:\r\n SIZE\r\n MDTM\r\n UTF8\r\n211 End\r\n");
            } else if (verb == "OPTS") {
                sendLine(clientSock, "200 OK\r\n");
            } else if (verb == "TYPE") {
                sendLine(clientSock, "200 Typ gesetzt\r\n");
            } else if (verb == "PWD" || verb == "XPWD") {
                sendLine(clientSock, "257 \"" + cwd + "\" ist das aktuelle Verzeichnis\r\n");
            } else if (!loggedIn) {
                sendLine(clientSock, "530 Bitte zuerst einloggen\r\n");
            } else if (verb == "CWD" || verb == "XCWD") {
                std::string target = normalizePath(cwd, arg);
                std::string real = toRealPath(target);
                struct stat st;
                if (stat(real.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    cwd = target;
                    sendLine(clientSock, "250 Verzeichnis gewechselt\r\n");
                } else {
                    sendLine(clientSock, "550 Verzeichnis nicht gefunden\r\n");
                }
            } else if (verb == "CDUP") {
                cwd = normalizePath(cwd, "..");
                sendLine(clientSock, "250 OK\r\n");
            } else if (verb == "PASV") {
                std::string reply;
                pasvListenSock = openPasvListener(reply);
                if (pasvListenSock < 0) sendLine(clientSock, "425 Kann Passivmodus nicht öffnen\r\n");
                else sendLine(clientSock, reply);
            } else if (verb == "PORT") {
                sendLine(clientSock, "502 Aktiver Modus wird nicht unterstützt, bitte PASV verwenden\r\n");
            } else if (verb == "LIST" || verb == "NLST") {
                if (pasvListenSock < 0) { sendLine(clientSock, "425 Erst PASV verwenden\r\n"); continue; }
                sendLine(clientSock, "150 Öffne Datenverbindung für Verzeichnisliste\r\n");
                int dataSock = acceptDataConnection(pasvListenSock);
                pasvListenSock = -1;
                if (dataSock < 0) { sendLine(clientSock, "425 Datenverbindung fehlgeschlagen\r\n"); continue; }

                std::string real = toRealPath(cwd);
                DIR* d = opendir(real.c_str());
                if (d) {
                    struct dirent* ent;
                    while ((ent = readdir(d)) != nullptr) {
                        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
                        std::string full = real + "/" + ent->d_name;
                        struct stat st;
                        long size = 0;
                        bool isDir = false;
                        if (stat(full.c_str(), &st) == 0) { size = (long)st.st_size; isDir = S_ISDIR(st.st_mode); }
                        if (verb == "NLST") {
                            std::string n = std::string(ent->d_name) + "\r\n";
                            send(dataSock, n.c_str(), n.size(), 0);
                        } else {
                            std::string entry = formatUnixLikeEntry(ent->d_name, isDir, size);
                            send(dataSock, entry.c_str(), entry.size(), 0);
                        }
                    }
                    closedir(d);
                }
                close(dataSock);
                sendLine(clientSock, "226 Übertragung abgeschlossen\r\n");
            } else if (verb == "SIZE") {
                std::string real = toRealPath(normalizePath(cwd, arg));
                struct stat st;
                if (stat(real.c_str(), &st) == 0) sendLine(clientSock, "213 " + std::to_string(st.st_size) + "\r\n");
                else sendLine(clientSock, "550 Datei nicht gefunden\r\n");
            } else if (verb == "MDTM") {
                std::string real = toRealPath(normalizePath(cwd, arg));
                struct stat st;
                if (stat(real.c_str(), &st) == 0) {
                    char buf[32];
                    strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", gmtime(&st.st_mtime));
                    sendLine(clientSock, std::string("213 ") + buf + "\r\n");
                } else {
                    sendLine(clientSock, "550 Datei nicht gefunden\r\n");
                }
            } else if (verb == "MKD" || verb == "XMKD") {
                std::string real = toRealPath(normalizePath(cwd, arg));
                if (mkdir(real.c_str(), 0777) == 0) sendLine(clientSock, "257 Verzeichnis erstellt\r\n");
                else sendLine(clientSock, "550 Konnte Verzeichnis nicht erstellen\r\n");
            } else if (verb == "RMD" || verb == "XRMD") {
                std::string real = toRealPath(normalizePath(cwd, arg));
                if (rmdir(real.c_str()) == 0) sendLine(clientSock, "250 Verzeichnis gelöscht\r\n");
                else sendLine(clientSock, "550 Konnte Verzeichnis nicht löschen\r\n");
            } else if (verb == "DELE") {
                std::string real = toRealPath(normalizePath(cwd, arg));
                if (remove(real.c_str()) == 0) sendLine(clientSock, "250 Datei gelöscht\r\n");
                else sendLine(clientSock, "550 Konnte Datei nicht löschen\r\n");
            } else if (verb == "RNFR") {
                renameFromPath = toRealPath(normalizePath(cwd, arg));
                sendLine(clientSock, "350 Bereit für RNTO\r\n");
            } else if (verb == "RNTO") {
                std::string to = toRealPath(normalizePath(cwd, arg));
                if (!renameFromPath.empty() && rename(renameFromPath.c_str(), to.c_str()) == 0)
                    sendLine(clientSock, "250 Umbenannt\r\n");
                else
                    sendLine(clientSock, "550 Umbenennen fehlgeschlagen\r\n");
                renameFromPath.clear();
            } else if (verb == "RETR") {
                if (pasvListenSock < 0) { sendLine(clientSock, "425 Erst PASV verwenden\r\n"); continue; }
                std::string ftpPath = normalizePath(cwd, arg);
                std::string real = toRealPath(ftpPath);
                FILE* f = fopen(real.c_str(), "rb");
                if (!f) { close(pasvListenSock); pasvListenSock = -1; sendLine(clientSock, "550 Datei nicht gefunden\r\n"); continue; }

                fseek(f, 0, SEEK_END);
                long totalSize = ftell(f);
                fseek(f, 0, SEEK_SET);

                sendLine(clientSock, "150 Sende Datei\r\n");
                int dataSock = acceptDataConnection(pasvListenSock);
                pasvListenSock = -1;
                if (dataSock < 0) { fclose(f); sendLine(clientSock, "425 Datenverbindung fehlgeschlagen\r\n"); continue; }

                setTransfer(true, false, ftpPath, (uint64_t)totalSize);
                char buf[16384];
                uint64_t sent = 0;
                size_t n;
                while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
                    if (send(dataSock, buf, n, 0) < 0) break;
                    sent += n;
                    updateTransferProgress(sent);
                }
                fclose(f);
                close(dataSock);
                clearTransfer();
                sendLine(clientSock, "226 Übertragung abgeschlossen\r\n");
            } else if (verb == "STOR") {
                if (pasvListenSock < 0) { sendLine(clientSock, "425 Erst PASV verwenden\r\n"); continue; }
                std::string ftpPath = normalizePath(cwd, arg);
                std::string real = toRealPath(ftpPath);
                FILE* f = fopen(real.c_str(), "wb");
                if (!f) { close(pasvListenSock); pasvListenSock = -1; sendLine(clientSock, "550 Konnte Datei nicht anlegen\r\n"); continue; }

                sendLine(clientSock, "150 Bereit zum Empfang\r\n");
                int dataSock = acceptDataConnection(pasvListenSock);
                pasvListenSock = -1;
                if (dataSock < 0) { fclose(f); sendLine(clientSock, "425 Datenverbindung fehlgeschlagen\r\n"); continue; }

                setTransfer(true, true, ftpPath, 0);
                char buf[16384];
                uint64_t received = 0;
                int n;
                while ((n = recv(dataSock, buf, sizeof(buf), 0)) > 0) {
                    fwrite(buf, 1, n, f);
                    received += n;
                    updateTransferProgress(received);
                }
                fclose(f);
                close(dataSock);
                clearTransfer();
                sendLine(clientSock, "226 Übertragung abgeschlossen\r\n");
            } else if (verb == "NOOP") {
                sendLine(clientSock, "200 NOOP\r\n");
            } else if (verb == "QUIT") {
                sendLine(clientSock, "221 Auf Wiedersehen\r\n");
                break;
            } else {
                sendLine(clientSock, "502 Befehl nicht implementiert\r\n");
            }
        }

        if (pasvListenSock >= 0) close(pasvListenSock);
    }

    std::atomic<bool> running_{false};
    std::atomic<bool> clientConnected_{false};
    std::thread thread_;
    int listenSock_ = -1;
    uint16_t port_ = 5000;

    std::mutex credMutex_;
    std::string user_, pass_;

    std::mutex transferMutex_;
    TransferInfo transferInfo_;
};

// =============================================================================
// UI – Touch-Oberfläche mit citro2d (Popup + Hauptbildschirm)
// =============================================================================

struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

static const u32 kColorBg         = C2D_Color32(0x20, 0x20, 0x28, 0xFF);
static const u32 kColorPanel      = C2D_Color32(0x30, 0x30, 0x3A, 0xFF);
static const u32 kColorField      = C2D_Color32(0x10, 0x10, 0x15, 0xFF);
static const u32 kColorButton     = C2D_Color32(0x40, 0x80, 0xE0, 0xFF);
static const u32 kColorButtonStop = C2D_Color32(0xE0, 0x50, 0x40, 0xFF);
static const u32 kColorButtonAlt  = C2D_Color32(0x50, 0x50, 0x60, 0xFF);
static const u32 kColorText       = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
static const u32 kColorTextDim    = C2D_Color32(0xC0, 0xC0, 0xC0, 0xFF);
static const u32 kColorBar        = C2D_Color32(0x40, 0xC0, 0x60, 0xFF);

static const Rect kUserField   { 10,  55, 300, 26 };
static const Rect kPassField   { 10,  91, 300, 26 };
static const Rect kApplyBtn    { 10, 150,  95, 34 };
static const Rect kCloseBtn    {113, 150,  95, 34 };
static const Rect kResetBtn    {216, 150,  95, 34 };

static const Rect kStartStopBtn{ 60,  70, 200, 55 };
static const Rect kSettingsBtn { 60, 145, 200, 40 };

static std::string getLocalIp() {
    u32 ip = gethostid();
    struct in_addr addr;
    addr.s_addr = ip;
    char buf[16];
    strncpy(buf, inet_ntoa(addr), sizeof(buf));
    return std::string(buf);
}

static void drawText(C2D_TextBuf buf, C2D_Font font, const std::string& str,
                      float x, float y, float scale, u32 color) {
    C2D_Text text;
    C2D_TextFontParse(&text, font, buf, str.c_str());
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.0f, scale, scale, color);
}

class App {
public:
    App() {
        gfxInitDefault();
        C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
        C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
        C2D_Prepare();
        cfguInit();

        topTarget_ = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
        botTarget_ = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

        textBuf_ = C2D_TextBufNew(4096);
        font_ = C2D_FontLoadSystem(CFG_REGION_USA);

        hidInit();

        settings_ = Settings::load();
        usernameBuf_ = settings_.username;
        passwordBuf_ = settings_.password;
        ipStr_ = getLocalIp();

        ftp_.setCredentials(settings_.username, settings_.password);
    }

    ~App() {
        ftp_.stop();
        if (font_) C2D_FontFree(font_);
        if (textBuf_) C2D_TextBufDelete(textBuf_);
        C2D_Fini();
        C3D_Fini();
        cfguExit();
        gfxExit();
    }

    void run() {
        while (aptMainLoop() && running_) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) running_ = false;

            update();
            render();
        }
    }

private:
    void update() {
        touchPosition touch;
        hidTouchRead(&touch);
        if (hidKeysDown() & KEY_TOUCH) {
            if (showPopup_) handleTouchPopup(touch);
            else handleTouchMain(touch);
        }
    }

    void openKeyboard(std::string& target, bool isPassword, const char* hint) {
        SwkbdState swkbd;
        char buf[65];
        strncpy(buf, target.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, 64);
        swkbdSetHintText(&swkbd, hint);
        swkbdSetInitialText(&swkbd, buf);
        if (isPassword) swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE_DELAY);

        SwkbdButton button = swkbdInputText(&swkbd, buf, sizeof(buf));
        if (button == SWKBD_BUTTON_RIGHT || button == SWKBD_BUTTON_CONFIRM) {
            target = buf;
        }
    }

    void applySettings() {
        settings_.username = usernameBuf_;
        settings_.password = passwordBuf_;
        settings_.save();
        ftp_.setCredentials(settings_.username, settings_.password);
    }

    void resetFields() {
        usernameBuf_.clear();
        passwordBuf_.clear();
    }

    void closePopup() {
        applySettings();
        showPopup_ = false;
    }

    void toggleServer() {
        if (ftp_.isRunning()) ftp_.stop();
        else ftp_.start(settings_.port);
    }

    void handleTouchPopup(const touchPosition& t) {
        float x = t.px, y = t.py;
        if (kUserField.contains(x, y)) openKeyboard(usernameBuf_, false, "Benutzername (optional)");
        else if (kPassField.contains(x, y)) openKeyboard(passwordBuf_, true, "Passwort (optional)");
        else if (kApplyBtn.contains(x, y)) applySettings();
        else if (kCloseBtn.contains(x, y)) closePopup();
        else if (kResetBtn.contains(x, y)) resetFields();
    }

    void handleTouchMain(const touchPosition& t) {
        float x = t.px, y = t.py;
        if (kStartStopBtn.contains(x, y)) toggleServer();
        else if (kSettingsBtn.contains(x, y)) showPopup_ = true;
    }

    void render() {
        C2D_TextBufClear(textBuf_);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(topTarget_, kColorBg);
        C2D_SceneBegin(topTarget_);
        drawTop();

        C2D_TargetClear(botTarget_, kColorBg);
        C2D_SceneBegin(botTarget_);
        if (showPopup_) drawBottomPopup();
        else drawBottomMain();

        C3D_FrameEnd(0);
    }

    void drawTop() {
        char buf[128];
        drawText(textBuf_, font_, "3DS FTP Server", 10, 10, 0.7f, kColorText);

        snprintf(buf, sizeof(buf), "IP-Adresse:  %s", ipStr_.c_str());
        drawText(textBuf_, font_, buf, 10, 40, 0.55f, kColorTextDim);

        snprintf(buf, sizeof(buf), "Port:        %d", settings_.port);
        drawText(textBuf_, font_, buf, 10, 60, 0.55f, kColorTextDim);

        snprintf(buf, sizeof(buf), "Benutzer:    %s",
                 settings_.username.empty() ? "(kein Login)" : settings_.username.c_str());
        drawText(textBuf_, font_, buf, 10, 80, 0.55f, kColorTextDim);

        snprintf(buf, sizeof(buf), "Passwort:    %s",
                 settings_.password.empty() ? "(kein Login)"
                                             : std::string(settings_.password.size(), '*').c_str());
        drawText(textBuf_, font_, buf, 10, 100, 0.55f, kColorTextDim);

        drawText(textBuf_, font_,
                 ftp_.isRunning() ? "Status: Server läuft" : "Status: Server gestoppt",
                 10, 130, 0.55f, ftp_.isRunning() ? kColorBar : kColorTextDim);

        if (ftp_.isRunning() && ftp_.hasClient()) {
            drawText(textBuf_, font_, "Client verbunden", 10, 150, 0.5f, kColorTextDim);
        }

        TransferInfo ti = ftp_.getTransferInfo();
        if (ti.active) {
            snprintf(buf, sizeof(buf), "%s: %s", ti.isUpload ? "Upload" : "Download", ti.path.c_str());
            drawText(textBuf_, font_, buf, 10, 175, 0.5f, kColorText);
        }
    }

    void drawBottomPopup() {
        C2D_DrawRectSolid(0, 0, 0, 320, 240, kColorBg);
        C2D_DrawRectSolid(5, 5, 0, 310, 230, kColorPanel);

        drawText(textBuf_, font_, "FTP-Server Einstellungen", 12, 10, 0.55f, kColorText);

        char buf[64];
        snprintf(buf, sizeof(buf), "%s : %d", ipStr_.c_str(), settings_.port);
        drawText(textBuf_, font_, buf, 12, 30, 0.5f, kColorTextDim);

        drawText(textBuf_, font_, "Benutzername (optional):", 12, 44, 0.42f, kColorTextDim);
        C2D_DrawRectSolid(kUserField.x, kUserField.y, 0, kUserField.w, kUserField.h, kColorField);
        drawText(textBuf_, font_, usernameBuf_.empty() ? "-" : usernameBuf_,
                 kUserField.x + 6, kUserField.y + 5, 0.5f, kColorText);

        drawText(textBuf_, font_, "Passwort (optional):", 12, 80, 0.42f, kColorTextDim);
        C2D_DrawRectSolid(kPassField.x, kPassField.y, 0, kPassField.w, kPassField.h, kColorField);
        std::string masked = passwordBuf_.empty() ? "-" : std::string(passwordBuf_.size(), '*');
        drawText(textBuf_, font_, masked, kPassField.x + 6, kPassField.y + 5, 0.5f, kColorText);

        C2D_DrawRectSolid(kApplyBtn.x, kApplyBtn.y, 0, kApplyBtn.w, kApplyBtn.h, kColorButton);
        drawText(textBuf_, font_, "Apply", kApplyBtn.x + 28, kApplyBtn.y + 9, 0.5f, kColorText);

        C2D_DrawRectSolid(kCloseBtn.x, kCloseBtn.y, 0, kCloseBtn.w, kCloseBtn.h, kColorButtonAlt);
        drawText(textBuf_, font_, "Close", kCloseBtn.x + 28, kCloseBtn.y + 9, 0.5f, kColorText);

        C2D_DrawRectSolid(kResetBtn.x, kResetBtn.y, 0, kResetBtn.w, kResetBtn.h, kColorButtonAlt);
        drawText(textBuf_, font_, "Reset", kResetBtn.x + 28, kResetBtn.y + 9, 0.5f, kColorText);

        drawText(textBuf_, font_, "Tippe auf ein Feld, um die Tastatur zu öffnen.",
                 12, 195, 0.38f, kColorTextDim);
    }

    void drawBottomMain() {
        bool running = ftp_.isRunning();

        C2D_DrawRectSolid(kStartStopBtn.x, kStartStopBtn.y, 0, kStartStopBtn.w, kStartStopBtn.h,
                           running ? kColorButtonStop : kColorButton);
        drawText(textBuf_, font_, running ? "Stop" : "Start",
                 kStartStopBtn.x + (running ? 85 : 82), kStartStopBtn.y + 18, 0.7f, kColorText);

        C2D_DrawRectSolid(kSettingsBtn.x, kSettingsBtn.y, 0, kSettingsBtn.w, kSettingsBtn.h, kColorButtonAlt);
        drawText(textBuf_, font_, "Settings", kSettingsBtn.x + 65, kSettingsBtn.y + 11, 0.55f, kColorText);

        TransferInfo ti = ftp_.getTransferInfo();
        if (ti.active) {
            char buf[96];
            double doneMiB = ti.bytesDone / 1024.0 / 1024.0;
            double totalMiB = ti.bytesTotal / 1024.0 / 1024.0;

            drawText(textBuf_, font_, ti.path, 10, 195, 0.42f, kColorText);

            if (ti.bytesTotal > 0)
                snprintf(buf, sizeof(buf), "%.1fMiB/%.1fMiB   %.0fKiB/s", doneMiB, totalMiB, ti.speedKiBs);
            else
                snprintf(buf, sizeof(buf), "%.1fMiB   %.0fKiB/s", doneMiB, ti.speedKiBs);
            drawText(textBuf_, font_, buf, 10, 212, 0.42f, kColorTextDim);

            if (ti.bytesTotal > 0) {
                float pct = (float)((double)ti.bytesDone / (double)ti.bytesTotal);
                if (pct > 1.0f) pct = 1.0f;
                C2D_DrawRectSolid(10, 228, 0, 300, 6, kColorField);
                C2D_DrawRectSolid(10, 228, 0, 300 * pct, 6, kColorBar);
            }
        } else if (running) {
            drawText(textBuf_, font_, "Warte auf Verbindung...", 10, 200, 0.45f, kColorTextDim);
        }
    }

    bool showPopup_ = true;
    std::string usernameBuf_;
    std::string passwordBuf_;
    std::string ipStr_;

    Settings settings_;
    FtpServer ftp_;

    C3D_RenderTarget* topTarget_ = nullptr;
    C3D_RenderTarget* botTarget_ = nullptr;
    C2D_TextBuf textBuf_ = nullptr;
    C2D_Font font_ = nullptr;

    bool running_ = true;
};

// =============================================================================
// main
// =============================================================================

#define SOC_ALIGN      0x1000
#define SOC_BUFFERSIZE 0x100000

int main(int argc, char** argv) {
    u32* socBuffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    socInit(socBuffer, SOC_BUFFERSIZE);

    {
        App app;
        app.run();
    }

    socExit();
    free(socBuffer);
    return 0;
}
