/*
 * HW2: AMB82-MINI focus dashboard.
 *
 * Channel 0: FHD H.264 -> RTSP/VLC
 * Channel 1: 640 x 360 JPEG -> browser latest-frame snapshots
 * Channel 3: 576 x 320 RGB -> YOLOv4-tiny + SCRFD head posture
 *
 * The web page, timer, API, and statistics all run on the board. No SD card
 * or external web server is required.
 */

#include "WiFi.h"
#include "StreamIO.h"
#include "VideoStream.h"
#include "RTSP.h"
#include "NNObjectDetection.h"
#include "NNFaceDetection.h"
#include "VideoStreamOverlay.h"
#include "dashboard_html.h"
#include "wifi_secrets.h"

#define STREAM_CHANNEL 0
#define WEB_CHANNEL 1
#define NN_CHANNEL 3
#define PERSON_CLASS_ID 0
#define BOOK_CLASS_ID 73
#define PERSON_MIN_SCORE 60
#define BOOK_MIN_SCORE 45
#define FACE_MIN_SCORE 55
#define PERSON_HOLD_MS 2500
#define FACE_HOLD_MS 1200
#define BOOK_HOLD_MS 1800
#define HEAD_DOWN_ENTER_SCORE 60
#define HEAD_DOWN_EXIT_SCORE 56
#define DASHBOARD_PORT 80
#define SNAPSHOT_PORT 81

VideoSetting streamConfig(VIDEO_FHD, 30, VIDEO_H264, 0);
// AmebaPro2 core 4.0.9 does not initialise dimensions for VIDEO_WVGA.
// Use the explicit-size constructor so channel 1 is really 640 x 360.
VideoSetting webConfig((uint16_t)640, (uint16_t)360, 10, VIDEO_JPEG, 1);
VideoSetting nnConfig(576, 320, 10, VIDEO_RGB, 0);

RTSP rtsp;
NNObjectDetection personDetector;

class HeadPoseFaceDetector: public NNFaceDetection {
public:
    void selectBundledScrfd()
    {
        _nntask = FACE_DETECTION;
        _yolomodel = NA_MODEL;
        _scrfdmodel = DEFAULT_SCRFD;
        _mfnmodel = NA_MODEL;
        _yamnetmodel = NA_MODEL;
        _imgclassmodel = NA_MODEL;
        _palmdetmodel = NA_MODEL;
        _handlandmarkmodel = NA_MODEL;
    }
};

HeadPoseFaceDetector faceDetector;
StreamIO rtspStreamer(1, 1);
StreamIO nnStreamer(1, 2);
WiFiServer dashboardServer(DASHBOARD_PORT);
WiFiServer snapshotServer(SNAPSHOT_PORT);

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASSWORD;
int wifiStatus = WL_IDLE_STATUS;

// iPhone Personal Hotspot sometimes completes WPA association but does not
// answer this board's DHCP request. Its IPv4 tethering subnet is consistently
// 172.20.10.0/28, so keep the board on the previously used, currently free .4.
IPAddress boardIp(172, 20, 10, 4);
IPAddress hotspotDns(172, 20, 10, 1);
IPAddress hotspotGateway(172, 20, 10, 1);
IPAddress hotspotSubnet(255, 255, 255, 240);

enum TimerPhase {
    PHASE_IDLE,
    PHASE_FOCUS,
    PHASE_BREAK,
    PHASE_PAUSED,
    PHASE_DONE
};

enum PostureState {
    POSTURE_UNKNOWN,
    POSTURE_HEAD_DOWN,
    POSTURE_HEAD_UP
};

volatile TimerPhase timerPhase = PHASE_IDLE;
volatile TimerPhase phaseBeforePause = PHASE_FOCUS;
volatile uint16_t focusMinutes = 25;
volatile uint16_t restMinutes = 5;
volatile uint16_t headUpSeconds = 2;
volatile bool autoPauseAbsent = true;
volatile uint32_t timerTotalSeconds = 25 * 60;
volatile uint32_t timerRemainingSeconds = 25 * 60;
volatile uint32_t focusedSeconds = 0;
volatile uint32_t unfocusedSeconds = 0;
volatile uint32_t awaySeconds = 0;
volatile uint32_t unknownSeconds = 0;
volatile uint32_t currentStreakSeconds = 0;
volatile uint32_t distractionCount = 0;
volatile bool personPresent = false;
volatile int personCount = 0;
volatile int bestPersonScore = 0;
volatile bool bookPresent = false;
volatile int bookCount = 0;
volatile int faceCount = 0;
volatile int bestFaceScore = 0;
volatile bool headPoseValid = false;
volatile int headPitchScore = 0;
volatile PostureState stablePosture = POSTURE_UNKNOWN;
volatile bool cameraReady = false;
volatile bool aiReady = false;
volatile bool headTrackingReady = false;

