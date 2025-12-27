#pragma once

// Returns 1 on success and 0 on failure.
int file_to_c_array(const char *file, const char *header, const char *source, const char *var_name);
const char *file_to_c_array_get_error();
