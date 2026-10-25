#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>
#include <Adafruit_NeoPixel.h>

#include "secrets.h"

// -----------------------------------
// ESP config
// -----------------------------------

#define DATA_PIN              25
#define MATRIX_SIZE           8
#define NUM_LEDS              (MATRIX_SIZE * MATRIX_SIZE)

#define SERPENTINE_WIRING     true

#define POLL_INTERVAL_MS      5000UL
#define TOKEN_REFRESH_MARGIN  300UL     // seconds before expiry to proactively refresh
Adafruit_NeoPixel ledmatrix(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// -----------------------------------
// Matrix color grid
// -----------------------------------

uint8_t gridR[MATRIX_SIZE][MATRIX_SIZE];
uint8_t gridG[MATRIX_SIZE][MATRIX_SIZE];
uint8_t gridB[MATRIX_SIZE][MATRIX_SIZE];


uint32_t sumR[MATRIX_SIZE][MATRIX_SIZE];
uint32_t sumG[MATRIX_SIZE][MATRIX_SIZE];
uint32_t sumB[MATRIX_SIZE][MATRIX_SIZE];
uint32_t sumCount[MATRIX_SIZE][MATRIX_SIZE];

// Dimensions of the source JPEG currently being decoded, set right before
// TJpgDec.drawJpg() is called so the callback knows how to bucket pixels.
uint16_t srcImgW = 0;
uint16_t srcImgH = 0;

// -----------------------------------
// WIFI
// -----------------------------------

struct WiFiCred {
  const char* ssid;
  const char* password;
};

WiFiCred wifiList[] = {
  { "Bony5a",   "05042024" },
  { "Bukowa", "misiaczki4" },
  { "Bukowa on the first floor",  "misiaczki4" },
  { "mozecieczyscicmiberlo",  "burgermeister2024!!" },
};

const size_t wifiCount = sizeof(wifiList) / sizeof(wifiList[0]);
const unsigned long WIFI_TIMEOUT_MS = 5000; // 5s per network

bool tryConnect(const char* ssid, const char* password) {
  Serial.printf("Trying SSID: %s", ssid);
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startAttempt > WIFI_TIMEOUT_MS) {
      Serial.println(" -> timed out");
      WiFi.disconnect(true);
      return false;
    }
    delay(300);
    Serial.print(".");
  }

  Serial.println(" -> connected!");
  return true;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);

  bool connected = false;
  for (size_t i = 0; i < wifiCount && !connected; i++) {
    connected = tryConnect(wifiList[i].ssid, wifiList[i].password);
  }

  if (connected) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to any known WiFi network.");
  }
}

// -----------------------------------
// Spotify API config
// -----------------------------------

String accessToken = "";
unsigned long tokenExpiresAtMs = 0; // in seconds since epoch
String lastSongId = "";
unsigned long lastPollMs = 0;

bool refreshAccessToken() {
  WiFiClientSecure client;
  client.setInsecure(); // skips TLS cert validation - simplest option on ESP32, it should only work locally anyway
 
  HTTPClient https;
  if (!https.begin(client, "https://accounts.spotify.com/api/token")) {
    Serial.println("Unable to connect to token endpoint");
    return false;
  }
 
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");
 
  String body =
      "grant_type=refresh_token"
      "&refresh_token=" + String(SPOTIFY_REFRESH_TOKEN) +
      "&client_id=" + String(SPOTIFY_CLIENT_ID) +
      "&client_secret=" + String(SPOTIFY_CLIENT_SECRET);
 
  int status = https.POST(body);
  if (status != 200) {
    Serial.printf("Token refresh failed, HTTP %d: %s\n", status, https.getString().c_str());
    https.end();
    return false;
  }
 
  String payload = https.getString();
  https.end();
 
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("Token JSON parse failed: %s\n", err.c_str());
    return false;
  }
 
  accessToken = doc["access_token"].as<String>();
  long expiresIn = doc["expires_in"] | 3600;
  tokenExpiresAtMs = millis() + (unsigned long)(expiresIn - TOKEN_REFRESH_MARGIN) * 1000UL;
 
  Serial.println("Access token refreshed");
  return true;
}
 
void ensureFreshToken() {
  if (accessToken.length() == 0 || millis() > tokenExpiresAtMs) {
    refreshAccessToken();
  }
}

struct CurrentSong {
  bool isPlaying = false;
  String id;
  String name;
  String imageUrl;
};
 
