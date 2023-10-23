#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

void reportError(char *msg)
{
    perror(msg);
    exit(1);
}

pthread_mutex_t lock;

void *clientHandler(void *sockfd)
{
    int clientSocket = *(int *)sockfd;
    int requestCount;
    read(clientSocket, &requestCount, sizeof(int));
    pthread_mutex_unlock(&lock);

    for (int i = 0; i < requestCount; i++)
    {
        char dataBuffer[4096];
        char tempFileName[30];
        memset(tempFileName, 0, sizeof(tempFileName));
        snprintf(tempFileName, 30, "temp_%d.c", gettid());

        char outputFileName[30];
        memset(outputFileName, 0, sizeof(outputFileName));
        snprintf(outputFileName, 30, "temp_%d", gettid());

        memset(dataBuffer, 0, sizeof(dataBuffer));
        ssize_t bytesRead;

        bytesRead = read(clientSocket, dataBuffer, sizeof(dataBuffer));

        if (bytesRead <= 0)
        {
            perror("ERROR source code not received");
            close(clientSocket);
        }

        int sourceFileDescriptor = open(tempFileName, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

        if (sourceFileDescriptor < 0)
        {
            perror("ERROR source file not created");
            close(clientSocket);
        }

        if (ftruncate(sourceFileDescriptor, 0) == -1)
        {
            perror("ftruncate");
            close(sourceFileDescriptor);
            close(clientSocket);
        }

        ssize_t bytesWritten = write(sourceFileDescriptor, dataBuffer, bytesRead);
        close(sourceFileDescriptor);

        if (bytesWritten < 0)
        {
            perror("ERROR writing to source file");
            close(clientSocket);
        }

        char errorFileName[30];
        memset(errorFileName, 0, sizeof(errorFileName));
        snprintf(errorFileName, 30, "errorfile_%d.txt", gettid());

        char actualOutputFileName[30];
        memset(actualOutputFileName, 0, sizeof(actualOutputFileName));
        snprintf(actualOutputFileName, 30, "actualOutput_%d.txt", gettid());

        char runtimeErrorFileName[30];
        memset(runtimeErrorFileName, 0, sizeof(runtimeErrorFileName));
        snprintf(runtimeErrorFileName, 30, "runtimeError_%d.txt", gettid());

        char command[100];

        snprintf(command, 100, "gcc %s -o %s 2>%s", tempFileName, outputFileName, errorFileName);
        int check = system(command);

        if (check == 0)
        {
            memset(command, 0, sizeof(command));
            snprintf(command, 100, "./%s 1>%s 2>%s", outputFileName, actualOutputFileName, runtimeErrorFileName);

            int flag = system(command);

            if (flag == 0)
            {
                char reply[10000];
                memset(reply, 0, sizeof(reply));

                char text1[35] = "OUTPUT ERROR\n";     // if output does not match
                strcat(reply, text1);

                char text2[40] = "\nThe output of 'diff' command is:\n";
                strcat(reply, text2);

                char diffErrorFileName[30];
                memset(diffErrorFileName, 0, sizeof(diffErrorFileName));
                snprintf(diffErrorFileName, 30, "diffErrorfile_%d.txt", gettid());

                snprintf(command, 100, "diff %s expected_output.txt 1>%s", actualOutputFileName, diffErrorFileName);

                flag = system(command);

                if (flag != 0)
                {
                    FILE *errorDFile = fopen(diffErrorFileName, "r");

                    if (errorDFile == NULL)
                    {
                        perror("Failed to open output files");
                    }
                    else
                    {
                        char errorDBuffer[1024];
                        memset(errorDBuffer, 0, sizeof(errorDBuffer));
                        size_t errorDLength = fread(errorDBuffer, 1, sizeof(errorDBuffer), errorDFile);
                        fclose(errorDFile);

                        if (errorDLength < 0)
                        {
                            perror("Error reading expected_output.txt");
                        }
                        else
                        {
                            errorDBuffer[errorDLength] = '\0';
                        }

                        strcat(reply, errorDBuffer);

                        ssize_t m = send(clientSocket, reply, sizeof(reply), 0);
                        remove(diffErrorFileName);
                        remove(errorFileName);
                        remove(runtimeErrorFileName);
                        remove(actualOutputFileName);
                        remove(tempFileName);
                        remove(outputFileName);
                    }
                }
                else
                {
                    send(clientSocket, "PASS", sizeof("PASS"), 0);
                    remove(diffErrorFileName);
                    remove(errorFileName);
                    remove(runtimeErrorFileName);
                    remove(actualOutputFileName);
                    remove(tempFileName);
                    remove(outputFileName);
                }
            }
            else
            {
                FILE *errorFd = fopen(runtimeErrorFileName, "r");

                if (errorFd == NULL)
                {
                    perror("Failed to open error files");
                }
                else
                {
                    char errorOBuffer[1024];
                    memset(errorOBuffer, 0, sizeof(errorOBuffer));
                    size_t errorOutputLength = fread(errorOBuffer, 1, sizeof(errorOBuffer), errorFd);
                    fclose(errorFd);

                    if (errorOutputLength < 0)
                    {
                        perror("Error reading error.txt");
                    }
                    else
                    {
                        errorOBuffer[errorOutputLength] = '\0';
                    }

                    send(clientSocket, errorOBuffer, sizeof(errorOBuffer), 0);
                    remove(errorFileName);
                    remove(runtimeErrorFileName);
                    remove(actualOutputFileName);
                    remove(tempFileName);
                    remove(outputFileName);
                }
            }
        }
        else
        {
            FILE *errorCFd = fopen(errorFileName, "r");

            if (errorCFd == NULL)
            {
                perror("Failed to open error files");
            }
            else
            {
                char errorCOBuffer[1024];
                size_t errorCOutputLength = fread(errorCOBuffer, 1, sizeof(errorCOBuffer), errorCFd);
                fclose(errorCFd);

                if (errorCOutputLength < 0)
                {
                    perror("Error reading error.txt");
                }
                else
                {
                    errorCOBuffer[errorCOutputLength] = '\0';
                }

                send(clientSocket, errorCOBuffer, sizeof(errorCOBuffer), 0);

                remove(errorFileName);
                remove(runtimeErrorFileName);
                remove(actualOutputFileName);
                remove(tempFileName);
                remove(outputFileName);
            }
        }
        remove(errorFileName);
        remove(runtimeErrorFileName);
        remove(actualOutputFileName);
        remove(tempFileName);
        remove(outputFileName);
    }
    close(clientSocket);
    pthread_exit(NULL);
}

int main(int argc, char *arguments[])
{
    int serverSocket, clientSocket, portNumber;
    socklen_t clientLength;
    struct sockaddr_in serverAddress, clientAddress;
    int n;

    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0)
        reportError("ERROR opening socket");

    bzero((char *)&serverAddress, sizeof(serverAddress));
    portNumber = atoi(arguments[1]);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(portNumber);

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress) < 0))
        reportError("ERROR on binding");

    listen(serverSocket, 100);
    clientLength = sizeof(clientAddress);
    pthread_mutex_init(&lock, NULL);

    while (1)
    {
        pthread_mutex_lock(&lock);
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientLength);

        if (clientSocket < 0)
            reportError("ERROR on accept");

        pthread_t thread;

        if (pthread_create(&thread, NULL, &clientHandler, &clientSocket) != 0)
            printf("Failed to create Thread\n");
    }

    return 0;
}

