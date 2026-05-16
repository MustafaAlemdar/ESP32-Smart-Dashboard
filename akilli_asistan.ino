#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <time.h>
#include <ctype.h>
#include <Preferences.h>

// DONANIM AYARLARI
#define USE_BUZZER true 
#define BUZZER_PIN 15   

#define T_CS_PIN  32
#define T_IRQ_PIN 33
#define T_CLK_PIN  25
#define T_MISO_PIN 27
#define T_MOSI_PIN 26
#define TFT_BROWN 0x8200 

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(T_CS_PIN, T_IRQ_PIN);
Preferences preferences; 

enum SystemState { HOME, APP_SUBS, APP_TIME, APP_EYE, APP_SNAKE, APP_WIFI, APP_CHESS, APP_BOMB, APP_BIRD, APP_BOOM_WAIT };
SystemState currentState = HOME;
bool screenNeedsRedraw = true;

// GLOBAL SKOR DEĞİŞKENLERİ
int birdHighScore = 0;
int snakeHighScore = 0;

// API BİLGİLERİ
const char* apiKey = "BURAYA_KENDI_API_ANAHTARINIZI_YAZIN";
const char* channelId = "BURAYA_KANAL_ID_YAZIN";
unsigned long lastEyeMove = 0, lastMove = 0, lastApiUpdate = 0;
long subCount = 0;

// BOMBA DEĞİŞKENLERİ
int bombCounter = 10;
unsigned long lastBombTick = 0;
unsigned long boomStartTime = 0;
int tickInterval = 1000; 
bool bombActive = false;

// YILAN OYUNU DEĞİŞKENLERİ
int snakeX[100], snakeY[100], snakeLen = 3, foodX, foodY, dir = 1;
int snakeState = 0; 
bool snakeWasTouched = false;

// SATRANÇ DEĞİŞKENLERİ
char chessBoard[8][8];
int chessTurn = 0, selX = -1, selY = -1, chessWinner = -1;

// KUŞ OYUNU DEĞİŞKENLERİ
float birdY = 120, birdV = 0;
int pipeX = 320, gapY = 80, birdScore = 0, birdState = 0;
unsigned long lastBirdFrame = 0;
bool birdWasTouched = false;

/* ==============================================================================
   YARDIMCI FONKSİYONLAR
============================================================================== */

void playBeep(int duration) {
  if (USE_BUZZER) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void drawCloseButton() {
  tft.fillRect(285, 5, 30, 30, TFT_RED);
  tft.drawRect(285, 5, 30, 30, TFT_WHITE);
  tft.drawLine(290, 10, 310, 30, TFT_WHITE);
  tft.drawLine(310, 10, 290, 30, TFT_WHITE);
}

bool isCloseButtonPressed(uint16_t x, uint16_t y) {
  return (x > 280 && y < 40);
}

/* ==============================================================================
   AÇILIŞ ANİMASYONU
============================================================================== */

void showBootLogo() {
  tft.fillScreen(TFT_BLACK);
  const uint8_t muzMap[16][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,2,2,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,2,2,1,2,0,0,0,0},
    {0,0,0,0,0,0,0,0,2,1,1,2,0,0,0,0},
    {0,0,0,0,0,0,0,2,1,1,1,2,0,0,0,0},
    {0,0,0,0,0,0,2,1,1,1,2,0,0,0,0,0},
    {0,0,0,0,0,2,1,1,1,1,2,0,0,0,0,0},
    {0,0,0,0,2,1,1,1,1,2,0,0,0,0,0,0},
    {0,0,0,2,1,1,1,1,2,0,0,0,0,0,0,0},
    {0,0,2,1,1,1,1,2,0,0,0,0,0,0,0,0},
    {0,2,1,1,1,2,2,0,0,0,0,0,0,0,0,0},
    {2,1,1,2,2,0,0,0,0,0,0,0,0,0,0,0},
    {2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
  };
  int pSize = 7; int sX = 110; int sY = 50;
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      if (muzMap[i][j] == 1) tft.fillRect(sX + (j*pSize), sY + (i*pSize), pSize, pSize, TFT_YELLOW);
      else if (muzMap[i][j] == 2) tft.fillRect(sX + (j*pSize), sY + (i*pSize), pSize, pSize, TFT_BROWN);
    }
  }
  tft.setTextSize(1); tft.setTextColor(TFT_DARKGREY);
  tft.setCursor(80, 200); tft.print("Sistem Baslatiliyor...");
}

void showWelcomeScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(3); tft.setTextColor(TFT_WHITE);
  tft.setCursor(45, 110); tft.print("Hos Geldiniz");
  delay(400); // Hizlandirilmis bekleme suresi
}

/* ==============================================================================
   UÇAN KUŞ OYUNU
============================================================================== */

void initBird() {
  birdY = 120; birdV = 0;
  pipeX = 320; gapY = random(40, 130);
  birdScore = 0; birdState = 0;
  tft.fillScreen(TFT_CYAN); 
  drawCloseButton();
  tft.fillRect(0, 220, 320, 20, TFT_BROWN); 
  tft.setTextSize(2); tft.setTextColor(TFT_BLACK);
  tft.setCursor(50, 100); tft.print("Ekrana Dokun!");
  
  tft.setTextSize(2); tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10, 10); tft.printf("REKOR: %d", birdHighScore);
}

void showBird(bool pressed, uint16_t tx, uint16_t ty) {
  if (screenNeedsRedraw) { initBird(); screenNeedsRedraw = false; birdWasTouched = false; }

  bool isTapped = pressed && !birdWasTouched;
  birdWasTouched = pressed;

  if (birdState == 0) {
    if (isTapped && ty > 40) { 
      birdState = 1; 
      tft.fillScreen(TFT_CYAN); 
      tft.fillRect(0, 220, 320, 20, TFT_BROWN); 
      drawCloseButton(); 
      birdV = -5.5; 
      playBeep(15);
    }
  }
  else if (birdState == 1) { 
    if (isTapped && ty > 40) { 
        birdV = -5.5; 
        playBeep(15); 
    }

    if (millis() - lastBirdFrame > 30) { 
      lastBirdFrame = millis();
      
      tft.fillCircle(60, (int)birdY, 6, TFT_CYAN);
      tft.fillRect(pipeX + 40, 0, 5, gapY, TFT_CYAN);
      tft.fillRect(pipeX + 40, gapY + 70, 5, 220 - (gapY + 70), TFT_CYAN);

      birdV += 0.5; if(birdV > 8) birdV = 8; birdY += birdV;
      pipeX -= 5;   

      if (pipeX < -40) {
        pipeX = 320; gapY = random(30, 130); 
        birdScore++; playBeep(40); 
        tft.fillRect(0, 0, 10, 220, TFT_CYAN); 
      }

      tft.fillRect(pipeX, 0, 40, gapY, TFT_GREEN); 
      tft.fillRect(pipeX, gapY + 70, 40, 220 - (gapY + 70), TFT_GREEN); 
      
      drawCloseButton();
      tft.setTextSize(3); tft.setTextColor(TFT_WHITE, TFT_CYAN);
      tft.setCursor(10, 10); tft.print(birdScore);

      tft.fillCircle(60, (int)birdY, 6, TFT_YELLOW);
      tft.fillCircle(62, (int)birdY - 2, 1, TFT_BLACK);

      bool hitGround = birdY > 214 || birdY < 6;
      bool inPipe = (60 + 6 > pipeX) && (60 - 6 < pipeX + 40);
      bool hitPipe = inPipe && (birdY - 5 < gapY || birdY + 5 > gapY + 70);

      if (hitGround || hitPipe) {
        birdState = 2; 
        playBeep(300); 
        
        if (birdScore > birdHighScore) {
          birdHighScore = birdScore;
          preferences.putInt("bird_hi", birdHighScore); 
        }

        tft.fillRect(50, 80, 220, 80, TFT_BLACK);
        tft.drawRect(50, 80, 220, 80, TFT_WHITE);
        tft.setTextSize(2); tft.setTextColor(TFT_RED);
        tft.setCursor(110, 95); tft.print("YANDIN!");
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(70, 130); tft.printf("SKOR: %d", birdScore);
      }
    }
  }
  else if (birdState == 2) { 
    if (isTapped && ty > 40) { initBird(); } 
  }
}