// Fetch currently-playing, retrying once with a fresh token on 401.
bool fetchCurrentSong(CurrentSong &out, bool isRetry = false) {
  WiFiClientSecure client;
  client.setInsecure();
 
  HTTPClient https;
  if (!https.begin(client, "https://api.spotify.com/v1/me/player/currently-playing")) {
    return false;
  }
  https.addHeader("Authorization", "Bearer " + accessToken);
 
  int status = https.GET();
 
  if (status == 204) {
    https.end();
    out.isPlaying = false;
    return true;
  }
 
  if (status == 401 && !isRetry) {
    https.end();
    refreshAccessToken();
    return fetchCurrentSong(out, true);
  }
 
  if (status != 200) {
    Serial.printf("currently-playing HTTP %d\n", status);
    https.end();
    return false;
  }
 
  // Only parse the fields we actually need - the full response is large.
  JsonDocument filter;
  filter["item"]["id"] = true;
  filter["item"]["name"] = true;
  filter["item"]["album"]["images"] = true;
 
  JsonDocument doc;
  DeserializationError err = deserializeJson(
      doc, https.getStream(), DeserializationOption::Filter(filter));
  https.end();
 
  if (err) {
    Serial.printf("currently-playing JSON parse failed: %s\n", err.c_str());
    return false;
  }
 
  if (doc["item"].isNull()) {
    out.isPlaying = false;
    return true;
  }
 
  out.isPlaying = true;
  out.id = doc["item"]["id"].as<String>();
  out.name = doc["item"]["name"].as<String>();
 
  JsonArray images = doc["item"]["album"]["images"];
  if (images.size() > 0) {
    // Spotify lists images largest-first; the last one is the smallest
    // (usually 64x64), which is what we want to decode on-chip.
    out.imageUrl = images[images.size() - 1]["url"].as<String>();
  }
 
  return true;
}
 
// ---------------------------------------------------------------------------
// JPEG decode -> 8x8 grid
// ---------------------------------------------------------------------------
// TJpg_Decoder calls this once per decoded MCU block at FULL source
// resolution (we no longer ask it to downscale). Every pixel gets mapped
// to the 8x8 output cell it falls into and summed there; averageGridFromSums()
// turns those sums into final pixel values once decoding is done.
bool jpegOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (srcImgW == 0 || srcImgH == 0) return false;
 
  for (uint16_t row = 0; row < h; row++) {
    for (uint16_t col = 0; col < w; col++) {
      int16_t px = x + col;
      int16_t py = y + row;
      if (px < 0 || px >= srcImgW || py < 0 || py >= srcImgH) continue;
 
      // Which output cell does this source pixel belong to?
      uint8_t outCol = (uint32_t)px * MATRIX_SIZE / srcImgW;
      uint8_t outRow = (uint32_t)py * MATRIX_SIZE / srcImgH;
      if (outCol >= MATRIX_SIZE) outCol = MATRIX_SIZE - 1;
      if (outRow >= MATRIX_SIZE) outRow = MATRIX_SIZE - 1;
 
      uint16_t c565 = bitmap[row * w + col];
      uint8_t r5 = (c565 >> 11) & 0x1F;
      uint8_t g6 = (c565 >> 5) & 0x3F;
      uint8_t b5 = c565 & 0x1F;
 
      sumR[outRow][outCol] += (r5 * 255) / 31;
      sumG[outRow][outCol] += (g6 * 255) / 63;
      sumB[outRow][outCol] += (b5 * 255) / 31;
      sumCount[outRow][outCol]++;
    }
  }
  return true;
}
 
// Turns the accumulated per-cell sums into final averaged grid values.
void averageGridFromSums() {
  for (uint8_t row = 0; row < MATRIX_SIZE; row++) {
    for (uint8_t col = 0; col < MATRIX_SIZE; col++) {
      uint32_t n = sumCount[row][col];
      if (n == 0) {
        // Shouldn't normally happen (every cell should get >=1 source
        // pixel), but guard against it so we don't divide by zero.
        gridR[row][col] = 0;
        gridG[row][col] = 0;
        gridB[row][col] = 0;
        continue;
      }
      gridR[row][col] = sumR[row][col] / n;
      gridG[row][col] = sumG[row][col] / n;
      gridB[row][col] = sumB[row][col] / n;
    }
  }
}
 
