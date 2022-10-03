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
#include <map>
 
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
 
struct colour
{
    std::uint8_t color[4];
};

float f(float x, float y, float t) {
    return (sin(x + t + y) - cos(y*2 + t) * cos(x) + sin(t / 2) + cos(x * y) * sin(t / 2)) / 4;
}

void init_coordinates_grid(std::vector<vec2>& points, long max_x, long max_y) {
    points.resize(max_x *  max_y);
    for (int i = 0; i < 2 * max_x; i += 2) {
        for (int j = 0; j < 2 * max_y; j += 2) {
            points[(i / 2) * max_y + (j / 2)] = {float(i - float(max_x - 1)) / float(max_x - 1), 
                float(j - float(max_y - 1)) / float(max_y - 1)};
        }
    }
}
 
void init_colour_grid(std::vector<colour>& point_colours, long max_x, long max_y) {
    point_colours.resize(max_x *  max_y);
    for (int i = 0; i < max_x; i++) {
        for (int j = 0; j < max_y; j++) {
            point_colours[i * max_y + j] = {255, 0, 0, 0};
        }
    }
}

void init_grid_indices(std::vector<unsigned int>& indices, long max_x, long max_y) {
    indices.clear();
    for (int i = 0; i < max_x - 1; i++) {
        for (int j = 0; j < max_y - 1; j++) {
            indices.push_back(i * max_y + j);
            indices.push_back((i + 1) * max_y + j);
            indices.push_back(i * max_y + (j + 1));
            indices.push_back((i + 1) * max_y + j);
            indices.push_back((i + 1) * max_y + (j + 1));
            indices.push_back(i * max_y + (j + 1));
        }
    }
}

void change_colour_grid(std::vector<vec2> const& points, std::vector<colour>& point_colours, float t) {
    for (int i = 0; i < point_colours.size(); i++) {
        uint8_t ans = abs(f(points[i].x, points[i].y, t)) * 255;
        point_colours[i] = {ans, 100, 100, 0};
    }
}

float coeff(float val1, float val2, int border) {
    return (border - val1) / (val2 - val1);
}

void add_new_point(uint8_t first_colour, int ind1, uint8_t second_colour, int ind2, float border, 
    std::vector<vec2> const& points, std::vector<vec2>& isopoints, std::map<std::pair<int, int>, int>& indices_map, 
        std::vector<unsigned int>& iso_indices) {
    float q = coeff(first_colour, second_colour, border);
    vec2 point = {points[ind1].x * (1 - q) + points[ind2].x * q, 
        points[ind1].y * (1 - q) + points[ind2].y * q};
    isopoints.push_back(point);
    if (indices_map.count({point.x, point.y}) == 0) {
        iso_indices.push_back(iso_indices.size());
    }
}

void create_isolines(std::vector<vec2>& isopoints, std::vector<unsigned int>& iso_indices, std::vector<vec2> const& points, 
                        std::vector<colour> const& point_colours, long max_x, long max_y, std::vector<uint8_t>& Cs) {
    isopoints.clear();
    iso_indices.clear();
    std::map<std::pair<int, int>, int> indices_map = {};
    for (int k = 0; k < Cs.size(); k++) {
        float border = Cs[k];
        for (int i = 0; i < max_x - 1; i++) {
            for (int j = 0; j < max_y - 1; j++) {
                auto lu = point_colours[i * max_y + j].color[0];
                auto lu_ind = i * max_y + j;
                auto ru = point_colours[(i + 1) * max_y + j].color[0];
                auto ru_ind = (i + 1) * max_y + j;
                auto ld = point_colours[i * max_y + (j + 1)].color[0];
                auto ld_ind = i * max_y + (j + 1);
                auto rd = point_colours[(i + 1) * max_y + (j + 1)].color[0];
                auto rd_ind = (i + 1) * max_y + (j + 1);
                int sum = (int)(lu > border) + (int)(ru > border) + (int)(ld > border) + (int)(rd > border);

                if (sum == 0 || sum == 4) {
                    continue;
                }
                else {
                    if ((lu > border) != (ru > border)) {
                        add_new_point(lu, lu_ind, ru, ru_ind, border, points, isopoints, indices_map, iso_indices);
                    }
                    if ((rd > border) != (ru > border)) {
                        add_new_point(rd, rd_ind, ru, ru_ind, border, points, isopoints, indices_map, iso_indices);
                    }
                    if ((ld > border) != (rd > border)) {
                        add_new_point(ld, ld_ind, rd, rd_ind, border, points, isopoints, indices_map, iso_indices);
                    }
                    if ((ld > border) != (lu > border)) {
                        add_new_point(ld, ld_ind, lu, lu_ind, border, points, isopoints, indices_map, iso_indices);
                    }
                }
            }
        }
    }
}

