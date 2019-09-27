#ifndef API_TEST_UTILS_H
#define API_TEST_UTILS_H

#ifdef __cplusplus
#include "../../aktualizr/tests/test_utils.h"

using TemporaryDirectory = TemporaryDirectory;
using UptaneGenerator = Process;

extern "C" {
#else
typedef struct TemporaryDirectory TemporaryDirectory;
typedef struct UptaneGenerator UptaneGenerator;
#endif

void Run_fake_http_server(const char* path);

UptaneGenerator* Get_uptane_generator(const char* path);
void Run_uptane_generator(UptaneGenerator* generator, const char** args, size_t args_count);
void Remove_uptane_generator(UptaneGenerator* generator);

TemporaryDirectory* Get_temporary_directory();
char* Get_temporary_directory_path(const TemporaryDirectory* dir);
void Remove_temporary_directory(TemporaryDirectory* dir);

#ifdef __cplusplus
}
#endif

#endif  // API_TEST_UTILS_H
