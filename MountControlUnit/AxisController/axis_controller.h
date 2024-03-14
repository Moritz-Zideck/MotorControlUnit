#ifndef AXIS_CONTROLLER_H
#define AXIS_CONTROLLER_H

// Include any necessary headers here
#include <winsock2.h>
#include <stdint.h>
// Declare any global constants or macros here

// Declare any global variables here

// Declare any function prototypes here

//SOCKET_UTILS.c
int createConnection(SOCKET *socket, char* ip_Address, int port);
int getSocketStatus(SOCKET socket, int *boardStatus);
int getBoardItemCount(SOCKET socket, int *itemcnt);
int getBoardItems(SOCKET socket, int itemCount);
int getBoardItem(SOCKET socket, char *receivedDataBuffer, int itemNumber);
int setupBoard(SOCKET socket,int vLItemCount);

int readFromBoard(SOCKET clientSocket, char *receivedDataBuffer, char *item_name);

int readFromBoardInt16(SOCKET clientSocket, char *item_name, int *num);
int readFromBoardUInt16(SOCKET clientSocket, char *item_name, uint16_t *num);
int readFromBoardInt32(SOCKET clientSocket, char *item_name, int32_t *num);
int readFromBoardUInt32(SOCKET clientSocket, char *item_name, uint32_t *num);
int readFromBoardFloat(SOCKET clientSocket, char *item_name, float *num);
int readFromBoardBitItem(SOCKET clientSocket, char *input, uint32_t *data);

int writeToBoard(SOCKET socket, char *input, uint32_t data);
int writeToBoardFloat(SOCKET socket, char *input, float data);
int writeToBoardInt16(SOCKET socket, char *input, int16_t data);
int writeToBoardInt32(SOCKET socket, char *input, int32_t data);
int writeToBoardBitItems(SOCKET socket,char *vlitemName, char **bitItemName,uint32_t *values,size_t size);
int clearBoardBitItems(SOCKET socket,char *vlitemName);

//AXLE_CONTROLLER.c
int initialise(SOCKET *clientSocket);
#endif // AXIS_CONTROLLER_H