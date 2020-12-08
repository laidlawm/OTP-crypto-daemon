#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MOD(a,b) ((((a)%(b))+(b))%(b))
#define BUF_SIZE 30

int nForks = 0;

void error(const char *msg) { perror(msg); exit(1); } // Error function used for reporting issues

void otpEncrypt(char* plaintext, char* key) {
    int i;
    for (i = 0; i < strlen(plaintext); i++) {
        //if (plaintext[i] == ' ') {plaintext[i] = '@';} // Set space characters to a character that is adjacent to the range of upper case letters on the ASCII table
	plaintext[i] -= 64; // Shift range of characters to 0-27
	plaintext[i] -= key[i]; // Add key to current character
	plaintext[i] = MOD(plaintext[i], 27); // Get a character in the range 0-27
	plaintext[i] += 64; // Restore range of characters to 64-90
	if (plaintext[i] == '@') plaintext[i] = ' ';
    }
}

void sendAll(int fd, char* str) {
    char buffer[BUF_SIZE] = {'\0'};
    char* temp = str;
    int max = strlen(str);
    int cur = 0;
    while (cur < max) { // While there are still chunks to send
	memset(buffer, '\0', BUF_SIZE); // Restore buffer
	strncpy(buffer, temp, BUF_SIZE); // Store chunk into buffer
	int charsWritten = send(fd, buffer, BUF_SIZE, 0); // Send chunk
	if (charsWritten < 0) error("SERVER: send"); // Throw error on bad send
	int charsRead = recv(fd, buffer, BUF_SIZE, 0); // Receive confirmation
	if (charsRead < 0) error("SERVER: read"); // Throw error on bad receive
	cur += charsWritten; // Update count of chars sent
	temp += BUF_SIZE; // Move to next chunk
    }
}

void recvAll(int fd, char* msg) {
    char buffer[BUF_SIZE + 1] = {'\0'};
    int endFlag = 0;
    do { // Loop until there are no more chunks to receive
	memset(buffer, '\0', BUF_SIZE); // Restore buffer
	int charsRead = recv(fd, buffer, BUF_SIZE, 0); // Read one chunk
	if (charsRead < 0) error("SERVER: recv"); // Throw error on bad read
	while (charsRead < BUF_SIZE) { // Loop until correct number of bytes read
	    send(fd, "0", 1, 0); // Send failure update
	    charsRead = recv(fd, buffer, BUF_SIZE, 0); // Re-receive chunk
	    if (charsRead < 0) error("SERVER: recv"); // Throw error on bad read
	}
	send(fd, "1", 1, 0); // Send success update
	strncat(msg, buffer, BUF_SIZE); // Append chunk to message
	int i;
	for (i = 0; i < BUF_SIZE; i++) {
	    if (buffer[i] == '\0') endFlag = 1; // Break out of loop when null terminator found
	}
    }
    while (!endFlag);
}

int main(int argc, char *argv[])
{
	int listenSocketFD, establishedConnectionFD, portNumber;
	socklen_t sizeOfClientInfo;
	//char buffer[256];
	struct sockaddr_in serverAddress, clientAddress;

	if (argc < 2) { fprintf(stderr,"USAGE: %s port\n", argv[0]); exit(1); } // Check usage & args

	// Set up the address struct for this process (the server)
	memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[1]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process

	// Set up the socket
	listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (listenSocketFD < 0) error("ERROR opening socket");

	// Enable the socket to begin listening
	if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
		error("ERROR on binding");
	listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections


	while (1) {
	    // Accept a connection, blocking if one is not available until one connects
	    sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
	    establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
	    if (establishedConnectionFD < 0) error("ERROR on accept");

	    pid_t pid = -1;
	    if (nForks < 5) {
		++nForks;
		pid = fork();
	    }
	    if (pid == 0) {
		sendAll(establishedConnectionFD, "dec");
		char key[80000] = {'\0'}; // Initialize key string
		recvAll(establishedConnectionFD, key); // Receive key string from client
		char plaintext[80000] = {'\0'}; // Initialize ciphertext string
		recvAll(establishedConnectionFD, plaintext); // Receive ciphertext string from client
		otpEncrypt(plaintext, key); // Decrypt ciphertext
		sendAll(establishedConnectionFD, plaintext); // Send decrypted text
		close(establishedConnectionFD);
		exit(0);
	    }
	    else if (pid == -1) {
		error("SERVER: fork");
	    }
	    else {
		waitpid(pid, 0, 0);
	    }
	    --nForks;
	}
	close(listenSocketFD);
	return 0;
}
