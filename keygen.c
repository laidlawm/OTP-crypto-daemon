#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv) {
    int length, i;
    char* key;

    srand(getpid()); // Seed random number generator with process id

    if (argc != 2) { // Check for correct number of arguments
        fprintf(stderr, "usage: keygen length\n"); // If invalid, print error
        return 1; // Return with error status 1
    }

    for (i = 0; i < strlen(argv[1]); i++) { // Loop through user input
        if (!isdigit(argv[1][i])) { // Check for non-number values
            fprintf(stderr, "keygen: input must be an integer\n"); // If non-number value found, print error
            return 1; // Return with error status 1
        }
    }

    length = atoi(argv[1]); // Convert user input to integer
    key = (char*)malloc((length + 2) * sizeof(char)); // Allocate space for key and '\n', '\0' characters
    for (i = 0; i < length; i++) { // Loop through key string
        key[i] = rand() % 26 + 65; // Store random upper case character at location
    }
    key[length] = '\n'; // Append '\n'
    key[length + 1] = '\0'; // Append '\0'

    printf("%s", key); // Print generated key

    return 0;
}
