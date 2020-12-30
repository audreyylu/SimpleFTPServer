#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <net/if.h>
#include <ifaddrs.h>

#define TRUE 1
#define FALSE !TRUE
// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.


int pasvFd = 0;
int isLoggedIn = 0; // set to 0 initially, when USER functionality merged'
int clientDataFd = 0;
struct sockaddr_in connectionAddress;
socklen_t sock_addr_len = sizeof(struct sockaddr); // need this, cannot just pass sizeof struct sockaddr as argument in getsockname, need a pointer
int ip_address = 0;
char *IP;
unsigned int ip1, ip2, ip3, ip4;
void* userFunction(char * buffer, int count, int clientd);
void * typeFunction (char * buffer, int count, int clientd);
void * modeFunction (char * buffer, int count, int clientd);
void * struFunction (char * buffer, int count, int clientd);
void * cdupFunction (char * buffer, int count, int clientd, char * originalParentPath);
void * cwdFunction (char * buffer, int count, int clientd);
void * pasvFunction (int count, int clientd);
void * retrFunction (int count, int clientd, char * argRETR);
void * nlstFunction (int count, int clientd);
void * removeCarriage (char * strToRemove);
void * getIpAddress();

void* interact(void* args)
{
    int clientd = *(int*) args;
    char buffer[1024];
    char tosend[1024];
    char typePointer[2];
    char modePointer[2];
    char struPointer[2];
    char originalParentPath[100];
    bzero(originalParentPath, 100);
    getcwd(originalParentPath, sizeof(originalParentPath));

    while (true)
    {
        bzero(buffer, 1024); 
        ssize_t length = recv(clientd, buffer, 1024, 0);

        if (length < 0){
            perror("Failed to read from the socket");
            break;
        }

        if (length == 0){
            printf("EOF\n");
            if (pasvFd != 0) {
                close(pasvFd);
                pasvFd = 0;
            }
            if(clientDataFd !=0) {
                close(clientDataFd);
                clientDataFd = 0;
            }
            break;
        }
    

        char bufferCheck[1024];
        char cmd[1024];
        bzero(cmd, 1024); 
        int cmdLength = 0;
        bzero(bufferCheck, 1024);
        memcpy (bufferCheck, buffer, strlen(buffer));

        char *numArgs;
        int count = 0;
        numArgs = strtok(bufferCheck, " ");
        while( numArgs != NULL ) {
            if (count == 0) {
                removeCarriage (numArgs);
                memcpy (cmd, numArgs, strlen(numArgs ));
                cmdLength = strlen(numArgs );
            }
            count ++;
            numArgs = strtok(NULL, " ");
        }

        char cmdRETR[5];
        bzero(cmdRETR, 5);
        memcpy (cmdRETR, buffer, 4);

        char argRETR[100];
        bzero(argRETR, 100);
        memcpy (argRETR, &buffer[5], 100);

        removeCarriage (argRETR);


        if (strncasecmp (cmd, "USER", cmdLength) == 0) {
            userFunction(buffer, count, clientd);
        }
        else if (strncasecmp (cmd, "TYPE", cmdLength) == 0) {
            typeFunction(buffer, count, clientd);
        }
        else if (strncasecmp (cmd, "MODE", cmdLength) == 0) {
            modeFunction(buffer, count, clientd);
        }
        else if (strncasecmp (cmd, "STRU", cmdLength) == 0) {
            struFunction(buffer, count, clientd);
        }
        else if (strncasecmp (cmd, "QUIT", cmdLength) == 0) {
            if (count > 1) {
                char* tooManyParameter = "501 Syntax error in parameters or arguments. too many arguments\r\n";
                write(clientd, tooManyParameter, strlen(tooManyParameter));
            } else {
                printf("client has asked to quit\n");
                break;
            }
        }
        else if (strncasecmp (cmd, "CDUP", cmdLength) == 0) {
            cdupFunction (buffer, count, clientd, originalParentPath);
        }
        else if (strncasecmp (cmd, "CWD", cmdLength) == 0) {
            cwdFunction (buffer, count, clientd);
        }
        else if (strncasecmp(cmd, "PASV", cmdLength) == 0) {
            pasvFunction (count, clientd);
        }
        else if (strncasecmp(cmdRETR, "RETR", 4) == 0) {
            retrFunction (count, clientd, argRETR);
        }
        else if (strncasecmp(cmd, "NLST", cmdLength) == 0) {
            nlstFunction (count, clientd);
        }

        else {
            printf("500: Command unrecognized.\r\n"); 
            char* notSupported = "500: Command not supported\r\n";
            write(clientd, notSupported, strlen(notSupported));
        }
    }
    close(clientd);

    return NULL;
}


