#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <random>

void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
unsigned int loadTexture(const char *path, bool gammaCorrection);
void renderFullScreen();
void renderCube();

// settings
const unsigned int SRC_WIDTH = 1600;
const unsigned int SRC_HEIGHT = 900;

const float SSAO_SAMPLE_RADIUS = 0.5f;
const float SSAO_SAMPLE_BIAS = 0.025f;
const float SSAO_DEPTH_RANGE_CLAMP = 0.2f;

const float HBAO_SAMPLE_RADIUS = 0.2f;
const float HBAO_MAX_RADIUS_PIXELS = 50.0f;
const unsigned int HBAO_DIRS = 6;
const unsigned int HBAO_SAMPLES = 3;

const float GTAO_ROTATIONS[6] = { 60.0f, 300.0f, 180.0f, 240.0f, 120.0f, 0.0f };
const float GTAO_OFFSETS[4] = { 0.0f, 0.5f, 0.25f, 0.75f };

const float CAMERA_NEAR_PLANE = 0.1f;
const float CAMERA_FAR_PLANE = 50.0f;

const int NOISE_TEXTURE_RES = 4;

// camera
Camera camera(glm::vec3(0.0f, 8.0f, 3.0f));
float lastX = (float)SRC_WIDTH / 2.0;
float lastY = (float)SRC_HEIGHT / 2.0;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

enum class RenderMode {
    NONE,
    SSAO,
    HBAO,
    GTAO,
};
RenderMode renderMode = RenderMode::SSAO;
bool enableBlur = true;

float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

unsigned int getSSAONoiseTexture()
{
    std::vector<glm::vec3> ssaoNoise;
    for (int i = 0; i < NOISE_TEXTURE_RES * NOISE_TEXTURE_RES; i++)
    {
        glm::vec3 noise(glm::linearRand(0.0f, 1.0f) * 2.0 - 1.0, glm::linearRand(0.0f, 1.0f) * 2.0 - 1.0, 0.0f); // rotate around z-axis (in tangent space)
        ssaoNoise.push_back(noise);
    }

    unsigned int noiseTexture;
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, NOISE_TEXTURE_RES, NOISE_TEXTURE_RES, 0, GL_RGB, GL_FLOAT, ssaoNoise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return noiseTexture;
}

unsigned int getHBAONoiseTexture()
{
    std::vector<glm::vec4> noise;
    for (int y = 0; y < NOISE_TEXTURE_RES; y++)
    {
        for (int x = 0; x < NOISE_TEXTURE_RES; x++)
        {
            glm::vec2 xy = glm::circularRand(1.0f);
            float z = glm::linearRand(0.0f, 1.0f);
            float w = glm::linearRand(0.0f, 1.0f);
            noise.push_back(glm::vec4(xy, z, w));
        }
    }
       
    unsigned int noiseTexture;
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, NOISE_TEXTURE_RES, NOISE_TEXTURE_RES, 0, GL_RGBA, GL_FLOAT, noise.data());
    
    return noiseTexture;
}

unsigned int getGTAONoiseTexture()
{
    const int BYTES_PER_PIXEL = 2;
    uint8_t noise[NOISE_TEXTURE_RES * NOISE_TEXTURE_RES * BYTES_PER_PIXEL];
    for (uint8_t i = 0; i < NOISE_TEXTURE_RES; i++) 
    {
        for (uint8_t j = 0; j < NOISE_TEXTURE_RES; j++) 
        {
            float dirnoise = 0.0625f * ((((i + j) & 0x3) << 2) + (i & 0x3));
            float offnoise = 0.25f * ((j - i) & 0x3);

            noise[(i * NOISE_TEXTURE_RES + j) * BYTES_PER_PIXEL + 0] = (uint8_t)(dirnoise * 255.0f);
            noise[(i * NOISE_TEXTURE_RES + j) * BYTES_PER_PIXEL + 1] = (uint8_t)(offnoise * 255.0f);
        }
    }

    unsigned int noiseTexture;
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, NOISE_TEXTURE_RES, NOISE_TEXTURE_RES, 0, GL_RG, GL_UNSIGNED_BYTE, noise);

    return noiseTexture;
}

unsigned int getEmptyAOTexture()
{
    float data[] = { 1.0f, 1.0f };
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 1, 1, 0, GL_RG, GL_FLOAT, data);

    return texture;
}

void openglMessageCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    __debugbreak();
}

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_RESIZABLE, false);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SRC_WIDTH, SRC_HEIGHT, "Research", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(openglMessageCallback, 0);

    // build and compile shaders
    // -------------------------
    Shader shaderGeometryPass("geometry.vs", "geometry.fs");
    Shader shaderLightingPass("fullscreen.vs", "lighting.fs");
    Shader shaderSSAO("fullscreen.vs", "ssao.fs");
    Shader shaderHBAO("fullscreen.vs", "hbao.fs");
    Shader shaderGTAO("fullscreen.vs", "gtao.fs");
    Shader shaderBoxBlur("fullscreen.vs", "box_blur.fs");

    // load models
    // -----------
    Model mainModel(FileSystem::getPath("resources/objects/nanosuit/nanosuit.obj"));

    // configure g-buffer framebuffer
    // ------------------------------
    unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    unsigned int gAlbedo, gNormal, gDepth;
    // color + specular color buffer
    glGenTextures(1, &gAlbedo);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SRC_WIDTH, SRC_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gAlbedo, 0);
    // normal color buffer
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SRC_WIDTH, SRC_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNormal, 0);
    // depth buffer
    glGenTextures(1, &gDepth);
    glBindTexture(GL_TEXTURE_2D, gDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SRC_WIDTH, SRC_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepth, 0);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    unsigned int attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(std::size(attachments), attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // also create framebuffer to hold SSAO processing stage 
    // -----------------------------------------------------
    unsigned int ssaoFBO, ssaoBlurFBO;
    glGenFramebuffers(1, &ssaoFBO);  
    glGenFramebuffers(1, &ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    unsigned int ssaoColorBuffer, ssaoColorBufferBlur;
    // SSAO color buffer
    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, SRC_WIDTH, SRC_HEIGHT, 0, GL_RG, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Framebuffer not complete!" << std::endl;
    // and blur stage
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glGenTextures(1, &ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, SRC_WIDTH, SRC_HEIGHT, 0, GL_RG, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // generate sample kernel
    // ----------------------
    std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
    std::vector<glm::vec3> ssaoKernel;
    int kernelSize = 16;
    for (unsigned int i = 0; i < kernelSize; ++i)
    {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / kernelSize;

        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    unsigned int ssaoNoiseTexture = getSSAONoiseTexture();
    unsigned int hbaoNoiseTexture = getHBAONoiseTexture();
    unsigned int gtaoNoiseTexture = getGTAONoiseTexture();
    unsigned int emptyAOTexture = getEmptyAOTexture();

    // shader configuration
    // --------------------
    shaderLightingPass.use();
    shaderLightingPass.setInt("gAlbedo", 0);
    shaderLightingPass.setInt("gNormal", 1);
    shaderLightingPass.setInt("ao", 2);

    shaderSSAO.use();
    shaderSSAO.setFloat("sampleRadius", SSAO_SAMPLE_RADIUS);
    shaderSSAO.setFloat("bias", SSAO_SAMPLE_BIAS);
    shaderSSAO.setFloat("depthRangeClamp", SSAO_DEPTH_RANGE_CLAMP);
    shaderSSAO.setInt("gDepth", 0);
    shaderSSAO.setInt("gNormal", 1);
    shaderSSAO.setInt("texNoise", 2);


    float fovRad = glm::radians(camera.Zoom);
    glm::vec2 FocalLen, InvFocalLen, UVToViewA, UVToViewB, LinMAD;

    FocalLen[0] = 1.0f / tanf(fovRad * 0.5f) * ((float)SRC_HEIGHT / (float)SRC_WIDTH);
    FocalLen[1] = 1.0f / tanf(fovRad * 0.5f);
    InvFocalLen[0] = 1.0f / FocalLen[0];
    InvFocalLen[1] = 1.0f / FocalLen[1];

    UVToViewA[0] = -2.0f * InvFocalLen[0];
    UVToViewA[1] = -2.0f * InvFocalLen[1];
    UVToViewB[0] = 1.0f * InvFocalLen[0];
    UVToViewB[1] = 1.0f * InvFocalLen[1];

    LinMAD[0] = (CAMERA_NEAR_PLANE - CAMERA_FAR_PLANE) / (2.0f * CAMERA_NEAR_PLANE * CAMERA_FAR_PLANE);
    LinMAD[1] = (CAMERA_NEAR_PLANE + CAMERA_FAR_PLANE) / (2.0f * CAMERA_NEAR_PLANE * CAMERA_FAR_PLANE);
    shaderHBAO.use();
    shaderHBAO.setVec2("FocalLen", FocalLen);
    shaderHBAO.setVec2("UVToViewA", UVToViewA);
    shaderHBAO.setVec2("UVToViewB", UVToViewB);
    shaderHBAO.setVec2("LinMAD", LinMAD);
    shaderHBAO.setVec2("AORes", glm::vec2(SRC_WIDTH, SRC_HEIGHT));
    shaderHBAO.setVec2("InvAORes", glm::vec2(1.0f / SRC_WIDTH, 1.0f / SRC_HEIGHT));
    shaderHBAO.setFloat("R", HBAO_SAMPLE_RADIUS);
    shaderHBAO.setFloat("R2", HBAO_SAMPLE_RADIUS * HBAO_SAMPLE_RADIUS);
    shaderHBAO.setFloat("NegInvR2", -1.0f / (HBAO_SAMPLE_RADIUS * HBAO_SAMPLE_RADIUS));
    shaderHBAO.setFloat("MaxRadiusPixels", HBAO_MAX_RADIUS_PIXELS);
    shaderHBAO.setVec2("NoiseScale", glm::vec2((float)SRC_WIDTH / NOISE_TEXTURE_RES, (float)SRC_HEIGHT / NOISE_TEXTURE_RES));
    shaderHBAO.setInt("NumDirections", HBAO_DIRS);
    shaderHBAO.setInt("NumSamples", HBAO_SAMPLES);
    shaderHBAO.setInt("gDepth", 0);
    shaderHBAO.setInt("texNoise", 1);

    int gtaoSampleIndex = 0;
    shaderGTAO.use();
    shaderGTAO.setInt("gDepth", 0);
    shaderGTAO.setInt("gNormal", 1);
    shaderGTAO.setInt("texNoise", 2);

    shaderBoxBlur.use();
    shaderBoxBlur.setInt("ssaoInput", 0);

    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. geometry pass: render scene's geometry/color data into gbuffer
        // -----------------------------------------------------------------
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SRC_WIDTH / (float)SRC_HEIGHT, CAMERA_NEAR_PLANE, CAMERA_FAR_PLANE);
            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 model = glm::mat4(1.0f);
            glm::mat4 invProjection = glm::inverse(projection);
            glm::mat4 invView = glm::inverse(view);
            shaderGeometryPass.use();
            shaderGeometryPass.setMat4("projection", projection);
            shaderGeometryPass.setMat4("view", view);
            // room cube
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0, 7.0f, 0.0f));
            model = glm::scale(model, glm::vec3(7.5f, 7.5f, 7.5f));
            shaderGeometryPass.setMat4("model", model);
            shaderGeometryPass.setInt("invertedNormals", 1); // invert normals as we're inside the cube
            renderCube();
            shaderGeometryPass.setInt("invertedNormals", 0); 
            // models renderer (we dont care about instancing as we measuring screen space ssao afterwards)
            for (int i = 0; i < 3; i++)
            {
                float xOffset[] = {-3.0f, 0.0f, 3.0f};
                model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(xOffset[i], -0.2f, 3.0));
                model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
                model = glm::scale(model, glm::vec3(0.3f));
                shaderGeometryPass.setMat4("model", model);
                mainModel.Draw(shaderGeometryPass);
            }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        if (renderMode == RenderMode::SSAO)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            shaderSSAO.use();
            for (unsigned int i = 0; i < kernelSize; ++i)
                shaderSSAO.setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
            shaderSSAO.setMat4("proj", projection);
            shaderSSAO.setMat4("invProj", invProjection);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gDepth);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, gNormal);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, ssaoNoiseTexture);
            renderFullScreen();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        if (renderMode == RenderMode::HBAO)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            shaderHBAO.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gDepth);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, hbaoNoiseTexture);
            renderFullScreen();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        if (renderMode == RenderMode::GTAO)
        {
            glm::vec4 projInfo = glm::vec4(
                2.0f / (SRC_WIDTH * projection[0][0]),
                2.0f / (SRC_HEIGHT * projection[1][1]),
                -1.0f / projection[0][0],
                -1.0f / projection[1][1]
            );
            glm::vec2 params = glm::vec2(
                GTAO_ROTATIONS[gtaoSampleIndex % 6] / 360.0f,
                GTAO_OFFSETS[(gtaoSampleIndex / 6) % 4]
            );
            glm::vec4 clipInfo = glm::vec4(
                CAMERA_NEAR_PLANE,
                CAMERA_FAR_PLANE,
                0.5f * (SRC_HEIGHT / (2.0f * tanf(fovRad * 0.5f))),
                0.0f
            );

            glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            shaderGTAO.use();
            shaderGTAO.setVec4("projInfo", projInfo);
            shaderGTAO.setVec4("clipInfo", clipInfo);
            shaderGTAO.setVec2("params", params);
            shaderGTAO.setMat4("invView", invView);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gDepth);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, gNormal);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, gtaoNoiseTexture);
            renderFullScreen();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            gtaoSampleIndex = (gtaoSampleIndex + 1) % 24;
        }


        if (enableBlur)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            shaderBoxBlur.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
            renderFullScreen();
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }


        // finilize to output
        bool hasAO = renderMode != RenderMode::NONE;
        unsigned int aoTexture = hasAO ?
            (enableBlur ? ssaoColorBufferBlur : ssaoColorBuffer) :
            emptyAOTexture;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderLightingPass.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gAlbedo);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, aoTexture);
        renderFullScreen();


        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
    // initialize (if necessary)
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
            // top face
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
             1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        // fill buffer
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        // link vertex attributes
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    // render Cube
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

unsigned int dummyVAO = 0;
void renderFullScreen()
{
    if (dummyVAO == 0)
    {
        glGenVertexArrays(1, &dummyVAO);
    }
    // do not bind anything, vertices are generated in vertex shader
    glBindVertexArray(dummyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
        enableBlur = !enableBlur;

    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS)
        renderMode = RenderMode::NONE;
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        renderMode = RenderMode::SSAO;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        renderMode = RenderMode::HBAO;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        renderMode = RenderMode::GTAO;
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