void update_grid(std::vector<vec2>& points, std::vector<colour>& point_colours, std::vector<unsigned int>& indices, 
        GLuint& points_vbo, GLuint& points_ebo, long grid_w, long grid_h) {
    init_coordinates_grid(points, grid_w, grid_h);
    init_colour_grid(point_colours, grid_w, grid_h);
    init_grid_indices(indices, grid_w, grid_h);

    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * points.size(), points.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, points_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), indices.data(), GL_DYNAMIC_DRAW);
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
 
    SDL_Window * window = SDL_CreateWindow("Homework 1",
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
 
    // Сетка из точек
 
    std::vector<vec2> points;
    std::vector<colour> point_colours;
    std::vector<unsigned int> points_indices;

    std::vector<vec2> isopoints;
    std::vector<unsigned int> iso_indices;
 
    int quality = 500;
    long grid_w = quality;
    long grid_h = quality;
 
    init_coordinates_grid(points, grid_w, grid_h);
    init_colour_grid(point_colours, grid_w, grid_h);
    init_grid_indices(points_indices, grid_w, grid_h);
 
    GLuint points_vbo;
    glGenBuffers(1, &points_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * points.size(), points.data(), GL_STATIC_DRAW);
 
    GLuint point_colours_vbo;
    glGenBuffers(1, &point_colours_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, point_colours_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colour) * point_colours.size(), point_colours.data(), GL_DYNAMIC_DRAW);

    GLuint points_ebo;
    glGenBuffers(1, &points_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, points_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * points_indices.size(), points_indices.data(), GL_DYNAMIC_DRAW);

    GLuint points_vao;
    glGenVertexArrays(1, &points_vao);
    glBindVertexArray(points_vao);

    glBindBuffer(GL_ARRAY_BUFFER, points_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), 0);

    glBindBuffer(GL_ARRAY_BUFFER, point_colours_vbo);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(colour), (void*)(8));

    // Запомнить на будущее: в цветах НЕ GL_FALSE, надо GL_TRUE
    // glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(colour), (void*)0);

    // Изолинии

    GLuint isolines_vbo;
    glGenBuffers(1, &isolines_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, isolines_vbo);

    GLuint isolines_vao;
    glGenVertexArrays(1, &isolines_vao);
    glBindVertexArray(isolines_vao);

    glBindBuffer(GL_ARRAY_BUFFER, isolines_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), (void*)0);

    GLuint isolines_ebo;
    glGenBuffers(1, &isolines_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, isolines_ebo);

    std::map<SDL_Keycode, bool> button_down;
    std::vector<uint8_t> Cs;
    Cs.push_back(200);
    Cs.push_back(100);
    Cs.push_back(50);

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
            }
            else if (event.button.button == SDL_BUTTON_RIGHT)
            {
 
            }
            break;
        case SDL_KEYDOWN:
            button_down[event.key.keysym.sym] = true;
            break;
        case SDL_KEYUP:
            button_down[event.key.keysym.sym] = false;
            break;
        }
 
        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;
 
        change_colour_grid(points, point_colours, time);
        glBindBuffer(GL_ARRAY_BUFFER, point_colours_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(colour) * point_colours.size(), point_colours.data(), GL_DYNAMIC_DRAW);

        create_isolines(isopoints, iso_indices, points, point_colours, grid_w, grid_h, Cs);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, isolines_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * iso_indices.size(), iso_indices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, isolines_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * isopoints.size(), isopoints.data(), GL_DYNAMIC_DRAW);
 
        glClear(GL_COLOR_BUFFER_BIT);

        float aspect_ratio = float(width) / height;
        float view[16] =
        {
            1.f/aspect_ratio, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
        };

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);

        glBindVertexArray(points_vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, points_ebo);
        glBindBuffer(GL_ARRAY_BUFFER, point_colours_vbo);
        glDrawElements(GL_TRIANGLES, points_indices.size(), GL_UNSIGNED_INT, (void*)0);

        glBindVertexArray(isolines_vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, isolines_ebo);
        glBindBuffer(GL_ARRAY_BUFFER, isolines_vbo);
        glLineWidth(5);
        glDrawElements(GL_LINES, iso_indices.size(), GL_UNSIGNED_INT, (void*)0);

        if (button_down[SDLK_LEFT]) {
            if (quality > 10) {
                quality -= 10;
                grid_w = quality;
                grid_h = quality;
                update_grid(points, point_colours, points_indices, points_vbo, points_ebo, grid_w, grid_h);
            }
        }
        else if (button_down[SDLK_RIGHT]) {
            quality += 10;
            grid_w = quality;
            grid_h = quality;
            update_grid(points, point_colours, points_indices, points_vbo, points_ebo, grid_w, grid_h);
        }
        else if (button_down[SDLK_UP]) {
            std::uint8_t last = Cs[Cs.size() - 1];
            Cs.push_back((std::uint8_t)((last + 200) % 255));
        }
        else if (button_down[SDLK_DOWN]) {
            if (Cs.size() > 1) {
                Cs.pop_back();
            }
        }

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