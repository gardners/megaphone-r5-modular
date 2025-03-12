#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parts-library.h"

#define INITIAL_PARTS_CAPACITY 50

typedef struct {
    char part_number[MAX_PROPERTY_LENGTH];
    char description[MAX_PROPERTY_LENGTH];
    char manufacturer[MAX_PROPERTY_LENGTH];
    char unit_price[MAX_PROPERTY_LENGTH];
    char stock[MAX_PROPERTY_LENGTH];
    char product_url[MAX_PROPERTY_LENGTH];
} PartInfo;

// Function to initialize the parts library
void init_parts_library(PartsLibrary *lib) {
    lib->size = 0;
    lib->capacity = INITIAL_PARTS_CAPACITY;
    lib->parts = (PartRecord *)malloc(lib->capacity * sizeof(PartRecord));
    if (!lib->parts) {
        fprintf(stderr, "Memory allocation failed for parts library\n");
        exit(1);
    }
}

// Function to grow the parts library dynamically
void expand_parts_library(PartsLibrary *lib) {
    lib->capacity *= 2;
    lib->parts = (PartRecord *)realloc(lib->parts, lib->capacity * sizeof(PartRecord));
    if (!lib->parts) {
        fprintf(stderr, "Memory reallocation failed\n");
        exit(1);
    }
}

// Function to trim newline characters from strings
void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
    }
}

// Function to load parts from CSV file
void load_parts_library(PartsLibrary *lib, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Warning: Could not open %s for reading. Starting with an empty library.\n", filename);
        return;
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        if (lib->size >= lib->capacity) {
            expand_parts_library(lib);
        }

        PartRecord *part = &lib->parts[lib->size];
        
        if (sscanf(line, "%63[^,],%127[^,],%127[^,],%127[^,],%255[^,]", part->symbol, part->value, part->footprint, part->mpn, part->digikey_url) == 5) {
	  // We don't read quantity, as we write baskets out to a separate file
            part->quantity = 0;
            trim_newline(part->digikey_url);
            lib->size++;
        }
    }
    fclose(file);
}

// Function to save parts back to CSV file
void save_parts_library(const PartsLibrary *lib, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return;
    }
    
    for (size_t i = 0; i < lib->size; i++) {
        fprintf(file, "%s,%s,%s,%s,%s,%d\n", lib->parts[i].symbol, lib->parts[i].value, lib->parts[i].footprint, lib->parts[i].mpn, lib->parts[i].digikey_url, lib->parts[i].quantity);
    }
    fclose(file);
}

void save_basket(const PartsLibrary *lib, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return;
    }

    fprintf(file,"MPN,quantity\n");
    
    for (size_t i = 0; i < lib->size; i++) {
        fprintf(file, "%s,%d\n", lib->parts[i].mpn, lib->parts[i].quantity);
    }
    fclose(file);
}

// Function to find a part by symbol, value, and footprint
PartRecord *find_part(PartsLibrary *lib, const char *symbol, const char *value, const char *footprint) {
    for (size_t i = 0; i < lib->size; i++) {
        if (strcmp(lib->parts[i].symbol, symbol) == 0 && strcmp(lib->parts[i].value, value) == 0 && strcmp(lib->parts[i].footprint, footprint) == 0) {
            return &lib->parts[i];
        }
    }
    return NULL;
}

// Function to add a new part to the library
void add_part(PartsLibrary *lib, const char *symbol, const char *value, const char *footprint, const char *mpn, const char *digikey_url, int quantity) {
    if (lib->size >= lib->capacity) {
        expand_parts_library(lib);
    }
    
    PartRecord *new_part = &lib->parts[lib->size];
    strncpy(new_part->symbol, symbol, sizeof(new_part->symbol) - 1);
    strncpy(new_part->value, value, sizeof(new_part->value) - 1);
    strncpy(new_part->footprint, footprint, sizeof(new_part->footprint) - 1);
    strncpy(new_part->mpn, mpn, sizeof(new_part->mpn) - 1);
    strncpy(new_part->digikey_url, digikey_url, sizeof(new_part->digikey_url) - 1);
    new_part->quantity = quantity;
    
    lib->size++;
}

// Cleanup function
void free_parts_library(PartsLibrary *lib) {
    free(lib->parts);
}

PartInfo parse_csv(const char *filename) {
    PartInfo part;
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];

    // Skip the header
    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Error reading CSV header.\n");
        exit(EXIT_FAILURE);
    }

    // Read the first (and only) result row
    if (fgets(line, sizeof(line), file) != NULL) {
        sscanf(line, "%255[^,],%255[^,],%255[^,],%255[^,],%255[^,],%255[^\n]",
               part.part_number, part.description, part.manufacturer,
               part.unit_price, part.stock, part.product_url);
    } else {
        fprintf(stderr, "Error reading CSV data.\n");
        exit(EXIT_FAILURE);
    }

    fclose(file);
    return part;
}

#ifdef DEMO_MODE
// Example usage
int main() {
    PartsLibrary lib;
    init_parts_library(&lib);
    
    load_parts_library(&lib, "parts-library.csv");
    
    PartRecord *found = find_part(&lib, "R1", "10k", "0805");
    if (found) {
        printf("Found part: %s, MPN: %s, DigiKey URL: %s\n", found->symbol, found->mpn, found->digikey_url);
    } else {
        printf("Part not found\n");
    }
    
    add_part(&lib, "R2", "4.7k", "0603", "MPN123", "https://www.digikey.com/en/products/detail/MPN123", 100);
    save_parts_library(&lib, "parts-library.csv");
    
    free_parts_library(&lib);
    return 0;
}
#endif 