unsigned long timerLastTick = 0;
unsigned long lastPersonSeen = 0;
unsigned long lastBookSeen = 0;
unsigned long lastHeadPoseSeen = 0;
unsigned long postureCandidateSince = 0;
unsigned long lastReconnectAttempt = 0;
bool hasSeenPerson = false;
bool wifiWasConnected = false;
PostureState postureCandidate = POSTURE_UNKNOWN;
PostureState lastTimedPosture = POSTURE_UNKNOWN;
bool pitchFilterReady = false;
float filteredPitchRatio = 0.0f;

char statusJson[1536];

const char *phaseName(TimerPhase phase)
{
    switch (phase) {
        case PHASE_FOCUS:
            return "FOCUS";
        case PHASE_BREAK:
            return "BREAK";
        case PHASE_PAUSED:
            return "PAUSED";
        case PHASE_DONE:
            return "DONE";
        default:
            return "IDLE";
    }
}

const char *attentionStateName()
{
    if ((timerPhase == PHASE_BREAK) ||
        ((timerPhase == PHASE_PAUSED) && (phaseBeforePause == PHASE_BREAK))) {
        return "REST";
    }
    if (!personPresent) {
        return "NO_PERSON";
    }
    if (!headPoseValid || (stablePosture == POSTURE_UNKNOWN)) {
        return "UNKNOWN";
    }
    return (stablePosture == POSTURE_HEAD_DOWN) ? "HEAD_DOWN" : "HEAD_UP";
}

