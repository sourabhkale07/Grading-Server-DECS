#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>

// A Function to report errors and exit
void reportError(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *arguments[])
{
    int socketFileDescriptor, portNumber, n;
    struct timeval startTime, endTime;
    struct timeval loopStartTime, loopEndTime;
    struct sockaddr_in serverAddress;
    struct hostent *serverHost;
    double totalResponseTime = 0;
    char *dataBuffer = NULL;
    long fileSize;
    int timeoutCounter = 0;

    // Check if the number of command line arguments is provided is correct?
    if (argc != 7)
    {
        fprintf(stderr, "usage %s <hostname>  <serverIP:port>  <sourceCodeFileTobeGraded>  <loopNum> <sleepTimeSeconds> <timeout-seconds>\n", arguments[0]);
        exit(0);
    }

    //  convert the arguments
    portNumber = atoi(arguments[2]);      // Second arg is port no.
    int loopCount = atoi(arguments[4]);
    int sleepTime = atoi(arguments[5]);
    int successfulRequests = atoi(arguments[4]);

    // Creating the socket
    socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDescriptor < 0)
        reportError("ERROR opening socket");

    // to the server host by name

    serverHost = gethostbyname(arguments[1]);
    if (serverHost == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    // Initializing  the server address struct
    bzero((char *)&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    bcopy((char *)serverHost->h_addr, (char *)&serverAddress.sin_addr.s_addr,
          serverHost->h_length);
    serverAddress.sin_port = htons(portNumber);


    // Connecting  to the server
    if (connect(socketFileDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress) < 0))
        reportError("ERROR connecting");

    // Set  socket timeout
    struct timeval connectionTimeout;
    connectionTimeout.tv_sec = atoi(arguments[6]); // Set the timeout to X seconds
    connectionTimeout.tv_usec = 0;
    if (setsockopt(socketFileDescriptor, SOL_SOCKET, SO_RCVTIMEO, &connectionTimeout, sizeof(connectionTimeout)) < 0)
        perror("timeout");

    // Initializing  time variables 
    gettimeofday(&loopStartTime, NULL);
    char temporaryBuffer[10000];

    //  number of loop iterations to the server
    n = write(socketFileDescriptor, &loopCount, sizeof(int));
    if (n < 0)
        reportError("ERROR writing to socket");


    //  Loop for the number of iterations
    for (int i = 0; i < loopCount; i++)
    {
        // Initializing  temporary buffer and open the source file
        memset(temporaryBuffer, 0, sizeof(temporaryBuffer));
        FILE *sourceFile = fopen(arguments[3], "rb");
        if (sourceFile == NULL)
        {
            reportError("ERROR opening file");
        }

        // Calculate the size of file
        fseek(sourceFile, 0, SEEK_END);
        fileSize = ftell(sourceFile);
        rewind(sourceFile);

        // Allocating  memory for the buffer based on file size
        dataBuffer = (char *)malloc(fileSize + 1);
        if (dataBuffer == NULL)
        {
            reportError("ERROR allocating memory");
        }

        // Reading the entire file  into the buffer
        size_t bytesRead = fread(dataBuffer, 1, fileSize, sourceFile);
        dataBuffer[bytesRead] = '\0'; // Null-terminate the buffer

        fclose(sourceFile);

        // Send all content of the file to the server
        gettimeofday(&startTime, NULL);
        n = write(socketFileDescriptor, dataBuffer, bytesRead);
        if (n < 0)
            perror("ERROR writing to socket");

        // Read and display the server response
        n = read(socketFileDescriptor, temporaryBuffer, 10000);
        if (n < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                printf("Timeout occurred. Decrementing successful requests.\n");
                successfulRequests--;
            }
            else
            {
                perror("ERROR reading from socket");
            }
        }
        gettimeofday(&endTime, NULL);
        double responseTime = (endTime.tv_sec - startTime.tv_sec) + (endTime.tv_usec - startTime.tv_usec) / 1e6;
        totalResponseTime = totalResponseTime + responseTime;
        temporaryBuffer[n] = '\0'; // Null-terminate the data received
        printf("Server response: %s\n", temporaryBuffer);
        if (i < loopCount - 1)
            sleep(sleepTime);

        // allocated the memory free
        free(dataBuffer);
    }
    

    gettimeofday(&loopEndTime, NULL);
    timeoutCounter = loopCount - successfulRequests;
    int requestRate;
    requestRate = loopCount / totalResponseTime;
    double loopTime = (loopEndTime.tv_sec - loopStartTime.tv_sec) + (loopEndTime.tv_usec - startTime.tv_usec) / 1e6;
    double averageResponseTime = totalResponseTime / loopCount;
    double throughput = (loopCount * 100000) / totalResponseTime;
    printf("request rate: %d\nRequests: %d\nError: %d\ntimeout: %d\nSuccessful responses: %d\nTotal time: %lf\nthroughput: %lf\n", requestRate, loopCount, timeoutCounter, timeoutCounter, successfulRequests, totalResponseTime, throughput);

    return 0;
}
