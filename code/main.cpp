#include <stdio.h>
#include <stdlib.h>

#define GLEW_STATIC
#include <glew.h>
#include <glfw3.h>

#define UNUSED(x) ((void)(x))
#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720

#define SCALE_FACTOR 0.05f
#define SCALE_MAX 10.0f
#define SCALE_MIN 0.1f

#define global static
#define internal static

#define QUAD_VERTICES 4
#define QUAD_TRIANGLES 2
#define QUAD_ELEMENTS 3

union Vec2
{
    float xy[2];

    struct {
        float x;
        float y;
    };
};

struct Triangle
{
    unsigned int a;
    unsigned int b;
    unsigned int c;
};

struct Camera
{
    float offset_x;
    float offset_y;

    float mouse_x;
    float mouse_y;

    float scale;
};

struct Renderer
{
    Vec2 vertices[QUAD_VERTICES];
    Triangle indices[QUAD_TRIANGLES];

    unsigned int shader_program;
    unsigned int VAO;
    unsigned int VBO;
    
    Camera camera;
};

global const char *vertex_shader =
    "#version 330\n"
    "layout (location = 0) in vec2 aPos;\n"
    "uniform vec2 resolution;\n"
    "void main() {\n"
    "  vec2 pos = (aPos / resolution) * 2.0 - 1.0;\n"
    "  gl_Position = vec4(pos.x, -pos.y, 0.0, 1.0);\n"
    "}";

global const char *fragment_shader =
    "#version 330\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "  frag_color = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "}";

internal inline void screen_to_world(Camera *camera, float sx, float sy, float *wx, float *wy)
{
    *wx = (sx / camera->scale) + camera->offset_x;
    *wy = (sy / camera->scale) + camera->offset_y;
}

internal inline void world_to_screen(Camera *camera, float wx, float wy, float *sx, float *sy)
{
    *sx = (wx - camera->offset_x) * camera->scale;
    *sy = (wy - camera->offset_y) * camera->scale;
}

internal void immediate_quad_centered(Renderer *renderer, float x, float y, float width, float height)
{
    float sx, sy;
    world_to_screen(&renderer->camera, x, y, &sx, &sy);
    
    width *= renderer->camera.scale;
    height *= renderer->camera.scale;
    
    renderer->vertices[0] = { sx - (width / 2.0f),         sy - (height / 2.0f) };
    renderer->vertices[1] = { sx - (width / 2.0f) + width, sy - (height / 2.0f) };
    renderer->vertices[2] = { sx - (width / 2.0f),         sy - (height / 2.0f) + height };
    renderer->vertices[3] = { sx - (width / 2.0f) + width, sy - (height / 2.0f) + height };
    
    renderer->indices[0] = { 0, 1, 2 };
    renderer->indices[1] = { 1, 2, 3 };
}

internal void gl_render(Renderer *renderer)
{
    glBindVertexArray(renderer->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(renderer->vertices), renderer->vertices);
    glDrawElements(GL_TRIANGLES, QUAD_TRIANGLES * QUAD_ELEMENTS, GL_UNSIGNED_INT, renderer->indices);
}

internal void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
        return;
    }
    
    Renderer *renderer = static_cast<Renderer *> (glfwGetWindowUserPointer(window));
    Camera *camera = &renderer->camera;
    
    unsigned int resolution_loc = glGetUniformLocation(renderer->shader_program, "resolution");

    Vec2 old_resolution;
    glGetUniformfv(renderer->shader_program, resolution_loc, old_resolution.xy);
    
    camera->offset_x = width * (camera->offset_x / old_resolution.x);
    camera->offset_y = height * (camera->offset_y / old_resolution.y);
    
    glUseProgram(renderer->shader_program);
    glUniform2f(resolution_loc, static_cast<float> (width), static_cast<float> (height));    
    glViewport(0, 0, width, height);
}

internal void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

    Renderer *renderer = static_cast<Renderer *> (glfwGetWindowUserPointer(window));
    Camera *camera = &renderer->camera;
    
    if (state == GLFW_PRESS) {
        camera->offset_x -= (static_cast<float> (xpos) - camera->mouse_x) / camera->scale;
        camera->offset_y -= (static_cast<float> (ypos) - camera->mouse_y) / camera->scale;
    }
    
    camera->mouse_x = static_cast<float> (xpos);
    camera->mouse_y = static_cast<float> (ypos);
}

internal void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    UNUSED(xoffset);

    Renderer *renderer = static_cast<Renderer *> (glfwGetWindowUserPointer(window));
    Camera *camera = &renderer->camera;
        
    float before_x, before_y;
    screen_to_world(camera, camera->mouse_x, camera->mouse_y, &before_x, &before_y);
    
    if (yoffset < 0.0) {
        camera->scale *= (1.0f - SCALE_FACTOR);
        if (camera->scale < SCALE_MIN) camera->scale = SCALE_MIN;
    } else if (yoffset > 0.0) {
        camera->scale *= (1.0f + SCALE_FACTOR);
        if (camera->scale > SCALE_MAX) camera->scale = SCALE_MAX;
    }

    float after_x, after_y;
    screen_to_world(camera, camera->mouse_x, camera->mouse_y, &after_x, &after_y);

    camera->offset_x += (before_x - after_x);
    camera->offset_y += (before_y - after_y);
}

internal GLFWwindow* create_window(unsigned int width, unsigned int height, const char* title)
{
    glfwInit();
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow *window = glfwCreateWindow(width, height, title, NULL, NULL);
    
    if (window == NULL) {
        fprintf(stderr, "[ERROR]: Could not create window!\n");
        glfwTerminate();
        exit(1);
    }

    glfwMakeContextCurrent(window);

    unsigned int err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "[ERROR]: Could not initialize glew: %s\n", glewGetErrorString(err));
        glfwTerminate();
        exit(1);        
    }

    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    return(window);
}

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    GLFWwindow *window = create_window(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Hello, Sailor!");
    Renderer renderer = {0};
    
    // Shader
    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vert, 1, &vertex_shader, NULL);
    glCompileShader(vert);

    glShaderSource(frag, 1, &fragment_shader, NULL);
    glCompileShader(frag);

    renderer.shader_program = glCreateProgram();

    glAttachShader(renderer.shader_program, vert);
    glAttachShader(renderer.shader_program, frag);
    glLinkProgram(renderer.shader_program);
    
    glDeleteShader(vert);
    glDeleteShader(frag);
    
    glUseProgram(renderer.shader_program);
    glUniform2f(glGetUniformLocation(renderer.shader_program, "resolution"), DEFAULT_WIDTH, DEFAULT_HEIGHT);
    
    // Render
    glGenVertexArrays(1, &renderer.VAO);
    glGenBuffers(1, &renderer.VBO);

    glBindVertexArray(renderer.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, renderer.VBO);
        
    glBufferData(GL_ARRAY_BUFFER, sizeof(renderer.vertices), renderer.vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), (void *) offsetof(Renderer, vertices));
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Initial conditions
    renderer.camera.offset_x = -(DEFAULT_WIDTH / 2.0f);
    renderer.camera.offset_y = -(DEFAULT_HEIGHT / 2.0f);
    renderer.camera.scale = 1.0f;

    glfwSetWindowUserPointer(window, &renderer);
    
    while (!glfwWindowShouldClose(window)) {
        immediate_quad_centered(&renderer, 0.0f, 0.0f, 150.0f, 300.0f);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        gl_render(&renderer);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &renderer.VAO);
    glDeleteBuffers(1, &renderer.VBO);
    glDeleteProgram(renderer.shader_program);
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
