#include <stdio.h>
#include <stdlib.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define GLEW_STATIC
#include <glew.h>
#include <glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define UNUSED(x) ((void)(x))
#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720

#define SCALE_FACTOR 0.09f
#define SCALE_MAX 10.0f
#define SCALE_MIN 0.1f

#define global static
#define internal static

#define QUAD_VERTICES 4
#define QUAD_TRIANGLES 2
#define QUAD_ELEMENTS 3

#define ARR_LEN(arr) ((sizeof(arr))/sizeof(*arr))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* 
 * @ToDo: Some of the functionality could be in separate file(s) and just #include them for "unity build" (https://en.wikipedia.org/wiki/Unity_build)
 * @ToDo: Double click to reset camera to its default position?
 * @ToDo: When trying to display image, make sure it was properly loaded, otherwise do not render anything
*/ 

union Vec2
{
    float xy[2];

    struct {
        float x;
        float y;
    };
};

struct Vertex {
    Vec2 vertex_pos;
    Vec2 texture_pos;
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
    Vertex vertices[QUAD_VERTICES];
    Triangle indices[QUAD_TRIANGLES];

    unsigned int shader_program;
    unsigned int VAO;
    unsigned int VBO;
    
    unsigned int texture;
    float texture_width;
    float texture_height;
    
    Camera camera;
};

global const char* SUPPORTED_EXTENSIONS[] = {
    ".png",
    ".jpg",
    ".jpeg",
};

global const char *vertex_shader =
    "#version 330\n"
    "layout (location = 0) in vec2 aVertex_pos;\n"
    "layout (location = 1) in vec2 aTexture_pos;\n"
    "uniform vec2 resolution;\n"
    "out vec2 texture_pos;\n"
    "void main() {\n"
    "  vec2 pos = (aVertex_pos / resolution) * 2.0 - 1.0;\n"
    "  gl_Position = vec4(pos.x, -pos.y, 0.0, 1.0);\n"
    "  texture_pos = aTexture_pos;\n"
    "}";

global const char *fragment_shader =
    "#version 330\n"
    "in vec2 texture_pos;\n"
    "out vec4 frag_color;\n"
    "uniform sampler2D texture_data;\n"
    "void main() {\n"
    "  frag_color = texture(texture_data, texture_pos);\n"
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

internal inline void win32_error(const char *msg, const char *title)
{
    MessageBox(NULL, msg, title, MB_OK | MB_ICONWARNING | MB_TASKMODAL);
}

internal bool check_file_extension(const char *filename)
{
    const char *extension = filename;
    int length = static_cast<int> (strlen(filename));
    
    for (int i = length; i >= 0; --i) {
        if (filename[i] == '.') {
            extension = (filename + i);
            break;
        }
    }
    
    for (int i = 0; i < ARR_LEN(SUPPORTED_EXTENSIONS); ++i) {
        if (strcmp(extension, SUPPORTED_EXTENSIONS[i]) == 0) {
            return(true);
        }
    }
    
    return(false);
}

// @ToDo: There's most likely a better way to get current dimensions
// without this function and without storing it in Renderer struct
internal inline Vec2 get_shader_resolution(unsigned int shader_program)
{        
    Vec2 resolution = {0};
    
    glGetUniformfv(shader_program,
                   glGetUniformLocation(shader_program, "resolution"),
                   resolution.xy);

    return(resolution);
}

internal inline void fit_image_to_window(Renderer *renderer, float width, float height)
{
    Vec2 resolution = get_shader_resolution(renderer->shader_program);
    float scale = MIN(resolution.x / width, resolution.y / height);

    renderer->texture_width = width * scale;
    renderer->texture_height = height * scale;
}

internal void load_create_texture(Renderer *renderer, const char *filename)
{
    unsigned long file_attr = GetFileAttributes(filename);
    if ((file_attr == INVALID_FILE_ATTRIBUTES) || (file_attr & FILE_ATTRIBUTE_DIRECTORY)) {
        win32_error("Could not find the requested file.", "Incorrect path");
        return;
    }

    if (!check_file_extension(filename)) {
        win32_error("File format not currently supported.", "Incorrect format");
        return;
    }
        
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, 0);

    if (data == NULL) { 
        win32_error("Could not properly load the image.", "Memory/File format exception");
        return;
    }

    int format = (channels == 4 ? (GL_RGBA) : (GL_RGB));
    
    glGenTextures(1, &renderer->texture);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    fit_image_to_window(renderer, static_cast<float> (width), static_cast<float> (height));

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
}

