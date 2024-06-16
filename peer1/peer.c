#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEFAULT_PORT_NUMBER     3000
#define DEFAULT_HOST_NAME       "localhost"

typedef struct pdu {
    char type;
    char data[100];
} pdu;

typedef struct sd_node {
    int sd;
    char contentName[11];
    struct sd_node* next;
} sd_node;

char peerName[11];      // Global variable for peer name makes it easier to deal with
sd_node* head = NULL;   // Global variable for linked list head also makes it easier

void registerContent(int, struct sockaddr_in*, pdu);
void deregisterContent(int, struct sockaddr_in*, pdu);
void listOnlineContent(int, struct sockaddr_in*);
void quit(int, struct sockaddr_in*);
void errorMessage(int, struct sockaddr_in*, pdu);
void readAcknowledgement(int socketDescriptor, struct sockaddr_in* server_addr);

/*********************** sd_node linked-list struct operations ***********************/
void printNodeList(sd_node* head) {
    sd_node* temp = head;
	if (temp == NULL) {
		printf("TCP Socket List is empty\n");
		return;
	}
    printf("\n\n*** Start of SD Linked List: ***\n");
	while (temp != NULL) {
		printf("sd=%d\tcontent=%s\n", temp->sd, temp->contentName);
		temp = temp->next;
	}
    printf("****************************\n");
}

sd_node* createNode(int sd, char* contentName){
    sd_node* newNode = malloc(sizeof(sd_node));
    if (newNode == NULL) {
		printf("Error: malloc failed to allocate memory for new node\n");
		exit(1);
	}
	newNode->sd = sd;    
    strncpy(newNode->contentName, contentName, sizeof(newNode->contentName)-1);
    newNode->contentName[sizeof(newNode->contentName) - 1] = '\0'; // null terminate
	newNode->next = NULL;
	return newNode;
}

void createAndInsertNodeAtEnd(sd_node** head, int sd, char* contentName){
    sd_node* nodeToAdd = createNode(sd, contentName);
	// handle case where list is empty
	if (*head == NULL) { // remember: the function parameter is initially node**, a pointer to a pointer. so we need to deference it once to get the pointer to head node itself (if it exists)
		*head = nodeToAdd;
		return;
	}
	sd_node* temp = *head; // running this line means the pointer of the head node itself exists. we again dereference (otherwise, it's just a pointer to a pointer, not a node pointer)
	while (temp->next != NULL) {
		temp = temp->next;
	}
	temp->next = nodeToAdd;	
}

void removeNodeBySpecificValue(sd_node** head, char* contentName) {
    if (head == NULL || *head == NULL) {
        return;
    }

    sd_node* temp = *head;
    sd_node* prev = NULL;
   
    if (strcmp(temp->contentName, contentName) == 0) { //in this case, head-node is the one that has the content to be removed
        *head = temp->next; 
        close(temp->sd);  // Close the TCP socket
        free(temp);
        return;
    }
    
    while (temp != NULL && strcmp(temp->contentName, contentName) != 0) {
        prev = temp;
        temp = temp->next;
    }
    
    if (temp == NULL) { // contentName not found in linked list
        return;
    }    
    prev->next = temp->next; // Unlink current node from the linked list by making the prev node's pointer -> next node's pointer
    close(temp->sd);  // Close the TCP socket
    free(temp); // Free memory
}

/*************************************************************************************/


