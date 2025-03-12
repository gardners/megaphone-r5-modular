#ifndef PARTS_LIBRARY_H
#define PARTS_LIBRARY_H

#include <stddef.h>

#define MAX_LINE_LENGTH 2048

// Structure to store a single part record
typedef struct {
    char symbol[64];
    char value[128];
    char footprint[128];
    char mpn[128];
    char digikey_url[256];
    int quantity;
} PartRecord;

// Structure to store the entire parts library
typedef struct {
    PartRecord *parts;
    size_t size;
    size_t capacity;
} PartsLibrary;

// Functions to manage the parts library
void init_parts_library(PartsLibrary *lib);
void load_parts_library(PartsLibrary *lib, const char *filename);
void save_parts_library(const PartsLibrary *lib, const char *filename);
void save_basket(const PartsLibrary *lib, const char *filename);
PartRecord *find_part(PartsLibrary *lib, const char *symbol, const char *value, const char *footprint);
void add_part(PartsLibrary *lib, const char *symbol, const char *value, const char *footprint, const char *mpn, const char *digikey_url, int quantity);
void free_parts_library(PartsLibrary *lib);
PartInfo parse_csv(const char *filename);

#endif // PARTS_LIBRARY_H
