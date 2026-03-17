// 2.delivery_run.cpp
// Simple 3D Delivery Run Game
// Controls: WASD to move, ESC to quit
// Goal: Pick up 3 packages and deliver them to the yellow zones before time runs out!

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/model.h>

#include <iostream>
#include <vector>
#include <string>

// -------------------------------------------------------
// Window
// -------------------------------------------------------
const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

// -------------------------------------------------------
// Callbacks
// -------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// -------------------------------------------------------
// Game constants
// -------------------------------------------------------
const float GAME_TIME        = 60.0f;   // seconds
const int   NUM_PACKAGES     = 3;
const float PICK_RADIUS      = 1.5f;    // distance to pick up package
const float DELIVER_RADIUS   = 1.8f;    // distance to deliver
const float PLAYER_SPEED     = 4.0f;
const float PLAYER_HALF_W    = 0.4f;    // AABB half-width for wall collision

// -------------------------------------------------------
// Structs
// -------------------------------------------------------
struct Package {
    glm::vec3 pos;
    bool collected = false;
};

struct Zone {
    glm::vec3 pos;
    bool delivered = false;
};

// Wall defined by center + half-extents (X and Z only, infinite height)
struct Wall {
    glm::vec3 center;
    float     halfX;
    float     halfZ;
};

// -------------------------------------------------------
// Game state (globals for simplicity)
// -------------------------------------------------------
glm::vec3 playerPos(0.0f, 0.0f, 0.0f);
float     playerAngle = 0.0f;           // Y-axis rotation in degrees
bool      carryingPackage = false;
int       carriedIdx      = -1;         // which package is being carried

Package packages[NUM_PACKAGES] = {
    { glm::vec3( 5.0f, 0.0f,  4.0f) },
    { glm::vec3(-6.0f, 0.0f, -3.0f) },
    { glm::vec3( 2.0f, 0.0f, -7.0f) }
};

Zone zones[NUM_PACKAGES] = {
    { glm::vec3(-4.0f, 0.0f,  6.0f) },
    { glm::vec3( 7.0f, 0.0f, -5.0f) },
    { glm::vec3(-2.0f, 0.0f,  9.0f) }
};

// Walls: a border fence + a few inner obstacles
// Format: center, halfX, halfZ
std::vector<Wall> walls = {
    // --- border walls ---
    { glm::vec3( 0.0f, 0.0f,  12.5f), 12.5f, 0.5f },  // north
    { glm::vec3( 0.0f, 0.0f, -12.5f), 12.5f, 0.5f },  // south
    { glm::vec3( 12.5f, 0.0f, 0.0f),  0.5f, 12.5f },  // east
    { glm::vec3(-12.5f, 0.0f, 0.0f),  0.5f, 12.5f },  // west
    // --- inner obstacles ---
    { glm::vec3( 3.0f, 0.0f,  0.0f),  0.6f, 3.0f  },
    { glm::vec3(-3.0f, 0.0f,  2.0f),  3.0f, 0.6f  },
    { glm::vec3( 7.0f, 0.0f,  3.0f),  0.6f, 2.5f  },
    { glm::vec3(-7.0f, 0.0f, -4.0f),  2.5f, 0.6f  },
    { glm::vec3( 0.0f, 0.0f, -5.0f),  2.0f, 0.6f  },
};

float timeRemaining = GAME_TIME;
bool  gameOver      = false;
bool  gameWon       = false;
int   deliveredCount = 0;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// -------------------------------------------------------
// Helper: check AABB wall collision and push player out
// -------------------------------------------------------
void resolveWallCollisions()
{
    for (const Wall& w : walls)
    {
        float dx = playerPos.x - w.center.x;
        float dz = playerPos.z - w.center.z;

        float overlapX = (w.halfX + PLAYER_HALF_W) - fabsf(dx);
        float overlapZ = (w.halfZ + PLAYER_HALF_W) - fabsf(dz);

        if (overlapX > 0 && overlapZ > 0)
        {
            // Push out along the axis of least overlap
            if (overlapX < overlapZ)
                playerPos.x += (dx > 0 ? overlapX : -overlapX);
            else
                playerPos.z += (dz > 0 ? overlapZ : -overlapZ);
        }
    }
}