int main(int argc, char** argv){
    /* get&set server port # */
    char* indexServer;
    int indexPort;
    switch(argc){
        case 1:
            indexServer = DEFAULT_HOST_NAME;
            indexPort = DEFAULT_PORT_NUMBER;
            break;
        case 2:
            indexServer = DEFAULT_HOST_NAME;
            indexPort = atoi(argv[2]);
            break;
        case 3:
            indexServer = argv[1];
            indexPort = atoi(argv[2]);
            break;
        default:
            perror("Bad # of args\n");
            exit(EXIT_FAILURE);
    }

    /** creating a struct to store socket address of host */    
    struct sockaddr_in indexServerSocketAddr;
    bzero((char*)&indexServerSocketAddr, sizeof(struct sockaddr_in));
    indexServerSocketAddr.sin_family = AF_INET;    
    indexServerSocketAddr.sin_port = htons((u_short)indexPort);

    struct hostent *indexServerName;
    if ( indexServerName = gethostbyname(indexServer) ){
        memcpy(&indexServerSocketAddr.sin_addr, indexServerName-> h_addr, indexServerName->h_length);
    }
    else if ( (indexServerSocketAddr.sin_addr.s_addr = inet_addr(indexServer)) == INADDR_NONE ){
		perror("Couldn't get host entry\n");
		exit(EXIT_FAILURE);
    }

    /* create UDP client socket */
    int clientSocketDescriptorUDP;
    if ( (clientSocketDescriptorUDP=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
        perror("Couldn't create UDP socket\n");
        exit(EXIT_FAILURE);
    }

    /* connect to host */
    if( (connect(clientSocketDescriptorUDP, (struct sockaddr*)&indexServerSocketAddr, sizeof(indexServerSocketAddr))) == -1){
        perror("Couldn't connect to host\n");
        exit(EXIT_FAILURE);
    }

    /* get name for peer (no more than 10 characters) */    
    char tempNameBuffer[100]; //temporary buffer for peer-name. needed to handle bad input
    printf("Choose a username -> ");    
    fgets(tempNameBuffer, sizeof(tempNameBuffer), stdin);
    sscanf(tempNameBuffer, "%10s", peerName);    
    peerName[10] = '\0'; // Ensure null termination    
    printf("Hi %s.\n", peerName);
    
    /* prompt user for a command */
    char commandType;
    char tempCommandBuffer[100]; //temporary buffer for handling bad command inputs
    printf("Command (type '?' for list of commands) -> ");
    fgets(tempCommandBuffer, sizeof(tempCommandBuffer), stdin);
    sscanf(tempCommandBuffer, " %c", &commandType);
    
    while(commandType != 'Q'){
        if (tempCommandBuffer[0] == '\n') {// in case user presses the Enter key
            printf("Command (type '?' for list of commands) -> ");
            fgets(tempCommandBuffer, sizeof(tempCommandBuffer), stdin);
            sscanf(tempCommandBuffer, " %c", &commandType);
            continue;
        }
        
        pdu commandPDU;
        commandPDU.type = commandType;
        switch(commandType){
            case '?':
                printf("\n[R]:\tContent Registration\n");
                printf("[T]:\tContent De-egistration\n");
                printf("[L]:\tList LOCAL Content\n");
                printf("[D]:\tDownload Content\n");
                printf("[O]:\tList ONLINE Content\n");
                printf("[Q]:\tQuit\n\n");
                break;
            case 'R':
                registerContent(clientSocketDescriptorUDP, &indexServerSocketAddr, commandPDU);
                printNodeList(head);
                break;
            case 'T':
                deregisterContent(clientSocketDescriptorUDP, &indexServerSocketAddr, commandPDU);
                printNodeList(head);
                break;
            case 'O':
                listOnlineContent(clientSocketDescriptorUDP, &indexServerSocketAddr);
                break;
            case 'E':
                errorMessage(clientSocketDescriptorUDP, &indexServerSocketAddr, commandPDU);
                readAcknowledgement(clientSocketDescriptorUDP, &indexServerSocketAddr);
                break;
            default:
                printf("Input Error.\n");
                break;
        }        
        
        printf("Command (type '?' for list of commands) -> ");
        fgets(tempCommandBuffer, sizeof(tempCommandBuffer), stdin);
        sscanf(tempCommandBuffer, " %c", &commandType);
    }
    printf("\n****************************************************************\n");
    printf("De-registering all content...\n");
    quit(clientSocketDescriptorUDP, &indexServerSocketAddr);
    printf("****************************************************************\n");

    printf("\n\nEXITING PROGRAM.\n");
    close(clientSocketDescriptorUDP);
    exit(0);
}

void registerContent(int sd, struct sockaddr_in* server_addr, pdu registerCommandPDU) {
    /* get name of content from user*/
    char contentName[11];
    char tempContentNameBuffer[100]; //temporary buffer for handling inputs
    printf("Name of Content (10 char long) -> ");
    fgets(tempContentNameBuffer, sizeof(tempContentNameBuffer), stdin);
    sscanf(tempContentNameBuffer, "%10s", contentName);    
    while(tempContentNameBuffer[0] == '\n'){ //handle user pressing Enter key
        printf("Name of Content (10 char long) -> ");
        fgets(tempContentNameBuffer, sizeof(tempContentNameBuffer), stdin);
        sscanf(tempContentNameBuffer, "%10s", contentName);
    }
    contentName[10] = '\0'; // Ensure null termination

    /* zero out the data field of the pdu, just in case */
    memset(registerCommandPDU.data, 0, sizeof(registerCommandPDU.data));
    
    /* copy peer name and content name to pdu data field */
    memcpy(registerCommandPDU.data, peerName, sizeof(peerName));              // bytes 0-10 for peer name
    memcpy(registerCommandPDU.data + 10, contentName, strlen(contentName)); // bytes 10-20 for content name
    
    /* create TCP socket for peer becoming a content server */    
    int clientSocketDescriptorTCP;
    if ( (clientSocketDescriptorTCP=socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Couldn't create TCP socket\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in reg_addr;
    reg_addr.sin_family = AF_INET;
    reg_addr.sin_port = htons(0); //assining 0 to htons() => TCP module to choose a unique port number
    reg_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ( (bind(clientSocketDescriptorTCP, (struct sockaddr*)&reg_addr, sizeof(reg_addr))) == -1){
        perror("Couldn't bind server sd to address struct");
        exit(EXIT_FAILURE);
    }

    socklen_t alen = sizeof(struct sockaddr_in);
    if (getsockname(clientSocketDescriptorTCP, (struct sockaddr*)&reg_addr, &alen) == -1) {
        perror("getsockname() failed");
        exit(EXIT_FAILURE);
    }
    
    /* store sd of this TCP socket in a linked list, otherwise lost forever after function call finishes*/
    createAndInsertNodeAtEnd(&head, clientSocketDescriptorTCP, contentName);    
    
    uint16_t networkBytePort = reg_addr.sin_port;
    uint16_t hostBytePort = ntohs(networkBytePort);
    memcpy(registerCommandPDU.data + 20, &networkBytePort, sizeof(networkBytePort));
    
    /* 
    // hard to print this out in one go because of the different formats inside registerCommandPDU.data; 
    // there's two 10-bytes strings which can easily be extracted with %c ...
    // but then there's the port# which is in uint16_t format, which isn't easily parsed by printf("%c").    
    printf("full data: ");
    for (int i = 0; i < sizeof(registerCommandPDU.data); ++i) {
        printf("%c", registerCommandPDU.data[i]);
    }
    */
    printf("Peer Name:\t\t%s\n", registerCommandPDU.data);
    printf("Content Name:\t\t%s\n", registerCommandPDU.data + 10);        
    printf("Host Port Number:\t%u\n", (unsigned int)hostBytePort);
    printf("Network Port Number:\t%u\n", (unsigned int)(*(uint16_t *)(registerCommandPDU.data + 20)));
    printf("Network Port Number:\t%u\n", (unsigned int)networkBytePort); // just to check if the pointer to registerCommandPDU.data+20 really is pointing to the right *value*    
    printf("\n");
    
    //write(sd, &registerCommandPDU, sizeof(registerCommandPDU)); //prefer to use use sendto() for UDP communication
    if (sendto(sd, &registerCommandPDU, sizeof(registerCommandPDU), 0, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in)) == -1) {
        perror("sendto() failed");
        exit(EXIT_FAILURE);
    }

    pdu responsePDU;
    int addr_len = sizeof(struct sockaddr_in);
    int n;
    if( (n = recvfrom(sd, &responsePDU, sizeof(responsePDU), 
            0, (struct sockaddr*)server_addr, &addr_len)) ==-1 ){
            perror("recvfrom error\n");
            exit(EXIT_FAILURE);
    }

    int success = 0;
    if (responsePDU.type == 'A') {
        printf("Acknowledgement from server: %s\n", responsePDU.data);
        success = 1;
        return;
    }

    while(success != 1){
        printf("Error from server: %s\n", responsePDU.data);

        char tempNameBuffer[100];
        printf("Choose a new username -> ");
        fgets(tempNameBuffer, sizeof(tempNameBuffer), stdin);
        sscanf(tempNameBuffer, "%10s", peerName);
        peerName[10] = '\0'; // Ensure null termination
        printf("Resending %s as %s.\n", contentName, peerName);

        memset(registerCommandPDU.data, 0, sizeof(registerCommandPDU.data));
        
        memcpy(registerCommandPDU.data, peerName, sizeof(peerName));
        memcpy(registerCommandPDU.data + 10, contentName, strlen(contentName));
        memcpy(registerCommandPDU.data + 20, &networkBytePort, sizeof(uint16_t));

        if (sendto(sd, &registerCommandPDU, sizeof(registerCommandPDU), 0, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in)) == -1) {
            perror("sendto() failed");
            exit(EXIT_FAILURE);
        }

        if( (n = recvfrom(sd, &responsePDU, sizeof(responsePDU), 
            0, (struct sockaddr*)server_addr, &addr_len)) ==-1 ){
            perror("recvfrom error\n");
            exit(EXIT_FAILURE);
        }

        if (responsePDU.type == 'A') {
            printf("Acknowledgement from server: %s\n", responsePDU.data);
            success = 1;
        }
    }
}

void listOnlineContent(int sd, struct sockaddr_in* server_addr) {
    pdu listCommandPDU;
    listCommandPDU.type = 'O';
    memset(listCommandPDU.data, 0, sizeof(listCommandPDU.data));

    if (sendto(sd, &listCommandPDU, sizeof(listCommandPDU), 0, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in)) == -1) {
        perror("sendto() failed");
        exit(EXIT_FAILURE);
    }

    pdu responsePDU;
    int addr_len = sizeof(struct sockaddr_in);
    int n;
    if ((n = recvfrom(sd, &responsePDU, sizeof(responsePDU), 0, (struct sockaddr*)server_addr, &addr_len)) == -1) {
        perror("recvfrom error\n");
        exit(EXIT_FAILURE);
    }

    if (responsePDU.type == 'O') {
        printf("Online Content:\n%s\n", responsePDU.data);
    } else {
        printf("Unexpected response from server.\n");
    }
}

