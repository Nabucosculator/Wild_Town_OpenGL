//
//  main.cpp
//  Proiect Final - Wild Town (fereastra + texturi + ground clamp + skybox + toggle ceata + muzica)
//

#if defined (__APPLE__)
#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.hpp"
#include "Model3D.hpp"
#include "Camera.hpp"
#include "SkyBox.hpp"

#include <iostream>
#include <string>
#include <vector>

// =========================
// MUZICA (Windows PlaySound)
// Toggle cu M
// =========================
#ifdef _WIN32
// IMPORTANT: previne ca headerele Windows sa defineasca macro-urile min/max (strica glm::max)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// siguranta extra (in caz ca vreun header tot le defineste)
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif


static bool musicEnabled = false;

static void startMusic()
{
#ifdef _WIN32
    // Necesita: assets\audio\ambient.wav
    PlaySoundA("assets\\audio\\ambient.wav", NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
#endif
}

static void stopMusic()
{
#ifdef _WIN32
    PlaySoundA(NULL, NULL, 0);
#endif
}

int glWindowWidth = 1500;
int glWindowHeight = 800;
int retina_width, retina_height;
GLFWwindow* glWindow = NULL;

gps::Model3D wildTown;

// Globale pentru SKYBOX
gps::SkyBox skybox;
gps::Shader skyboxShader;

glm::mat4 model;
GLuint modelLoc;
glm::mat4 view;
GLuint viewLoc;
glm::mat4 projection;
GLuint projectionLoc;
glm::mat3 normalMatrix;
GLuint normalMatrixLoc;

// =========================
// UMBRE
// =========================
gps::Shader shadowShader;

const unsigned int SHADOW_W = 2048;
const unsigned int SHADOW_H = 2048;

GLuint shadowFBO = 0;
GLuint shadowDepthTex = 0;

glm::mat4 lightSpaceMatrix;
GLint lightSpaceMatrixLoc = -1;   // in shader-ul de scena
GLint shadowMapLoc = -1;          // in shader-ul de scena
GLint enableShadowsLoc = -1;      // in shader-ul de scena

bool enableShadows = true;

// Lumina directionala (WORLD)
glm::vec3 lightDir;
GLint lightDirLoc = -1;
glm::vec3 lightColor;
GLint lightColorLoc = -1;

// =========================
// MAI MULTE SURSE PUNCTUALE (lampi)
// =========================
static const int MAX_POINT_LIGHTS = 8;

int numPointLights = 4;
glm::vec3 pointLightPosWorld[MAX_POINT_LIGHTS];
glm::vec3 pointLightColorArr[MAX_POINT_LIGHTS];

GLint numPointLightsLoc = -1;
GLint pointLightPosEyeLocArr[MAX_POINT_LIGHTS]{};
GLint pointLightColorLocArr[MAX_POINT_LIGHTS]{};

// =========================
// NOU: pozitia initiala a camerei (din valorile printate de tine)
// =========================
static const glm::vec3 kStartCamPos = glm::vec3(-120.593f, 3.84375f, -217.279f);
static const glm::vec3 kStartCamFront = glm::vec3(0.982691f, 0.0948037f, -0.159156f);
static const glm::vec3 kStartCamTarget = kStartCamPos + kStartCamFront;

// Pastreaza turul ancorat in jurul acestei pozitii initiale (ca preview-ul cu P sa inceapa "aici")
static const glm::vec3 kTourAnchorPos = kStartCamPos;
static const glm::vec3 kTourAnchorTarget = kStartCamTarget;

// NOTA: constructorul camerei foloseste (position, target, up)
gps::Camera myCamera(
    kStartCamPos,
    kStartCamTarget,
    glm::vec3(0.0f, 1.0f, 0.0f)
);

bool pressedKeys[1024]{};
float mouseSensitivity = 0.08f;
bool firstMouse = true;
double lastX = 0.0, lastY = 0.0;

float walkSpeed = 0.25f;
float flySpeedY = 0.6f;
float turboMult = 4.0f;

float playerRadius = 0.01f;

bool lockToHumanHeight = true;
float eyeHeight = 1.8f;
float groundSnapEps = 0.05f;

gps::Shader sceneShader;

// =========================
// CEATA (stil Silent Hill)
// =========================
glm::vec3 fogColor = glm::vec3(0.68f, 0.65f, 0.72f);
float fogStart = 15.0f;
float fogEnd = 120.0f;

// Optional: skybox "inghitit" de ceata (0..1)
float skyboxFog = 1.0f;

// Locatii uniforme pentru ceata (shader-ul de scena)
GLint fogColorLoc = -1;
GLint fogStartLoc = -1;
GLint fogEndLoc = -1;

// Uniforma de activare ceata (0/1) (shader-ul de scena)
GLint fogEnabledLoc = -1;

// =========================
// TOGGLE CEATA + SKYBOX (tasta 3)
// 3: ceata ON + skybox OFF
// 3 din nou: ceata OFF + skybox ON
// =========================
bool fogEnabled = false;     // start: fara ceata
bool skyboxEnabled = true;   // start: skybox ON

// Locatii uniforme pentru ceata in skybox
GLint skyboxFogColorLoc = -1;
GLint skyboxFogLoc = -1;

// =========================
// Controale transformare scena
// I/J/K/L translatie, Q/E rotatie, Z/X scale, P auto demo
// =========================
glm::vec3 sceneTranslate(0.0f, 0.0f, 0.0f);
float sceneYawDeg = 0.0f;
float sceneScale = 0.1f;

float sceneTranslateStep = 0.30f;
float sceneRotateStepDeg = 2.0f;
float sceneScaleStep = 0.01f;

bool autoDemo = false;
double lastFrameTime = 0.0;
float autoOrbitAngleDeg = 0.0f;
float autoOrbitSpeedDeg = 18.0f;     // grade/secunda
float autoSceneYawSpeedDeg = 6.0f;   // grade/secunda

// =========================
// TUR CINEMATIC (P)
// Turul camerei prin mai multe keyframe-uri (pos + target + durata)
// =========================
struct CamKeyframe {
    glm::vec3 pos;
    glm::vec3 target;
    float duration; // secunde pentru segmentul A->B
};

// =========================
// NOU: turul incepe din pozitia camerei aleasa de tine (ancora),
// apoi face un mic "preview" local in zona respectiva.
// =========================
static std::vector<CamKeyframe> gTour = {
    // Start exact unde vrei
    { kTourAnchorPos,                               kTourAnchorTarget,                               0.01f },

    // Impingere usoara in fata (in directia privirii)
    { kTourAnchorPos + kStartCamFront * 8.0f,       kTourAnchorTarget + kStartCamFront * 10.0f,      3.5f },

    // Deplasare usoara la stanga
    { kTourAnchorPos + glm::vec3(-8.0f, 1.0f, 0.0f),kTourAnchorTarget + glm::vec3(-6.0f, 0.5f, 0.0f),4.0f },

    // Deplasare usoara la dreapta
    { kTourAnchorPos + glm::vec3(8.0f, 1.0f, 0.0f), kTourAnchorTarget + glm::vec3(6.0f, 0.5f, 0.0f),4.0f },

    // Retragere + putin mai sus pentru un cadru mai larg
    { kTourAnchorPos + glm::vec3(0.0f, 6.0f, 18.0f),kTourAnchorTarget + glm::vec3(0.0f, 2.0f, 10.0f),5.0f }
};

static int   gTourIndex = 0;
static float gTourT = 0.0f;

static float smooth01(float t)
{
    // smoothstep(0,1,t)
    t = glm::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// =========================
// Toggle-uri lumini
// 1 = toggle directional, 2 = toggle point (lampi)
// H = toggle umbre
// =========================
bool enableDirLight = true;
bool enablePointLight = true;

// =========================
// MODURI RANDARE (cerinte)
// 4 = SOLID (fill)
// 5 = WIREFRAME
// 6 = POLYGONAL (points)
// 7 = SMOOTH (activeaza hint-uri de netezire)
// IMPORTANT: nu suprascrie taste existente.
// =========================
enum RenderMode {
    RM_SOLID = 0,
    RM_WIREFRAME = 1,
    RM_POINTS = 2
};

static RenderMode gRenderMode = RM_SOLID;
static bool gSmoothEnabled = false;

static void applySmoothState(bool enabled)
{
    if (enabled) {
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

        // Optional (poate fi ignorat pe unele drivere / contexte core)
        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    }
    else {
        glDisable(GL_LINE_SMOOTH);
        glDisable(GL_POLYGON_SMOOTH);
    }
}

static void applyRenderModeForNormalPass()
{
    switch (gRenderMode) {
    case RM_SOLID:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
    case RM_WIREFRAME:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        break;
    case RM_POINTS:
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
        break;
    default:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
    }

    applySmoothState(gSmoothEnabled);
}

// =========================
// Helper-e pentru UMBRE
// =========================
static void initShadowMap()
{
    glGenFramebuffers(1, &shadowFBO);

    glGenTextures(1, &shadowDepthTex);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        SHADOW_W, SHADOW_H, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.f, 1.f, 1.f, 1.f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// LUMINA: sus -> jos, setata fata de centrul scenei (nu dupa camera)
static void computeLightSpaceMatrix()
{
    glm::vec3 Lrays = glm::normalize(lightDir);

    glm::vec3 camPos = myCamera.getPosition();
    glm::vec3 center(camPos.x, 0.0f, camPos.z);

    float lightDist = 120.0f;
    glm::vec3 lightPos = center - Lrays * lightDist;

    glm::mat4 lightView = glm::lookAt(lightPos, center, glm::vec3(0, 1, 0));

    float orthoSize = 220.0f;
    float nearPlane = 1.0f;
    float farPlane = 600.0f;

    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize,
        -orthoSize, orthoSize,
        nearPlane, farPlane);

    lightSpaceMatrix = lightProj * lightView;
}

// helper: seteaza fog uniforms pentru scena (sceneShader)
static void setSceneFogUniforms()
{
    sceneShader.useShaderProgram();
    if (fogColorLoc != -1) glUniform3fv(fogColorLoc, 1, glm::value_ptr(fogColor));
    if (fogStartLoc != -1) glUniform1f(fogStartLoc, fogStart);
    if (fogEndLoc != -1)   glUniform1f(fogEndLoc, fogEnd);

    if (fogEnabledLoc != -1) glUniform1i(fogEnabledLoc, fogEnabled ? 1 : 0);
}

// helper: seteaza fog uniforms pentru skybox (skyboxShader)
static void setSkyboxFogUniforms()
{
    skyboxShader.useShaderProgram();
    if (skyboxFogColorLoc != -1) glUniform3fv(skyboxFogColorLoc, 1, glm::value_ptr(fogColor));
    if (skyboxFogLoc != -1)      glUniform1f(skyboxFogLoc, skyboxFog);
}

// helper: recalculeaza model = T * R * S si trimite model + normalMatrix
static void rebuildModelAndSend()
{
    model = glm::mat4(1.0f);
    model = glm::translate(model, sceneTranslate);
    model = glm::rotate(model, glm::radians(sceneYawDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(sceneScale));

    sceneShader.useShaderProgram();
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    view = myCamera.getViewMatrix();
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
}

static void updateViewRelatedUniforms()
{
    view = myCamera.getViewMatrix();
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));

    sceneShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    glm::vec3 lightDirWorldToLight = -lightDir;
    glm::vec3 lightDirEye = glm::inverseTranspose(glm::mat3(view)) * lightDirWorldToLight;
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDirEye));

    glm::vec3 dirColor = enableDirLight ? lightColor : glm::vec3(0.0f);
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(dirColor));

    int activeCount = enablePointLight ? numPointLights : 0;
    if (numPointLightsLoc != -1) glUniform1i(numPointLightsLoc, activeCount);

    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        if (pointLightPosEyeLocArr[i] == -1 || pointLightColorLocArr[i] == -1) continue;

        if (i >= activeCount) {
            glUniform3f(pointLightPosEyeLocArr[i], 0.0f, 0.0f, 0.0f);
            glUniform3f(pointLightColorLocArr[i], 0.0f, 0.0f, 0.0f);
            continue;
        }

        glm::vec3 pEye = glm::vec3(view * glm::vec4(pointLightPosWorld[i], 1.0f));
        glUniform3fv(pointLightPosEyeLocArr[i], 1, glm::value_ptr(pEye));
        glUniform3fv(pointLightColorLocArr[i], 1, glm::value_ptr(pointLightColorArr[i]));
    }

    if (enableShadowsLoc != -1) {
        glUniform1i(enableShadowsLoc, enableShadows ? 1 : 0);
    }
}

