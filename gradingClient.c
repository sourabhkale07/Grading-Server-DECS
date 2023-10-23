#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>

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

    if (argc != 7)
    {
        fprintf(stderr, "usage %s <hostname>  <serverIP:port>  <sourceCodeFileTobeGraded>  <loopNum> <sleepTimeSeconds> <timeout-seconds>\n", arguments[0]);
        exit(0);
    }

    portNumber = atoi(arguments[2]);      // Second arg is port no.
    int loopCount = atoi(arguments[4]);
    int sleepTime = atoi(arguments[5]);
    int successfulRequests = atoi(arguments[4]);

    socketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFileDescriptor < 0)
        reportError("ERROR opening socket");

    serverHost = gethostbyname(arguments[1]);
    if (serverHost == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    bzero((char *)&serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    bcopy((char *)serverHost->h_addr, (char *)&serverAddress.sin_addr.s_addr,
          serverHost->h_length);
    serverAddress.sin_port = htons(portNumber);

    if (connect(socketFileDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
        reportError("ERROR connecting");

    struct timeval connectionTimeout;
    connectionTimeout.tv_sec = atoi(arguments[6]); // Set the timeout to X seconds
    connectionTimeout.tv_usec = 0;
    if (setsockopt(socketFileDescriptor, SOL_SOCKET, SO_RCVTIMEO, &connectionTimeout, sizeof(connectionTimeout)) < 0)
        perror("timeout");

    gettimeofday(&loopStartTime, NULL);
    char temporaryBuffer[10000];

    n = write(socketFileDescriptor, &loopCount, sizeof(int));
    if (n < 0)
        reportError("ERROR writing to socket");

    for (int i = 0; i < loopCount; i++)
    {
        memset(temporaryBuffer, 0, sizeof(temporaryBuffer));
        FILE *sourceFile = fopen(arguments[3], "rb");
        if (sourceFile == NULL)
        {
            reportError("ERROR opening file");
        }

        // Calculate the file size
        fseek(sourceFile, 0, SEEK_END);
        fileSize = ftell(sourceFile);
        rewind(sourceFile);

        // Allocate memory for the buffer based on file size
        dataBuffer = (char *)malloc(fileSize + 1);
        if (dataBuffer == NULL)
        {
            reportError("ERROR allocating memory");
        }

        // Read the entire file into the buffer
        size_t bytesRead = fread(dataBuffer, 1, fileSize, sourceFile);
        dataBuffer[bytesRead] = '\0'; // Null-terminate the buffer

        fclose(sourceFile);

        // Send the content of the file to the server
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
        temporaryBuffer[n] = '\0'; // Null-terminate the received data
        printf("Server response: %s\n", temporaryBuffer);
        if (i < loopCount - 1)
            sleep(sleepTime);

        // Free allocated memory
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