void deregisterContent(int sd, struct sockaddr_in* server_addr, pdu deregisterCommandPDU){
    char contentName[11];
    char tempContentNameBuffer[100];
    printf("Name of Content to De-register (10 char long) -> ");
    fgets(tempContentNameBuffer, sizeof(tempContentNameBuffer), stdin);
    sscanf(tempContentNameBuffer, "%10s", contentName);
    while(tempContentNameBuffer[0] == '\n'){ // handle user pressing Enter key
        printf("Name of Content to De-register (10 char long) -> ");
        fgets(tempContentNameBuffer, sizeof(tempContentNameBuffer), stdin);
        sscanf(tempContentNameBuffer, "%10s", contentName);
    }
    contentName[10] = '\0'; // Ensure null termination

    memset(deregisterCommandPDU.data, 0, sizeof(deregisterCommandPDU.data));    //zero out
    memcpy(deregisterCommandPDU.data, peerName, sizeof(peerName));              //set
    memcpy(deregisterCommandPDU.data + 10, contentName, strlen(contentName));   //set

    if ((sendto(sd, &deregisterCommandPDU, sizeof(deregisterCommandPDU), 0, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in))) == -1) {
        perror("sendto() failed");
        exit(EXIT_FAILURE);
    }

    pdu responsePDU;
    int addr_len = sizeof(struct sockaddr_in);
    int n;
    if ((n = recvfrom(sd, &responsePDU, sizeof(responsePDU), 0, (struct sockaddr*)server_addr, &addr_len)) == -1) {
        perror("recvfrom error\n");
        exit(EXIT_FAILURE);
    }

    if (responsePDU.type == 'A') {
        printf("Acknowledgement from server: %s\n", responsePDU.data);        
        removeNodeBySpecificValue(&head, contentName); // Remove from local list only after successful acknowledgement
    }

    else {
        printf("Error from server: %s\n", responsePDU.data);
    }    
}