void windowResizeCallback(GLFWwindow* window, int width, int height)
{
    glfwGetFramebufferSize(window, &retina_width, &retina_height);

    projection = glm::perspective(
        glm::radians(60.0f),
        (float)retina_width / (float)retina_height,
        0.1f,
        20000.0f
    );

    sceneShader.useShaderProgram();
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    skyboxShader.useShaderProgram();
    GLuint skyboxProjLoc = glGetUniformLocation(skyboxShader.shaderProgram, "projection");
    glUniformMatrix4fv(skyboxProjLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    // P = toggle tur cinematic (autoDemo)
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        autoDemo = !autoDemo;

        // reseteaza turul cand il activezi
        if (autoDemo) {
            gTourIndex = 0;
            gTourT = 0.0f;

            // NOU: forteaza camera pe pozitia de start/ancora cand incepe preview-ul
            myCamera.setPosition(kTourAnchorPos);
            updateViewRelatedUniforms();
        }
    }

    if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
        enableDirLight = !enableDirLight;
        updateViewRelatedUniforms();
    }
    if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
        enablePointLight = !enablePointLight;
        updateViewRelatedUniforms();
    }

    // TOGGLE UMBRE
    if (key == GLFW_KEY_H && action == GLFW_PRESS) {
        enableShadows = !enableShadows;
        sceneShader.useShaderProgram();
        if (enableShadowsLoc != -1) glUniform1i(enableShadowsLoc, enableShadows ? 1 : 0);
    }

    // TOGGLE CEATA <-> SKYBOX (3)
    if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
        fogEnabled = !fogEnabled;
        skyboxEnabled = !fogEnabled; // cand ceata ON -> skybox OFF

        skyboxFog = fogEnabled ? 1.0f : 0.0f;

        sceneShader.useShaderProgram();
        if (fogEnabledLoc != -1) glUniform1i(fogEnabledLoc, fogEnabled ? 1 : 0);

        setSkyboxFogUniforms();
    }

    // TOGGLE MUZICA (M)
    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        musicEnabled = !musicEnabled;
        if (musicEnabled) startMusic();
        else stopMusic();
    }

    // =========================
    // MODURI RANDARE
    // 4 = solid, 5 = wireframe, 6 = polygonal(points), 7 = toggle smooth
    // =========================
    if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
        gRenderMode = RM_SOLID;
    }
    if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
        gRenderMode = RM_WIREFRAME;
    }
    if (key == GLFW_KEY_6 && action == GLFW_PRESS) {
        gRenderMode = RM_POINTS;
    }
    if (key == GLFW_KEY_7 && action == GLFW_PRESS) {
        gSmoothEnabled = !gSmoothEnabled;
    }

    // =========================
    // Tasta 8 -> printeaza pozitia camerei (si directia) in consola
    // =========================
    if (key == GLFW_KEY_8 && action == GLFW_PRESS) {
        glm::vec3 p = myCamera.getPosition();
        glm::vec3 f = myCamera.getFront();
        glm::vec3 t = p + f;

        std::cout
            << "\n[CAM] position = (" << p.x << ", " << p.y << ", " << p.z << ")"
            << " | front = (" << f.x << ", " << f.y << ", " << f.z << ")"
            << " | target = (" << t.x << ", " << t.y << ", " << t.z << ")\n";
    }

    if (key >= 0 && key < 1024)
    {
        if (action == GLFW_PRESS) pressedKeys[key] = true;
        else if (action == GLFW_RELEASE) pressedKeys[key] = false;
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse) {
        firstMouse = false;
        lastX = xpos;
        lastY = ypos;
        return;
    }

    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    float yawDelta = (float)xoffset * mouseSensitivity;
    float pitchDelta = (float)yoffset * mouseSensitivity;

    myCamera.rotate(pitchDelta, yawDelta);
    updateViewRelatedUniforms();
}

