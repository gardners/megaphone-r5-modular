#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>

#define MAX_NESTED_BLOCKS 128  // Adjust as needed

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

void check_symbol(const char *mapped_data, size_t start, size_t end, FILE *temp_fp) {
    int modified = 0; // Flag to track whether modifications were made

    // Placeholder logic that might modify the symbol
    // (Currently, no modifications are made, so the flag remains 0)
    // This is where we would add parsing and editing logic in the future.

    if (!modified) {
        // No modifications, write the symbol as-is
        fwrite(mapped_data + start, 1, end - start , temp_fp);
    } else {
        // If modifications were made, they would be written here instead
        // (e.g., modifying some parts of the symbol before writing)
    }
}


void check_schematic(const char *filename) {
    size_t file_size;
    void *mapped_data = mmap_file(filename, &file_size);
    if (!mapped_data) {
        fprintf(stderr, "Failed to mmap: %s\n", filename);
        return;
    }

    // Create a temporary file
    char temp_filename[] = "temp_output.kicad_sch";
    FILE *temp_fp = fopen(temp_filename, "w");
    if (!temp_fp) {
        perror("fopen");
        munmap(mapped_data, file_size);
        return;
    }

    // Tracking block nesting
    size_t block_positions[MAX_NESTED_BLOCKS] = {0};  // Stack for '(' positions
    size_t block_depth = 0;
    int in_quotes = 0; // Flag to track whether inside quotes

    size_t last_written_pos = 0;  // Last position written to the temp file

    for (size_t i = 0; i < file_size; i++) {
        char c = ((char *)mapped_data)[i];

        if (c == '"') {
            in_quotes = !in_quotes;  // Toggle quote state
        } else if (!in_quotes) {
            if (c == '(') {
                if (block_depth < MAX_NESTED_BLOCKS) {
                    block_positions[block_depth++] = i;
                } else {
                    fprintf(stderr, "Error: Too many nested blocks in %s\n", filename);
                    break;
                }
            } else if (c == ')') {
                if (block_depth > 0) {
                    size_t start_pos = block_positions[--block_depth];

                    // Ensure start_pos points to the '(' and end points to the ')'
                    size_t end_pos = i;

                    // Check if it's a "(symbol" block
                    if (start_pos + 7 < file_size && 
                        strncmp((char *)mapped_data + start_pos + 1, "symbol", 6) == 0 &&
                        isspace(((char *)mapped_data)[start_pos + 7])) {

                        // Write non-symbol content before this symbol block
                        if (last_written_pos < start_pos) {
                            fwrite(mapped_data + last_written_pos, 1, start_pos - last_written_pos, temp_fp);
                        }

                        // Pass the correct start and end positions to check_symbol()
                        check_symbol(mapped_data, start_pos, end_pos, temp_fp);

                        // Update last written position
                        last_written_pos = end_pos + 1;
                    }
                }
            }
        }
    }

    // Write remaining non-symbol content after the last processed symbol
    if (last_written_pos < file_size) {
        fwrite((char *)mapped_data + last_written_pos, 1, file_size - last_written_pos, temp_fp);
    }

    fclose(temp_fp);
    munmap(mapped_data, file_size);

    printf("Processed schematic: %s -> %s\n", filename, temp_filename);
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

    search_kicad_sch(argv[1]);

    return EXIT_SUCCESS;
}
