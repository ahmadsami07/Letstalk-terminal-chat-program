//CMPT 300 Assignment 3
//Ahmad As Sami, SFU ID:301404717
//Lets-Talk Program

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <limits.h>
#include "list.h"

#define MAX 8000

int myPortNum;
char remoteMachineName[255];
int remotePortNumber;

bool isRunning = true;
bool isHost = false;

// Thread Related
pthread_t tid[4];
int *ptr[4];

int sockfd = 123;
struct sockaddr_in servaddr, clientaddr , cliaddr;


//These two lists are created to work with the threads.
//The toSendList is going to be used by keyboard input and sender thread.
//The screenToOutputList will be used by the receiver and the printer thread.
List* screenToOutputList;
List* toSendList;
//Main flow of code we will follow: socket() -> bind() -> recvfrom() -> sendto()

//Following function is mainly to check the !status, if the server
//is online or offline.

//reference for bzero https://man7.org/linux/man-pages/man3/bzero.3.html


//For reference: https://man7.org/linux/man-pages/man7/ip.7.html
           //struct sockaddr_in {
             //  sa_family_t    sin_family; /* address family: AF_INET */
               //in_port_t      sin_port;   /* port in network byte order */
               //struct in_addr sin_addr;   /* internet address */
          // };

//int bind(int sockfd, const struct sockaddr *addr,
                //socklen_t addrlen);

void encrypt(char* data, int key);
void decrypt(char* data, int key);


//The program mainly runs on the basis of a crucial boolean
//isRunning, which dictates if the threads terminate or not.