void applyGroundClamp()
{
    glm::vec3 pos = myCamera.getPosition();

    float groundY;
    if (wildTown.getGroundHeightAtWorldXZ(model, pos.x, pos.z, groundY)) {
        float minY = groundY + eyeHeight;
        if (pos.y < minY + groundSnapEps) {
            pos.y = minY;
            myCamera.setPosition(pos);
        }
    }
}

// Demo vechi de orbitare (pastrat, dar nu mai este folosit cand autoDemo este activ)
static void runAutoDemo(float dt)
{
    sceneYawDeg += autoSceneYawSpeedDeg * dt;
    autoOrbitAngleDeg += autoOrbitSpeedDeg * dt;

    float a = glm::radians(autoOrbitAngleDeg);
    float radius = 10.0f;
    float y = 2.2f;

    glm::vec3 center = glm::vec3(0.0f, y, 0.0f);

    glm::vec3 camPos;
    camPos.x = center.x + cos(a) * radius;
    camPos.z = center.z + sin(a) * radius;
    camPos.y = y;

    myCamera.setPosition(camPos);

    glm::vec3 toCenter = glm::normalize(center - camPos);
    float targetYaw = glm::degrees(atan2(toCenter.z, toCenter.x));
    float targetPitch = glm::degrees(asin(toCenter.y));

    float k = glm::clamp(dt * 8.0f, 0.0f, 1.0f);

    glm::vec3 curFront = myCamera.getFront();
    float curYaw = glm::degrees(atan2(curFront.z, curFront.x));
    float curPitch = glm::degrees(asin(curFront.y));

    float yawDelta = (targetYaw - curYaw) * k;
    float pitchDelta = (targetPitch - curPitch) * k;

    myCamera.rotate(pitchDelta, yawDelta);

    rebuildModelAndSend();
    updateViewRelatedUniforms();
}