/* ==============================================================================
   SATRANÇ UYGULAMASI
============================================================================== */

bool isPathClear(int sr, int sc, int er, int ec) {
  int rStep = (er > sr) ? 1 : ((er < sr) ? -1 : 0);
  int cStep = (ec > sc) ? 1 : ((ec < sc) ? -1 : 0);
  int currR = sr + rStep; int currC = sc + cStep;
  while (currR != er || currC != ec) {
    if (chessBoard[currR][currC] != ' ') return false;
    currR += rStep; currC += cStep;
  }
  return true;
}

bool isValidChessMove(int sr, int sc, int er, int ec) {
  char p = chessBoard[sr][sc]; char t = chessBoard[er][ec];
  if (t != ' ') { if ((isupper(p) && isupper(t)) || (islower(p) && islower(t))) return false; }
  int dr = er - sr; int dc = ec - sc;
  int absDr = abs(dr); int absDc = abs(dc);
  char lowerP = tolower(p);
  
  if (lowerP == 'p') {
    int dir = isupper(p) ? -1 : 1; 
    if (dc == 0) {
      if (dr == dir && t == ' ') return true; 
      if (dr == 2 * dir && t == ' ' && isPathClear(sr, sc, er, ec)) {
        if ((isupper(p) && sr == 6) || (islower(p) && sr == 1)) return true;
      }
    } else if (absDc == 1 && dr == dir && t != ' ') return true;
    return false;
  }
  if (lowerP == 'r') { if (sr != er && sc != ec) return false; return isPathClear(sr, sc, er, ec); }
  if (lowerP == 'n') { return ((absDr == 2 && absDc == 1) || (absDr == 1 && absDc == 2)); }
  if (lowerP == 'b') { if (absDr != absDc) return false; return isPathClear(sr, sc, er, ec); }
  if (lowerP == 'q') { if (sr != er && sc != ec && absDr != absDc) return false; return isPathClear(sr, sc, er, ec); }
  if (lowerP == 'k') { return (absDr <= 1 && absDc <= 1); }
  return false;
}

void initChess() {
  const char* setup[8] = { "rnbqkbnr", "pppppppp", "        ", "        ", "        ", "        ", "PPPPPPPP", "RNBQKBNR" };
  for(int i=0; i<8; i++) { for(int j=0; j<8; j++) chessBoard[i][j] = setup[i][j]; }
  selX = -1; selY = -1; chessTurn = 0; chessWinner = -1;
  tft.fillScreen(TFT_BLACK); drawCloseButton();
}

