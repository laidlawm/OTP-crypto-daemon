#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>

#define BUF_SIZE 30

void error(const char *msg) { perror(msg); exit(0); } // Error function used for reporting issues

char* fileToStr(char* path) {
    FILE* fd;
    char* buffer = NULL;
    size_t bufferSize = 0;

    if (!(fd = fopen(path, "r"))) { // Check if file opened successfuly
	fprintf(stderr, "fopen(%s): %s\n", path, strerror(errno)); // If not, print error
	exit(1); // Exit with error status 1
    }

    getline(&buffer, &bufferSize, fd); // Allocate buffer, store contents of file
    buffer[strlen(buffer) - 1] = '\0'; // Trims trailing '\n' character

    fclose(fd); // Close file
    return buffer; // Return buffer string
}

void sendAll(int fd, char* str) {
    char* temp = str;
    strcpy(temp, str);
    int max = strlen(str);
    int cur = 0;
    while (cur < max) { // While there are still chunks to send
	char buffer[BUF_SIZE + 1] = {'\0'}; // Restore buffer
	strncpy(buffer, temp, BUF_SIZE); // Store chunk into buffer
	int charsWritten = send(fd, buffer, BUF_SIZE, 0); // Send one chunk
	if (charsWritten < 0) error("CLIENT: send"); // Throw error on bad send
	memset(buffer, '\0', BUF_SIZE); // Restore buffer
	int charsRead = recv(fd, buffer, BUF_SIZE, 0); // Receive confirmation
	if (charsRead < 0) error("SERVER: read"); // Throw error on bad receive
	if (!strcmp(buffer, "0")) { // If failure update received
	    strncpy(buffer, temp, BUF_SIZE); // Restore chunk
	    charsWritten = send(fd, buffer, BUF_SIZE, 0); // Re-send chunk
	}
	cur += charsWritten; // Update count of chars sent
	temp += BUF_SIZE; // Move to next chunk
    }
}

void recvAll(int fd, char* msg) {
    char buffer[BUF_SIZE + 1] = {'\0'};
    int endFlag = 0;
    do { // While there are still chunks to receive
	memset(buffer, '\0', BUF_SIZE); // Restore buffer
	int charsRead = recv(fd, buffer, BUF_SIZE, 0); // Receive one chunk
	if (charsRead < 0) error ("SERVER: recv"); // Throw error on bad receive
	while (charsRead < BUF_SIZE) { // Loop until correct number of chars read
	    send(fd, "0", 1, 0); // Send failure update
	    charsRead = recv(fd, buffer, BUF_SIZE, 0); // Re-receive chunk
	    if (charsRead < 0) error("recv"); // Throw error on bad receive
	}
	send(fd, "1", 1, 0); // Send success update
	strncat(msg, buffer, BUF_SIZE); // Append chunk to message
	int i;
	for (i = 0; i < BUF_SIZE; i++) {
	    if (buffer[i] == '\0') endFlag = 1; // Break loop when null terminator found
	}
    }
    while (!endFlag);
}

void _send(int fd, char* str) {
    int charsWritten = send(fd, str, strlen(str) + 1, 0);
    if (charsWritten < 0) error("CLIENT: ERROR: writing");
    if (charsWritten < strlen(str) + 1) fprintf(stderr, "CLIENT: WARNING: not all data written\n");
}

int _recv(int fd, char* buffer, size_t length) {
    int charsRead = recv(fd, buffer, length, 0);
    if (charsRead < 0) error("CLIENT: ERROR: reading");
    return charsRead;
}

int main(int argc, char *argv[]) {
	int socketFD, portNumber;
	struct sockaddr_in serverAddress;
	struct hostent* serverHostInfo;
//	char buffer[256];

	if (argc < 3) { fprintf(stderr,"USAGE: %s plaintext key port\n", argv[0]); exit(0); } // Check usage & args

	char* plaintext = fileToStr(argv[1]);
	char* key = fileToStr(argv[2]);
	if (strlen(key) < strlen(plaintext)) { fprintf(stderr, "CLIENT: ERROR: key must be equal or greater than size of plaintext\n"); exit(1); }

	// Set up the server address struct
	memset((char*)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[3]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverHostInfo = gethostbyname("localhost"); // Convert the machine name into a special form of address
	if (serverHostInfo == NULL) { fprintf(stderr, "CLIENT: ERROR, no such host\n"); exit(0); }
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

	// Set up the socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (socketFD < 0) error("CLIENT: ERROR opening socket");
	
	// Connect to server
	if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to address
		error("CLIENT: ERROR connecting");

	char daemon[4] = {'\0'};
	recvAll(socketFD, daemon);
	if (strcmp(daemon, "dec")) { fprintf(stderr, "CLIENT: ERROR: attempting connection to invalid daemon %s\n", argv[3]); exit(2); }
	sendAll(socketFD, key); // Send key
	sendAll(socketFD, plaintext); // Send ciphertext
	char ciphertext[70000] = {'\0'};
	recvAll(socketFD, ciphertext); // Receive decrypted text
	printf("%s\n", ciphertext); // Print decrypted text
	close(socketFD); // Close the socket
	return 0;
}
