#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum Output
{
    OUTPUT_CLIENT,
    OUTPUT_SERVER,
    OUTPUT_GENKEY,
    N_OUTPUTS,
};

static const char *sources_client[] =
{
    "src/engine/bih.cpp",
    "src/engine/blend.cpp",
    "src/engine/blob.cpp",
    "src/engine/client.cpp",
    "src/engine/command.cpp",
    "src/engine/console.cpp",
    "src/engine/decal.cpp",
    "src/engine/dynlight.cpp",
    "src/engine/glare.cpp",
    "src/engine/grass.cpp",
    "src/engine/irc.cpp",
    "src/engine/lightmap.cpp",
    "src/engine/main.cpp",
    "src/engine/master.cpp",
    "src/engine/material.cpp",
    "src/engine/menus.cpp",
    "src/engine/movie.cpp",
    "src/engine/normal.cpp",
    "src/engine/octa.cpp",
    "src/engine/octaedit.cpp",
    "src/engine/octarender.cpp",
    "src/engine/physics.cpp",
    "src/engine/pvs.cpp",
    "src/engine/rendergl.cpp",
    "src/engine/rendermodel.cpp",
    "src/engine/renderparticles.cpp",
    "src/engine/rendersky.cpp",
    "src/engine/rendertext.cpp",
    "src/engine/renderva.cpp",
    "src/engine/server.cpp",
    "src/engine/serverbrowser.cpp",
    "src/engine/shader.cpp",
    "src/engine/shadowmap.cpp",
    "src/engine/sound.cpp",
    "src/engine/texture.cpp",
    "src/engine/ui.cpp",
    "src/engine/water.cpp",
    "src/engine/world.cpp",
    "src/engine/worldio.cpp",
    "src/game/ai.cpp",
    "src/game/bomber.cpp",
    "src/game/capture.cpp",
    "src/game/client.cpp",
    "src/game/defend.cpp",
    "src/game/entities.cpp",
    "src/game/game.cpp",
    "src/game/hud.cpp",
    "src/game/physics.cpp",
    "src/game/projs.cpp",
    "src/game/scoreboard.cpp",
    "src/game/server.cpp",
    "src/game/waypoint.cpp",
    "src/game/weapons.cpp",
    "src/shared/crypto.cpp",
    "src/shared/geom.cpp",
    "src/shared/glemu.cpp",
    "src/shared/stream.cpp",
    "src/shared/tools.cpp",
    "src/shared/zip.cpp",
    NULL,
};
static const char *sources_server[] =
{
    "src/shared/crypto.cpp",
    "src/shared/geom.cpp",
    "src/shared/stream.cpp",
    "src/shared/tools.cpp",
    "src/shared/zip.cpp",
    "src/engine/command.cpp",
    "src/engine/irc.cpp",
    "src/engine/master.cpp",
    "src/engine/server.cpp",
    "src/game/server.cpp",
    NULL,
};
static const char *sources_genkey[] =
{
    "src/engine/genkey.cpp",
    "src/shared/crypto.cpp",
    NULL,
};

static const char **sources_all[] =
{
    sources_client,
    sources_server,
    sources_genkey,
};
static const char *names_all[] =
{
    "client",
    "server",
    "genkey",
};
static const char *executables_all[] =
{
    "fragility",
    "fragility_server",
    "genkey",
};
static const char *flags_all[] =
{
    "",
    "-DSTANDALONE",
    "-DSTANDALONE",
};

static const char *incdirs[] =
{
    "src/enet/include",
    "src/engine",
    "src/game",
    "src/shared",
    "/usr/include/SDL2",
};
#define CXXFLAGS "-std=gnu++14 -Wall -O3 -DNDEBUG -ffast-math -fsigned-char -fno-rtti -fno-exceptions -fomit-frame-pointer"
#define LIBS "-lm -lrt -lz -lGL -lSDL2 -lSDL2_mixer -lSDL2_image"
#define STATIC_LIBS "src/enet/libenet.a"

#define ARR_LEN(a) (sizeof (a) / sizeof *(a))

struct Str
{
    const char *data;
    size_t len;
};

static struct Str to_str(const char *s)
{
    return (struct Str){s, strlen(s)};
}