int clampInt(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

bool hasUsableWiFi()
{
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    IPAddress ip = WiFi.localIP();
    bool emptyAddress = (ip[0] == 0) && (ip[1] == 0) && (ip[2] == 0) && (ip[3] == 0);
    // Ameba core 4.0.9 prints 192.168.1.80 as a fallback after DHCP timeout.
    bool coreFallback = (ip[0] == 192) && (ip[1] == 168) && (ip[2] == 1) && (ip[3] == 80);
    return !emptyAddress && !coreFallback;
}

void resetSessionStats()
{
    focusedSeconds = 0;
    unfocusedSeconds = 0;
    awaySeconds = 0;
    unknownSeconds = 0;
    currentStreakSeconds = 0;
    distractionCount = 0;
    lastTimedPosture = POSTURE_UNKNOWN;
}

void startFocusSession()
{
    resetSessionStats();
    timerTotalSeconds = (uint32_t)focusMinutes * 60UL;
    timerRemainingSeconds = timerTotalSeconds;
    phaseBeforePause = PHASE_FOCUS;
    timerPhase = PHASE_FOCUS;
    timerLastTick = millis();
}

void pauseSession()
{
    if ((timerPhase == PHASE_FOCUS) || (timerPhase == PHASE_BREAK)) {
        phaseBeforePause = timerPhase;
        timerPhase = PHASE_PAUSED;
    }
}

void resumeSession()
{
    if (timerPhase == PHASE_PAUSED) {
        timerPhase = phaseBeforePause;
        timerLastTick = millis();
    }
}

void resetSession()
{
    resetSessionStats();
    timerTotalSeconds = (uint32_t)focusMinutes * 60UL;
    timerRemainingSeconds = timerTotalSeconds;
    phaseBeforePause = PHASE_FOCUS;
    timerPhase = PHASE_IDLE;
    timerLastTick = millis();
}

void processTimerSecond()
{
    if (timerPhase == PHASE_FOCUS) {
        if (!personPresent) {
            awaySeconds++;
            currentStreakSeconds = 0;
            lastTimedPosture = POSTURE_UNKNOWN;
        } else if (!headPoseValid || (stablePosture == POSTURE_UNKNOWN)) {
            unknownSeconds++;
            currentStreakSeconds = 0;
            lastTimedPosture = POSTURE_UNKNOWN;
        } else if (stablePosture == POSTURE_HEAD_DOWN) {
            focusedSeconds++;
            currentStreakSeconds++;
            lastTimedPosture = POSTURE_HEAD_DOWN;
        } else {
            unfocusedSeconds++;
            currentStreakSeconds = 0;
            if (lastTimedPosture == POSTURE_HEAD_DOWN) {
                distractionCount++;
            }
            lastTimedPosture = POSTURE_HEAD_UP;
        }

        if (autoPauseAbsent && !personPresent) {
            return;
        }

        if (timerRemainingSeconds > 0) {
            timerRemainingSeconds--;
        }
        if (timerRemainingSeconds == 0) {
            timerPhase = PHASE_BREAK;
            phaseBeforePause = PHASE_BREAK;
            timerTotalSeconds = (uint32_t)restMinutes * 60UL;
            timerRemainingSeconds = timerTotalSeconds;
        }
    } else if (timerPhase == PHASE_BREAK) {
        if (timerRemainingSeconds > 0) {
            timerRemainingSeconds--;
        }
        if (timerRemainingSeconds == 0) {
            timerPhase = PHASE_DONE;
        }
    }
}

void updateTimer()
{
    unsigned long now = millis();
    while ((unsigned long)(now - timerLastTick) >= 1000UL) {
        timerLastTick += 1000UL;
        processTimerSecond();
    }
}

bool readRequestLine(WiFiClient &client, char *buffer, size_t bufferSize)
{
    size_t index = 0;
    unsigned long started = millis();

    while (client.connected() && ((unsigned long)(millis() - started) < 1200UL)) {
        while (client.available()) {
            int value = client.read();
            if (value < 0) {
                continue;
            }
            char c = (char)value;
            if (c == '\n') {
                buffer[index] = '\0';
                return index > 0;
            }
            if ((c != '\r') && (index + 1 < bufferSize)) {
                buffer[index++] = c;
            }
        }
        delay(1);
    }

    buffer[index] = '\0';
    return index > 0;
}

bool drainRequestHeaders(WiFiClient &client)
{
    bool lineHasContent = false;
    unsigned long started = millis();

    while (client.connected() && ((unsigned long)(millis() - started) < 1200UL)) {
        while (client.available()) {
            int value = client.read();
            if (value < 0) {
                continue;
            }
            char c = (char)value;
            if (c == '\n') {
                if (!lineHasContent) {
                    return true;
                }
                lineHasContent = false;
            } else if (c != '\r') {
                lineHasContent = true;
            }
        }
        delay(1);
    }
    return false;
}

void sendHttpBody(
    WiFiClient &client,
    const char *status,
    const char *contentType,
    const char *body,
    size_t bodyLength
)
{
    client.print("HTTP/1.1 ");
    client.println(status);
    client.print("Content-Type: ");
    client.println(contentType);
    client.println("Cache-Control: no-store, no-cache, must-revalidate");
    client.println("Access-Control-Allow-Origin: *");
    client.println("X-Content-Type-Options: nosniff");
    client.print("Content-Length: ");
    client.println((unsigned long)bodyLength);
    client.println("Connection: close");
    client.println();

    size_t sent = 0;
    if (bodyLength > 4096) {
        // This core reports the requested size even when lwIP performs a
        // partial multi-byte write.  One-byte writes are exact and match the
        // official ReadHTMLFile example, so use them for the large page.
        while (sent < bodyLength) {
            if (client.write((uint8_t)body[sent]) != 1) {
                break;
            }
            ++sent;
            if ((sent & 0xFFU) == 0U) {
                delay(1);
            }
        }
    } else {
        while (sent < bodyLength) {
            size_t blockSize = ((bodyLength - sent) > 256) ? 256 : (bodyLength - sent);
            size_t written = client.write((uint8_t *)(body + sent), blockSize);
            if (written == 0) {
                break;
            }
            sent += written;
            delay(3);
        }
    }
    delay((bodyLength > 4096) ? 200 : 15);
}

void sendText(WiFiClient &client, const char *status, const char *contentType, const char *body)
{
    sendHttpBody(client, status, contentType, body, strlen(body));
}

int queryInt(const char *request, const char *key, int fallback)
{
    char token[28];
    snprintf(token, sizeof(token), "%s=", key);
    const char *cursor = request;

    while ((cursor = strstr(cursor, token)) != NULL) {
        if ((cursor == request) || (cursor[-1] == '?') || (cursor[-1] == '&')) {
            return atoi(cursor + strlen(token));
        }
        cursor += strlen(token);
    }
    return fallback;
}

bool queryText(const char *request, const char *key, char *output, size_t outputSize)
{
    char token[28];
    snprintf(token, sizeof(token), "%s=", key);
    const char *cursor = request;

    while ((cursor = strstr(cursor, token)) != NULL) {
        if ((cursor == request) || (cursor[-1] == '?') || (cursor[-1] == '&')) {
            cursor += strlen(token);
            size_t index = 0;
            while ((*cursor != '\0') && (*cursor != '&') && (*cursor != ' ') &&
                   (index + 1 < outputSize)) {
                output[index++] = *cursor++;
            }
            output[index] = '\0';
            return index > 0;
        }
        cursor += strlen(token);
    }
    return false;
}

void sendStatusJson(WiFiClient &client)
{
    IPAddress ip = WiFi.localIP();
    uint32_t classified = focusedSeconds + unfocusedSeconds;
    uint32_t focusRate = (classified > 0) ? ((focusedSeconds * 100UL) / classified) : 0;
    bool connected = hasUsableWiFi();
    bool running = (timerPhase == PHASE_FOCUS) || (timerPhase == PHASE_BREAK);
    bool paused = (timerPhase == PHASE_PAUSED);

    snprintf(
        statusJson,
        sizeof(statusJson),
        "{\"phase\":\"%s\",\"running\":%s,\"paused\":%s,"
        "\"state\":\"%s\",\"remainingSec\":%lu,\"totalSec\":%lu,"
        "\"settings\":{\"focusMinutes\":%u,\"restMinutes\":%u,"
        "\"headUpSeconds\":%u,\"autoPauseAbsent\":%s},"
        "\"stats\":{\"focusedSec\":%lu,\"unfocusedSec\":%lu,"
        "\"awaySec\":%lu,\"unknownSec\":%lu,\"distractionCount\":%lu,"
        "\"streakSec\":%lu,\"focusRate\":%lu},"
        "\"device\":{\"wifiConnected\":%s,\"ip\":\"%u.%u.%u.%u\"," 
        "\"rssi\":%ld,\"rtsp\":\"rtsp://%u.%u.%u.%u:%d\"," 
        "\"personCount\":%d,\"personScore\":%d,"
        "\"bookPresent\":%s,\"bookCount\":%d,"
        "\"faceCount\":%d,\"faceScore\":%d,"
        "\"headPoseValid\":%s,\"headPitchScore\":%d,"
        "\"cameraReady\":%s,\"aiReady\":%s,"
        "\"headTrackingReady\":%s,\"uptimeSec\":%lu}}",
        phaseName(timerPhase),
        running ? "true" : "false",
        paused ? "true" : "false",
        attentionStateName(),
        (unsigned long)timerRemainingSeconds,
        (unsigned long)timerTotalSeconds,
        (unsigned int)focusMinutes,
        (unsigned int)restMinutes,
        (unsigned int)headUpSeconds,
        autoPauseAbsent ? "true" : "false",
        (unsigned long)focusedSeconds,
        (unsigned long)unfocusedSeconds,
        (unsigned long)awaySeconds,
        (unsigned long)unknownSeconds,
        (unsigned long)distractionCount,
        (unsigned long)currentStreakSeconds,
        (unsigned long)focusRate,
        connected ? "true" : "false",
        ip[0],
        ip[1],
        ip[2],
        ip[3],
        (long)(connected ? WiFi.RSSI() : 0),
        ip[0],
        ip[1],
        ip[2],
        ip[3],
        rtsp.getPort(),
        (int)personCount,
        (int)bestPersonScore,
        bookPresent ? "true" : "false",
        (int)bookCount,
        (int)faceCount,
        (int)bestFaceScore,
        headPoseValid ? "true" : "false",
        (int)headPitchScore,
        cameraReady ? "true" : "false",
        aiReady ? "true" : "false",
        headTrackingReady ? "true" : "false",
        (unsigned long)(millis() / 1000UL)
    );

    sendText(client, "200 OK", "application/json; charset=utf-8", statusJson);
}

void applyConfig(const char *request)
{
    focusMinutes = (uint16_t)clampInt(queryInt(request, "focus", focusMinutes), 1, 180);
    restMinutes = (uint16_t)clampInt(queryInt(request, "rest", restMinutes), 1, 60);
    headUpSeconds = (uint16_t)clampInt(queryInt(request, "headUp", headUpSeconds), 1, 30);
    autoPauseAbsent = queryInt(request, "autoPause", autoPauseAbsent ? 1 : 0) != 0;

    if ((timerPhase == PHASE_IDLE) || (timerPhase == PHASE_DONE)) {
        timerTotalSeconds = (uint32_t)focusMinutes * 60UL;
        timerRemainingSeconds = timerTotalSeconds;
    }
}

void applyAction(const char *request)
{
    char action[20];
    if (!queryText(request, "name", action, sizeof(action))) {
        return;
    }
    if (strcmp(action, "start") == 0) {
        startFocusSession();
    } else if (strcmp(action, "pause") == 0) {
        pauseSession();
    } else if (strcmp(action, "resume") == 0) {
        resumeSession();
    } else if (strcmp(action, "reset") == 0) {
        resetSession();
    }
}

void handleDashboardClient(WiFiClient &client)
{
    char request[384];
    if (!readRequestLine(client, request, sizeof(request))) {
        client.stop();
        return;
    }
    drainRequestHeaders(client);

    if (strncmp(request, "GET / ", 6) == 0) {
        sendHttpBody(
            client,
            "200 OK",
            "text/html; charset=utf-8",
            DASHBOARD_HTML,
            strlen(DASHBOARD_HTML)
        );
    } else if (strncmp(request, "GET /api/status ", 16) == 0) {
        sendStatusJson(client);
    } else if (strncmp(request, "GET /api/config?", 16) == 0) {
        applyConfig(request);
        sendText(client, "200 OK", "application/json; charset=utf-8", "{\"ok\":true}");
    } else if (strncmp(request, "GET /api/action?", 16) == 0) {
        applyAction(request);
        sendText(client, "200 OK", "application/json; charset=utf-8", "{\"ok\":true}");
    } else if (strncmp(request, "GET /favicon.ico ", 17) == 0) {
        sendText(client, "204 No Content", "text/plain", "");
    } else {
        sendText(client, "404 Not Found", "text/plain; charset=utf-8", "Not found");
    }
    delay(20);
    client.stop();
}

void dashboardTask(void *parameter)
{
    (void)parameter;
    while (true) {
        WiFiClient client = dashboardServer.available();
        if (client) {
            handleDashboardClient(client);
        }
        delay(2);
    }
}

void sendLatestSnapshot(WiFiClient &client)
{
    uint32_t imageAddress = 0;
    uint32_t imageLength = 0;
    Camera.getImage(WEB_CHANNEL, &imageAddress, &imageLength);

    if ((imageAddress == 0) || (imageLength == 0)) {
        sendText(client, "503 Service Unavailable", "text/plain", "Camera is not ready");
        return;
    }

    cameraReady = true;
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Cache-Control: no-store, no-cache, must-revalidate");
    client.println("Access-Control-Allow-Origin: *");
    client.print("Content-Length: ");
    client.println((unsigned long)imageLength);
    client.println("Connection: close");
    client.println();

    uint32_t offset = 0;
    while (offset < imageLength && client.connected()) {
        uint32_t remaining = imageLength - offset;
        size_t blockSize = (remaining > 256UL) ? 256 : (size_t)remaining;
        size_t written = client.write((uint8_t *)(imageAddress + offset), blockSize);
        if (written == 0) {
            break;
        }
        offset += written;
        if ((offset & 0xFFFU) == 0U) {
            delay(1);
        }
    }
    delay(3);
}

void snapshotTask(void *parameter)
{
    (void)parameter;
    while (true) {
        WiFiClient client = snapshotServer.available();
        if (!client) {
            delay(2);
            continue;
        }

        char request[192];
        bool validRequest = readRequestLine(client, request, sizeof(request));
        drainRequestHeaders(client);

        if (validRequest &&
            ((strncmp(request, "GET /snapshot", 13) == 0) ||
             (strncmp(request, "GET /stream", 11) == 0))) {
            sendLatestSnapshot(client);
        } else {
            sendText(client, "404 Not Found", "text/plain", "Not found");
        }
        client.stop();
    }
}

void printStreamingInfo()
{
    IPAddress ip = WiFi.localIP();

    Serial.println();
    Serial.println("------------------------------------------------");
    Serial.println("AMB82 focus dashboard is ready");
    Serial.print("Dashboard: http://");
    Serial.println(ip);
    Serial.print("Browser snapshot: http://");
    Serial.print(ip);
    Serial.println(":81/snapshot");
    Serial.print("VLC URL: rtsp://");
    Serial.print(ip);
    Serial.print(":");
    Serial.println(rtsp.getPort());
    Serial.println("Head up/down classification: SCRFD 5-point posture estimate");
    Serial.println("No SD card is required.");
    Serial.println("------------------------------------------------");
}

void setup()
{
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    if ((strlen(ssid) == 0) || (strcmp(ssid, "YOUR_WIFI_SSID") == 0)) {
        Serial.println("Set WIFI_SSID and WIFI_PASSWORD in wifi_secrets.h first.");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("Connecting to Wi-Fi...");
    WiFi.config(boardIp, hotspotDns, hotspotGateway, hotspotSubnet);
    wifiStatus = WiFi.begin(ssid, pass);
    unsigned long lastConnectNotice = millis();
    while (!hasUsableWiFi()) {
        // Ameba's driver already retries in the background. Calling begin()
        // again here can interrupt an association that is still in progress.
        delay(250);
        if ((unsigned long)(millis() - lastConnectNotice) >= 10000UL) {
            Serial.println("Waiting for Wi-Fi auto reconnect...");
            lastConnectNotice = millis();
        }
    }
    Serial.println("Wi-Fi connected.");

    wifiWasConnected = true;
    WiFi.disablePowerSave();

    // A lower bitrate is more stable when RTSP and browser video run together.
    streamConfig.setBitrate(2 * 1024 * 1024);
    webConfig.setJpegQuality(2);

    Camera.configVideoChannel(STREAM_CHANNEL, streamConfig);
    Camera.configVideoChannel(WEB_CHANNEL, webConfig);
    Camera.configVideoChannel(NN_CHANNEL, nnConfig);
    Camera.videoInit();

    rtsp.configVideo(streamConfig);
    rtsp.begin();

    personDetector.configVideo(nnConfig);
    // Keep this call on one line: Realtek's model packer detects it as text.
    personDetector.modelSelect(OBJECT_DETECTION, DEFAULT_YOLOV4TINY, DEFAULT_SCRFD, NA_MODEL);
    personDetector.begin();

    faceDetector.configVideo(nnConfig);
    // The 4.0.9 packer only keeps one modelSelect line. The line above bundles
    // both model files; this sets the second runtime module without replacing it.
    faceDetector.selectBundledScrfd();
    faceDetector.begin();

    rtspStreamer.registerInput(Camera.getStream(STREAM_CHANNEL));
    rtspStreamer.registerOutput(rtsp);
    if (rtspStreamer.begin() != 0) {
        Serial.println("ERROR: RTSP StreamIO link failed.");
        while (true) {
            delay(1000);
        }
    }

    nnStreamer.registerInput(Camera.getStream(NN_CHANNEL));
    nnStreamer.registerOutput1(personDetector);
    nnStreamer.registerOutput2(faceDetector);
    if (nnStreamer.begin() != 0) {
        Serial.println("ERROR: NN StreamIO link failed.");
        while (true) {
            delay(1000);
        }
    }

    OSD.configVideo(STREAM_CHANNEL, streamConfig);
    OSD.begin();

    Camera.channelBegin(STREAM_CHANNEL);
    Camera.channelBegin(WEB_CHANNEL);
    Camera.channelBegin(NN_CHANNEL);

    dashboardServer.begin();
    snapshotServer.begin();

    if (xTaskCreate(
            dashboardTask,
            (const char *)"dashboardTask",
            4096,
            NULL,
            tskIDLE_PRIORITY + 1,
            NULL
        ) != pdPASS) {
        Serial.println("ERROR: dashboard task creation failed.");
    }
    if (xTaskCreate(
            snapshotTask,
            (const char *)"snapshotTask",
            3072,
            NULL,
            tskIDLE_PRIORITY + 1,
            NULL
        ) != pdPASS) {
        Serial.println("ERROR: snapshot task creation failed.");
    }

    timerLastTick = millis();
    delay(1000);
    printStreamingInfo();
}

void updatePostureState(bool validPose, int pitchScore, unsigned long now)
{
    if (!validPose) {
        if ((unsigned long)(now - lastHeadPoseSeen) > FACE_HOLD_MS) {
            headPoseValid = false;
            stablePosture = POSTURE_UNKNOWN;
            postureCandidate = POSTURE_UNKNOWN;
            pitchFilterReady = false;
        }
        return;
    }

    headPoseValid = true;
    headPitchScore = pitchScore;
    lastHeadPoseSeen = now;

    int threshold = (stablePosture == POSTURE_HEAD_DOWN)
                        ? HEAD_DOWN_EXIT_SCORE
                        : HEAD_DOWN_ENTER_SCORE;
    PostureState measured = (pitchScore >= threshold)
                                ? POSTURE_HEAD_DOWN
                                : POSTURE_HEAD_UP;

    if (measured != postureCandidate) {
        postureCandidate = measured;
        postureCandidateSince = now;
    }

    unsigned long confirmationMs = (unsigned long)headUpSeconds * 1000UL;
    if ((unsigned long)(now - postureCandidateSince) >= confirmationMs) {
        stablePosture = postureCandidate;
    }
}

void updateVisionDetection()
{
    std::vector<ObjectDetectionResult> objects = personDetector.getResult();
    std::vector<FaceDetectionResult> faces = faceDetector.getResult();
    aiReady = true;
    headTrackingReady = true;

    bool personDetectedNow = false;
    bool bookDetectedNow = false;
    int detectedPersonCount = 0;
    int detectedPersonBestScore = 0;
    int detectedBookCount = 0;
    int qualifiedFaceCount = 0;
    int primaryFaceIndex = -1;
    float primaryFaceArea = 0.0f;
    int detectedFaceBestScore = 0;
    const int imageWidth = streamConfig.width();
    const int imageHeight = streamConfig.height();
    const int textHeight = OSD.getTextHeight(STREAM_CHANNEL);

    OSD.createBitmap(STREAM_CHANNEL);

    for (size_t i = 0; i < objects.size(); ++i) {
        ObjectDetectionResult item = objects[i];
        bool isPerson = (item.type() == PERSON_CLASS_ID) &&
                        (item.score() >= PERSON_MIN_SCORE);
        bool isBook = (item.type() == BOOK_CLASS_ID) &&
                      (item.score() >= BOOK_MIN_SCORE);
        if (!isPerson && !isBook) {
            continue;
        }

        int xMin = (int)(item.xMin() * imageWidth);
        int xMax = (int)(item.xMax() * imageWidth);
        int yMin = (int)(item.yMin() * imageHeight);
        int yMax = (int)(item.yMax() * imageHeight);
        int labelY = (yMin >= textHeight) ? (yMin - textHeight) : 0;
        char label[32];

        if (isPerson) {
            personDetectedNow = true;
            ++detectedPersonCount;
            if (item.score() > detectedPersonBestScore) {
                detectedPersonBestScore = item.score();
            }
            snprintf(label, sizeof(label), "PERSON %d%%", item.score());
            OSD.drawRect(STREAM_CHANNEL, xMin, yMin, xMax, yMax, 3, OSD_COLOR_GREEN);
            OSD.drawText(STREAM_CHANNEL, xMin, labelY, label, OSD_COLOR_CYAN);
        } else {
            bookDetectedNow = true;
            ++detectedBookCount;
            snprintf(label, sizeof(label), "BOOK %d%%", item.score());
            OSD.drawRect(STREAM_CHANNEL, xMin, yMin, xMax, yMax, 3, OSD_COLOR_CYAN);
            OSD.drawText(STREAM_CHANNEL, xMin, labelY, label, OSD_COLOR_WHITE);
        }
    }

    for (size_t i = 0; i < faces.size(); ++i) {
        FaceDetectionResult item = faces[i];
        if (item.score() < FACE_MIN_SCORE) {
            continue;
        }
        ++qualifiedFaceCount;
        if (item.score() > detectedFaceBestScore) {
            detectedFaceBestScore = item.score();
        }
        float area = (item.xMax() - item.xMin()) * (item.yMax() - item.yMin());
        if ((primaryFaceIndex < 0) || (area > primaryFaceArea)) {
            primaryFaceArea = area;
            primaryFaceIndex = (int)i;
        }
    }

    bool validPose = false;
    int measuredPitchScore = headPitchScore;
    if (primaryFaceIndex >= 0) {
        FaceDetectionResult face = faces[(size_t)primaryFaceIndex];
        int xMin = (int)(face.xMin() * imageWidth);
        int xMax = (int)(face.xMax() * imageWidth);
        int yMin = (int)(face.yMin() * imageHeight);
        int yMax = (int)(face.yMax() * imageHeight);
        OSD.drawRect(STREAM_CHANNEL, xMin, yMin, xMax, yMax, 3, OSD_COLOR_WHITE);

        for (int point = 0; point < 5; ++point) {
            int x = (int)(face.xFeature(point) * imageWidth);
            int y = (int)(face.yFeature(point) * imageHeight);
            OSD.drawPoint(STREAM_CHANNEL, x, y, 7, OSD_COLOR_RED);
        }

        float eyeX = (face.xFeature(0) + face.xFeature(1)) * 0.5f;
        float eyeY = (face.yFeature(0) + face.yFeature(1)) * 0.5f;
        float mouthX = (face.xFeature(3) + face.xFeature(4)) * 0.5f;
        float mouthY = (face.yFeature(3) + face.yFeature(4)) * 0.5f;
        float axisX = mouthX - eyeX;
        float axisY = mouthY - eyeY;
        float axisSquared = axisX * axisX + axisY * axisY;

        if (axisSquared >= 0.0001f) {
            float noseX = face.xFeature(2);
            float noseY = face.yFeature(2);
            float rawPitch = ((noseX - eyeX) * axisX +
                              (noseY - eyeY) * axisY) / axisSquared;
            if ((rawPitch >= -0.20f) && (rawPitch <= 1.20f)) {
                if (!pitchFilterReady) {
                    filteredPitchRatio = rawPitch;
                    pitchFilterReady = true;
                } else {
                    filteredPitchRatio = filteredPitchRatio * 0.75f + rawPitch * 0.25f;
                }
                measuredPitchScore = (int)(filteredPitchRatio * 100.0f + 0.5f);
                validPose = true;
            }
        }
    }

    unsigned long now = millis();
    updatePostureState(validPose, measuredPitchScore, now);

    faceCount = qualifiedFaceCount;
    bestFaceScore = detectedFaceBestScore;

    if (bookDetectedNow) {
        lastBookSeen = now;
        bookCount = detectedBookCount;
    } else if ((unsigned long)(now - lastBookSeen) > BOOK_HOLD_MS) {
        bookCount = 0;
    }
    bookPresent = bookDetectedNow ||
                  ((bookCount > 0) && ((unsigned long)(now - lastBookSeen) <= BOOK_HOLD_MS));

    bool presenceDetectedNow = personDetectedNow || (qualifiedFaceCount > 0);
    if (presenceDetectedNow) {
        hasSeenPerson = true;
        lastPersonSeen = now;
        personCount = (detectedPersonCount > 0) ? detectedPersonCount : qualifiedFaceCount;
        bestPersonScore = (detectedPersonBestScore > 0)
                              ? detectedPersonBestScore
                              : detectedFaceBestScore;
    } else if (!hasSeenPerson ||
               ((unsigned long)(now - lastPersonSeen) > PERSON_HOLD_MS)) {
        personCount = 0;
        bestPersonScore = 0;
    }
    personPresent = presenceDetectedNow ||
                    (hasSeenPerson &&
                     ((unsigned long)(now - lastPersonSeen) <= PERSON_HOLD_MS));

    char statusText[48];
    const char *stateText = attentionStateName();
    if (strcmp(stateText, "REST") == 0) {
        snprintf(statusText, sizeof(statusText), "REST");
        OSD.drawText(STREAM_CHANNEL, 20, 20, statusText, OSD_COLOR_CYAN);
    } else if (strcmp(stateText, "HEAD_DOWN") == 0) {
        snprintf(statusText, sizeof(statusText), bookPresent ? "HEAD DOWN + BOOK" : "HEAD DOWN");
        OSD.drawText(STREAM_CHANNEL, 20, 20, statusText, OSD_COLOR_GREEN);
    } else if (strcmp(stateText, "HEAD_UP") == 0) {
        snprintf(statusText, sizeof(statusText), "HEAD UP");
        OSD.drawText(STREAM_CHANNEL, 20, 20, statusText, OSD_COLOR_RED);
    } else if (personPresent) {
        snprintf(statusText, sizeof(statusText), "POSE UNKNOWN");
        OSD.drawText(STREAM_CHANNEL, 20, 20, statusText, OSD_COLOR_CYAN);
    } else {
        snprintf(statusText, sizeof(statusText), "NO PERSON");
        OSD.drawText(STREAM_CHANNEL, 20, 20, statusText, OSD_COLOR_RED);
    }
    OSD.update(STREAM_CHANNEL);

    bool focusedNow = headPoseValid && (stablePosture == POSTURE_HEAD_DOWN);
    digitalWrite(LED_BUILTIN, focusedNow ? HIGH : LOW);

    static int previousState = -1;
    int currentState = !personPresent ? 0 :
                       (!headPoseValid || (stablePosture == POSTURE_UNKNOWN)) ? 1 :
                       (stablePosture == POSTURE_HEAD_DOWN) ? 2 : 3;
    if (currentState != previousState) {
        Serial.print("ATTENTION=");
        Serial.print(attentionStateName());
        Serial.print(" PEOPLE=");
        Serial.print(personCount);
        Serial.print(" FACES=");
        Serial.print(faceCount);
        Serial.print(" BOOK=");
        Serial.print(bookPresent ? "YES" : "NO");
        Serial.print(" PITCH=");
        Serial.println(headPitchScore);
        previousState = currentState;
    }
}

void updateWiFiConnection()
{
    bool connected = hasUsableWiFi();
    unsigned long now = millis();

    if (connected && !wifiWasConnected) {
        wifiWasConnected = true;
        WiFi.disablePowerSave();
        dashboardServer.begin();
        snapshotServer.begin();
        Serial.println("Wi-Fi reconnected.");
        printStreamingInfo();
    } else if (!connected && wifiWasConnected) {
        wifiWasConnected = false;
        dashboardServer.stop();
        snapshotServer.stop();
        Serial.println("Wi-Fi disconnected; retrying in the background.");
    }

    if (!connected &&
        ((unsigned long)(now - lastReconnectAttempt) >= 10000UL)) {
        // The Wi-Fi driver owns reconnection; only report progress here so a
        // second WiFi.begin() cannot race the driver's active join attempt.
        Serial.println("Waiting for Wi-Fi auto reconnect...");
        lastReconnectAttempt = now;
    }
}

void loop()
{
    static unsigned long lastVisionUpdate = 0;
    unsigned long now = millis();

    updateTimer();
    updateWiFiConnection();

    if ((unsigned long)(now - lastVisionUpdate) >= 150UL) {
        lastVisionUpdate = now;
        updateVisionDetection();
    }

    delay(5);
}
