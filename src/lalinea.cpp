#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <shlwapi.h>

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

LK_CLIENT_EXPORT
void lk_client_frame(LK_Platform* platform)
{
    if (!glewExperimental)
    {
        glewExperimental = true;
        glewInit();
    }

    glViewport(0, 0, lk_platform.window.width, lk_platform.window.height);
    glClearColor(0.125, 0.125, 0.125, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    float w = lk_platform.window.width / (float) lk_platform.window.height * 20.0f;
    float h = 20;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-w*0.5f, w*0.5f, -h*0.5f, h*0.5f, -1, 1);

    float shear_matrix[] = {
        0.977568548, -0.210617505, 0, 0,
        0.210617505, 0.977568548, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLoadMatrixf(shear_matrix);

    glBegin(GL_QUADS);
    for (int xi = -200; xi < 200; xi++)
        for (int yi = -200; yi < 200; yi++)
        {
            if ((xi + yi) % 2) continue;

            int ri = (xi + 200) % 30;
            int gi = (yi + 200) % 30;
            float r = (ri < 15) ? (ri / 15.0f) : (1 - (ri - 15) / 15.0f);
            float g = (gi < 15) ? (gi / 15.0f) : (1 - (gi - 15) / 15.0f);

            glColor3f(r, 1, g);
            glVertex2f(xi,     yi);
            glVertex2f(xi + 1, yi);
            glVertex2f(xi + 1, yi + 1);
            glVertex2f(xi,     yi + 1);
        }
    glEnd();

    glLoadIdentity();
    glLineWidth(5);
    glBegin(GL_LINES);
    glColor3f(1, 0, 0);
    glVertex2f(-200, 0); glVertex2f(200, 0);
    glVertex2f(0, -200); glVertex2f(0, 200);
    glEnd();
}

}