static int str_eq(struct Str a, struct Str b)
{
    if(a.len != b.len) return 0;
    for(size_t i = 0; i < a.len; i++)
    {
        if(a.data[i] != b.data[i]) return 0;
    }
    return 1;
}

struct StrSet
{
    struct Str *arr;
    size_t len;
    size_t alloc;
};

static int set_add(struct StrSet *set, struct Str str)
{
    // Don't add if already exists.
    for(size_t i = 0; i < set->len; i++)
    {
        if(str_eq(set->arr[i], str))
        {
            return 0;
        }
    }

    if(set->len == set->alloc)
    {
        set->alloc = set->alloc < 8 ? 8 : set->alloc * 2;
        set->arr = realloc(set->arr, set->alloc * sizeof *set->arr);
    }

    set->arr[set->len] = str;
    set->len++;
    return 1;
}

static int file_exists(const char *filename)
{
    FILE *f = fopen(filename, "r");
    if(f) fclose(f);
    return f != NULL;
}

static char *make_filename(char *storage, size_t storage_size, struct Str basedir, struct Str filename)
{
    snprintf(storage, storage_size, "%.*s/%.*s",
            basedir.len, basedir.data, filename.len, filename.data);
    // TODO: Handle ".."
    return storage;
}

static char *fix_include_name(const char *basefile, struct Str include)
{
    struct Str basedir = {basefile, 0};
    for(size_t i = 0; basefile[i]; i++)
    {
        if(basefile[i] == '/') basedir.len = i;
    }

    char test[512];

    if(file_exists(make_filename(test, sizeof test, basedir, include)))
    {
        return strdup(test);
    }
    for(int i = 0; i < ARR_LEN(incdirs); i++)
    {
        if(file_exists(make_filename(test, sizeof test, to_str(incdirs[i]), include)))
        {
            return strdup(test);
        }
    }
    //fprintf(stderr, "Not found: '%.*s' from '%s'\n", include.len, include.data, basefile);
    return NULL;
}

// Returns number of characters read and puts included filename in `result` if found.
// Returns 0 if an include statement was not found.
static size_t read_include_line(struct Str *result, const char *buf)
{
    size_t i = 0;
    while(buf[i] == ' ') i++;
    if(buf[i] == '#') i++;
    else return 0;
    while(buf[i] == ' ') i++;
    if(strncmp(&buf[i], "include", 7) == 0) i += 7;
    else return 0;
    while(buf[i] == ' ') i++;
    if(buf[i] == '"' || buf[i] == '<')
    {
        char quote = buf[i] == '<' ? '>' : buf[i];
        i++;
        size_t start = i;
        while(buf[i] && buf[i] != quote) i++;
        if(buf[i] == quote)
        {
            // We found an include!
            *result = (struct Str){buf + start, i - start};
            return i + 1;
        }
        else return 0;
    }
    else return 0;
}

