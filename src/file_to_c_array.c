#include "file_to_c_array.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#define SET_ERROR(fmt, ...) snprintf(error_msg, sizeof(error_msg), fmt, __VA_ARGS__)
#define SET_ERROR_ERRNO(fmt, ...) SET_ERROR(fmt ": %s", __VA_ARGS__, strerror(errno))
#define RESET_ERROR() do { error_msg[0] = '\0'; } while (0)

static char error_msg[1024] = "";
const char *file_to_c_array_get_error() { return error_msg; }

// helper functions

// returns a pointer to the beginning of the basename and sets the value of `basename_length`
static const char *get_basename(const char *path, size_t *basename_length);
// if `malloc` fails, it returns `NULL` and sets the error message
static char *generate_include_guard(const char *header_path);
// returns 1 on success and 0 on failure; when it fails, it does not set the error message
static int get_file_size(FILE *fp, size_t *size);
// if `malloc` fails, it returns `NULL` and sets the error message
static void *read_file_content(const char *file_path, size_t *size);

// returns 1 on success and 0 on failure; when it fails, it sets the error message
static int generate_header(const char *header_path, const char *var_name, size_t file_size);
// returns 1 on success and 0 on failure; when it fails, it sets the error message
static int generate_source(const char *source_path, const char *var_name,
                            const unsigned char *file_content, size_t file_size);

int file_to_c_array(const char *file_path, const char *header_path, const char *source_path, const char *var_name) {
    size_t file_size;
    unsigned char *file_content = read_file_content(file_path, &file_size);
    if (file_content == NULL) {
        return 0;
    }

    if (!generate_header(header_path, var_name, file_size)) {
        free(file_content);
        return 0;
    }

    if (!generate_source(source_path, var_name, file_content, file_size)) {
        free(file_content);
        unlink(header_path);
        return 0;
    }

    RESET_ERROR();
    free(file_content);
    return 1;
}

static const char *get_basename(const char *path, size_t *basename_length) {
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            path = p + 1;
        }
    }

    size_t name_length = strlen(path);
    while (1) {
        char last_c = path[name_length - 1];
        if (last_c == '/' || last_c == '\\') {
            --name_length;
        } else {
            break;
        }
    }

    *basename_length = name_length;
    return path;
}

static char *generate_include_guard(const char *header_path) {
    size_t basename_length;
    const char *basename = get_basename(header_path, &basename_length);

    // '_' + include guard + '\0'
    char *include_guard = malloc(basename_length + 2);
    if (!include_guard) {
        SET_ERROR("Error allocating %zu bytes", basename_length + 2);
        return NULL;
    }

    include_guard[0] = '_';
    include_guard[basename_length + 1] = '\0';
    for (size_t i = 0; i < basename_length; i++) {
        unsigned char c = basename[i];
        if (isalpha(c)) c = toupper(c);
        else if (!isalnum(c)) c = '_';
        include_guard[i+1] = c;
    }

    return include_guard;
}

static int get_file_size(FILE *fp, size_t *size) {
    long pos = ftell(fp);
    if (pos == -1) return 0;

    if (fseek(fp, 0, SEEK_END) == -1)
        return 0;

    long size_tell = ftell(fp);
    if (size_tell == -1) return 0;

    if (fseek(fp, pos, SEEK_SET) == -1)
        return 0;

    *size = size_tell;
    return 1;
}

static void *read_file_content(const char *file_path, size_t *size) {
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        SET_ERROR_ERRNO("Failed to open %s", file_path);
        return NULL;
    }

    if (!get_file_size(fp, size)) {
        SET_ERROR_ERRNO("Failed to obtain the size of %s", file_path);
        fclose(fp);
        return NULL;
    }

    void *content = malloc(*size);
    if (!content) {
        SET_ERROR("Failed to allocate %zu bytes", *size);
        fclose(fp);
        return NULL;
    }

    if (fread(content, 1, *size, fp) != *size) {
        SET_ERROR("Failed to read %s", file_path);
        free(content);
        fclose(fp);
        return NULL;
    }

    return content;
}

static int generate_header(const char *header_path, const char *var_name, size_t file_size) {
    FILE *fp = fopen(header_path, "w");
    if (!fp) {
        SET_ERROR_ERRNO("Failed to open %s", header_path);
        return 0;
    }

    char *include_guard = generate_include_guard(header_path);
    if (!include_guard) {
        fclose(fp);
        return 0;
    }

    const char *header_fmt = \
        "#ifndef %s\n" \
        "#define %s\n" \
        "\n" \
        "extern char %s[%zu];\n" \
        "\n" \
        "#endif\n";
    if (fprintf(
            fp, header_fmt,
            include_guard,
            include_guard,
            var_name, file_size
        ) < 0) {
        SET_ERROR_ERRNO("Failed to write to %s", header_path);
        free(include_guard);
        fclose(fp);
        unlink(header_path);
        return 0;
    }

    free(include_guard);
    fclose(fp);
    return 1;
}

static int generate_source(const char *source_path, const char *var_name,
                           const unsigned char *file_content, size_t file_size) {
    FILE *fp = fopen(source_path, "w");
    if (!fp) {
        SET_ERROR_ERRNO("Failed to open %s", source_path);
        return 0;
    }

    const char *var_fmt = "char %s[%zu] = {\n    ";
    if (fprintf(fp, var_fmt, var_name, file_size) < 0) {
        SET_ERROR_ERRNO("Failed to write to %s", source_path);
        fclose(fp);
        return 0;
    }

    for (size_t i = 0, line = 0; i < file_size; i++, line++) {
        const char *fmt = "0x%02x, ";

        if ((i + 1) == file_size) {
            fmt = "0x%02x\n};\n";
        } else if (line == 11) {
            line = -1;
            fmt = "0x%02x,\n    ";
        }

        if (fprintf(fp, fmt, file_content[i]) < 0) {
            SET_ERROR_ERRNO("Failed to write to %s", source_path);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return 1;
}
