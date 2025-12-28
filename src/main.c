#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "file_to_c_array.h"

static int get_option(const char *option, const char *arg, const char **value);
static int add_extension(const char *path, const char *extension, char *out, size_t out_len);
static int is_valid_ident(const char *ident, int enable_non_ascii);

int main(int argc, char **argv) {
    const char *file = NULL;
    const char *header = NULL;
    const char *source = NULL;
    const char *variable = NULL;

    for (int i = 1; i < argc; i++) {
        if (get_option("file=", argv[i], &file));
        else if (get_option("header=", argv[i], &header));
        else if (get_option("head=", argv[i], &header));
        else if (get_option("source=", argv[i], &source));
        else if (get_option("src=", argv[i], &source));
        else if (get_option("variable=", argv[i], &variable));
        else if (get_option("var=", argv[i], &variable));
        else {
            fprintf(stderr, "Invalid option: %s\n", argv[i]);
        }
    }

    if (file == NULL) {
        fputs("Error: unspecified file\n", stderr);
        return 1;
    }

    if (variable == NULL) {
        fputs("Error: unspecified variable name\n", stderr);
        return 1;
    } else if (!is_valid_ident(variable, 0)) {
        if (is_valid_ident(variable, 1)) {
            fputs("Warning: some compilers do not support non ascii variable names\n", stderr);
        } else {
            fprintf(stderr, "Error: %s is not a valid variable name\n", variable);
            return 1;
        }
    }

    if (header == NULL) {
        char *header_gen = alloca(1024);
        if (!add_extension(file, ".h", header_gen, 1024)) {
            printf("Error generating header file path: file path too large\n");
            return 1;
        }
        printf("Auto generated header path: %s\n", header_gen);
        header = header_gen;
    }

    if (source == NULL) {
        char *source_gen = alloca(1024);
        if (!add_extension(file, ".c", source_gen, 1024)) {
            printf("Error generating source file path: file path too large\n");
            return 1;
        }
        printf("Auto generated source path: %s\n", source_gen);
        source = source_gen;
    }

    if (file_to_c_array(file, header, source, variable)) {
        fputs("Success\n", stdout);
    } else {
        fprintf(stderr, "%s\n", file_to_c_array_get_error());
    }
}

static int get_option(const char *option, const char *arg, const char **value) {
    size_t len = strlen(option);
    if (strncmp(option, arg, len) == 0) {
        if (arg[len] != '\0')
            *value = arg + len;
        return 1;
    } else {
        return 0;
    }
}

static int add_extension(const char *path, const char *extension, char *out, size_t out_len) {
    if (path[0] == '\0') return 0;

    size_t i = strlen(path) - 1;
    while (i >= 0 && (path[i] == '/' || path[i] == '\\')) i--;
    i++;

    if (i > out_len) return 0;
    memcpy(out, path, i);
    out += i;
    out_len -= i;

    size_t ext_len = strlen(extension);
    if (ext_len > out_len) return 0;
    memcpy(out, extension, ext_len);
    out += ext_len;
    out_len -= ext_len;

    if (out_len < 1) return 0;
    out[0] = '\0';

    return 1;
}

static int is_valid_ident(const char *ident, int enable_non_ascii) {
    if (ident[0] == '\0') return 0;

    const unsigned char *uident = (const unsigned char*)ident;

    if (enable_non_ascii) {
        if (isascii(uident[0]) && !isalpha(uident[0]) && uident[0] != '_')
            return 0;

        for (size_t i = 1; ident[i] != '\0'; i++) {
            if (isascii(uident[i]) && !isalnum(uident[i]) && uident[i] != '_')
                return 0;
        }
    } else {
        if (!isascii(uident[0]) && !isalpha(uident[0]) && uident[0] != '_')
            return 0;

        for (size_t i = 1; ident[i] != '\0'; i++) {
            if (!isalnum(uident[i]) && uident[i] != '_')
                return 0;
        }
    }

    return 1;
}
