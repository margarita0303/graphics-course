#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif
 
#include <GL/glew.h>
 
#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
 
std::string to_string(std::string_view str)
{
    return std::string(str.begin(), str.end());
}
 
void sdl2_fail(std::string_view message)
{
    throw std::runtime_error(to_string(message) + SDL_GetError());
}
 
void glew_fail(std::string_view message, GLenum error)
{
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}
 
const char vertex_shader_source[] =
R"(#version 330 core
 
uniform mat4 view;
 
layout (location = 0) in vec2 in_position;
layout (location = 1) in vec4 in_color;
 
out vec4 color;
 
void main()
{
    gl_Position = view * vec4(in_position, 0.0, 1.0);
    color = in_color;
}
)";
 
const char fragment_shader_source[] =
R"(#version 330 core
 
in vec4 color;
 
layout (location = 0) out vec4 out_color;
 
void main()
{
    out_color = color;
}
)";
 
GLuint create_shader(GLenum type, const char * source)
{
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}
 
GLuint create_program(GLuint vertex_shader, GLuint fragment_shader)
{
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);
 
    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE)
    {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }
 
    return result;
}
 
struct vec2
{
    float x;
    float y;
};
 
struct vertex
{
    vec2 position;
    std::uint8_t color[4];
};
 
vec2 bezier(std::vector<vertex> const & vertices, float t)
{
    std::vector<vec2> points(vertices.size());
 
    for (std::size_t i = 0; i < vertices.size(); ++i)
        points[i] = vertices[i].position;
 
    // De Casteljau's algorithm
    for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
        for (std::size_t i = 0; i + k + 1 < vertices.size(); ++i) {
            points[i].x = points[i].x * (1.f - t) + points[i + 1].x * t;
            points[i].y = points[i].y * (1.f - t) + points[i + 1].y * t;
        }
    }
    return points[0];
}

void count_bezier_vertices(int quality, std::vector<vertex> &vertices, std::vector<vertex> &vertices_bezier) {
    vertices_bezier = {};
    for(int i = 0; i < quality; i++) {
        float t = 1.f * i / (quality - 1);
        vec2 new_vertex = bezier(vertices, t);
        vertices_bezier.push_back(vertex {{new_vertex}, {0, 0, 0, 0}});
    }
}

void update_vbos(int quality, std::vector<vertex> &vertices, 
                std::vector<vertex> &vertices_bezier, GLuint vbo, GLuint vbo_bezier) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(vertices[0]),
        vertices.data(), GL_STATIC_DRAW);

    count_bezier_vertices(quality, vertices, vertices_bezier);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_bezier);
    glBufferData(GL_ARRAY_BUFFER,
        vertices_bezier.size() * sizeof(vertices_bezier[0]),
        vertices_bezier.data(), GL_STATIC_DRAW);
}
 
int main() try
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");
 
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
 
    SDL_Window * window = SDL_CreateWindow("Graphics course practice 3",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
 
    if (!window)
        sdl2_fail("SDL_CreateWindow: ");
 
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
 
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");
 
    SDL_GL_SetSwapInterval(0);
 
    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);
 
    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");
 
    glClearColor(0.8f, 0.8f, 1.f, 0.f);
 
    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);
 
    GLuint view_location = glGetUniformLocation(program, "view");
 
    auto last_frame_start = std::chrono::high_resolution_clock::now();
 
    float time = 0.f;
 
    vertex v1 = vertex {{100, 100}, {20, 20, 20, 20}};
    vertex v2 = vertex {{100, 600}, {20, 20, 20, 20}};
    vertex v3 = vertex {{600, 100}, {20, 20, 20, 20}};
 
    std::vector<vertex> vertices = {v1, v2, v3};
 
    GLuint vbo;
    glGenBuffers(1, &vbo);
 
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(vertices[0]),
        vertices.data(), GL_STATIC_DRAW);
 
    float tmp_value;
    glGetBufferSubData(GL_ARRAY_BUFFER, sizeof(vertex), sizeof(float), &tmp_value);
    std::cout << tmp_value;
 
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
 
    // сейчас мы настраиваем атрибуты вершин, атрибута два: position, color (в структуре vertex)
 
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(vertex), (void*)(8));
 

    // Задание 6 - кривые Безье
 
    int quality = 30;
    std::vector<vertex> vertices_bezier = {};
    count_bezier_vertices(quality, vertices, vertices_bezier);
 
    GLuint vbo_bezier;
    glGenBuffers(1, &vbo_bezier);
 
    glBindBuffer(GL_ARRAY_BUFFER, vbo_bezier);
    glBufferData(GL_ARRAY_BUFFER,
        vertices_bezier.size() * sizeof(vertices_bezier[0]),
        vertices_bezier.data(), GL_STATIC_DRAW);
 
    GLuint vao_bezier;
    glGenVertexArrays(1, &vao_bezier);
    glBindVertexArray(vao_bezier);
 
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(vertex), (void*)(8));
 
    bool running = true;
    while (running)
    {
        for (SDL_Event event; SDL_PollEvent(&event);) switch (event.type)
        {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_WINDOWEVENT: switch (event.window.event)
            {
            case SDL_WINDOWEVENT_RESIZED:
                width = event.window.data1;
                height = event.window.data2;
                glViewport(0, 0, width, height);
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT)
            {
                int mouse_x = event.button.x;
                int mouse_y = event.button.y;
                vertices.push_back(vertex {{1.f * mouse_x, 1.f * mouse_y}, {20, 20, 20, 20}});
                update_vbos(quality, vertices, vertices_bezier, vbo, vbo_bezier);
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
                if (vertices.size() != 0) {
                    vertices.pop_back();
                    update_vbos(quality, vertices, vertices_bezier, vbo, vbo_bezier);
                }
            }
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_LEFT)
            {
                if (quality > 1) {
                    quality--;
                    update_vbos(quality, vertices, vertices_bezier, vbo, vbo_bezier);
                }
            }
            else if (event.key.keysym.sym == SDLK_RIGHT)
            {
                quality++;
                update_vbos(quality, vertices, vertices_bezier, vbo, vbo_bezier);
            }
            break;
        }
 
        if (!running)
            break;
 
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;
 
        glClear(GL_COLOR_BUFFER_BIT);
 
        float view[16] =
        {
            2.f/width, 0.f, 0.f, -1.f,
            0.f, -2.f/height, 0.f, 1.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };
 
        glUseProgram(program);
 
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);
 
        glLineWidth(5.f);
        glPointSize(40);
        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, vertices.size());
        glDrawArrays(GL_LINE_STRIP, 0, vertices.size());
        glBindVertexArray(vao_bezier);
        glPointSize(20);
        // glDrawArrays(GL_POINTS, 0, vertices_bezier.size());
        glDrawArrays(GL_LINE_STRIP, 0, vertices_bezier.size());
 
        SDL_GL_SwapWindow(window);
    }
 
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
 