// -------------------------------------------------------
// Helper: check package pickup (sphere)
// -------------------------------------------------------
void checkPickup()
{
    if (carryingPackage) return; // already holding one

    for (int i = 0; i < NUM_PACKAGES; i++)
    {
        if (packages[i].collected) continue;
        float dist = glm::length(glm::vec2(playerPos.x - packages[i].pos.x,
                                           playerPos.z - packages[i].pos.z));
        if (dist < PICK_RADIUS)
        {
            carryingPackage = true;
            carriedIdx      = i;
            packages[i].collected = true;
            std::cout << "[PICKUP] Picked up package " << (i+1)
                      << "! Bring it to a delivery zone.\n";
            return;
        }
    }
}

// -------------------------------------------------------
// Helper: check delivery (sphere)
// -------------------------------------------------------
void checkDelivery()
{
    if (!carryingPackage) return;

    for (int i = 0; i < NUM_PACKAGES; i++)
    {
        if (zones[i].delivered) continue;
        float dist = glm::length(glm::vec2(playerPos.x - zones[i].pos.x,
                                           playerPos.z - zones[i].pos.z));
        if (dist < DELIVER_RADIUS)
        {
            zones[i].delivered = true;
            carryingPackage    = false;
            deliveredCount++;
            std::cout << "[DELIVERY] Delivered! " << deliveredCount
                      << "/" << NUM_PACKAGES << " done.\n";
            if (deliveredCount == NUM_PACKAGES)
            {
                gameWon = true;
                std::cout << "\n*** YOU WIN! All packages delivered! ***\n";
            }
            return;
        }
    }
}

