#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <chrono>
#include "Matrix.h"
#include "Vector.h"
#include <SOIL.h>
#include <string>

using namespace std;

GLFWwindow* g_window;
int screen_width = 800, screen_height = 600;

GLfloat lastX = screen_width / 2.0f, lastY = screen_height / 2.0f;
GLfloat yaw = -90.0f, pitch = 0.0f;
bool firstMouse = true;

GLuint g_shaderProgram;
GLint g_uMVP;
GLint g_uMV;
GLint g_uN;

chrono::time_point<chrono::system_clock> g_callTime;

class Model
{
public:
    GLuint vbo;
    GLuint ibo;
    GLuint vao;
    GLsizei indexCount;
};

Model g_model;

bool keys[1024];

Matrix4 g_P = createPerspectiveProjectionMatrix(100.0f, 0.01f, 40.0f, screen_width, screen_height);

Vector3 cameraPos = Vector3(0.0f, 0.0f, 30.0f);
Vector3 cameraFront = Vector3(0.0f, 0.0f, -1.0f);
Vector3 cameraUp = Vector3(0.0f, 1.0f, 0.0f);

GLuint createShader(const GLchar* code, GLenum type);

GLuint createProgram(GLuint vsh, GLuint fsh);

bool createShaderProgram();

bool createModel();

bool init();

void reshape(GLFWwindow* window, int width, int height);

void draw(double deltaTime);

void cleanup();

bool initOpenGL();

void tearDownOpenGL();

GLfloat to_radians(GLfloat degrees);

GLfloat to_degrees(GLfloat radians);

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

void do_movement(double deltaTime);

void mouse_callback(GLFWwindow* window, double xpos, double ypos);

class BillBoard
{
public:
    Model model;
    GLuint shaderProgram;
    GLint uMVP;

    bool createModel()
    {
        const GLfloat vertices[] =
        {
            -1.0, -1.0, 0.0,
            1.0, 1.0, 0.0,
            -1.0, 1.0, 0.0,
            1.0, -1.0, 0.0
        };

        const GLint indices[] =
        {
            0, 1, 2, 0, 3, 1
        };

        glGenVertexArrays(1, &this->model.vao);
        glBindVertexArray(this->model.vao);

        glGenBuffers(1, &this->model.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, this->model.vbo);
        glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

        glGenBuffers(1, &this->model.ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->model.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof(GLint), indices, GL_STATIC_DRAW);

        this->model.indexCount = 6;

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const GLvoid*)0);

        return this->model.vbo != 0 && this->model.ibo != 0 && this->model.vao != 0;
    }

    bool createShaderProgram()
    {
        this->shaderProgram = 0;

        const GLchar vsh[] =
            "#version 330\n"
            ""
            "layout(location = 0) in vec3 a_position;"
            ""
            "uniform mat4 u_mvp;"
            ""
            "void main()"
            "{"
            "    gl_Position = u_mvp * vec4(a_position, 1.0);"
            "}"
            ;

        const GLchar fsh[] =
            "#version 330\n"
            ""
            "layout(location = 0) out vec4 o_color;"
            ""
            "void main()"
            "{"
            "    o_color = vec4(0.0, 1.0, 1.0, 0.3);"
            "}"
            ;

        GLuint vertexShader, fragmentShader;

        vertexShader = createShader(vsh, GL_VERTEX_SHADER);
        fragmentShader = createShader(fsh, GL_FRAGMENT_SHADER);

        this->shaderProgram = createProgram(vertexShader, fragmentShader);

        this->uMVP = glGetUniformLocation(this->shaderProgram, "u_mvp");

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return this->shaderProgram != 0;
    }

    void cleanup()
    {
        if (this->shaderProgram != 0)
            glDeleteProgram(this->shaderProgram);
        if (this->model.vbo != 0)
            glDeleteBuffers(1, &this->model.vbo);
        if (this->model.ibo != 0)
            glDeleteBuffers(1, &this->model.ibo);
        if (this->model.vao != 0)
            glDeleteVertexArrays(1, &this->model.vao);
    }
};

BillBoard billboard;