void showChess(bool pressed, uint16_t tx, uint16_t ty) {
  if (screenNeedsRedraw) { initChess(); screenNeedsRedraw = false; }

  static bool chessWasTouched = false;
  bool isChessTapped = pressed && !chessWasTouched;
  chessWasTouched = pressed;

  if (isChessTapped && chessWinner == -1) {
    int c = (tx - 10) / 30; int r = ty / 30;
    if (c >= 0 && c < 8 && r >= 0 && r < 8) {
      if (selX == -1) { 
        if (chessBoard[r][c] != ' ') {
          bool isWhite = (chessBoard[r][c] >= 'A' && chessBoard[r][c] <= 'Z');
          if ((chessTurn == 0 && isWhite) || (chessTurn == 1 && !isWhite)) {
            selX = c; selY = r; playBeep(20); 
          }
        }
      } else { 
        if (selX == c && selY == r) { selX = -1; selY = -1; } 
        else if (isValidChessMove(selY, selX, r, c)) { 
          char target = chessBoard[r][c];
          if (target == 'k') chessWinner = 0; if (target == 'K') chessWinner = 1; 
          chessBoard[r][c] = chessBoard[selY][selX]; chessBoard[selY][selX] = ' ';
          selX = -1; selY = -1; chessTurn = 1 - chessTurn; playBeep(40); 
        } else { selX = -1; selY = -1; playBeep(150); }
      }
    }
  }

  for(int r=0; r<8; r++) {
    for(int c=0; c<8; c++) {
      uint16_t color = ((r+c)%2 == 0) ? TFT_LIGHTGREY : TFT_DARKGREY;
      if (r == selY && c == selX) color = TFT_YELLOW; 
      int sqX = 10 + c*30; int sqY = r*30;
      tft.fillRect(sqX, sqY, 30, 30, color);
      char p = chessBoard[r][c];
      if (p != ' ') {
        if (p >= 'a' && p <= 'z') tft.setTextColor(TFT_RED); else tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2); tft.setCursor(sqX + 8, sqY + 8); tft.print((char)toupper(p)); 
      }
    }
  }
  if (chessWinner != -1) {
    tft.fillRect(30, 100, 220, 40, TFT_BLACK); tft.drawRect(30, 100, 220, 40, TFT_WHITE);
    tft.setTextSize(2); tft.setTextColor((chessWinner == 0) ? TFT_WHITE : TFT_RED);
    tft.setCursor(45, 112); tft.print(chessWinner == 0 ? "BEYAZ KAZANDI!" : "KIRMIZI KAZANDI!");
  }
}

/* ==============================================================================
   YILAN OYUNU 
============================================================================== */

void initSnake() {
  snakeLen = 3; dir = 1; snakeState = 0;
  for(int i=0; i<snakeLen; i++) { snakeX[i] = 10 - i; snakeY[i] = 12; }
  foodX = random(1, 30); foodY = random(5, 23); 
  tft.fillScreen(TFT_BLACK); 
  tft.drawFastHLine(0, 42, 320, TFT_WHITE); 
  drawCloseButton();
  tft.setTextSize(2); tft.setTextColor(TFT_GREEN);
  tft.setCursor(50, 120); tft.print("Baslamak Icin Dokun");
}