//The following function is primarily for testing purposes
//mainly for the function when user types !status; basically 
//if binding fails for the temporary socket, this means
//that there already exists a cleint (true) so online, and
//if binding is successful, then there does not exist a client,
//which means offline.
bool checkIfServerIsAvailable()
{
	//Declaring socket
  //https://habibiefaried.medium.com/cnet-01-basic-c-socket-programming-a818b0da5ae0

	//We will first create a temporary socket for testing purposes
    int tempSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (tempSocket != -1) {

        bzero(&servaddr, sizeof(servaddr));


        // assign IP,PORT and ADDRESS FAMILY
        servaddr.sin_family = AF_INET;

        if(strcmp(remoteMachineName, "localhost") == 0)
        {
            servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        }else
        {
            servaddr.sin_addr.s_addr = inet_addr(remoteMachineName);
        }
        servaddr.sin_port = htons(remotePortNumber);

         //assigning the temporary socket's sockfd to the server address.
        //If we find the that we can successfully bind, we return true as
        //it will return 0 if successfully the socket is assigned.That  means
       // if the socket cannot be created, then it is online, and offline otherwise.
        if ((bind(tempSocket, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0) {
            close(tempSocket);
            return true;
        }else
        {
            close(tempSocket);
            return false;
        }
    }

    isRunning = false;
    return false;;
}


//Following function contains a while loop,
//Which takes the input from the keyboard and inserts it
//into the toSendList, which will be used
//by the sender thread as well.
//Just two of its outputs(checking for status)
//are directly routed tot he screenToOutputlist
//It waits for the user to input from the keyboard constantly.
void* KeyboardInput()
{
    char *buffer;
    while(isRunning)
    {
        size_t bufsize = 32;
        buffer = (char *)malloc(bufsize * sizeof(char));
        //At first, we use getline to take the message from the terminal,
        //and put it into our buffer, which will contain the message.
        getline(&buffer,&bufsize,stdin);


        //Here, there are three if clauses,
        //which will check if the user wants to know
        //status whether other server/client is available.
        //If not, the user wants to exit then it will check for exit
        //and will insert !exit into the toSendList to take it into
        //the toSendList to send it over socket as it will be displayed
        //to the other user. 
        if (strcmp(buffer, "!status\n") == 0)
        {
            if (checkIfServerIsAvailable())
                List_insert(screenToOutputList, "Online\n");
            else
                List_insert(screenToOutputList, "Offline\n");
        }
        else if (strcmp(buffer, "!exit\n") == 0)
        {
            List_insert(toSendList, "!exit");
        }
        //Here, if user sends a normal message, it will be put into the
        //toSendList to be accessed by the sender thread.
        else
        {
            List_insert(toSendList, buffer);
        }


    }

    free(buffer);
    pthread_exit((void *) 200);
}

// Function designed for chat between client and server.
//Will constantly wait for messages to come from other
//client/server through socket, and move it to the
//screenToOutputList to be used by the printer thread.
//Split it into different function for more convenience
//References: https://linux.die.net/man/2/recvfrom
//size_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 //struct sockaddr *src_addr, socklen_t *addrlen)
void RecieveChat()
{
    while(isRunning)
    {
        char buffer[MAX];

        int len, n;

        len = sizeof(cliaddr);  

        //If server, we wait for messages coming in from the cliaddr socket
        if(isHost)
        {
            n = recvfrom(sockfd, (char *)buffer, MAX,
                         MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                         (socklen_t *) &len);
        }
        //If client, we wait for messages comng in from the servaddr socket
        else
        {
            n = recvfrom(sockfd, (char *)buffer, MAX,
                         MSG_WAITALL, (struct sockaddr *) &servaddr,
                         (socklen_t *) &len);
        }

        //Ending the msg with the null terminator
        buffer[n] = '\0';

        decrypt(buffer, 1);
        List_insert(screenToOutputList, &buffer);
    }

    close(sockfd);
}


//The most important thread, which initiates the 
//sockets. It creats a socket for the inputted addressl
//It also checks if another socket is already created. 
//It then binds the sockets together for client and server
//After this, it initiates the RecieveChat, which
//then starts listening for messages coming through socket
//void bzero(void *s, size_t n);
// Referenced from //https://www.binarytides.com/programming-udp-sockets-c-linux/
void* SocInitRecThread()
{
    int portToUse;

    if(checkIfServerIsAvailable())
        portToUse = remotePortNumber;
    else
        portToUse = myPortNum;

    // socket create and verification
    //As per the assignment we have to check everytime if a socket
    //is correctly created or not
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        isRunning = false;
        printf("socket creation failed... Type \"!exit\" If needed or close program.\n");
    }
    else
    {
    	//bzero erases data of the servaddr and memsets to 0
        bzero(&servaddr, sizeof(servaddr));

        // assign IP,PORT and ADDRESS FAMILY for the server
        servaddr.sin_family = AF_INET;


        //Localhost ip is converted into 127.0.0.1 (same machine)
        //or if different named machine, it is converted to an address
        //usable my socket using inet_addr
        if(strcmp(remoteMachineName, "localhost") == 0)
        {
            servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        }else
        {
            servaddr.sin_addr.s_addr = inet_addr(remoteMachineName);
        }

        servaddr.sin_port = htons(portToUse);

        // Binding newly created socket to given IP and verification
        if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) != 0) {
            // If binding failed, that means  client.
            

            bzero(&clientaddr, sizeof(clientaddr));

            // assign IP,PORT and ADDRESS FAMILY of client
            clientaddr.sin_family = AF_INET;

             //Localhost ip is converted into 127.0.0.1 (same machine)
      		 //or if different named machine, it is converted to an address
        	//usable my socket using inet_addr
            if(strcmp(remoteMachineName, "localhost") == 0)
            {
                clientaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            }
            else
            {
                clientaddr.sin_addr.s_addr = inet_addr(remoteMachineName);
            }

            clientaddr.sin_port = htons(myPortNum);

            //we now bind the client socket
            if (bind(sockfd, (struct sockaddr *) &clientaddr, sizeof(clientaddr)) == 0)
            {
                //Testing if sendto works
                if (sendto( sockfd, "", 0,
                            MSG_CONFIRM, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) {
                    printf("Sendto failed.");
                }

                RecieveChat();
            }
        }
        else
        {
            isHost = true;
            RecieveChat();
        }
    }

    close(sockfd);
    isRunning = false;
    pthread_exit((void *) 200);
}