int main()
{
    // Initialize OpenGL
    if (!initOpenGL())
        return -1;

    // Initialize graphical resources.
    bool isOk = init();

    if (isOk)
    {
        glfwSetKeyCallback(g_window, key_callback);
        glfwSetCursorPosCallback(g_window, mouse_callback);
        glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        g_callTime = chrono::system_clock::now();
        // Main loop until window closed or escape pressed.
        while (!glfwWindowShouldClose(g_window))
        {
            auto callTime = chrono::system_clock::now();
            chrono::duration<double> elapsed = callTime - g_callTime;
            g_callTime = callTime;

            double deltaTime = elapsed.count();
            // Draw scene.
            draw(deltaTime);

            // Swap buffers.
            glfwSwapBuffers(g_window);
            // Poll window events.
            glfwPollEvents();

            do_movement(deltaTime);
        }
    }

    // Cleanup graphical resources.
    cleanup();

    // Tear down OpenGL.
    tearDownOpenGL();

    return isOk ? 0 : -1;
}

GLuint createShader(const GLchar* code, GLenum type)
{
    GLuint result = glCreateShader(type);

    glShaderSource(result, 1, &code, NULL);
    glCompileShader(result);

    GLint compiled;
    glGetShaderiv(result, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0)
        {
            char* infoLog = (char*)alloca(infoLen);
            glGetShaderInfoLog(result, infoLen, NULL, infoLog);
            cout << "Shader compilation error" << endl << infoLog << endl;
        }
        glDeleteShader(result);
        return 0;
    }

    return result;
}

GLuint createProgram(GLuint vsh, GLuint fsh)
{
    GLuint result = glCreateProgram();

    glAttachShader(result, vsh);
    glAttachShader(result, fsh);

    glLinkProgram(result);

    GLint linked;
    glGetProgramiv(result, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0)
        {
            char* infoLog = (char*)alloca(infoLen);
            glGetProgramInfoLog(result, infoLen, NULL, infoLog);
            cout << "Shader program linking error" << endl << infoLog << endl;
        }
        glDeleteProgram(result);
        return 0;
    }

    return result;
}

bool createShaderProgram()
{
    g_shaderProgram = 0;

    const GLchar vsh[] =
        "#version 330\n"
        ""
        "layout(location = 0) in vec3 a_position;"
        "layout(location = 1) in vec3 a_normal;"
        ""
        "uniform mat4 u_mvp;"
        "uniform mat4 u_mv;"
        "uniform mat3 u_n;"
        ""
        "out vec3 v_normal;"
        "out vec3 v_position;"
        ""
        "void main()"
        "{"
        "   vec4 p0 = vec4(a_position, 1.0);"
        "   v_normal = transpose(inverse(u_n)) * normalize(a_normal);"
        "   v_position = vec3(u_mv * p0);"
        "   gl_Position = u_mvp * p0;"
        "}"
        ;

    const GLchar fsh[] =
        "#version 330\n"
        ""
        "in vec3 v_normal;"
        "in vec3 v_position;"
        ""
        "layout(location = 0) out vec4 o_color;"
        ""
        "void main()"
        "{"
        "   vec3 color = vec3(0.23, 0.75, 0.21);"
        ""
        "   vec3 E = vec3(0.0, 0.0, 0.0);"
        "   vec3 L = vec3(5.0, 5.0, 0.0);"
        ""
        "   vec3 n = normalize(v_normal);"
        "   vec3 l = normalize(L - v_position);"
        ""
        "   float d = max(dot(n, l), 0.3);"
        ""
        "   o_color = vec4(color * d, 1.0);"
        "}"
        ;

    GLuint vertexShader, fragmentShader;

    vertexShader = createShader(vsh, GL_VERTEX_SHADER);
    fragmentShader = createShader(fsh, GL_FRAGMENT_SHADER);

    g_shaderProgram = createProgram(vertexShader, fragmentShader);

    g_uMVP = glGetUniformLocation(g_shaderProgram, "u_mvp");
    g_uMV = glGetUniformLocation(g_shaderProgram, "u_mv");
    g_uN = glGetUniformLocation(g_shaderProgram, "u_n");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return g_shaderProgram != 0;
}

