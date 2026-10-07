#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <unistd.h>
#include <dlfcn.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>

#define LOG_TAG "OxideCheat"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// ==================== ОФФСЕТЫ ====================
#define OFF_PLAYER_LIST       0x10
#define OFF_NETWORK_CLIENT    0x48
#define OFF_PLAYER_CAMERA     0x68
#define OFF_PLAYER_TEAM       0x120
#define OFF_PLAYER_LOOK       0x1C8

// ==================== RVA ====================
#define RVA_GET_TRANSFORM     0xCB6FE80
#define RVA_GET_POSITION      0xCB89588
#define RVA_GET_WORLD_CAMERA  0xCAF2F44
#define RVA_WORLD_TO_SCREEN   0xCAF3928

// ==================== ТИПЫ ====================
struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct Matrix4x4 { float m[16]; };

template<typename T> T Read(uintptr_t addr) { return *(T*)addr; }
template<typename T> void Write(uintptr_t addr, T value) { *(T*)addr = value; }

uintptr_t baseAddr = 0;
bool aimbotEnabled = false;
bool espEnabled = false;
float aimbotFOV = 90.0f;

// ==================== УКАЗАТЕЛИ ====================
typedef void* (*GetTransform_t)(void*);
typedef Vec3 (*GetPosition_t)(void*);
typedef Matrix4x4 (*GetWorldToCamera_t)(void*);
typedef Vec2 (*WorldToScreen_t)(void*, Vec3);

GetTransform_t GetTransform = nullptr;
GetPosition_t GetPosition = nullptr;
GetWorldToCamera_t GetWorldToCamera = nullptr;
WorldToScreen_t WorldToScreen = nullptr;

// ==================== EGL ====================
EGLDisplay eglDisplay = EGL_NO_DISPLAY;
EGLSurface eglSurface = EGL_NO_SURFACE;
EGLContext eglContext = EGL_NO_CONTEXT;
int screenW = 1080, screenH = 2400;
GLuint shaderProgram = 0;

const char* vsSrc =
    "attribute vec4 vPosition; attribute vec4 vColor; varying vec4 fColor;"
    "void main() { gl_Position = vPosition; fColor = vColor; }";
const char* fsSrc =
    "precision mediump float; varying vec4 fColor;"
    "void main() { gl_FragColor = fColor; }";

GLuint LoadShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

void InitGL() {
    GLuint vs = LoadShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = LoadShader(GL_FRAGMENT_SHADER, fsSrc);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);
}

void InitEGL(ANativeWindow* window) {
    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eglDisplay, nullptr, nullptr);
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint numConfigs;
    eglChooseConfig(eglDisplay, attribs, &config, 1, &numConfigs);
    EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
    eglSurface = eglCreateWindowSurface(eglDisplay, config, window, nullptr);
    eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
    InitGL();
}

void DrawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    float l = (x / screenW) * 2.0f - 1.0f;
    float rr = ((x + w) / screenW) * 2.0f - 1.0f;
    float t = 1.0f - (y / screenH) * 2.0f;
    float bt = 1.0f - ((y + h) / screenH) * 2.0f;
    float v[] = {
        l, t, r, g, b, a,
        rr, t, r, g, b, a,
        l, bt, r, g, b, a,
        rr, bt, r, g, b, a
    };
    GLint pos = glGetAttribLocation(shaderProgram, "vPosition");
    GLint col = glGetAttribLocation(shaderProgram, "vColor");
    glVertexAttribPointer(pos, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), v);
    glVertexAttribPointer(col, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), v + 2);
    glEnableVertexAttribArray(pos);
    glEnableVertexAttribArray(col);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

// ==================== ИГРОКИ ====================
std::vector<uintptr_t> GetPlayerList() {
    std::vector<uintptr_t> players;
    uintptr_t listPtr = Read<uintptr_t>(baseAddr + OFF_PLAYER_LIST);
    if (!listPtr) return players;
    uintptr_t items = Read<uintptr_t>(listPtr + 0x10);
    int size = Read<int>(listPtr + 0x18);
    for (int i = 0; i < size && i < 100; i++) {
        uintptr_t p = Read<uintptr_t>(items + i * 8);
        if (p) players.push_back(p);
    }
    return players;
}

