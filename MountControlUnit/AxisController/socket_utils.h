/*
 * socket_utils.h
 *
 *  Created on: 03.11.2023
 *      Author: morit
 */

#ifndef SOCKET_UTILS_H_
#define SOCKET_UTILS_H_

#define DEFAULT_BUFLEN 128
#define IP_ADDRESS "192.168.0.2"
#define PORT 1000
#include <winsock2.h>
#include <stdint.h>
#include <ws2tcpip.h>
#include "json_utils.h"
void encode(unsigned char *data, unsigned char *function);
void recv_dataf(SOCKET socket, char *receivedDataBuffer, int expectedBytes);
char lenTypToByte(char lenTyp);
void getDefaultValue(SOCKET client, char *vlitem_data, char *defaultValue);
int getVLItem(VLItem *item,char*item_name);

int createConnection(SOCKET *socket, char* ip_Address, int port);
int getSocketStatus(SOCKET socket, int *boardStatus);
int getBoardItemCount(SOCKET socket, int *itemcnt);
int getBoardItems(SOCKET socket, int itemCount);
int getBoardItem(SOCKET socket, char *receivedDataBuffer, int itemNumber);
int writeRamF(SOCKET clientSocket, char *dataToSend, int dataAmount, VLItem *item);
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

int cleanup(SOCKET socket);

#endif /* SOCKET_UTILS_H_ */






