#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <shlwapi.h>

#include <vector>

#include "libraries/GL/glew.h"

#define LK_PLATFORM_IMPLEMENTATION
#define LK_PLATFORM_NO_DLL
#include "libraries/lk_platform.h"


namespace colorbars
{

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef intptr_t smm;

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef uintptr_t umm;


#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))


#define Assert(test) \
    if (!(test)) \
    { \
        printf("Assertion (%s) failed! %s, line %d\n", #test, __FILE__, __LINE__); \
        DebugBreak(); \
    }

#ifdef DEBUG_BUILD
#define DebugAssert(test) Assert(test)
#else
#define DebugAssert(test)
#endif

#define BeginFunction(return_type, name, argument_list) struct StaticFunctionBoilerplate_##name { static return_type name argument_list
#define EndFunction() };
#define Function(name) StaticFunctionBoilerplate_##name::name

#define IllegalDefaultCase default: DebugAssert(!"Illegal default case");


template <typename F>
struct Defer_RAII
{
    F f;
    Defer_RAII(F f): f(f) {}
    ~Defer_RAII() { f(); }
};

template <typename F>
Defer_RAII<F> defer_function(F f)
{
    return Defer_RAII<F>(f);
}

#define Defer_1(x, y) x##y
#define Defer_2(x, y) Defer_1(x, y)
#define Defer_3(x)    Defer_2(x, __COUNTER__)
#define Defer(code)   auto Defer_3(_defer_) = defer_function([&] () { code; })


// Also appends a null terminator, in case you care about that, but that isn't returned in the length.
u8* read_entire_file(const char* path, u64* out_length = NULL)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        printf("Couldn't open file %s for reading!\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    u64 length = ftell(file);
    fseek(file, 0, SEEK_SET);

    u8* data = (u8*) malloc(length + 1);
    if (fread(data, length, 1, file) != 1)
    {
        printf("Couldn't read file %s!\n", path);
        free(data);
        return NULL;
    }

    data[length] = 0;

    if (out_length)
    {
        *out_length = length;
    }

    fclose(file);

    return data;
}

void set_working_directory()
{
    int size = MAX_PATH + 1;
    char* dir = (char*) LocalAlloc(LMEM_FIXED, size);
    while (GetModuleFileNameA(NULL, dir, size) == size)
    {
        LocalFree(dir);
        size *= 2;
        dir = (char*) LocalAlloc(LMEM_FIXED, size);
    }

    char* c = dir;
    while (*c) c++;
    while (c > dir)
    {
        c--;
        if (*c == '/' || *c == '\\')
        {
            *(c + 1) = 0;
            break;
        }
    }

    SetCurrentDirectoryA(dir);
    LocalFree(dir);
}


LK_CLIENT_EXPORT
void lk_client_init(LK_Platform* platform)
{
    set_working_directory();

    platform->window.title = "La Linea";
    platform->window.backend = LK_WINDOW_OPENGL;
    platform->window.disable_animations = true;

    platform->opengl.major_version = 1;
    platform->opengl.minor_version = 1;
    platform->opengl.swap_interval = 1;
    platform->opengl.sample_count  = 4;
    platform->opengl.depth_bits    = 0;
    platform->opengl.stencil_bits  = 0;
}

u8* current_file;

float animation_time = 0;

float source_modelview[] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

float target_modelview[] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

float modelview[16];

struct Point
{
    float x;
    float y;
};

std::vector<Point> points;

void parse_control()
{
    char* string = (char*) current_file;

    static char command[128];
    int bytes_read;

    points.clear();
    while (*string && sscanf(string, "%s%n", command, &bytes_read))
    {
        string += bytes_read;
        if (!strcmp(command, "transform"))
        {
            float input_matrix[9];
            sscanf(string, "%f%f%f%f%f%f%f%f%f%n", input_matrix + 0, input_matrix + 1, input_matrix + 2,
                                                   input_matrix + 3, input_matrix + 4, input_matrix + 5,
                                                   input_matrix + 6, input_matrix + 7, input_matrix + 8,
                                                   &bytes_read);

            string += bytes_read;

            memcpy(source_modelview, target_modelview, sizeof(source_modelview));
            animation_time = 0;
            target_modelview[0*4+0] = input_matrix[0*3+0];
            target_modelview[0*4+1] = input_matrix[1*3+0];
            target_modelview[0*4+2] = input_matrix[2*3+0];
            target_modelview[1*4+0] = input_matrix[0*3+1];
            target_modelview[1*4+1] = input_matrix[1*3+1];
            target_modelview[1*4+2] = input_matrix[2*3+1];
            target_modelview[2*4+0] = input_matrix[0*3+2];
            target_modelview[2*4+1] = input_matrix[1*3+2];
            target_modelview[2*4+2] = input_matrix[2*3+2];
        }
        else if (!strcmp(command, "point"))
        {
            float point[2];
            sscanf(string, "%f%f%n", point, point + 1, &bytes_read);
            string += bytes_read;

            points.push_back({ point[0], point[1] });
        }
    }
}

LK_CLIENT_EXPORT
void lk_client_frame(LK_Platform* platform)
{
    if (!glewExperimental)
    {
        glewExperimental = true;
        glewInit();
    }

    u8* file = read_entire_file("control.txt");
    if (file && (!current_file || strcmp((const char*) file, (const char*) current_file)))
    {
        free(current_file);
        current_file = file;
        parse_control();
    }

    animation_time += lk_platform.time.delta_seconds * 0.3f;
    if (animation_time > 1)
        animation_time = 1;

    for (int i = 0; i < 16; i++)
    {
        float t = animation_time * animation_time * (3 - 2 * animation_time);
        modelview[i] = t * target_modelview[i] + (1 - t) * source_modelview[i];
    }


    glViewport(0, 0, lk_platform.window.width, lk_platform.window.height);
    glClearColor(0.125, 0.125, 0.125, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    float w = lk_platform.window.width / (float) lk_platform.window.height * 20.0f;
    float h = 20;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-w*0.5f, w*0.5f, -h*0.5f, h*0.5f, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(modelview);

    glBegin(GL_QUADS);
    for (int xi = -200; xi < 200; xi++)
        for (int yi = -200; yi < 200; yi++)
        {
            if ((xi + yi) % 2) continue;

            int ri = (xi + 200) % 30;
            int gi = (yi + 200) % 30;
            float r = (ri < 15) ? (ri / 15.0f) : (1 - (ri - 15) / 15.0f);
            float g = (gi < 15) ? (gi / 15.0f) : (1 - (gi - 15) / 15.0f);

            glColor3f(r * 0.9, 0.9, g * 0.9);
            glVertex2f(xi,     yi);
            glVertex2f(xi + 1, yi);
            glVertex2f(xi + 1, yi + 1);
            glVertex2f(xi,     yi + 1);
        }
    glEnd();

    glLoadIdentity();

    glLineWidth(5);
    glBegin(GL_LINES);
    glColor3f(1, 0, 1);
    glVertex2f(-200, 0); glVertex2f(200, 0);
    glVertex2f(0, -200); glVertex2f(0, 200);
    glEnd();

    for (Point p : points)
    {
        glColor3f(1, 0.5, 0);
        glBegin(GL_LINES);
        glVertex2f(0, 0);
        glVertex2f(p.x, p.y);
        glEnd();

        glBegin(GL_POLYGON);
        for (int i = 0; i < 100; i++)
        {
            float x = p.x + sinf(i / 100.0f * 6.28318530718f) * 0.4f;
            float y = p.y + cosf(i / 100.0f * 6.28318530718f) * 0.4f;
            glVertex2f(x, y);
        }
        glEnd();
    }

    for (Point p : points)
    {
        float px = p.x * modelview[0*4+0] + p.y * modelview[1*4+0] + modelview[2*4+0];
        float py = p.x * modelview[0*4+1] + p.y * modelview[1*4+1] + modelview[2*4+1];

        glColor3f(1, 0, 0);
        glBegin(GL_LINES);
        glVertex2f(0, 0);
        glVertex2f(px, py);
        glEnd();

        glBegin(GL_POLYGON);
        for (int i = 0; i < 100; i++)
        {
            float x = px + sinf(i / 100.0f * 6.28318530718f) * 0.4f;
            float y = py + cosf(i / 100.0f * 6.28318530718f) * 0.4f;
            glVertex2f(x, y);
        }
        glEnd();
    }
}

}