// Finds all files included (recursively) by `filename` and puts all included filenames in `set`.
static void find_includes(struct StrSet *set, const char *filename)
{
    FILE *f = fopen(filename, "r");
    if(f == NULL)
    {
        perror("fopen");
        exit(1);
    }
    char *buf = NULL;
    size_t buflen = 0;
    // Read file into memory.
    for(;;)
    {
        size_t alloc_size = buflen * 2 < 4096 ? 4096 : buflen * 2;
        buf = realloc(buf, alloc_size);
        size_t s = fread(buf + buflen, 1, alloc_size - buflen - 1, f);  // -1 for '\0' at end.
        if(s == 0) break;
        buflen += s;
    }
    buf[buflen] = '\0';

    size_t i = 0;
    // Find #include lines.
    for(;;)
    {
        struct Str incstr;
        size_t s = read_include_line(&incstr, buf + i);
        if(s)
        {
            char *incname = fix_include_name(filename, incstr);
            if(incname)
            {
                if(set_add(set, to_str(incname)))
                {
                    find_includes(set, incname);
                }
            }
            i += s;
        }
        else while(buf[i] > '\n') i++;
        if(buf[i] && buf[i] != '\n') i++;
        if(buf[i] == '\n') i++;
        if(buf[i] == '\0') break;
    }

    free(buf);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *output_type = "make";
    for(char **arg = &argv[1]; *arg; arg++)
    {
        if(strncmp(*arg, "type=", 5) == 0) output_type = *arg + 5;
    }

    if(strcmp(output_type, "make") == 0)
    {
        FILE *outfd = fopen("Makefile", "wb");
        if(outfd == NULL)
        {
            perror("fopen");
            exit(1);
        }

        fprintf(outfd, "# Auto-generated by tools/make-project-files\n\n");
        fprintf(outfd, "CXX=c++\n");
        fprintf(outfd, "CXXFLAGS=%s", CXXFLAGS);
        for(int i = 0; i < ARR_LEN(incdirs); i++)
        {
            fprintf(outfd, " -I%s", incdirs[i]);
        }
        fprintf(outfd, "\n");
        fprintf(outfd, "LDFLAGS=%s\n", LIBS);

        fprintf(outfd, "\nall:");
        for(enum Output output = 0; output < N_OUTPUTS; output++)
        {
            const char *output_file = executables_all[output];
            fprintf(outfd, " %s", output_file);
        }
        fprintf(outfd, "\n");

        for(enum Output output = 0; output < N_OUTPUTS; output++)
        {
            const char **source_files = sources_all[output];
            const char *output_file = executables_all[output];
            const char *output_name = names_all[output];

            fprintf(outfd, "\n%s:", output_file);
            for(int i = 0; source_files[i]; i++)
            {
                fprintf(outfd, " %s.%s.o", source_files[i], output_name);
            }
            fprintf(outfd, " %s\n", STATIC_LIBS);
            fprintf(outfd, "\t$(CXX) $^ $(LDFLAGS) -o %s\n\n", output_file);

            // All the files.
            for(int i = 0; source_files[i]; i++)
            {
                struct StrSet set = {0};
                find_includes(&set, source_files[i]);
                fprintf(outfd, "%s.%s.o: %s", source_files[i], output_name, source_files[i]);
                for(int i = 0; i < set.len; i++)
                {
                    // Don't add absolute include paths to dependency list.
                    if(set.arr[i].data[0] != '/') fprintf(outfd, " %s", set.arr[i]);
                }
                fprintf(outfd, "\n\t$(CXX) $(CXXFLAGS) %s -c $< -o $@\n", flags_all[output]);
            }

            fprintf(outfd, "\n.PHONY: clean_%s\nclean_%s:\n\trm -f %s", output_name, output_name, output_file);
            for(int i = 0; source_files[i]; i++)
            {
                fprintf(outfd, " %s.%s.o", source_files[i], output_name);
            }
            fprintf(outfd, "\n");
        }
        fprintf(outfd, "\n.PHONY: clean\nclean:");
        for(enum Output output = 0; output < N_OUTPUTS; output++)
        {
            const char *output_name = names_all[output];
            fprintf(outfd, " clean_%s", output_name);
        }
        fprintf(outfd, "\n");

        fclose(outfd);
    }
    else if(strcmp(output_type, "sh") == 0)
    {
        FILE *outfd = fopen("build.sh", "wb");
        if(outfd == NULL)
        {
            perror("fopen");
            exit(1);
        }
        chmod("build.sh", 0744);

        fprintf(outfd, "#!/bin/sh\n");
        fprintf(outfd, "# Auto-generated by tools/make-project-files\n\n");
        for(enum Output output = 0; output < N_OUTPUTS; output++)
        {
            const char **source_files = sources_all[output];
            const char *output_file = executables_all[output];

            fprintf(outfd, "c++ %s %s", CXXFLAGS, flags_all[output]);
            for(int i = 0; i < ARR_LEN(incdirs); i++)
            {
                fprintf(outfd, " -I%s", incdirs[i]);
            }
            for(int i = 0; source_files[i]; i++)
            {
                fprintf(outfd, " %s", source_files[i]);
            }
            fprintf(outfd, " %s", STATIC_LIBS);
            fprintf(outfd, " %s", LIBS);
            fprintf(outfd, " -o %s\n", output_file);
        }

        fclose(outfd);
    }
    else
    {
        fprintf(stderr, "Unknown output type: %s\n", output_type);
        fprintf(stderr, "Possible values: make, sh\n");
        exit(1);
    }
}