// Downloads the album art and decodes it straight into the 8x8 grid.
bool downloadAndDecodeAlbumArt(const String &url) {
  WiFiClientSecure client;
  client.setInsecure();
 
  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.println("Unable to connect to image URL");
    return false;
  }
 
  int status = https.GET();
  if (status != 200) {
    Serial.printf("Image download HTTP %d\n", status);
    https.end();
    return false;
  }
 
  int len = https.getSize();
  if (len <= 0 || len > 60000) { // sanity bound - a 64x64 jpg is only a few KB
    Serial.printf("Unexpected image size: %d\n", len);
    https.end();
    return false;
  }
 
  uint8_t *buf = (uint8_t *)malloc(len);
  if (!buf) {
    Serial.println("Out of memory downloading image");
    https.end();
    return false;
  }
 
  WiFiClient *stream = https.getStreamPtr();
  int received = 0;
  unsigned long start = millis();
  while (received < len && millis() - start < 8000) {
    if (stream->available()) {
      received += stream->readBytes(buf + received, len - received);
    } else {
      delay(1);
    }
  }
  https.end();
 
  if (received != len) {
    Serial.println("Image download incomplete");
    free(buf);
    return false;
  }

 
  memset(sumR, 0, sizeof(sumR));
  memset(sumG, 0, sizeof(sumG));
  memset(sumB, 0, sizeof(sumB));
  memset(sumCount, 0, sizeof(sumCount));
 
  TJpgDec.getJpgSize(&srcImgW, &srcImgH, buf, len);
// Sanity bound: this is meant for the ~64x64 "smallest" Spotify image.
  // Decoding much larger than that at full resolution risks running out
  // of RAM/time on the ESP32, so bail out rather than trying.
  if (srcImgW == 0 || srcImgH == 0 || srcImgW > 256 || srcImgH > 256) {
    Serial.printf("Source image too large for full-res decode: %dx%d\n", srcImgW, srcImgH);
    free(buf);
    return false;
  }
 
  // Full resolution decode - no decoder-side scaling. jpegOutputCallback()
  // buckets and sums every source pixel into the right 8x8 cell as it comes
  // in; averageGridFromSums() turns those sums into the final grid.
  TJpgDec.setJpgScale(1);
  bool ok = (TJpgDec.drawJpg(0, 0, buf, len) == JDR_OK);
  free(buf);
 
  if (!ok) {
    Serial.println("JPEG decode failed");
    return false;
  }
 
  averageGridFromSums();
  return true;
}
// ---------------------------------------------------------------------------
// LED matrix output
// ---------------------------------------------------------------------------
// Maps (col,row) in an 8x8 grid to a NeoPixel strip index, accounting for
// serpentine wiring if enabled.
// float color_scale = 75.0/255.0; // Scale 0-100 to 0-255
float color_scale = 255.0/255.0; // Scale 0-100 to 0-255

uint16_t pixelIndex(uint8_t col, uint8_t row) {
  if (SERPENTINE_WIRING && (row % 2 == 1)) {
    col = MATRIX_SIZE - 1 - col;
  }
  return row * MATRIX_SIZE + col;
}
 
void renderGridToMatrix() {
  for (uint8_t row = 0; row < MATRIX_SIZE; row++) {
    for (uint8_t col = 0; col < MATRIX_SIZE; col++) {
      uint16_t idx = pixelIndex(row, col);
      ledmatrix.setPixelColor(idx, ledmatrix.Color((uint8_t)(gridR[row][col] * color_scale), (uint8_t)(gridG[row][col] * color_scale), (uint8_t)(gridB[row][col] * color_scale)));
    }
  }
  ledmatrix.show();
}
 

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------

void setup() {  
  Serial.begin(115200);
  delay(200);
  
  ledmatrix.begin();
  ledmatrix.setBrightness(50);
  ledmatrix.clear();
  ledmatrix.show();
 
  connectWiFi();
 
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(jpegOutputCallback);
 
  ensureFreshToken();
}


void loop() {
if (millis() - lastPollMs < POLL_INTERVAL_MS) {
    return;
  }
  lastPollMs = millis();
 
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }
  ensureFreshToken();
 
  CurrentSong song;
  if (!fetchCurrentSong(song)) {
    Serial.println("Failed to fetch currently playing");
    return;
  }
 
  if (!song.isPlaying) {
    Serial.println("Nothing playing");
    return;
  }
 
  Serial.printf("Now playing: %s (%s)\n", song.name.c_str(), song.id.c_str());
 
  if (song.id != lastSongId && song.imageUrl.length() > 0) {
    if (downloadAndDecodeAlbumArt(song.imageUrl)) {
      renderGridToMatrix();
      lastSongId = song.id;
    }
  }
}