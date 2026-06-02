#ifndef _NUTTYELFLOADER_H
#define _NUTTYELFLOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <esp_err.h>

typedef struct NuttyELFLoaderOptions
{
    const char *path;
    int argc;
    char **argv;
} NuttyELFLoaderOptions;

esp_err_t NuttyELFLoader_Init(void);
esp_err_t NuttyELFLoader_RunFromPath(const char *elf_path, int argc, char **argv);
esp_err_t NuttyELFLoader_Run(const NuttyELFLoaderOptions *options);
esp_err_t NuttyELFLoader_RegisterHostSymbols(void);
void NuttyELFLoader_Deinit(void);

#endif /* _NUTTYELFLOADER_H */