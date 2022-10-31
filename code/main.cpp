#include <stdio.h>
#include <stdlib.h>

#define GLEW_STATIC
#include <glew.h>
#include <glfw3.h>

#define UNUSED(x) ((void)(x))
#define WIDTH 1280
#define HEIGHT 720

#define global static
#define internal static

#define QUAD_VERTICES 4
#define QUAD_TRIANGLES 2
#define SCALE_FACTOR 0.05f

// TODO(Aiden): This is a macro for now
#define CAMERA_PTR(window) (&(((Renderer *) glfwGetWindowUserPointer((window)))->camera))

typedef struct
{
    float x;
    float y;
} Vec2;

typedef struct
{
    unsigned int a;
    unsigned int b;
    unsigned int c;
} Triangle;

typedef struct
{
    float offset_x;
    float offset_y;

    float mouse_x;
    float mouse_y;

    float scale;
} Camera;

typedef struct
{
    Vec2 vertices[QUAD_VERTICES];
    Triangle indices[QUAD_TRIANGLES];
    
    Camera camera;
} Renderer;

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

internal void immediate_quad_centered(Renderer *renderer, float x, float y, float size)
{
    float wx, wy;
    world_to_screen(&renderer->camera, x, y, &wx, &wy);
    
    size *= renderer->camera.scale;
    renderer->vertices[0] = { wx - (size / 2.0f),        wy - (size / 2.0f) };
    renderer->vertices[1] = { wx - (size / 2.0f) + size, wy - (size / 2.0f) };
    renderer->vertices[2] = { wx - (size / 2.0f),        wy - (size / 2.0f) + size };
    renderer->vertices[3] = { wx - (size / 2.0f) + size, wy - (size / 2.0f) + size };

    renderer->indices[0] = { 0, 1, 2 };
    renderer->indices[1] = { 1, 2, 3 };
}

internal void cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
    int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    Camera *camera = CAMERA_PTR(window);
    
    if (state == GLFW_RELEASE) {
        camera->mouse_x = static_cast<float> (xpos);
        camera->mouse_y = static_cast<float> (ypos);
    } else {
        camera->offset_x -= (static_cast<float> (xpos) - camera->mouse_x) / camera->scale;
        camera->offset_y -= (static_cast<float> (ypos) - camera->mouse_y) / camera->scale;

        camera->mouse_x = static_cast<float> (xpos);
        camera->mouse_y = static_cast<float> (ypos);
    }
}

internal void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    UNUSED(xoffset);
    
    Camera *camera = CAMERA_PTR(window);
    
    float before_x, before_y;
    screen_to_world(camera, camera->mouse_x, camera->mouse_y, &before_x, &before_y);
    
    if (yoffset < 0.0) {
        camera->scale *= (1.0f - SCALE_FACTOR);
    } else if (yoffset > 0.0) {
        camera->scale *= (1.0f + SCALE_FACTOR);
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
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    
    Renderer renderer = {0};
    
    glBufferData(GL_ARRAY_BUFFER, QUAD_VERTICES * sizeof(Vec2), renderer.vertices, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vec2), (void *) offsetof(Renderer, vertices));
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Initial conditions
    renderer.camera.offset_x = -(WIDTH / 2.0f);
    renderer.camera.offset_y = -(HEIGHT / 2.0f);
    renderer.camera.scale = 1.0f;

    glfwSetWindowUserPointer(window, &renderer);
    
    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        immediate_quad_centered(&renderer, 0.0f, 0.0f, 150.0f);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, QUAD_VERTICES * sizeof(Vec2), renderer.vertices);
        glDrawElements(GL_TRIANGLES, QUAD_TRIANGLES * 3, GL_UNSIGNED_INT, renderer.indices);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shader_program);
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