bool createModel()
{
    const GLfloat vertices[] =
    {
        -1.0, -1.0, 1.0, 0.0, 0.0, 1.0, // 0
        1.0, -1.0, 1.0, 0.0, 0.0, 1.0, // 1
        1.0, 1.0, 1.0, 0.0, 0.0, 1.0, // 2
        -1.0, 1.0, 1.0, 0.0, 0.0, 1.0, // 3

        1.0, -1.0, 1.0, 1.0, 0.0, 0.0, // 4
        1.0, -1.0, -1.0, 1.0, 0.0, 0.0, // 5
        1.0, 1.0, -1.0, 1.0, 0.0, 0.0, // 6
        1.0, 1.0, 1.0, 1.0, 0.0, 0.0, // 7

        1.0, 1.0, 1.0, 0.0, 1.0, 0.0, // 8
        1.0, 1.0, -1.0, 0.0, 1.0, 0.0, // 9
        -1.0, 1.0, -1.0, 0.0, 1.0, 0.0, // 10
        -1.0, 1.0, 1.0, 0.0, 1.0, 0.0, // 11

        -1.0, 1.0, 1.0, -1.0, 0.0, 0.0, // 12
        -1.0, 1.0, -1.0, -1.0, 0.0, 0.0, // 13
        -1.0, -1.0, -1.0, -1.0, 0.0, 0.0, // 14
        -1.0, -1.0, 1.0, -1.0, 0.0, 0.0, // 15

        -1.0, -1.0, 1.0, 0.0, -1.0, 0.0, // 16
        -1.0, -1.0, -1.0, 0.0, -1.0, 0.0, // 17
        1.0, -1.0, -1.0, 0.0, -1.0, 0.0, // 18
        1.0, -1.0, 1.0, 0.0, -1.0, 0.0, // 19

        -1.0, -1.0, -1.0, 0.0, 0.0, -1.0, // 20
        -1.0, 1.0, -1.0, 0.0, 0.0, -1.0, // 21
        1.0, 1.0, -1.0, 0.0, 0.0, -1.0, // 22
        1.0, -1.0, -1.0, 0.0, 0.0, -1.0, // 23
    };

    const GLuint indices[] =
    {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    glGenVertexArrays(1, &g_model.vao);
    glBindVertexArray(g_model.vao);

    glGenBuffers(1, &g_model.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_model.vbo);
    glBufferData(GL_ARRAY_BUFFER, 24 * 6 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &g_model.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_model.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * 6 * sizeof(GLuint), indices, GL_STATIC_DRAW);

    g_model.indexCount = 6 * 6;

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const GLvoid*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const GLvoid*)(3 * sizeof(GLfloat)));

    return g_model.vbo != 0 && g_model.ibo != 0 && g_model.vao != 0;
}



bool init()
{
    // Set initial color of color buffer to white.
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    bool mainShaderProgamCreated = createShaderProgram() && createModel();
    bool billboardShaderProgramCreated = billboard.createShaderProgram() && billboard.createModel();

    return mainShaderProgamCreated && billboardShaderProgramCreated;
}

void reshape(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
    g_P = createPerspectiveProjectionMatrix(100.0f, 0.01f, 40.0f, width, height);
}

void draw(double deltaTime)
{
    // Draw main scene
    static float radius = 12.0;
    static float rotationAngle = 0.0f;

    // Clear color buffer.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_shaderProgram);
    glBindVertexArray(g_model.vao);

    static Matrix4 initialRotate = createRotateZMatrix(45.0f);
    static Matrix4 scale = createScaleMatrix(1.5f, 1.5f, 0.75f);

    Matrix4 M = initialRotate *
        createTranslateMatrix(cos(rotationAngle) * radius, 0.0f, sin(rotationAngle) * radius) *
        createRotateXMatrix(to_degrees(rotationAngle)) *
        createRotateZMatrix(to_degrees(rotationAngle)) * scale;

    Matrix4 V = createLookAtMatrix(cameraPos, cameraPos + cameraFront, cameraUp);

    Matrix4 MV = V * M;
    Matrix4 MVP = g_P * MV;
    Matrix3 N = getMainMinor(MV);

    glUniformMatrix4fv(g_uMVP, 1, GL_TRUE, MVP.elements);
    glUniformMatrix4fv(g_uMV, 1, GL_TRUE, MV.elements);
    glUniformMatrix3fv(g_uN, 1, GL_TRUE, N.elements);

    glDrawElements(GL_TRIANGLES, g_model.indexCount, GL_UNSIGNED_INT, NULL);

    rotationAngle = fmodf(rotationAngle + deltaTime, 2.0f * PI);

    // Draw billboard
    glUseProgram(billboard.shaderProgram);
    glBindVertexArray(billboard.model.vao);

    float s[3] = {
        Vector3(MV[0], MV[4], MV[8]).length() * 1.75f,
        Vector3(MV[1], MV[5], MV[9]).length() * 1.75f,
        Vector3(MV[2], MV[6], MV[10]).length() * 1.75f
    };

    Matrix4 billboardMVP = g_P * Matrix4(
        s[0], 0.0f, 0.0f, MV[3],
        0.0f, s[1], 0.0f, MV[7],
        0.0f, 0.0f, s[2], MV[11],
        0.0f, 0.0f, 0.0f, 1.0f
    );

    glUniformMatrix4fv(billboard.uMVP, 1, GL_TRUE, billboardMVP.elements);

    glDrawElements(GL_TRIANGLES, billboard.model.indexCount, GL_UNSIGNED_INT, NULL);
}

