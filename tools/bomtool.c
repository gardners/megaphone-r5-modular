#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

#include "parts-library.h"

#define MAX_NESTED_BLOCKS 128
#define MAX_PROPERTIES 256
#define MAX_PROPERTY_NAME 128
#define MAX_PROPERTY_VALUE 256

int debug_parsing = 0;

PartsLibrary parts;

typedef struct {
    char name[MAX_PROPERTY_NAME];
    char value[MAX_PROPERTY_VALUE];
} Property;

typedef struct {
    char symbol_name[MAX_PROPERTY_VALUE];
    Property properties[MAX_PROPERTIES];
    int property_count;
} SymbolInfo;

// mmap function to read files
void *mmap_file(const char *filename, size_t *size) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return NULL;
    }

    *size = st.st_size;
    void *mapped = mmap(NULL, *size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (mapped == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    return mapped;
}

void check_symbol(const unsigned char *mapped_data, size_t start, size_t end, FILE *temp_fp, SymbolInfo *symbol) {

  int modified=0;
  // Ignore power symbols (including GND)
  if (strncmp("power:",symbol->symbol_name,strlen("power:"))) {

    char device[1024] = "";
    char value[1024]="";
    char footprint[1024]="";     
    snprintf(device,sizeof(device),"%s",symbol->symbol_name);

    for (int i = 0; i < symbol->property_count; i++) {
      if (0) printf("  Property: %s = %s\n", symbol->properties[i].name, symbol->properties[i].value);
      if (!strcasecmp("Value",symbol->properties[i].name))
	snprintf(value,sizeof(value),"%s",symbol->properties[i].value);
      if (!strcasecmp("Footprint",symbol->properties[i].name))
	snprintf(footprint,sizeof(footprint),"%s",symbol->properties[i].value);
    }
    printf("Searching for part (%s, %s, %s)\n", device, value, footprint);	   
    
    PartRecord *found = find_part(&parts, device, value, footprint);
    
    
    
    // Placeholder logic that might modify the symbol
    // (Currently, no modifications are made, so the flag remains 0)
    // This is where we would add parsing and editing logic in the future.
  }
    
    if (!modified) {
        // No modifications, write the symbol as-is
        fwrite(mapped_data + start, 1, end - start + 1 , temp_fp);
    } else {
        // If modifications were made, they would be written here instead
        // (e.g., modifying some parts of the symbol before writing)
    }
}

void check_schematic(const char *filename) {
    size_t file_size;
    unsigned char *mapped_data = mmap_file(filename, &file_size);
    if (!mapped_data) {
        fprintf(stderr, "Failed to mmap: %s\n", filename);
        return;
    }

    char temp_filename[] = "temp_output.kicad_sch";
    FILE *temp_fp = fopen(temp_filename, "w");
    if (!temp_fp) {
        perror("fopen");
        munmap(mapped_data, file_size);
        return;
    }

    size_t block_positions[MAX_NESTED_BLOCKS] = {0};
    char block_names[MAX_NESTED_BLOCKS][256] = {{0}};
    size_t block_depth = 0;
    int in_quotes = 0;
    int lib_symbols_ended = 0;

    size_t last_written_pos = 0;
    SymbolInfo current_symbol = { .symbol_name = "", .property_count = 0 };

    for (size_t i = 0; i < file_size; i++) {
        unsigned char c = mapped_data[i];

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (!in_quotes) {
            if (c == '(') {
                if (block_depth < MAX_NESTED_BLOCKS) {
                    block_positions[block_depth] = i;

                    size_t name_start = i + 1;
                    size_t name_end = name_start;
                    while (name_end < file_size && !isspace(mapped_data[name_end]) && mapped_data[name_end] != ')') {
                        name_end++;
                    }
                    size_t name_length = name_end - name_start;
                    if (name_length > 0 && name_length < 255) {
		      strncpy(block_names[block_depth], (char *)(mapped_data + name_start), name_length);
                        block_names[block_depth][name_length] = '\0';
                    } else {
                        strcpy(block_names[block_depth], "(unknown)");
                    }
		    
		    if (debug_parsing) {
		      for (int j = 0; j < block_depth; j++) printf("  ");		    
		      printf("(%s at index %zu, depth %zu\n", block_names[block_depth], i, block_depth);
		    }

                    // If this is a (symbol ...) block, capture the symbol name
                    if (strcmp(block_names[block_depth], "symbol") == 0) {
                        current_symbol.property_count = 0; // Reset properties

                        size_t symbol_start = name_end + 1;
                        while (symbol_start < file_size && mapped_data[symbol_start] != '"') {
                            symbol_start++;
                        }
                        size_t symbol_end = symbol_start + 1;
                        while (symbol_end < file_size && mapped_data[symbol_end] != '"') {
                            symbol_end++;
                        }

                        size_t symbol_len = symbol_end - symbol_start - 1;
                        if (symbol_len > 0 && symbol_len < MAX_PROPERTY_VALUE) {
			  strncpy(current_symbol.symbol_name,
				  (const char *)(mapped_data + symbol_start + 1),
				  symbol_len);
                            current_symbol.symbol_name[symbol_len] = '\0';
                        }
                    }

                    block_depth++;

	    if (block_depth > 1 && strcmp(block_names[block_depth - 1], "property") == 0) {
	      if (block_depth > 2 && strcmp(block_names[block_depth - 2], "symbol") == 0) {
		
		size_t prop_start = i;
		while (prop_start < file_size && mapped_data[prop_start] != '"') {
		  prop_start++;
		}
		size_t prop_end = prop_start + 1;
		while (prop_end < file_size && mapped_data[prop_end] != '"') {
		  prop_end++;
		}
		// printf(">> Property block at (%d,%d)\n",prop_start,prop_end);
		
		// **Extract Property Name**
		size_t prop_name_len = prop_end - prop_start - 1;
		if (prop_name_len > 0 && prop_name_len < MAX_PROPERTY_NAME) {
		  strncpy(current_symbol.properties[current_symbol.property_count].name, 
			  (char *)(mapped_data + prop_start + 1), prop_name_len);
		  current_symbol.properties[current_symbol.property_count].name[prop_name_len] = '\0';
		  
		  if (0) printf(">> property name = '%s' @ %zu\n", 
				current_symbol.properties[current_symbol.property_count].name, prop_start);
		}
		
		// **Skip to property value**
		prop_start = prop_end + 1;
		while (prop_start < file_size && mapped_data[prop_start] != '"') {
		  prop_start++;
		}
		prop_end = prop_start + 1;
		while (prop_end < file_size && mapped_data[prop_end] != '"') {
		  prop_end++;
		}
		
		// **Extract Property Value**
		size_t prop_value_len = prop_end - prop_start - 1;
		if (prop_value_len > 0 && prop_value_len < MAX_PROPERTY_VALUE) {
		  strncpy(current_symbol.properties[current_symbol.property_count].value, 
			  (char *)(mapped_data + prop_start + 1), prop_value_len);
		  current_symbol.properties[current_symbol.property_count].value[prop_value_len] = '\0';
		  
		  if (0) printf(">> property value = '%s'\n", 
				current_symbol.properties[current_symbol.property_count].value);
		}
		
		// **Ensure properties are correctly paired**
		if (strlen(current_symbol.properties[current_symbol.property_count].name) > 0 &&
		    strlen(current_symbol.properties[current_symbol.property_count].value) > 0) {
		  current_symbol.property_count++;
		}
		
	      }
	    }
	    


		    
                } else {
                    fprintf(stderr, "Error: Too many nested blocks in %s\n", filename);
                    break;
                }
            } else if (c == ')') {
                if (block_depth > 0) {
                    block_depth--;
                    size_t start_pos = block_positions[block_depth];
                    size_t end_pos = i;
		    
		    if (debug_parsing) {
		      for (int j = 0; j <= block_depth; j++) printf("  ");
		      printf("%s) at index %zu, depth %zu\n", block_names[block_depth], i, block_depth);
		    }

                    if (!strcmp(block_names[block_depth], "lib_symbols")) {
                        lib_symbols_ended = 1;
                    }

                    if (lib_symbols_ended) {
                        if (start_pos + 7 < file_size &&
                            strncmp((char *)mapped_data + start_pos + 1, "symbol", 6) == 0 &&
                            isspace(((char *)mapped_data)[start_pos + 7])) {
                            
                            if (last_written_pos < start_pos) {
                                fwrite(mapped_data + last_written_pos, 1, start_pos - last_written_pos, temp_fp);
                            }

                            check_symbol(mapped_data, start_pos, end_pos, temp_fp, &current_symbol);

                            last_written_pos = end_pos + 1;
                        }
                    }
                }
            } 
	    
        }
    }

    if (last_written_pos < file_size) {
        fwrite((char *)mapped_data + last_written_pos, 1, file_size - last_written_pos, temp_fp);
    }

    fclose(temp_fp);
    munmap(mapped_data, file_size);
}

// Recursive function to traverse directories and find *.kicad_sch files
void search_kicad_sch(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct full path
        char full_path[4096];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        // Check if it's a directory
        struct stat st;
        if (stat(full_path, &st) == -1) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recurse into subdirectory
            search_kicad_sch(full_path);
	} else {
	  size_t name_len = strlen(entry->d_name);
	  size_t ext_len = strlen(".kicad_sch");
	  
	  if (name_len > ext_len && strcmp(entry->d_name + name_len - ext_len, ".kicad_sch") == 0) {
	    check_schematic(full_path);
	  }
	}
    }

    closedir(dir);
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    init_parts_library(&parts);
    load_parts_library(&parts,"parts-library.csv");
    
    search_kicad_sch(argv[1]);

    return EXIT_SUCCESS;
}
