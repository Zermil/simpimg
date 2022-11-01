#include <stdio.h>
#include <stdlib.h>

#define GLEW_STATIC
#include <glew.h>
#include <glfw3.h>

#define UNUSED(x) ((void)(x))
#define WIDTH 1280
#define HEIGHT 720

#define SCALE_FACTOR 0.05f
#define SCALE_MAX 10.0f
#define SCALE_MIN 0.1f

#define global static
#define internal static

#define QUAD_VERTICES 4
#define QUAD_TRIANGLES 2
#define QUAD_ELEMENTS 3

struct Vec2
{
    float x;
    float y;
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

internal void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    Camera *camera = static_cast<Camera *> (glfwGetWindowUserPointer(window));
    
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
    
    Camera *camera = static_cast<Camera *> (glfwGetWindowUserPointer(window));
    
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

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

    return(window);
}

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    GLFWwindow *window = create_window(WIDTH, HEIGHT, "Hello, Sailor!");

    // Shader
    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vert, 1, &vertex_shader, NULL);
    glCompileShader(vert);

    glShaderSource(frag, 1, &fragment_shader, NULL);
    glCompileShader(frag);

    unsigned int shader_program = glCreateProgram();

    glAttachShader(shader_program, vert);
    glAttachShader(shader_program, frag);
    glLinkProgram(shader_program);
    
    glDeleteShader(vert);
    glDeleteShader(frag);
    
    glUseProgram(shader_program);
    glUniform2f(glGetUniformLocation(shader_program, "resolution"), WIDTH, HEIGHT);
    
    // Render
    Renderer renderer = {0};
    
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
    renderer.camera.offset_x = -(WIDTH / 2.0f);
    renderer.camera.offset_y = -(HEIGHT / 2.0f);
    renderer.camera.scale = 1.0f;

    glfwSetWindowUserPointer(window, &renderer.camera);
    
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
    glDeleteProgram(shader_program);
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