//The sender thread waits until the keyboard input
//thread adds messages to the toSendList, and will relay it to 
//the other client.
void* SenderThread()
{
    while(isRunning)
    {
    	//First, we check if the sending list is empty or not, and if not emprty,
        // we proceed to send it over to socket
        if(List_count(toSendList) > 0)
        {   
            //to check if current item is null
            if(List_curr(toSendList) != NULL)
            {
            	//For encryption purposes we will create another char array
            	char messageToSend[MAX];
                strcpy(messageToSend, List_last(toSendList));

                encrypt(messageToSend, 1);

                if(!isHost)
                {
                    if (sendto( sockfd, messageToSend, strlen(List_last(toSendList)),
                                MSG_CONFIRM, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ) 
                                {
                        printf("Sendto failed!!");
                    }
                }
                //otherwise it is host so we sent message to cliaddr 
                else
                {
                    int len;
                    len = sizeof(cliaddr);

                    sendto(sockfd, messageToSend, strlen(List_last(toSendList)),
                           MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                           len);
                }
            }
            else
            {
                printf("A value came NULL that you are trying to send.\n");
            }

            //To stop the program, we wait for !exit, and
            // we finally turn off the isRunning to false
            //which allows the other threads to terminate
            if(strcmp(List_last(toSendList), "!exit") == 0)
            {
                isRunning = false;
                printf("Press any key to exit.\n");
                fflush(stdout);
            }

            //removing the last item on the tosendlist after sending
            List_trim(toSendList);
            
        }

        sleep(0.1);
    }
    
    
    pthread_exit((void *) 200);
}


//This function is the printer thread which deals with printing the 
//messages on screen.
//It constantly waits if there are outstanding items on the 
//screenToOutputlist, and whenever there is an item,
//it immideately prints it and trims the list,
//and fflushes the stdout.
void* PrinterThread()
{
    while(isRunning)
    {
        if(List_count(screenToOutputList) > 0)
        {
            if(List_curr(screenToOutputList) != NULL)
            {
            	//If !exit is sent over the socket, then we stop the program
            	//by making isRunning=false.
                if(strcmp(List_last(screenToOutputList), "!exit") == 0)
                {
                    isRunning = false;
                    printf("%s\n", List_last(screenToOutputList));
                    printf("Press any key to exit.\n");
                }
                //Otherwise, we just print the message to the terminal
                else
                {
                    printf("%s",List_last(screenToOutputList));
                }
            }else
            {
                printf("A value came NULL\n");
            }

            //remove the message from the list
            List_trim(screenToOutputList);
            fflush(stdout);
        }

        sleep(0.1);
    }

    //Memory leak preventions
    List_free(screenToOutputList, NULL);
    List_free(toSendList, NULL);

    pthread_exit((void *) 200);
}

int main(int argc, char* argv[]){

//The main creates all the thread functions and joins them 
//Also assigns the remote machine names
    screenToOutputList = List_create();
    toSendList = List_create();


    //First, we assign all the arguments to the respective variables:the port number and machine names
    myPortNum = atoi(argv[1]);
    strcpy(remoteMachineName, argv[2]);
    remotePortNumber = atoi(argv[3]);
    fflush(stdout);

    // Checks
    if(!isRunning)
    {
        return(0);
    }


    //Creating all threads
    int keyboardInputThread = pthread_create(&(tid[0]), NULL, &KeyboardInput, NULL);
    int SenderThrd = pthread_create(&(tid[1]), NULL, &SenderThread, NULL);
    int Socinitchatthread = pthread_create(&(tid[2]), NULL, &SocInitRecThread, NULL);
    int PrinterThrd= pthread_create(&(tid[3]), NULL, &PrinterThread, NULL);

    printf("Welcome to Lets-Talk! Please type your messages now.\n");

    // Important that first pthread checked to be "Joined" is the Keyboard Input thread,
    // as it manages if we are to exit application or not.
    //Mainly, as soon as !exit is inputted all threads are exited in order one by one.
    pthread_join(tid[0], (void**)&(ptr[0]));
    //printf("Joined keyboard\n");

    pthread_cancel(tid[2]);

    pthread_join(tid[2], (void**)&(ptr[2]));
    //printf("Joined socinit\n");

    pthread_join(tid[1], (void**)&(ptr[1]));
    //printf("Joined sender\n");


    pthread_join(tid[3], (void**)&(ptr[3]));
    //printf("Joined printr\n");


    // Must ensure that sockets are closed.
    if(sockfd != 123)
    {
        close(sockfd);
    }

    return 0;
}


//Idea referenced from: https://www.thecrazyprogrammer.com/2016/11/caesar-cipher-c-c-encryption-decryption.html

//We decrypt by doing the opposite of encrypt;
//by subtracting 1 from the ascii value of each character we enter.
void decrypt(char* data, int key)
{
    char temp;
    int count;
    for(count = 0; data[count] != '\0'; count++)
    {
        temp = data[count];
        if(temp >= 'a' && temp <= 'z')
        {
            temp = temp + key;
            if(temp > 'z')
            {
                temp = temp - 'z' + 'a' - 1;
            }
            data[count] = temp;
        }
        else if(temp >= 'A' && temp <= 'Z')
        {
            temp = temp + key;
            if(temp > 'Z')
            {
                temp = temp - 'Z' + 'A' - 1;
            }
            data[count] = temp;
        }
    }
}

//We encrypt by adding 1 to the ascii value of each character entered
void encrypt(char* data, int key)
{
    char temp;
    int count;

    for(count = 0; data[count] != '\0'; count++)
    {
        temp = data[count];
        if(temp >= 'a' && temp <= 'z')
        {
            temp = temp - key;
            if(temp < 'a')
            {
                temp = temp + 'z' - 'a' + 1;
            }
            data[count] = temp;
        }
        else if(temp >= 'A' && temp <= 'Z')
        {
            temp = temp - key;
            if(temp < 'A')
            {
                temp = temp + 'Z' - 'A' + 1;
            }
            data[count] = temp;
        }
    }
}







