/*
 *
 * This will take a BDF file and convert it into a C array usable with ctOS. 
 * I have not systematically developed this against the BDF specification,
 * but specifically made it work with the font that I use (the uni-vga font
 * which can be found at http://www.inp.nsk.su./~bolkhov/files/fonts/univga/)
 * 
 */
 
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
 
#define MAX_CHARS 256
 
int main(int argc, char** argv) {
    char* bdffile;
    FILE* input;
    char *line = NULL;
    int read;
    size_t len;
    int in_char = 0;
    int in_bitmap = 0;
    long int current_char = 0;
    char* argument;
    int current_line = 0;
    int bytes_per_char = 16;
    int i;
    /*
     * The index table - in this table, we keep track at which 
     * line in the big array we have which character
     */
    int indices[MAX_CHARS];
    memset(indices, -1, MAX_CHARS*sizeof(int));
    /*
     * First parse the arguments
     */ 
    if (argc < 2) {
        printf("Usage: convertbdf.c <bdf-file>\n");
        exit(1);
    }
    bdffile = argv[1];
    input = fopen(bdffile, "r");
    if (!input) {
        printf("Could not open BDF input file %s\n", bdffile);
        exit(1);
    }
    /*
     * Print start of font_data array
     */
    printf("char __bdf_font_data[] = {\n");
    /*
     * Now go through this line by line
    */
    while ((read = getline(&line, &len, input)) != -1) {
        /*
         * STARTCHAR?
         */
        if (0 == strncmp("STARTCHAR", line, strlen("STARTCHAR"))) {
            /*
             * Found STARTCHAR - so we are in a char section
             */
            if (1 == in_char) {
                printf("Found STARTCHAR before ENDCHAR\n");
                exit(1);
            }
            in_char = 1;
        }
        if (0 == strncmp("ENDCHAR", line, strlen("ENDCHAR"))) {
            /*
             * Found ENDCHAR - so we are leaving a char section
             */
            if (0 == in_char) {
                printf("Found ENDCHAR before STARTCHAR\n");
                exit(1);
            }
            in_char = 0;
            in_bitmap = 0;
            if (current_char < MAX_CHARS) {
                printf("\n");
                /*
                 * Mark that character in index array
                 */
                indices[current_char] = bytes_per_char*current_line;
                 /*
                 * Keep track of the current line
                 * in our "big array"
                 */
                current_line++;
            }
        }
        if (0 == strncmp("BITMAP", line, strlen("BITMAP"))) {
            /*
             * Starting a BITMAP section
             */
            if (0 == in_char) {
                printf("Found BITMAP before STARTCHAR\n");
                exit(1);
            }
            in_bitmap = 1;
        }
        if (0 == strncmp("ENCODING", line, strlen("ENCODING"))) {
            if (0 == in_char) {
                printf("Found ENCODING outside of a CHAR block\n");
                exit(1);
            }
            argument = strtok(line, " ");
            argument = strtok(NULL, " ");
            if (0 == argument) {
                printf("Could not get argument of ENCODING\n");
                exit(1);
            }
            current_char = strtol(argument, 0, 10);
        }
        /*
         * Ignore all characters with ASCII values greater than 255
         */
        if (current_char >= MAX_CHARS) {
            continue;
        }
        /*
         * If we are in a bitmap section, interpret the current line as a byte
         * for character current_char unless we are in the line with the
         * string BITMAP itself
         */
        if (1 == in_bitmap) {
        if (strncmp("BITMAP", line, strlen("BITMAP"))) {
                line[2] = 0;
                printf("0x%s, ", line);
            }
        }
    }
    printf("\n};\n");
    /*
     * Print the code for the index table
     */
    printf("\n\nint __bdf_font_indices__[] = {\n");
    for (i = 0; i < MAX_CHARS; i++) {
        printf("%d,", indices[i]);
        if ((0 == (i % 40)) && (0 != i))
            printf("\n");
    }
    printf("\n};\n");
 }