// ==================== AIMBOT ====================
void Aimbot(uintptr_t localPlayer) {
    uintptr_t bestTarget = 0;
    float bestDist = 999999.0f;
    Vec3 localPos = GetPosition(GetTransform((void*)localPlayer));
    uintptr_t localTeam = Read<uintptr_t>(localPlayer + OFF_PLAYER_TEAM);
    
    for (auto p : GetPlayerList()) {
        if (p == localPlayer) continue;
        uintptr_t team = Read<uintptr_t>(p + OFF_PLAYER_TEAM);
        if (team == localTeam) continue;
        Vec3 enemyPos = GetPosition(GetTransform((void*)p));
        float dist = sqrt(pow(enemyPos.x - localPos.x, 2) + pow(enemyPos.y - localPos.y, 2) + pow(enemyPos.z - localPos.z, 2));
        if (dist < bestDist) { bestDist = dist; bestTarget = p; }
    }
    
    if (bestTarget) {
        Vec3 targetPos = GetPosition(GetTransform((void*)bestTarget));
        float dx = targetPos.x - localPos.x;
        float dy = targetPos.y - localPos.y;
        float yaw = atan2(dy, dx) * 180.0f / M_PI;
        Write<float>(localPlayer + OFF_PLAYER_LOOK, yaw);
    }
}

// ==================== ESP ====================
void DrawESP(uintptr_t localPlayer) {
    uintptr_t camera = Read<uintptr_t>(localPlayer + OFF_PLAYER_CAMERA);
    if (!camera) return;
    Matrix4x4 vm = GetWorldToCamera((void*)camera);
    uintptr_t localTeam = Read<uintptr_t>(localPlayer + OFF_PLAYER_TEAM);
    
    for (auto p : GetPlayerList()) {
        if (p == localPlayer) continue;
        uintptr_t team = Read<uintptr_t>(p + OFF_PLAYER_TEAM);
        if (team == localTeam) continue;
        Vec3 pos = GetPosition(GetTransform((void*)p));
        Vec2 screen = WorldToScreen((void*)camera, pos);
        if (screen.x > 0 && screen.x < screenW && screen.y > 0 && screen.y < screenH) {
            DrawRect(screen.x - 25, screen.y - 50, 50, 100, 1.0f, 0.0f, 0.0f, 1.0f);
        }
    }
}

// ==================== JNI ====================
extern "C" JNIEXPORT void JNICALL
Java_com_oxide_cheat_MainActivity_initCheat(JNIEnv* env, jobject thiz, jobject surface) {
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    InitEGL(window);
    void* handle = dlopen("libil2cpp.so", RTLD_LAZY);
    if (handle) {
        baseAddr = (uintptr_t)handle;
        GetTransform = (GetTransform_t)(baseAddr + RVA_GET_TRANSFORM);
        GetPosition = (GetPosition_t)(baseAddr + RVA_GET_POSITION);
        GetWorldToCamera = (GetWorldToCamera_t)(baseAddr + RVA_GET_WORLD_CAMERA);
        WorldToScreen = (WorldToScreen_t)(baseAddr + RVA_WORLD_TO_SCREEN);
        LOGI("Functions loaded");
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_oxide_cheat_MainActivity_startCheat(JNIEnv* env, jobject thiz) {
    while (true) {
        uintptr_t localPlayer = Read<uintptr_t>(baseAddr + OFF_NETWORK_CLIENT);
        if (localPlayer) {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            if (aimbotEnabled) Aimbot(localPlayer);
            if (espEnabled) DrawESP(localPlayer);
            eglSwapBuffers(eglDisplay, eglSurface);
        }
        usleep(16000);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_oxide_cheat_MainActivity_setAimbot(JNIEnv* env, jobject thiz, jboolean enabled) {
    aimbotEnabled = enabled;
}

extern "C" JNIEXPORT void JNICALL
Java_com_oxide_cheat_MainActivity_setESP(JNIEnv* env, jobject thiz, jboolean enabled) {
    espEnabled = enabled;
}

extern "C" JNIEXPORT void JNICALL
Java_com_oxide_cheat_MainActivity_setFOV(JNIEnv* env, jobject thiz, jfloat fov) {
    aimbotFOV = fov;
}