int main(int argc, const char * argv[])
{
    int base = 10; 
    char* end; 
    if (argc != 2) {
      usage(argv[0]);
      return -1;
    }
    int portNum = strtol(argv[1], &end, base);

    // Create a TCP socket
    int socketd = socket(PF_INET, SOCK_STREAM, 0);

    if (socketd < 0)
    {
        perror("Failed to create the socket.\r\n");

        exit(-1);
    }

    // Reuse the address
    int value = 1;

    if (setsockopt(socketd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) != 0)
    {
        perror("Failed to set the socket option\r\n");
        exit(-1);
    }

    // Bind the socket to a port
    struct sockaddr_in address;
    bzero(&address, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_port = htons(portNum);
    // address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socketd, (const struct sockaddr*) &address, sizeof(struct sockaddr_in)) != 0)
    {
        perror("Failed to bind the socket\r\n");

        exit(-1);
    }

    // Set the socket to listen for connections
    if (listen(socketd, 5) != 0)
    {
        perror("Failed to listen for connections\r\n");
        exit(-1);
    }

    while (true)
    {
        // Accept the connection
        struct sockaddr_in clientAddress;

        socklen_t clientAddressLength = sizeof(struct sockaddr_in);

        printf("Waiting for incoming connections...\r\n");

        int clientd = accept(socketd, (struct sockaddr*) &clientAddress, &clientAddressLength);

        if (clientd < 0)
        {
            perror("Failed to accept the client connection\r\n");
            continue;
        }
        isLoggedIn = 0;

        printf("Accepted the client connection from %s:%d.\r\n", inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
        char* welcome = "220 Welcome!\r\n";
        write(clientd, welcome, strlen(welcome));

        getsockname(socketd, (struct sockaddr *) &connectionAddress, &sock_addr_len);

        getIpAddress();
        printf("IP of machine: %s\n", IP);
        char *ipSegments;
        int count = 0;
        ipSegments = strtok(IP, ".");
        ip1 = (unsigned char) strtol(ipSegments, &end, base);
        ipSegments = strtok(NULL, ".");
        ip2 = (unsigned char) strtol(ipSegments, &end, base);
        ipSegments = strtok(NULL, ".");
        ip3 = (unsigned char) strtol(ipSegments, &end, base);
        ipSegments = strtok(NULL, ".");
        ip4 = (unsigned char) strtol(ipSegments, &end, base);

        // Create a separate thread to interact with the client
        pthread_t thread;

        if (pthread_create(&thread, NULL, interact, &clientd) != 0)
        {
            perror("Failed to create the thread\r\n");

            continue;
        }

        // The main thread just waits until the interaction is done
        pthread_join(thread, NULL);

        printf("Interaction thread has finished.\r\n");
    }

    return 0;

}

void * userFunction (char * buffer, int count, int clientd) {
    char* username = strtok(buffer, " ");
    username = strtok(NULL, " ");
    if (count > 2 || count == 1) {
        char* tooManyParameter = "501 Syntax error in parameters or arguments. too many arguments\r\n";
        write(clientd, tooManyParameter, strlen(tooManyParameter));
    } else if (username != NULL && strncasecmp (username, "username", 5) == 0) {
        isLoggedIn = 1;
        char* loggedInMsg = "230 User logged in, proceed.\r\n";
        write(clientd, loggedInMsg, strlen(loggedInMsg));
    } else {
        isLoggedIn = 0;
        char* notLoggedInMsg = "530 Not logged in.\r\n";
        write(clientd, notLoggedInMsg, strlen(notLoggedInMsg));
    }
    return NULL;
}

void * typeFunction (char * buffer, int count, int clientd) {
    if(isLoggedIn == 1) {
        char* type = strtok(buffer, " ");
        type = strtok(NULL, " ");
        if (count > 2 || count == 1) {
            char* tooManyParameter = "501 Syntax error in parameters or arguments. too many arguments\r\n";
            write(clientd, tooManyParameter, strlen(tooManyParameter));
        } else if (strncasecmp (type, "A", 1) == 0) {
            char* commandOkay = "200 Command okay. Type set to Ascii\r\n";
            write(clientd, commandOkay, strlen(commandOkay));
        } else if (strncasecmp (type, "I", 1) == 0) {
            char* commandOkay = "200 Command okay. Type set to Image\r\n";
            write(clientd, commandOkay, strlen(commandOkay));
        } else {
            char* serviceUnavailable = "504 Type unsupported. Supports only Ascii and Image\r\n";
            write(clientd, serviceUnavailable, strlen(serviceUnavailable));
        }
    } else {
        char* notLoggedInMsg = "530 Not logged in.\r\n";
        write(clientd, notLoggedInMsg, strlen(notLoggedInMsg));
    }
    return NULL;
}

void * modeFunction (char * buffer, int count, int clientd) {
    if(isLoggedIn) {   
        char* mode = strtok(buffer, " ");
        mode = strtok(NULL, " ");
        if (count > 2 || count == 1) {
            char* tooManyParameter = "501 Syntax error in parameters or arguments. too many arguments\r\n";
            write(clientd, tooManyParameter, strlen(tooManyParameter));
        } else if (strncasecmp (mode, "S", 1) == 0) {
            char* commandOkay = "200 Command okay. Mode set to Stream\r\n";
            write(clientd, commandOkay, strlen(commandOkay));
        } else {
            char* serviceUnavailable = "504 Mode unsupported. Supports only Stream mode\r\n";
            write(clientd, serviceUnavailable, strlen(serviceUnavailable));
        }
    }  else {
        char* notLoggedInMsg = "530 Not logged in.\r\n";
        write(clientd, notLoggedInMsg, strlen(notLoggedInMsg));
    }
    return NULL;
}

void * struFunction (char * buffer, int count, int clientd) {
    if(isLoggedIn) { 
        char* structure = strtok(buffer, " ");
        structure = strtok(NULL, " ");
        if (count > 2 || count == 1) {
            char* tooManyParameter = "501 Syntax error in parameters or arguments. too many arguments\r\n";
            write(clientd, tooManyParameter, strlen(tooManyParameter));
        } else if (strncasecmp (structure, "F", 1) == 0) {
            char* commandOkay = "200 Command okay. Structure set to File\r\n";
            write(clientd, commandOkay, strlen(commandOkay));
        } else {
            char* serviceUnavailable = "504 Structure unsupported. Supports only File structure\r\n";
            write(clientd, serviceUnavailable, strlen(serviceUnavailable));
        }
    } else {
        char* notLoggedInMsg = "530 Not logged in.\r\n";
        write(clientd, notLoggedInMsg, strlen(notLoggedInMsg));
    }
    return NULL;
}

void * cdupFunction (char * buffer, int count, int clientd, char * originalParentPath) {
    if(!isLoggedIn){
        char* notLoggedInMsg = "530 Not logged in.\r\n";
        write(clientd, notLoggedInMsg, strlen(notLoggedInMsg));
    } else if (count > 1) {
        char* tooManyParameter = "501 Syntax error in parameters or arguments. too many arguments\r\n";
        write(clientd, tooManyParameter, strlen(tooManyParameter));
    } else {
        char currentParentPath[100];
        bzero(currentParentPath, 100);
        getParentDirectoryPath(currentParentPath);
        if(strlen(currentParentPath) >= strlen(originalParentPath)) {
            chdir(currentParentPath);
            char* actionTaken = "200 Command okay. Moved up\r\n";
            write(clientd, actionTaken, strlen(actionTaken));
        } else {
            char* actionNotTaken = "550 Requested action not taken. Can't go above root directory\r\n";
            write(clientd, actionNotTaken, strlen(actionNotTaken));
        }
    }
    return NULL;
}

void * cwdFunction (char * buffer, int count, int clientd) {
    if(isLoggedIn) {
        char *substring1 = strstr(buffer, "../") ;
        char *substring2 = strstr(buffer, "./") ;
        if (substring1 != NULL || substring2 != NULL) {

            char* actionNotTaken = "550 Requested action not taken. Use CDUP \r\n";
            write(clientd, actionNotTaken, strlen(actionNotTaken));

        } else if (count > 2 || count == 1) {

            char* tooManyParameter = "501 Invalid number of arguments\r\n";
            write(clientd, tooManyParameter, strlen(tooManyParameter));

        } else {
            char* directory = strtok(buffer, " ");
            directory = strtok(NULL, " ");
            removeCarriage (directory);
            if (chdir(directory) != 0) {
                char* actionNotTaken = "550 Requested action not taken. Invalid file \r\n";
                write(clientd, actionNotTaken, strlen(actionNotTaken));
            } else {
                char* actionTaken = "200 Command okay. Moved down\r\n";
                write(clientd, actionTaken, strlen(actionTaken));
            }
        }
    }  else {
        char* notLoggedInMsg = "530 Not logged in.\r\n";
        write(clientd, notLoggedInMsg, strlen(notLoggedInMsg));
    }
    return NULL;
}


void * pasvFunction (int count, int clientd) {
    if(isLoggedIn) {

        if (count > 1) {
            char* tooManyParameter = "501 Too many arguments\r\n";
            write(clientd, tooManyParameter, strlen(tooManyParameter));
        } else {
            if (pasvFd != 0) {
                close(pasvFd);
            }

            struct sockaddr_in pasv_address;
            pasvFd = socket(PF_INET, SOCK_STREAM, 0); // create new socket

            if (pasvFd == 0) {                        // check if socket creation failed
                perror("Failed to create data connection socket\r\n");
                exit(-1);
            }

            bzero(&pasv_address, sizeof(struct sockaddr_in));

            pasv_address.sin_family = PF_INET;
            pasv_address.sin_port = htons(0); 
            pasv_address.sin_addr.s_addr = connectionAddress.sin_addr.s_addr;

            if (bind(pasvFd, (const struct sockaddr*) &pasv_address, sizeof(struct sockaddr_in)) != 0) {
                perror("Failed to bind the socket for data connection\r\n");
                exit(-1);
            }

            if (listen(pasvFd, 1) != 0) {
                perror("Failed to listen for connections\r\n");
                exit(-1);
            }

            struct sockaddr_in pasv_address_after;
            // use getsockname to retrieve the kernel assigned port number:
            if (getsockname(pasvFd, (struct sockaddr*) &pasv_address_after, &sock_addr_len) != 0) {
                perror("Failed to get new socket information for data connection\r\n");
                exit(-1);
            }

            int portEntire = ntohs(pasv_address_after.sin_port); // get entire port # (unsigned short = 0 to 65,535, 2 bytes = 16 bits)in host byte order
            printf("Entire port #: %d\r\n", portEntire);

            int p1 = (portEntire >> 8) & 0xFF; // higher order 8 bits of entire port number
            int p2 = portEntire & 0xFF;        // lower order 8 bits of entire port number

            char to_client[100];
            bzero(to_client, sizeof(to_client));
            sprintf(to_client, "227 Entering Passive Mode: (%d, %d, %d, %d, %d, %d).\r\n", ip1, ip2, ip3, ip4, p1, p2);
            write(clientd, to_client, strlen(to_client));

            struct sockaddr_in dataConnectionAddr;
            socklen_t dataConnectionAddrLength = sizeof(struct sockaddr_in);

            clientDataFd = accept(pasvFd, (struct sockaddr*) &dataConnectionAddr, &dataConnectionAddrLength);

        }


    } else {
        char* notLoggedInMsg = "530 Not logged in.\r\n";
        write(clientd, notLoggedInMsg, strlen(notLoggedInMsg));
    }
    return NULL;
}

void * retrFunction (int count, int clientd, char * argRETR) {

    if (!isLoggedIn) {
        char* notLoggedInMsg = "530 Not logged in.\r\n";
        write(clientd, notLoggedInMsg, strlen(notLoggedInMsg));
    } else if (count > 2 || count == 1) {
        char* tooManyParameter = "501 Syntax error in parameters or arguments. too many arguments\r\n";
        write(clientd, tooManyParameter, strlen(tooManyParameter));
    } else if (pasvFd == 0) {
        char* noPassiveModeMsg = "425 Can't open data connection.\r\n";
        write(clientd, noPassiveModeMsg, strlen(noPassiveModeMsg));
    } else {
        FILE* filePtr = fopen(argRETR, "r");
        if (filePtr != NULL) {
            char* retrSuccessMsg = "125 Data connection already open; Transfer starting.\r\n";
            write(clientd, retrSuccessMsg, strlen(retrSuccessMsg));
            char* statusOkay = "150 File status okay. Transfer starting.\r\n";
            write(clientd, statusOkay, strlen(statusOkay));
            char sendbuffer[100];
            bzero(sendbuffer, sizeof(sendbuffer));
            int bytesRead = 0;
            while ((bytesRead = fread(sendbuffer, 1, sizeof(sendbuffer), filePtr))> 0) {
                send(clientDataFd, sendbuffer, bytesRead, 0);
            }
            fclose(filePtr);
            close(pasvFd);
            close(clientDataFd);
            pasvFd = 0;
            clientDataFd = 0;
            char* retrTransferCompleteMsg = "226 Closing data connection.\n Requested file action successful.\r\n";
            write(clientd, retrTransferCompleteMsg, strlen(retrTransferCompleteMsg));

        } else {
            char* retrFailedMsg = "450 Requested file action not taken.\r\n";
            write(clientd, retrFailedMsg, strlen(retrFailedMsg));
        }
    }
    return NULL;
}

void * nlstFunction (int count, int clientd) {
    if (!isLoggedIn) {
        char* notLoggedInMsg = "530 Not logged in.\r\n";
        write(clientd, notLoggedInMsg, strlen(notLoggedInMsg));
    } else if (count > 1) {
        char* tooManyParameter = "501 Too many arguments\r\n";
        write(clientd, tooManyParameter, strlen(tooManyParameter));
    } else if (pasvFd == 0) { 
        char* noPassiveModeMsg = "425 Can't open data connection.\r\n";
        write(clientd, noPassiveModeMsg, strlen(noPassiveModeMsg));
    } else {
        char* retrSuccessMsg = "150 Here comes directory listing\r\n";
        write(clientd, retrSuccessMsg, strlen(retrSuccessMsg));

        char current[100];
        getcwd(current, sizeof(current));

        listFiles(clientDataFd, current);

        close(pasvFd);
        close(clientDataFd);
        pasvFd = 0;
        clientDataFd = 0;
        
        char* retrTransferCompleteMsg = "226 Closing data connection. Directory send OK\r\n";
        write(clientd, retrTransferCompleteMsg, strlen(retrTransferCompleteMsg));

    }
    return NULL;
}

void * removeCarriage (char * strToRemove) {
    char* replacen = strchr(strToRemove, '\n');
    if (replacen != NULL) {
        *replacen= '\0';
    }
    char* replacer = strchr(strToRemove, '\r');
    if (replacer != NULL) {
        *replacer = '\0';
    }

    return NULL;
}

//Code posted by Alan in Piazza
void * getIpAddress() {
   char host[256];
   char ipstr[INET_ADDRSTRLEN];
   struct hostent *host_entry;
   if ( gethostname(host, sizeof(host)) <0  ) {
    perror("Failed to get host name");
    exit(-1);
   }   
   if ( (host_entry = gethostbyname(host)) == NULL ) {
    perror("Failed to get host IP address");
    exit(-1);    
   }
   IP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));
   printf("Hostname is %s and IP is %s\n",host,IP);

   // do it by getaddrinfo
    int status; 
    struct addrinfo hints, *p; 
    struct addrinfo *servinfo; 
    char hostname[128];
    memset(&hints, 0, sizeof hints);     
    hints.ai_family   = AF_UNSPEC;    
    hints.ai_socktype = SOCK_STREAM;  
    hints.ai_flags    = AI_PASSIVE;     
    gethostname(hostname, 128);   
    if ((status = getaddrinfo(hostname, NULL, &hints, &servinfo)) == -1) { 
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status)); 
        exit(1); 
    }          
    for (p=servinfo; p!=NULL; p=p->ai_next) { 
        struct in_addr  *addr;  
        if (p->ai_family == AF_INET) { 
            struct sockaddr_in *ipv = (struct sockaddr_in *)p->ai_addr; 
            addr = &(ipv->sin_addr);  
        } 
        else { 
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr; 
            addr = (struct in_addr *) &(ipv6->sin6_addr); 
        }
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr); 
    }    
}