// Demo tur cinematic (folosit cand autoDemo este activ)
static void runCinematicTour(float dt)
{
    if (gTour.size() < 2) return;

    const CamKeyframe& A = gTour[gTourIndex];
    const CamKeyframe& B = gTour[(gTourIndex + 1) % (int)gTour.size()];

    gTourT += dt;
    float segDur = glm::max(0.001f, A.duration);
    float u = gTourT / segDur;
    float s = smooth01(u);

    glm::vec3 pos = glm::mix(A.pos, B.pos, s);
    glm::vec3 target = glm::mix(A.target, B.target, s);

    myCamera.setPosition(pos);

    // orienteaza camera catre target folosind delta-uri de yaw/pitch
    glm::vec3 dir = glm::normalize(target - pos);

    float desiredYaw = glm::degrees(atan2(dir.z, dir.x));
    float desiredPitch = glm::degrees(asin(dir.y));

    glm::vec3 curFront = myCamera.getFront();
    float curYaw = glm::degrees(atan2(curFront.z, curFront.x));
    float curPitch = glm::degrees(asin(curFront.y));

    float yawDelta = desiredYaw - curYaw;
    float pitchDelta = desiredPitch - curPitch;

    // amortizeaza ca sa nu "sara" brusc
    float k = glm::clamp(dt * 6.0f, 0.0f, 1.0f);
    myCamera.rotate(pitchDelta * k, yawDelta * k);

    rebuildModelAndSend();
    updateViewRelatedUniforms();

    if (u >= 1.0f) {
        gTourIndex = (gTourIndex + 1) % (int)gTour.size();
        gTourT = 0.0f;
    }
}