void quit(int sd, struct sockaddr_in* server_addr) {

    /* 
    unfortunately can't make use of the already existing 
    deregisterContent() function since it actually
    prompts for the name of the content instead of automatically
    removing from linked list    
    */

    sd_node* temp = head;
    pdu commandPDU;
    commandPDU.type = 'T';

    while (temp != NULL) {
        sd_node* next = temp->next;
        memset(commandPDU.data, 0, sizeof(commandPDU.data));

        memcpy(commandPDU.data, peerName, sizeof(peerName));
        memcpy(commandPDU.data + 10, temp->contentName, sizeof(temp->contentName));
        
        if (sendto(sd, &commandPDU, sizeof(commandPDU), 0, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in)) == -1) {
            perror("sendto() failed");
        }

        pdu responsePDU;
        int addr_len = sizeof(struct sockaddr_in);
        int n;
        if ((n = recvfrom(sd, &responsePDU, sizeof(responsePDU), 0, (struct sockaddr*)server_addr, &addr_len)) == -1) {
            perror("recvfrom error\n");
        }

        if (responsePDU.type == 'A') {
            printf("Acknowledgement from server: %s\n", responsePDU.data);
            close(temp->sd);  // Close the TCP socket
            free(temp);       // Free the memory
        } 
        else {
            printf("Error from server: %s\n", responsePDU.data);
        }
        temp = next;
    }

    head = NULL; // idk if this makes a diff
}

void errorMessage(int sd, struct sockaddr_in* server_addr, pdu errorCommandPDU){
    strcpy(errorCommandPDU.data, "SPECIFIED ERROR.");
    if (sendto(sd, &errorCommandPDU, sizeof(errorCommandPDU), 0, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in)) == -1) {
        perror("sendto() failed");
        exit(EXIT_FAILURE);
    }
}

void readAcknowledgement(int sd, struct sockaddr_in* server_addr) {
    pdu ackPDU;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int n = recvfrom(sd, &ackPDU, sizeof(ackPDU), 0, (struct sockaddr*)server_addr, &addr_len);
    if (n < 0) {
        perror("Error reading from socket");
        exit(EXIT_FAILURE);
    }
    printf("Acknowledgement from server: %s\n", ackPDU.data);
}