// -------------------------------------------------------
// Draw a flat quad as ground (no model file needed)
// -------------------------------------------------------
unsigned int groundVAO = 0, groundVBO = 0;
void setupGround()
{
    // A large flat quad in the XZ plane
    float s = 12.5f;
    float groundVertices[] = {
        // pos              normal          texcoords
        -s, 0.0f, -s,   0,1,0,   0.0f, 0.0f,
         s, 0.0f, -s,   0,1,0,   1.0f, 0.0f,
         s, 0.0f,  s,   0,1,0,   1.0f, 1.0f,
        -s, 0.0f,  s,   0,1,0,   0.0f, 1.0f,
    };
    unsigned int indices[] = { 0,1,2, 2,3,0 };

    unsigned int EBO;
    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(groundVertices), groundVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    // texcoords
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void drawGround(Shader& shader)
{
    shader.setBool("useColor", true);
    shader.setVec3("flatColor", glm::vec3(0.35f, 0.55f, 0.25f)); // green ground
    glm::mat4 model = glm::mat4(1.0f);
    shader.setMat4("model", model);
    glBindVertexArray(groundVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// -------------------------------------------------------
// Draw a simple box (package / zone marker)
// -------------------------------------------------------
unsigned int boxVAO = 0, boxVBO = 0;
void setupBox()
{
    // Unit cube centered at origin
    float v[] = {
        // back face
        -0.5f,-0.5f,-0.5f, 0,0,-1, 0,0,
         0.5f,-0.5f,-0.5f, 0,0,-1, 1,0,
         0.5f, 0.5f,-0.5f, 0,0,-1, 1,1,
         0.5f, 0.5f,-0.5f, 0,0,-1, 1,1,
        -0.5f, 0.5f,-0.5f, 0,0,-1, 0,1,
        -0.5f,-0.5f,-0.5f, 0,0,-1, 0,0,
        // front face
        -0.5f,-0.5f, 0.5f, 0,0,1, 0,0,
         0.5f,-0.5f, 0.5f, 0,0,1, 1,0,
         0.5f, 0.5f, 0.5f, 0,0,1, 1,1,
         0.5f, 0.5f, 0.5f, 0,0,1, 1,1,
        -0.5f, 0.5f, 0.5f, 0,0,1, 0,1,
        -0.5f,-0.5f, 0.5f, 0,0,1, 0,0,
        // left face
        -0.5f, 0.5f, 0.5f,-1,0,0, 1,0,
        -0.5f, 0.5f,-0.5f,-1,0,0, 1,1,
        -0.5f,-0.5f,-0.5f,-1,0,0, 0,1,
        -0.5f,-0.5f,-0.5f,-1,0,0, 0,1,
        -0.5f,-0.5f, 0.5f,-1,0,0, 0,0,
        -0.5f, 0.5f, 0.5f,-1,0,0, 1,0,
        // right face
         0.5f, 0.5f, 0.5f, 1,0,0, 1,0,
         0.5f, 0.5f,-0.5f, 1,0,0, 1,1,
         0.5f,-0.5f,-0.5f, 1,0,0, 0,1,
         0.5f,-0.5f,-0.5f, 1,0,0, 0,1,
         0.5f,-0.5f, 0.5f, 1,0,0, 0,0,
         0.5f, 0.5f, 0.5f, 1,0,0, 1,0,
        // bottom face
        -0.5f,-0.5f,-0.5f, 0,-1,0, 0,1,
         0.5f,-0.5f,-0.5f, 0,-1,0, 1,1,
         0.5f,-0.5f, 0.5f, 0,-1,0, 1,0,
         0.5f,-0.5f, 0.5f, 0,-1,0, 1,0,
        -0.5f,-0.5f, 0.5f, 0,-1,0, 0,0,
        -0.5f,-0.5f,-0.5f, 0,-1,0, 0,1,
        // top face
        -0.5f, 0.5f,-0.5f, 0,1,0, 0,1,
         0.5f, 0.5f,-0.5f, 0,1,0, 1,1,
         0.5f, 0.5f, 0.5f, 0,1,0, 1,0,
         0.5f, 0.5f, 0.5f, 0,1,0, 1,0,
        -0.5f, 0.5f, 0.5f, 0,1,0, 0,0,
        -0.5f, 0.5f,-0.5f, 0,1,0, 0,1,
    };
    glGenVertexArrays(1, &boxVAO);
    glGenBuffers(1, &boxVBO);
    glBindVertexArray(boxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, boxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void drawBox(Shader& shader, glm::vec3 pos, glm::vec3 scale, glm::vec3 color)
{
    shader.setBool("useColor", true);
    shader.setVec3("flatColor", color);
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    model = glm::scale(model, scale);
    shader.setMat4("model", model);
    glBindVertexArray(boxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

// -------------------------------------------------------
// main
// -------------------------------------------------------
int main()
{
    // --- GLFW init ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Delivery Run", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    stbi_set_flip_vertically_on_load(true);
    glEnable(GL_DEPTH_TEST);

    // --- Shader ---
    Shader shader("2.delivery_run.vs", "2.delivery_run.fs");

    // --- Load models ---
    Model playerModel(FileSystem::getPath("resources/objects/player/character-male-b.obj"));
    Model wallModel  (FileSystem::getPath("resources/objects/wall/building-a.obj"));

    // --- Setup procedural geometry ---
    setupGround();
    setupBox();

    // --- Print instructions ---
    std::cout << "=== DELIVERY RUN ===\n";
    std::cout << "WASD: move player\n";
    std::cout << "Goal: Pick up 3 packages (BROWN boxes) and deliver to zones (YELLOW markers)\n";
    std::cout << "You have 60 seconds!\n\n";

    // --- Render loop ---
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // --- Input ---
        processInput(window);

        // --- Game logic (only if game still running) ---
        if (!gameOver && !gameWon)
        {
            // Update timer
            timeRemaining -= deltaTime;
            if (timeRemaining <= 0.0f) {
                timeRemaining = 0.0f;
                gameOver = true;
                std::cout << "\n*** GAME OVER! Time ran out! ***\n";
            }

            // Print timer every 5 seconds
            static float lastPrint = GAME_TIME;
            if ((int)timeRemaining % 5 == 0 && (int)timeRemaining != (int)lastPrint) {
                lastPrint = (float)(int)timeRemaining;
                std::cout << "[TIMER] " << (int)timeRemaining << "s remaining\n";
            }

            resolveWallCollisions();
            checkPickup();
            checkDelivery();
        }

        // --- Render ---
        glClearColor(0.53f, 0.81f, 0.98f, 1.0f); // sky blue
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        // Light setup
        shader.setVec3("lightDir",   glm::vec3(0.5f, -1.0f, 0.3f));
        shader.setVec3("lightColor", glm::vec3(1.0f,  1.0f, 1.0f));

        // Camera: fixed isometric-style follow camera
        // Constant offset above and behind the player (south = -Z side)
        // This makes controls consistent: W always goes away from camera (+Z), S toward camera (-Z)
        glm::vec3 camPos    = playerPos + glm::vec3(0.0f, 10.0f, -10.0f);
        glm::vec3 camTarget = playerPos + glm::vec3(0.0f,  0.5f,   0.0f);

        glm::mat4 view       = glm::lookAt(camPos, camTarget, glm::vec3(0,1,0));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                                 (float)SCR_WIDTH/(float)SCR_HEIGHT,
                                                 0.1f, 100.0f);
        shader.setMat4("view",       view);
        shader.setMat4("projection", projection);

        // --- Draw ground ---
        drawGround(shader);

        // --- Draw walls using colored boxes that exactly match AABB extents ---
        for (int i = 0; i < (int)walls.size(); i++)
        {
            const Wall& w = walls[i];
            float sx = w.halfX * 2.0f;
            float sz = w.halfZ * 2.0f;
            float wallH = 2.5f;

            if (i < 4) {
                // Border walls: flat colored box
                drawBox(shader,
                        glm::vec3(w.center.x, wallH * 0.5f, w.center.z),
                        glm::vec3(sx, wallH, sz),
                        glm::vec3(0.4f, 0.25f, 0.1f));
            } else {
                // Inner obstacles: use building model scaled to footprint
                shader.setBool("useColor", false);
                glm::mat4 mdl = glm::mat4(1.0f);
                mdl = glm::translate(mdl, glm::vec3(w.center.x, 0.0f, w.center.z));
                mdl = glm::scale(mdl, glm::vec3(sx, 1.0f, sz));
                shader.setMat4("model", mdl);
                wallModel.Draw(shader);
            }
        }

        // --- Draw packages ---
        for (int i = 0; i < NUM_PACKAGES; i++)
        {
            if (packages[i].collected) continue;
            // Brown box sitting on the ground (y=0.4 so it sits on ground)
            drawBox(shader, glm::vec3(packages[i].pos.x, 0.4f, packages[i].pos.z),
                    glm::vec3(0.8f, 0.8f, 0.8f),
                    glm::vec3(0.6f, 0.35f, 0.1f)); // brown
        }

        // --- Draw delivery zones (flat yellow marker + tall pole) ---
        for (int i = 0; i < NUM_PACKAGES; i++)
        {
            if (zones[i].delivered) continue;
            glm::vec3 zp = zones[i].pos;
            // Flat marker on ground
            drawBox(shader, glm::vec3(zp.x, 0.05f, zp.z),
                    glm::vec3(2.0f, 0.1f, 2.0f),
                    glm::vec3(1.0f, 0.85f, 0.0f)); // yellow
            // Tall pole so it's visible from far
            drawBox(shader, glm::vec3(zp.x, 1.5f, zp.z),
                    glm::vec3(0.1f, 3.0f, 0.1f),
                    glm::vec3(1.0f, 0.85f, 0.0f)); // yellow pole
        }

        // --- Draw carried package floating above player ---
        if (carryingPackage) {
            drawBox(shader,
                    glm::vec3(playerPos.x, 2.0f, playerPos.z),
                    glm::vec3(0.6f, 0.6f, 0.6f),
                    glm::vec3(0.6f, 0.35f, 0.1f));
        }

        // --- Draw player ---
        shader.setBool("useColor", false);
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, playerPos);
            model = glm::rotate(model, glm::radians(playerAngle), glm::vec3(0,1,0));
            // Scale down — Kenney characters are typically a few units tall
            model = glm::scale(model, glm::vec3(2.0f, 2.0f, 2.0f));
            shader.setMat4("model", model);
            playerModel.Draw(shader);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

// -------------------------------------------------------
// Input: WASD moves player relative to a fixed top-down
//        camera orientation (W = forward into screen = +Z)
// -------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (gameOver || gameWon) return;

    glm::vec3 move(0.0f);

    // W/S = move along +Z/-Z (into/out of screen from top-down view)
    // A/D = move along -X/+X (left/right)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move.z += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move.z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move.x += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move.x -= 1.0f;

    if (glm::length(move) > 0.0f)
    {
        move = glm::normalize(move);
        playerPos += move * PLAYER_SPEED * deltaTime;

        // Update player facing angle based on movement direction
        playerAngle = glm::degrees(atan2f(move.x, move.z));
    }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