internal void display_image_centered(Renderer *renderer)
{    
    float sx, sy;
    world_to_screen(&renderer->camera, 0.0f, 0.0f, &sx, &sy);

    float width = renderer->texture_width * renderer->camera.scale;
    float height = renderer->texture_height * renderer->camera.scale;
    
    renderer->vertices[0] = {{ sx - (width / 2.0f),         sy - (height / 2.0f) },          { 0.0f, 0.0f }};
    renderer->vertices[1] = {{ sx - (width / 2.0f) + width, sy - (height / 2.0f) },          { 1.0f, 0.0f }};
    renderer->vertices[2] = {{ sx - (width / 2.0f),         sy - (height / 2.0f) + height }, { 0.0f, 1.0f }};
    renderer->vertices[3] = {{ sx - (width / 2.0f) + width, sy - (height / 2.0f) + height }, { 1.0f, 1.0f }};
    
    renderer->indices[0] = {0, 1, 2};
    renderer->indices[1] = {1, 2, 3};
}

internal void gl_render(Renderer *renderer)
{    
    glBindVertexArray(renderer->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->VBO);
    
    // NOTE(Aiden): We are never rendering more than one texture really,
    // if that happens to be the case at some point (multiple images in one window or something)
    // this alongside immediate_quad_centered(); would need to be modified with something
    // like an array of textures and their respective IDs.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, renderer->texture);
        
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

    Vec2 old_resolution = get_shader_resolution(renderer->shader_program);
    camera->offset_x = width * (camera->offset_x / static_cast<float> (old_resolution.x));
    camera->offset_y = height * (camera->offset_y / static_cast<float> (old_resolution.y));
    
    glUseProgram(renderer->shader_program);
    glUniform2f(glGetUniformLocation(renderer->shader_program, "resolution"),
                static_cast<float> (width),
                static_cast<float> (height));
    
    fit_image_to_window(renderer, renderer->texture_width, renderer->texture_height);
    glViewport(0, 0, width, height);
}

// NOTE(Aiden): We _could_ update the mouse position everytime we click instead of doing
// this, however, this is a much simpler solution and it doesn't cause that much "overhead"
// (if you can even call it that). Otherwise we'd have to update the mouse position both
// on-click and on-scroll as well as keep the update here because of panning logic.
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

    // @ToDo: Find a better way to make the motion smooth, maybe something with delta-time?
    // instead of enabling v-sync.
    glfwSwapInterval(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    return(window);
}

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    GLFWwindow *window = create_window(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Hello, Sailor!");
    Renderer renderer = {0};
    
    // Shader setup
    {
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
    }
    
    // Render setup
    {
        glGenVertexArrays(1, &renderer.VAO);
        glGenBuffers(1, &renderer.VBO);

        glBindVertexArray(renderer.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, renderer.VBO);
        
        glBufferData(GL_ARRAY_BUFFER, sizeof(renderer.vertices), renderer.vertices, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, vertex_pos));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *) offsetof(Vertex, texture_pos));
        glEnableVertexAttribArray(1);
    
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    // Initial conditions
    renderer.camera.offset_x = -(DEFAULT_WIDTH / 2.0f);
    renderer.camera.offset_y = -(DEFAULT_HEIGHT / 2.0f);
    renderer.camera.scale = 1.0f;

    load_create_texture(&renderer, "../example.png");
    glfwSetWindowUserPointer(window, &renderer);
    
    while (!glfwWindowShouldClose(window)) {
        display_image_centered(&renderer);
        
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        gl_render(&renderer);
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &renderer.VAO);
    glDeleteBuffers(1, &renderer.VBO);
    glDeleteTextures(1, &renderer.texture);
    glDeleteProgram(renderer.shader_program);
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