void cleanup()
{
    if (g_shaderProgram != 0)
        glDeleteProgram(g_shaderProgram);
    if (g_model.vbo != 0)
        glDeleteBuffers(1, &g_model.vbo);
    if (g_model.ibo != 0)
        glDeleteBuffers(1, &g_model.ibo);
    if (g_model.vao != 0)
        glDeleteVertexArrays(1, &g_model.vao);

    billboard.cleanup();
}

bool initOpenGL()
{
    // Initialize GLFW functions.
    if (!glfwInit())
    {
        cout << "Failed to initialize GLFW" << endl;
        return false;
    }

    // Request OpenGL 3.3 without obsoleted functions.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window.
    g_window = glfwCreateWindow(screen_width, screen_height, "Billboard OpenGL", NULL, NULL);
    if (g_window == NULL)
    {
        cout << "Failed to open GLFW window" << endl;
        glfwTerminate();
        return false;
    }

    // Initialize OpenGL context with.
    glfwMakeContextCurrent(g_window);

    // Set internal GLEW variable to activate OpenGL core profile.
    glewExperimental = true;

    // Initialize GLEW functions.
    if (glewInit() != GLEW_OK)
    {
        cout << "Failed to initialize GLEW" << endl;
        return false;
    }

    // Ensure we can capture the escape key being pressed.
    glfwSetInputMode(g_window, GLFW_STICKY_KEYS, GL_TRUE);

    // Set callback for framebuffer resizing event.
    glfwSetFramebufferSizeCallback(g_window, reshape);

    return true;
}

void tearDownOpenGL()
{
    // Terminate GLFW.
    glfwTerminate();
}

GLfloat to_radians(GLfloat degrees)
{
    return PI / 180.0f * degrees;
}

GLfloat to_degrees(GLfloat radians)
{
    return 180.0f / PI * radians;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (action == GLFW_PRESS)
        keys[key] = true;
    else if (action == GLFW_RELEASE)
        keys[key] = false;
}

void do_movement(double deltaTime)
{
    // Camera controls
    GLfloat cameraSpeed = 5.0f * deltaTime;
    if (keys[GLFW_KEY_W])
        cameraPos = cameraPos + cameraSpeed * cameraFront;
    if (keys[GLFW_KEY_S])
        cameraPos = cameraPos - cameraSpeed * cameraFront;
    if (keys[GLFW_KEY_A])
        cameraPos = cameraPos - Vector3::normalize(Vector3::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (keys[GLFW_KEY_D])
        cameraPos = cameraPos + Vector3::normalize(Vector3::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (keys[GLFW_KEY_Q])
        cameraPos = cameraPos - cameraUp * cameraSpeed;
    if (keys[GLFW_KEY_E])
        cameraPos = cameraPos + cameraUp * cameraSpeed;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    GLfloat xoffset = xpos - lastX;
    GLfloat yoffset = lastY - ypos; // Обратный порядок вычитания потому что оконные Y-координаты возрастают с верху вниз 
    lastX = xpos;
    lastY = ypos;

    GLfloat sensitivity = 0.075f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw = fmodf(yaw + xoffset, 360.0f);
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    else if (pitch < -89.0f)
        pitch = -89.0f;

    Vector3 front = Vector3(
        cos(to_radians(yaw)) * cos(to_radians(pitch)),
        sin(to_radians(pitch)),
        sin(to_radians(yaw)) * cos(to_radians(pitch))
    );

    cameraFront = Vector3::normalize(front);
}