void processMovement()
{
    static bool lastT = false;
    bool nowT = pressedKeys[GLFW_KEY_T];
    if (nowT && !lastT) lockToHumanHeight = !lockToHumanHeight;
    lastT = nowT;

    bool sceneChanged = false;

    if (pressedKeys[GLFW_KEY_I]) { sceneTranslate.z -= sceneTranslateStep; sceneChanged = true; }
    if (pressedKeys[GLFW_KEY_K]) { sceneTranslate.z += sceneTranslateStep; sceneChanged = true; }
    if (pressedKeys[GLFW_KEY_J]) { sceneTranslate.x -= sceneTranslateStep; sceneChanged = true; }
    if (pressedKeys[GLFW_KEY_L]) { sceneTranslate.x += sceneTranslateStep; sceneChanged = true; }

    if (pressedKeys[GLFW_KEY_Q]) { sceneYawDeg += sceneRotateStepDeg; sceneChanged = true; }
    if (pressedKeys[GLFW_KEY_E]) { sceneYawDeg -= sceneRotateStepDeg; sceneChanged = true; }

    if (pressedKeys[GLFW_KEY_Z]) {
        sceneScale = glm::max(0.01f, sceneScale - sceneScaleStep);
        sceneChanged = true;
    }
    if (pressedKeys[GLFW_KEY_X]) {
        sceneScale += sceneScaleStep;
        sceneChanged = true;
    }

    if (sceneChanged) {
        rebuildModelAndSend();
        updateViewRelatedUniforms();
    }

    if (autoDemo) return;

    float speedMult = pressedKeys[GLFW_KEY_LEFT_SHIFT] ? turboMult : 1.0f;
    float moveSpeed = walkSpeed * speedMult;

    bool moved = false;

    if (pressedKeys[GLFW_KEY_W]) { myCamera.move(gps::MOVE_FORWARD, moveSpeed); moved = true; }
    if (pressedKeys[GLFW_KEY_S]) { myCamera.move(gps::MOVE_BACKWARD, moveSpeed); moved = true; }
    if (pressedKeys[GLFW_KEY_A]) { myCamera.move(gps::MOVE_LEFT, moveSpeed); moved = true; }
    if (pressedKeys[GLFW_KEY_D]) { myCamera.move(gps::MOVE_RIGHT, moveSpeed); moved = true; }

    if (!lockToHumanHeight) {
        glm::vec3 pos = myCamera.getPosition();
        if (pressedKeys[GLFW_KEY_SPACE]) { pos.y += flySpeedY * speedMult; moved = true; }
        if (pressedKeys[GLFW_KEY_LEFT_CONTROL]) { pos.y -= flySpeedY * speedMult; moved = true; }
        myCamera.setPosition(pos);
    }

    {
        glm::vec3 pos = myCamera.getPosition();
        wildTown.resolveSphereCollisions(model, pos, playerRadius);
        myCamera.setPosition(pos);
    }

    if (lockToHumanHeight) {
        applyGroundClamp();
    }

    if (moved) updateViewRelatedUniforms();
}