void showSnake(bool pressed, uint16_t tx, uint16_t ty) {
  static int lastScore = -1; 

  if (screenNeedsRedraw) { 
    initSnake(); 
    screenNeedsRedraw = false; 
    snakeWasTouched = false; 
    lastScore = -1; 
  }
  
  bool isTapped = pressed && !snakeWasTouched;
  snakeWasTouched = pressed;

  int currentScore = snakeLen - 3;
  
  if (lastScore != currentScore && snakeState != 2) {
    tft.fillRect(5, 5, 270, 30, TFT_BLACK); 
    tft.setTextSize(2); tft.setTextColor(TFT_WHITE);
    tft.setCursor(5, 12); tft.printf("SKOR: %d", currentScore);
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(140, 12); tft.printf("REKOR: %d", snakeHighScore);
    lastScore = currentScore; 
  }

  if (snakeState == 0) { 
    if (isTapped && ty > 45) { 
      snakeState = 1; 
      tft.fillRect(0, 45, 320, 195, TFT_BLACK); 
      lastScore = -1; 
    }
  }
  else if (snakeState == 1) { 
    if (pressed && ty > 45) {
      if (ty < 90 && dir != 2) dir = 0; 
      else if (ty > 190 && dir != 0) dir = 2; 
      else if (tx < 160 && dir != 1) dir = 3; 
      else if (tx > 160 && dir != 3) dir = 1; 
    }
    
    if (millis() - lastMove > 130) {
      lastMove = millis();
      tft.fillRect(snakeX[snakeLen-1]*10, snakeY[snakeLen-1]*10, 9, 9, TFT_BLACK);
      
      for(int i = snakeLen-1; i > 0; i--) { 
        snakeX[i] = snakeX[i-1]; snakeY[i] = snakeY[i-1]; 
      }
      
      if(dir == 0) snakeY[0]--; else if(dir == 1) snakeX[0]++; 
      else if(dir == 2) snakeY[0]++; else if(dir == 3) snakeX[0]--;
      
      bool selfHit = false;
      for(int i=1; i<snakeLen; i++) {
        if(snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) selfHit = true;
      }
      
      if(snakeX[0] < 0 || snakeX[0] > 31 || snakeY[0] < 4 || snakeY[0] > 23 || selfHit) { 
        snakeState = 2; 
        playBeep(300);
        int finalScore = snakeLen - 3;
        
        if (finalScore > snakeHighScore) {
          snakeHighScore = finalScore;
          preferences.putInt("snake_hi", snakeHighScore); 
        }
        
        tft.fillRect(50, 80, 220, 80, TFT_BLACK);
        tft.drawRect(50, 80, 220, 80, TFT_WHITE);
        tft.setTextSize(2); tft.setTextColor(TFT_RED);
        tft.setCursor(110, 95); tft.print("YANDIN!");
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(70, 130); tft.printf("SKOR: %d", finalScore);
        return; 
      }
      
      if(snakeX[0] == foodX && snakeY[0] == foodY) { 
        snakeLen++; 
        foodX = random(1, 30); foodY = random(5, 23); 
        playBeep(20);
      }
      
      tft.fillRect(foodX*10, foodY*10, 8, 8, TFT_RED); 
      tft.fillRect(snakeX[0]*10, snakeY[0]*10, 9, 9, TFT_GREEN);
    }
  }
  else if (snakeState == 2) { 
    if (isTapped && ty > 45) { initSnake(); }
  }
}

/* ==============================================================================
   BOMBA UYGULAMASI
============================================================================== */

void showBombApp() {
  if (screenNeedsRedraw) {
    tft.fillScreen(TFT_BLACK); drawCloseButton(); 
    bombCounter = 10; bombActive = true; tickInterval = 1000; lastBombTick = millis();
    screenNeedsRedraw = false; playBeep(200); 
    tft.setTextSize(8); tft.setTextColor(TFT_RED, TFT_BLACK); tft.setCursor(115, 110); tft.print(bombCounter);
  }
  if (bombActive) {
    if (millis() - lastBombTick >= tickInterval) {
      lastBombTick = millis(); bombCounter--;
      if (bombCounter <= 5) tickInterval = 500; if (bombCounter <= 2) tickInterval = 250; 
      tft.fillRect(100, 100, 140, 80, TFT_BLACK); 
      if (bombCounter > 0) {
        playBeep(40); tft.setTextSize(8); tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.setCursor(bombCounter == 10 ? 115 : 135, 110); tft.print(bombCounter);
      } else {
        bombActive = false; digitalWrite(BUZZER_PIN, HIGH);
        for(int i = 0; i < 150; i++) { int r = random(10, 70); tft.fillCircle(random(r, 320-r), random(r, 240-r), r, random(0xFFFF)); }
        digitalWrite(BUZZER_PIN, LOW); 
        tft.fillScreen(TFT_WHITE); tft.setTextSize(7); tft.setTextColor(TFT_BLACK);
        tft.setCursor(65, 100); tft.print("BOOM!");
        boomStartTime = millis(); currentState = APP_BOOM_WAIT; 
      }
    }
  }
}

/* ==============================================================================
   ZAMAN UYGULAMASI
============================================================================== */

