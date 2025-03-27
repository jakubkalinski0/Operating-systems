#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_PATH_LENGTH 1024
#define MAX_LINE_LENGTH 4096

// Function to reverse a given string
void reverse_string(char *str) {
    int length = strlen(str);
    int i, j;
    char temp;

    // Remove trailing newline character if present
    if (str[length - 1] == '\n') {
        str[length - 1] = '\0';
        length--;
    }

    // Reverse the string in place
    for (i = 0, j = length - 1; i < j; i++, j--) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
    }
}

// Function to process a file: read lines, reverse them, and write to a destination file
int process_file(const char *source_path, const char *dest_path) {
    FILE *source_file, *dest_file;
    char line[MAX_LINE_LENGTH];

    // Open the source file for reading
    source_file = fopen(source_path, "r");
    if (source_file == NULL) {
        fprintf(stderr, "Error opening source file: %s\n", source_path);
        return 1;
    }

    // Open the destination file for writing
    dest_file = fopen(dest_path, "w");
    if (dest_file == NULL) {
        fprintf(stderr, "Error opening destination file: %s\n", dest_path);
        fclose(source_file);
        return 1;
    }

    // Read each line, reverse it, and write to the destination file
    while (fgets(line, sizeof(line), source_file) != NULL) {
        reverse_string(line);
        fprintf(dest_file, "%s\n", line);
    }

    fclose(source_file);
    fclose(dest_file);

    return 0;
}

// Function to create the destination directory if it doesn't exist
int create_dest_dir(const char *dest_dir) {
    struct stat st = {0};

    // Check if the directory exists, if not, create it
    if (stat(dest_dir, &st) != 0) {
        if (mkdir(dest_dir, 0700) != 0) {
            fprintf(stderr, "Error creating destination directory: %s\n", dest_dir);
            return 1;
        }
    }

    return 0;
}

// Function to check if a file has a .txt extension
int is_text_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    return (ext && strcmp(ext, ".txt") == 0);
}

// Function to process all text files in a directory
int process_directory(const char *source_dir, const char *dest_dir) {
    DIR *dir;
    struct dirent *entry;
    char source_path[MAX_PATH_LENGTH];
    char dest_path[MAX_PATH_LENGTH];
    struct stat file_stat;

    // Create destination directory if necessary
    if (create_dest_dir(dest_dir) != 0) {
        return 1;
    }

    // Open the source directory
    dir = opendir(source_dir);
    if (dir == NULL) {
        fprintf(stderr, "Error opening source directory: %s\n", source_dir);
        return 1;
    }

    // Iterate through files in the directory
    while ((entry = readdir(dir)) != NULL) {
        snprintf(source_path, sizeof(source_path), "%s/%s", source_dir, entry->d_name);

        // Check if the entry is a regular text file
        if (stat(source_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode) && is_text_file(entry->d_name)) {
            snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, entry->d_name);

            // Process the file (reverse content)
            if (process_file(source_path, dest_path) != 0) {
                fprintf(stderr, "Error processing file: %s\n", entry->d_name);
            }
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    // Ensure correct command-line arguments usage
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_directory> <destination_directory>\n", argv[0]);
        return 1;
    }

    // Process the given directory
    if (process_directory(argv[1], argv[2]) == -1) {
        fprintf(stderr, "Flipper operation failed.\n");
        return 1;
    }

    printf("Flipper operation completed successfully.\n");
    return 0;
}