bool initOpenGLWindow()
{
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    glWindow = glfwCreateWindow(glWindowWidth, glWindowHeight, "Project Final - Wild Town", NULL, NULL);
    if (!glWindow) {
        glfwTerminate();
        return false;
    }

    glfwSetWindowSizeCallback(glWindow, windowResizeCallback);
    glfwSetKeyCallback(glWindow, keyboardCallback);
    glfwSetCursorPosCallback(glWindow, mouseCallback);

    glfwSetInputMode(glWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwMakeContextCurrent(glWindow);
    glfwSwapInterval(1);

#if not defined (__APPLE__)
    glewExperimental = GL_TRUE;
    glewInit();
#endif

    glfwGetFramebufferSize(glWindow, &retina_width, &retina_height);
    return true;
}

void initObjects()
{
    wildTown.LoadModel("models/wild_town/wild_town.obj");

    std::vector<const GLchar*> faces = {
        "skybox/posx.jpg",
        "skybox/negx.jpg",
        "skybox/posy.jpg",
        "skybox/negy.jpg",
        "skybox/posz.jpg",
        "skybox/negz.jpg"
    };
    skybox.Load(faces);
}

void initShaders()
{
    sceneShader.loadShader("shaders/shaderPPL.vert", "shaders/shaderPPL.frag");
    sceneShader.useShaderProgram();

    skyboxShader.loadShader("shaders/skyboxShader.vert", "shaders/skyboxShader.frag");
    skyboxShader.useShaderProgram();

    shadowShader.loadShader("shaders/shadowDepth.vert", "shaders/shadowDepth.frag");
}

void initUniforms()
{
    sceneTranslate = glm::vec3(0.0f, 0.0f, 0.0f);
    sceneYawDeg = 0.0f;
    sceneScale = 0.1f;

    view = myCamera.getViewMatrix();
    projection = glm::perspective(
        glm::radians(60.0f),
        (float)retina_width / (float)retina_height,
        0.1f,
        20000.0f
    );

    lightDir = glm::normalize(glm::vec3(-0.2f, -1.0f, -0.35f));
    lightColor = glm::vec3(1.0f);

    numPointLights = 1;
    pointLightPosWorld[0] = glm::vec3(-4.0f, 21.5f, -25.0f);
    pointLightColorArr[0] = glm::vec3(1.0f, 0.85f, 0.45f);

    sceneShader.useShaderProgram();

    modelLoc = glGetUniformLocation(sceneShader.shaderProgram, "model");
    viewLoc = glGetUniformLocation(sceneShader.shaderProgram, "view");
    projectionLoc = glGetUniformLocation(sceneShader.shaderProgram, "projection");
    normalMatrixLoc = glGetUniformLocation(sceneShader.shaderProgram, "normalMatrix");

    lightDirLoc = glGetUniformLocation(sceneShader.shaderProgram, "lightDir");
    lightColorLoc = glGetUniformLocation(sceneShader.shaderProgram, "lightColor");

    fogColorLoc = glGetUniformLocation(sceneShader.shaderProgram, "fogColor");
    fogStartLoc = glGetUniformLocation(sceneShader.shaderProgram, "fogStart");
    fogEndLoc = glGetUniformLocation(sceneShader.shaderProgram, "fogEnd");

    fogEnabledLoc = glGetUniformLocation(sceneShader.shaderProgram, "fogEnabled");
    if (fogEnabledLoc != -1) glUniform1i(fogEnabledLoc, fogEnabled ? 1 : 0);

    numPointLightsLoc = glGetUniformLocation(sceneShader.shaderProgram, "numPointLights");
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        std::string posName = "pointLightPosEye[" + std::to_string(i) + "]";
        std::string colName = "pointLightColor[" + std::to_string(i) + "]";
        pointLightPosEyeLocArr[i] = glGetUniformLocation(sceneShader.shaderProgram, posName.c_str());
        pointLightColorLocArr[i] = glGetUniformLocation(sceneShader.shaderProgram, colName.c_str());
    }

    lightSpaceMatrixLoc = glGetUniformLocation(sceneShader.shaderProgram, "lightSpaceMatrix");
    shadowMapLoc = glGetUniformLocation(sceneShader.shaderProgram, "shadowMap");
    enableShadowsLoc = glGetUniformLocation(sceneShader.shaderProgram, "enableShadows");

    if (shadowMapLoc != -1) glUniform1i(shadowMapLoc, 3);
    if (enableShadowsLoc != -1) glUniform1i(enableShadowsLoc, enableShadows ? 1 : 0);

    rebuildModelAndSend();

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    setSceneFogUniforms();
    updateViewRelatedUniforms();

    skyboxShader.useShaderProgram();
    skyboxFogColorLoc = glGetUniformLocation(skyboxShader.shaderProgram, "fogColor");
    skyboxFogLoc = glGetUniformLocation(skyboxShader.shaderProgram, "skyboxFog");
    setSkyboxFogUniforms();
}