void showTimeApp() {
  static unsigned long lastTimeUpdate = 0; 
  struct tm ti;
  
  if (screenNeedsRedraw) {
    tft.fillScreen(TFT_BLACK); drawCloseButton(); tft.setTextSize(2); tft.setTextColor(TFT_YELLOW);
    tft.setCursor(10, 20); tft.print("ZAMAN"); screenNeedsRedraw = false; lastTimeUpdate = 0;
  }
  
  if (millis() - lastTimeUpdate > 1000 || lastTimeUpdate == 0) {
    if(!getLocalTime(&ti)) return; 
    lastTimeUpdate = millis();
    
    tft.setTextSize(4); tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setCursor(15, 75);
    tft.printf("%02d.%02d.%04d   ", ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
    tft.setTextSize(6); tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setCursor(15, 125);
    tft.printf("%02d:%02d:%02d   ", ti.tm_hour, ti.tm_min, ti.tm_sec);
  }
}

/* ==============================================================================
   DİĞER UYGULAMALAR 
============================================================================== */

void drawSingleEye(int centerX, int centerY, int offX, int offY) {
  tft.fillCircle(centerX, centerY, 40, TFT_WHITE); tft.fillCircle(centerX + offX, centerY + offY, 15, TFT_BLUE); tft.fillCircle(centerX + offX, centerY + offY, 6, TFT_BLACK);
}

void showEye() {
  static bool isBlinking = false; static unsigned long lastBlinkTime = 0;
  if (screenNeedsRedraw) { tft.fillScreen(TFT_BLACK); drawCloseButton(); screenNeedsRedraw = false; lastEyeMove = 0; }
  if (!isBlinking && (millis() - lastEyeMove > 1500)) {
    lastEyeMove = millis(); isBlinking = (random(0, 100) < 5); 
    if (isBlinking) {
      tft.fillRect(40, 80, 250, 80, TFT_BLACK); tft.drawFastHLine(50, 120, 80, TFT_WHITE); tft.drawFastHLine(190, 120, 80, TFT_WHITE); lastBlinkTime = millis();
    } else {
      int ox = random(-12, 13); int oy = random(-12, 13); drawSingleEye(90, 120, ox, oy); drawSingleEye(230, 120, ox, oy);
    }
  }
  if (isBlinking && (millis() - lastBlinkTime > 150)) { isBlinking = false; drawSingleEye(90, 120, 0, 0); drawSingleEye(230, 120, 0, 0); }
}

void showSubs() {
  if (screenNeedsRedraw) {
    tft.fillScreen(TFT_BLACK); drawCloseButton();
    tft.setCursor(10, 20); tft.setTextColor(TFT_RED); tft.setTextSize(3); tft.println("YouTube");
    screenNeedsRedraw = false; lastApiUpdate = 0;
  }
  if (millis() - lastApiUpdate > 60000 || lastApiUpdate == 0) {
    if(WiFi.status() == WL_CONNECTED){
      HTTPClient http; http.begin("https://www.googleapis.com/youtube/v3/channels?part=statistics&id=" + String(channelId) + "&key=" + String(apiKey));
      if(http.GET() > 0) { DynamicJsonDocument doc(1024); deserializeJson(doc, http.getString()); subCount = doc["items"][0]["statistics"]["subscriberCount"]; } http.end();
    }
    lastApiUpdate = millis();
    tft.fillRect(10, 110, 300, 60, TFT_BLACK); tft.setCursor(10, 115); tft.setTextSize(4); tft.setTextColor(TFT_YELLOW); tft.printf("%ld Abone", subCount);
  }
}

void startWiFiPortal() {
  tft.fillScreen(TFT_BLACK); tft.setCursor(10, 80); tft.setTextSize(2); tft.setTextColor(TFT_GREEN); tft.println("Telefonundan Baglan:");
  tft.setTextColor(TFT_WHITE); tft.println("SSID: ESP32_Ayarlar"); tft.setTextColor(TFT_RED); tft.setTextSize(1); tft.setCursor(10, 140);
  tft.println("NOT: Baglanti ekraninda cihaz bekler."); tft.println("30 SANIYE icinde islem yapilmazsa"); tft.println("otomatik olarak ana menuye doner.");
  WiFiManager wm; wm.setConfigPortalTimeout(30); wm.startConfigPortal("ESP32_Ayarlar");
  
  unsigned long touchTimeout = millis();
  while (ts.touched() && (millis() - touchTimeout < 500)) { ts.getPoint(); delay(10); } 
  
  currentState = HOME; screenNeedsRedraw = true;
}

/* ==============================================================================
   ANA DÖNGÜ
============================================================================== */

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  
  preferences.begin("game_scores", false);
  birdHighScore = preferences.getInt("bird_hi", 0);
  snakeHighScore = preferences.getInt("snake_hi", 0);

  tft.init(); tft.setRotation(1); showBootLogo();
  touchSPI.begin(T_CLK_PIN, T_MISO_PIN, T_MOSI_PIN, T_CS_PIN); ts.begin(touchSPI); ts.setRotation(1);
  WiFiManager wm; wm.setConfigPortalTimeout(15); wm.autoConnect("ESP32_Alemdar");
  configTime(10800, 0, "pool.ntp.org");
  showWelcomeScreen(); screenNeedsRedraw = true;
}