void initOpenGLState()
{
    glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
    glViewport(0, 0, retina_width, retina_height);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

// =========================
// RANDARE (2 PASS: umbre + normal)
// =========================
void renderScene()
{
    // 1) PASS UMBRE
    // Forteaza solid in pass-ul de umbre (wireframe/points ar strica depth map-ul)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    computeLightSpaceMatrix();

    glViewport(0, 0, SHADOW_W, SHADOW_H);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    shadowShader.useShaderProgram();
    GLint lsLoc = glGetUniformLocation(shadowShader.shaderProgram, "lightSpaceMatrix");
    GLint mLoc = glGetUniformLocation(shadowShader.shaderProgram, "model");
    if (lsLoc != -1) glUniformMatrix4fv(lsLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    if (mLoc != -1) glUniformMatrix4fv(mLoc, 1, GL_FALSE, glm::value_ptr(model));

    wildTown.Draw(shadowShader);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2) PASS NORMAL
    glViewport(0, 0, retina_width, retina_height);

    // Aplica modul de randare cerut doar pentru pass-ul normal
    applyRenderModeForNormalPass();

    if (fogEnabled) glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f);
    else glClearColor(0.05f, 0.06f, 0.08f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (skyboxEnabled) {
        setSkyboxFogUniforms();
        skybox.Draw(skyboxShader, myCamera.getViewMatrix(), projection);
    }

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex);

    setSceneFogUniforms();
    sceneShader.useShaderProgram();

    updateViewRelatedUniforms();

    if (lightSpaceMatrixLoc != -1) {
        glUniformMatrix4fv(lightSpaceMatrixLoc, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    }
    if (enableShadowsLoc != -1) {
        glUniform1i(enableShadowsLoc, enableShadows ? 1 : 0);
    }

    wildTown.Draw(sceneShader);
}

void cleanup()
{
    // opreste muzica daca ruleaza
    stopMusic();

    if (shadowDepthTex) glDeleteTextures(1, &shadowDepthTex);
    if (shadowFBO) glDeleteFramebuffers(1, &shadowFBO);

    glfwDestroyWindow(glWindow);
    glfwTerminate();
}

int main(int argc, const char* argv[])
{
    if (!initOpenGLWindow()) return 1;

    initOpenGLState();

    initShadowMap();

    initObjects();
    initShaders();
    initUniforms();

    // NOU: asigura ca pornim exact din pozitia camerei dorita
    myCamera.setPosition(kStartCamPos);
    applyGroundClamp();
    updateViewRelatedUniforms();

    lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(glWindow)) {

        double now = glfwGetTime();
        float dt = (float)(now - lastFrameTime);
        lastFrameTime = now;

        // P activeaza/dezactiveaza autoDemo; cand e activ, rulam turul cinematic
        if (autoDemo) runCinematicTour(dt);
        else processMovement();

        renderScene();

        glfwPollEvents();
        glfwSwapBuffers(glWindow);
    }

    cleanup();
    return 0;
}