void loop() {
  uint16_t tx = 0, ty = 0; bool pressed = false;
  if (ts.touched()) { 
    TS_Point p = ts.getPoint(); 
    tx = map(p.x, 300, 3800, 320, 0); 
    ty = map(p.y, 300, 3800, 240, 0); 
    pressed = true; 
  }

  switch (currentState) {
    case HOME:
      if (screenNeedsRedraw) { 
        tft.fillScreen(TFT_BLACK); tft.setTextSize(2); tft.setTextColor(TFT_YELLOW); tft.setCursor(110, 15); tft.print("KONTROL");
        String lbl[] = {"ABONE", "ZAMAN", "GOZ", "YILAN", "WIFI", "SATRANC", "BOMBA", "KUS"};
        for (int i = 0; i < 8; i++) {
          int x = 8 + ((i%4)*78); int y = 55 + ((i/4)*85); 
          tft.drawRoundRect(x, y, 70, 75, 8, TFT_WHITE);
          tft.setTextSize(1); tft.setTextColor(TFT_CYAN); tft.setCursor(x + 10, y + 32); tft.print(lbl[i]);
        }
        screenNeedsRedraw = false; 
      }
      if (pressed && ty >= 55) {
        int btnIdx = (tx/80) + ((ty-55)/85 * 4);
        if (btnIdx == 0) currentState = APP_SUBS;
        else if (btnIdx == 1) currentState = APP_TIME;
        else if (btnIdx == 2) currentState = APP_EYE;
        else if (btnIdx == 3) currentState = APP_SNAKE;
        else if (btnIdx == 4) currentState = APP_WIFI;
        else if (btnIdx == 5) currentState = APP_CHESS;
        else if (btnIdx == 6) currentState = APP_BOMB;
        else if (btnIdx == 7) currentState = APP_BIRD;
        if (currentState != HOME) { screenNeedsRedraw = true; delay(300); }
      }
      break;

    case APP_BOOM_WAIT:
      if (millis() - boomStartTime >= 3000) { currentState = HOME; screenNeedsRedraw = true; } break;

    default:
      if (currentState == APP_SUBS) showSubs();
      else if (currentState == APP_TIME) showTimeApp();
      else if (currentState == APP_EYE) showEye();
      else if (currentState == APP_SNAKE) showSnake(pressed, tx, ty);
      else if (currentState == APP_CHESS) showChess(pressed, tx, ty);
      else if (currentState == APP_WIFI) startWiFiPortal();
      else if (currentState == APP_BOMB) showBombApp();
      else if (currentState == APP_BIRD) showBird(pressed, tx, ty);

      if (pressed && isCloseButtonPressed(tx, ty)) {
        digitalWrite(BUZZER_PIN, LOW); 
        unsigned long touchTimeout = millis();
        while (ts.touched() && (millis() - touchTimeout < 500)) { 
          ts.getPoint(); delay(10); 
        } 
        currentState = HOME; screenNeedsRedraw = true;
      }
      break;
  }
}