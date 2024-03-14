/**
 * @file socket_utils.c
 * @author Moritz Zideck <moritz.zideck@fantana.at>
 * @date 26.01.2024
 *
 * @brief Communication with ASA Board
 *
 * @details This file provides all needed method to create and use the socket interface.
 *	For communication TCP is used to ensure a stable connection. Furthermore a CRC is used to provide
 *	further guarantee
 **/

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <conio.h>
#include <unistd.h>
#include <string.h>
#include "cJSON.h"
#include <math.h>
#include <stdint.h>

#include "json_utils.h"
#include "vlitem_handler.h"
#include "sockets.h"
#include "logz.h"
#include "common_utils.h"


#define DEFAULT_BUFLEN 128

/**
 * @brief Buffer for sending data to the ASA board
 */
unsigned char sendData[16] ={ 0 };

/**
 * @brief Buffer for receiving data from ASA board
 */
char recieved_data[DEFAULT_BUFLEN] = {0};

/**
 * @brief Cache buffer for previously accessed VLItems.
 *
 * This buffer is designed to optimize I/O operations by caching VLItems that have already been
 * read from the vlItem.json file. When a VLItem is fetched from vlItem.json, it is stored in this buffer.
 * Subsequent accesses to the same VLItem will utilize this cache, significantly reducing the need
 * for repetitive reads from the file, thereby enhancing performance and efficiency.
 */
VLItem items[MAXSIZE];

/**
 * @brief String for log message. Used with the 'logz' logging libary
 */
char message[DEFAULT_BUFLEN*10];

/**
 * @brief Retrieves VLItem from buffer
 *
 * Searches cache for item, if not found searches in json file (IO Operation)
 *
 * @param item	Requested item
 * @param item_name	Name of requested item
 * @return	0 if successful 1 otherwise
 */
int getVLItem(VLItem *item,char*item_name){
	VLItem *itemPtr = getVlItemFromArray(item_name, items);
	if (itemPtr == NULL) {
		if(getVLItemFromJson(item, item_name)==1)return 1;
		if(0!=strcmp(item->name,item_name)){
			fprintf(stderr,"Item with the name : (%s) not found (%s)",item_name,item->name);
			fflush(stderr);
			return 1;
		}else{
			addVlItemToArray(*item, items);
		}
	} else {
		memcpy(item,itemPtr,sizeof(VLItem));
	}
	return 0;
}


/**
 * @brief Builds send function and adds CRC Code
 * @param data Information to be send
 * @param function send function to be used
 */
int encode(unsigned char *data, unsigned char *function)
{
	int len = 16;
	for (int i = 0; i < len; i++)
	{
		data[i] = 0;
	}
	data[0] = function[0];
	data[1] = function[1];
	if (function[1] == 2)
	{
		data[4] = function[2];
	}
	if (function[1] == 3 || function[1] == 4)
	{
		for (int i = 2; i < function[0] + 1; i++)
		{
			data[i] = function[i];
		}
	}
	int sum = 0;
	for (int i = 1; i < len; i++)
	{
		sum += (int) data[i];
	}
	sum = sum % 256;
	sum = 256 - sum;
	data[function[0] + 1] = sum;
	return 0;
}


/**
 * @brief Creates and configures socket and connects to ASA Board
 * @param clientSocket	Created socket
 * @important this code is WINDOWS specific
 * @return 0 if succeeded 1 otherwise
 */
int createConnection(SOCKET *clientSocket,char *ip_Address, int port){
	printf("StartUp!\n");
	fflush(stdout);
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		sprintf(message,"Connection to Board failed. Error: Failed to initialize Winsock. IP-Address : %s, Port, %d.",ip_Address,port);
		logz(message);
		printf("Failed to initialize Winsock.\n");
		return 1;
	}

	*clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*clientSocket == INVALID_SOCKET)
	{
		sprintf(message,"Connection to Board failed. Error: Failed to create socket. ERRORCODE: %d. IP-Address : %s, Port, %d.",WSAGetLastError(),ip_Address,port);
		logz(message);
		printf("Failed to create socket. Error code: %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}
	struct sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port);
	if (inet_pton(AF_INET, ip_Address, &(serverAddress.sin_addr)) <= 0)
	{
		sprintf(message,"Connection to Board failed. Error: Invalid address or address not supported. IP-Address : %s, Port, %d.",ip_Address,port);
		logz(message);
		printf("Invalid address or address not supported.\n");
		closesocket(*clientSocket);
		WSACleanup();
		return 1;
	}

	if (connect(*clientSocket, (struct sockaddr*) &serverAddress,
			sizeof(serverAddress)) < 0)
	{
		sprintf(message,"Connection to Board failed. Error: Failed to connect to the server. ERRORCODE: %d. IP-Address : %s, Port, %d.",WSAGetLastError(),ip_Address,port);
		logz(message);
		printf("Failed to connect to the server. Error code: %d\n",
				WSAGetLastError());
		closesocket(*clientSocket);
		WSACleanup();
		return 1;
	}
	else
	{

		sprintf(message,"Connection to Board established. IP-Address : %s, Port, %d.",ip_Address,port);
		logz(message);
		printf("Connected\n");
	}
	fflush(stdout);
	return 0;
}


/**
 * Receives and validates data from a TCP socket. It reads the expected amount of data plus overhead,
 * checks for connection issues, and performs a CRC check on the received data.
 * If any error occurs or CRC check fails, it logs the error, cleans up, and exits.
 *
 * @param socket TCP socket for the connection.
 * @param receivedDataBuffer Buffer for the received data.
 * @param expectedBytes Number of expected data bytes, excluding protocol overhead.
 * @return 0 on success, 1 on CRC error.
 */
int recv_dataf(SOCKET socket, char *receivedDataBuffer, int expectedBytes)
{
	int bytesRead = 0;
	int byteSum = 0;
	int totalBytes;
	int packages = expectedBytes/14;
	if(expectedBytes%14==0){
		totalBytes = expectedBytes + (packages * 2);
	}else{
		totalBytes = expectedBytes + ((packages+1) * 2);
	}


	do
	{
		bytesRead = recv(socket, receivedDataBuffer + byteSum, DEFAULT_BUFLEN,
				0);

		if (bytesRead > 0)
		{
			byteSum += bytesRead;
		}
		else if (bytesRead == 0)
		{
			logz("Board Read Operation failed: Connection closed by the server.");
			printf("Connection closed by the server.\n");
			fflush(stdout);
			closesocket(socket);
			WSACleanup();
			printf("Close Socket!!!");
			exit(EXIT_FAILURE);
		}
		else
		{
			sprintf(message,"Board Read Operation failed: Failed to receive data from the server. Error code: %d\n",
					WSAGetLastError());
			logz(message);
			printf("Failed to receive data from the server. Error code: %d\n",
					WSAGetLastError());
			fflush(stdout);
			closesocket(socket);
			WSACleanup();
			exit(EXIT_FAILURE);
		}
	} while (byteSum < totalBytes);

	//CRC Check
	int i=0;
	int numBytes;
	int leftBytes = totalBytes;
	int checksum = 0;
	do{
		if(leftBytes>=16){
			numBytes = 16;
			leftBytes-= 16;
		}else{
			numBytes = leftBytes;
		}
		for(int j = 0;j<numBytes; j++){
			checksum += (int)receivedDataBuffer[i+j];
		}
		if((0xff & checksum) != 0){
			printf("CRC ERROR WHILE READING!");
			logz("Board Read Operation failed: CRC Error at incoming data");
			return 1;
		}
		i+=16;
	}while(i<totalBytes);
	return 0;
}


/**
 * Handles sending and receiving data to/from a control board via TCP. It sends a command/data to the board and expects a response.
 * The function ensures all bytes are sent and received correctly, performing retries if necessary. It incorporates error handling
 * for both sending and receiving phases, including a CRC check through `recv_dataf`.
 *
 * @param clientSocket The socket used for communication with the control board.
 * @param bytesToSend Buffer containing bytes to send to the board.
 * @param bytesToReceive Buffer to store bytes received from the board.
 * @param bytesToSendSize Number of bytes to send.
 * @param bytesToReceiveSize Expected number of bytes to receive.
 * @return Returns 0 on successful communication, 1 on failure after retries.
 */
int controlBoardCommWR(SOCKET clientSocket, unsigned char *bytesToSend,char *bytesToReceive, size_t bytesToSendSize, size_t bytesToReceiveSize){
	size_t totalBytesSend = 0;
	size_t bytesSend = 0;

	int inc = 0;
	int error;
	do{
		 error = 0;
		while(bytesToSendSize > totalBytesSend){
			bytesSend += send(clientSocket,(char*) bytesToSend, bytesToSendSize, 0);
			if(bytesSend ==-1){
				logz("Board Communication failed. ERROR: Send failed");
				error+=1;
			}
			totalBytesSend += bytesSend;
		}
		error += recv_dataf(clientSocket,(char*) bytesToReceive, bytesToReceiveSize);

		if(bytesToSend[1] != bytesToReceive[0]){
			error += 1;
		}
		inc += error;
		if(inc == 3){
			Sleep(100);
			logz("Board GetBoardItems Operation: Data transmission failed (CRC Error)");
			return 1;
		}
	}while(error >= 1);
	return 0;
}


/**
 * Maps a length type code to its byte size. Supports codes for 2-byte types (VL_INT16, VL_UINT16)
 * and 4-byte types (VL_INT32, VL_UINT32, FLOAT). Codes 8 and 9 map to 2 bytes, while codes 10 to 12 map to 4 bytes.
 *
 * @param lenTyp Length type code.
 * @return Byte size for the given length type.
 */
size_t lenTypToByte(char lenTyp)
{
	//8: VL_INT16 Bytes: 2
	//9: VL_UINT16 Bytes: 2
	//10: VL_INT32 Bytes: 4
	//11: VL_UINT32 Bytes: 4
	//12: FLOAT Bytes: 4
	if (lenTyp > 9)
	{
		return 4;
	}
	return 2;
}


/**
 * Modifies a data buffer by shifting bytes to remove protocol-specific offsets. After the first byte,
 * every 14th byte is skipped, compacting the buffer to exclude these positions.
 *
 * @param recievedData Pointer to the data buffer.
 * @param expectedBytes Number of bytes to process.
 * @return 0 on completion.
 */
int extractInformationFromData(char *recievedData, int expectedBytes)
{
	int j = 0;
	for (int i = 0; i < expectedBytes; i++)
	{
		if (i == 0)
		{
			j += 2;
		}
		else if (i == 13 || i == 26 || i == 39 || i == 52 || i == 65)
		{
			j += 3;
		}
		recievedData[i] = recievedData[i + j];
	}
	return 0;
}


/**
 * Reads data from a board by sending a read command for a specified item. It constructs a command using the item's address
 * and length type, sends it, and processes the received data.
 *
 * @param clientSocket Socket used for communication with the board.
 * @param item_name Name of the item to read from the board.
 * @param data Buffer to store the read data.
 * @return 0 on success, 1 on failure to read or if the item is not found.
 */
int readFromBoard(SOCKET clientSocket, char *item_name, char *data)
{
	unsigned char readRam[] =
	{ 5, 4, 0, 0, 0, 0 };
	unsigned char sendData[16] =
	{ 0 };

	char receivedDataBuffer[DEFAULT_BUFLEN];
	VLItem item;
	if(getVLItem(&item,item_name)==1){
		return 1;
	}

	memcpy(&readRam[2], item.Address, sizeof(item.Address));
	size_t size	 =	lenTypToByte(item.LenTyp[0]);
	readRam[5] = size;
	encode(sendData, readRam);

	if(controlBoardCommWR(clientSocket,sendData,receivedDataBuffer,16,size)==1){
		return 1;
	}
	memcpy(data,&(receivedDataBuffer[1]),4);
	return 0;
}


/**
 * Reads numerical data from a board for a given item and converts it to a uint32_t value.
 *
 * @param clientSocket Socket for communication with the board.
 * @param item_name Name of the item to read.
 * @param num Pointer to store the converted numerical value.
 * @param size Size of the data to read and convert.
 * @return 0 on successful read and conversion, 1 on failure.
 */
int readFromBoardNumber(SOCKET clientSocket, char *item_name, uint32_t *num, size_t size){
	char data[4] ={0,0,0,0};

	readFromBoard(clientSocket, item_name, data);
	if(charArrayToUint32(data, size, num))return 1;
	return 0;
}


/**
 * Reads a float value from a board for a specified item. Validates the item's data type before reading,
 * ensuring it matches the expected float type code. Logs detailed error messages if the data type does not match.
 *
 * @param clientSocket Socket for communication with the board.
 * @param item_name Name of the item to read.
 * @param num Pointer to store the read float value.
 * @return 0 on successful read and conversion, 1 on failure or data type mismatch.
 */
int readFromBoardFloat(SOCKET clientSocket, char *item_name, float *num){
	VLItem item;

	if(getVLItem(&item,item_name)==1){
		return 1;
	}
	if(item.LenTyp[0]!=12){
		fprintf(stderr,"Wrong data type. Item %s need data type %d.",item_name,item.LenTyp[0]);
		sprintf(message,"Board Read Operation: failed. Error: Wrong data type. Item %s need data type %d.",item_name,item.LenTyp[0]);
		logz(message);
		fflush(stderr);
		return 1;
	}
	uint32_t tempNum;
	if (readFromBoardNumber(clientSocket, item_name, &tempNum, 4) == 1) {
		return 1;
	}
	memcpy(num,&tempNum,sizeof(float));

	sprintf(message, "Board Read Operation: Item='%s', Value= %f (Type: float)", item_name, *num);
	logz(message);
	return 0;
}


/**
 * Reads an int16 value from a board for a specified item. It checks the item's data type for compatibility
 * before reading, and logs errors if the data type is incorrect.
 *
 * @param clientSocket Socket for board communication.
 * @param item_name Name of the item to read.
 * @param num Pointer to store the read int16 value.
 * @return 0 on successful read and conversion, 1 on data type mismatch or read failure.
 */
int readFromBoardInt16(SOCKET clientSocket, char *item_name, int *num){
	VLItem item;
	if(getVLItem(&item,item_name)==1){
		return 1;
	}
	if(item.LenTyp[0]!=8){
		fprintf(stderr,"Wrong data type. Item %s need data type %d.",item_name,item.LenTyp[0]);
		sprintf(message,"Board Read Operation: failed. Error: Wrong data type. Item %s need data type %d.",item_name,item.LenTyp[0]);
		logz(message);
		fflush(stderr);
		return 1;
	}
	uint32_t tempNum;
	if (readFromBoardNumber(clientSocket, item_name, &tempNum, 2) == 1) {
		return 1;
	}
	memcpy(num,&tempNum,sizeof(int16_t));
	sprintf(message, "Board Read Operation: Item='%s', Value= %d (Type: int16_t)", item_name, *num);
	logz(message);
	return 0;

}


/**
 * Reads a uint16 value from a board for a specified item, ensuring the item's data type matches the expected uint16 type.
 * Errors are logged if the data type is incorrect, providing clear feedback for troubleshooting.
 *
 * @param clientSocket Socket for board communication.
 * @param item_name Name of the item to read.
 * @param num Pointer to store the read uint16 value.
 * @return 0 on successful read and conversion, 1 on data type mismatch or read failure.
 */
int readFromBoardUInt16(SOCKET clientSocket, char *item_name, uint16_t *num){
	VLItem item;
	if(getVLItem(&item,item_name)==1){
		return 1;
	}
	if(item.LenTyp[0]!=9){
		fprintf(stderr,"Wrong data type. Item %s need data type %d.",item_name,item.LenTyp[0]);
		sprintf(message,"Board Read Operation: failed. Error: Wrong data type. Item %s need data type %d.",item_name,item.LenTyp[0]);
		logz(message);
		fflush(stderr);
		return 1;
	}
	uint32_t tempNum;
	if (readFromBoardNumber(clientSocket, item_name, &tempNum, 2) == 1) {
		return 1;
	}

	memcpy(num,&tempNum,sizeof(uint16_t));
	sprintf(message, "Board Read Operation: Item='%s', Value= %u (Type: uint16_t)", item_name, *num);
	logz(message);
	return 0;
}


/**
 * Reads an int32 value from a board for a specified item, verifying the item's data type is correctly an int32.
 * If the data type does not match, an error is logged, and the function fails, providing feedback for diagnosis.
 *
 * @param clientSocket Socket for communication with the board.
 * @param item_name Name of the item to read.
 * @param num Pointer to store the read int32 value.
 * @return 0 on success, indicating the value was read and matches the expected data type; 1 on failure, due to type mismatch or other read errors.
 */
int readFromBoardInt32(SOCKET clientSocket, char *item_name, int32_t *num) {
    VLItem item;
    if (getVLItem(&item, item_name) == 1) {
        return 1;
    }
    if (item.LenTyp[0] != 10) {
        fprintf(stderr, "Wrong data type. Item %s needs data type %d.\n", item_name, item.LenTyp[0]);
		sprintf(message,"Board Read Operation: failed. Error: Wrong data type. Item %s need data type %d.",item_name,item.LenTyp[0]);
		logz(message);
		fflush(stderr);
        return 1;
    }
    uint32_t tempNum;
    if (readFromBoardNumber(clientSocket, item_name, &tempNum, 4) == 1) {
        return 1;
    }
    memcpy(num,&tempNum,sizeof(int32_t));

    sprintf(message, "Board Read Operation: Item='%s', Value= %d (Type: int32_t)", item_name, *num);
	logz(message);
    return 0;
}


/**
 * Reads a uint32 value from a board for a specified item, checking the item's data type to ensure it is a uint32.
 * If the data type is incorrect, it logs an error and fails, ensuring data integrity and correct operation handling.
 *
 * @param clientSocket Socket for board communication.
 * @param item_name Name of the item to be read.
 * @param num Pointer to store the read uint32 value.
 * @return 0 on successful data read and conversion, 1 on data type mismatch or conversion error.
 */
int readFromBoardUInt32(SOCKET clientSocket, char *item_name, uint32_t *num){
	VLItem item;
	char data[4] ={0,0,0,0};
	if(getVLItem(&item,item_name)==1){

		return 1;
	}
	if(item.LenTyp[0]!=11){
		fprintf(stderr,"Wrong data type. Item %s need data type %d.",item_name,item.LenTyp[0]);
		sprintf(message,"Board Read Operation: failed. Error: Wrong data type. Item %s need data type %d.",item_name,item.LenTyp[0]);
		fflush(stderr);
		logz(message);
		return 1;
	}

	readFromBoard(clientSocket, item_name, data);

	if(charArrayToUint32(data, 4, num)==1)return 1;

	sprintf(message, "Board Read Operation: Item='%s', Value= %u (Type: uint32_t)", item_name, *num);
	logz(message);
	return 0;
}


/**
 * Reads a bit field value from a composite board item specified by dot-notation (e.g., "item.field").
 * It parses the item and field names, retrieves the item's full data, and then extracts the designated bit field value.
 * Ensures memory is managed appropriately for dynamic allocations used during parsing.
 *
 * @param clientSocket The communication socket with the board.
 * @param input String indicating the item and its bit field.
 * @param data Pointer to store the extracted value.
 * @return 0 if successful, 1 on any failure (e.g., item not found, memory allocation error).
 */
int readFromBoardBitItem(SOCKET clientSocket, char *input, uint32_t *data){
	size_t length = strlen(input);
	char *itemName = malloc(length*sizeof(char));
	char *bitName = NULL;

	int i;
	for (i = 0; i < length && input[i] != '.'; i++) {
		itemName[i] = input[i];
	}

	itemName[i] = '\0'; // Null-terminate the first part

	if (input[i] == '.') {
		bitName = malloc(length*sizeof(char));
		strcpy(bitName, input + i + 1);
	}
	if(bitName == NULL){
		free(bitName);
		free(itemName);
	}
	VLItem item;
	BitItem *bitItem;
	uint32_t tempNum;
	size_t itemSize;
	char boardData[4] ={0,0,0,0};
	if(getVLItem(&item,itemName)==1){
		return 1;
	}
	if(item.BitItemCount<=0){
		return 1;
	}

	if(getBitItemFromVlItem(item, bitName, &bitItem)==1)return 1;

	if(readFromBoard(clientSocket, itemName, boardData)==1)return 1;

	itemSize = lenTypToByte(item.LenTyp[0]);

	if(charArrayToUint32(boardData, itemSize, &tempNum)==1)return 1;

	uint32_t bitmask = (1u << bitItem->size) - 1;
	*data = (tempNum >> bitItem->startBit)& bitmask;

	sprintf(message, "Board Read Operation: Item='%s', BitItem='%s', Value= %u (Type: bits)", itemName, bitName, *data);
	logz(message);

	free(bitName);
	free(itemName);
	return 0;
}


/**
 * Sends data to a specified memory location (item) on a board via RAM write operation. Validates the data size against
 * the expected size derived from the item's length type, constructs a command packet including the item address and data,
 * and communicates with the board using a standard send-receive protocol.
 *
 * @param clientSocket The socket used for communication with the board.
 * @param dataToSend Pointer to the data to be written to the item's memory location.
 * @param dataAmount Size of the data to send, in bytes.
 * @param item Pointer to the VLItem structure containing item details (name, address, length type).
 * @return 0 on successful write operation, 1 on error (e.g., data size mismatch, memory allocation failure).
 */
int writeRamF(SOCKET clientSocket, char *dataToSend, int dataAmount, VLItem *item)
{

	unsigned char sendData[16] = { 0 };
	char *writeRam;
	char receivedDataBuffer[DEFAULT_BUFLEN];


	size_t requiredSize = lenTypToByte(item->LenTyp[0]);

	if (dataAmount != requiredSize)
	{
		printf("Error: Data sent for %s is not the required size of %I64llu . Instead, got %u.\n",
				item->name, requiredSize, dataAmount);
		return 1;

	}

	// Allocate memory for writeRam
	writeRam = malloc((4 + requiredSize) * sizeof(char));
	if (writeRam == NULL)
	{
		printf("Error: Memory allocation failed.\n");
		return 1;
	}

	memset(writeRam, 0, 4 + requiredSize * sizeof(char));
	writeRam[0] = 4 + requiredSize;
	writeRam[1] = 3;
	memcpy(&writeRam[2], item->Address, sizeof(item->Address));
	memcpy(&writeRam[5], dataToSend, dataAmount);
	encode((unsigned char*)sendData, (unsigned char*)writeRam);
	return controlBoardCommWR(clientSocket,sendData,receivedDataBuffer,16,0);
}


/**
 * Fetches the default value of a specified board item.
 *
 * Communicates with the control board using a provided socket to retrieve the default value
 * for a board item identified by its address and type/length within `BoardItem_data`.
 * The retrieved value is stored in `defaultValue`.
 *
 * @param client Socket for communication with the control board.
 * @param BoardItem_data Pointer to data with the board item's address and type/length.
 * @param defaultValue Pointer to store the retrieved default value (up to 4 bytes).
 */
void getDefaultValue(SOCKET client, char *BoardItem_data, char *defaultValue)
{
	char receivedDataBuffer[16] =
	{ 0 };
	unsigned char sendData[16] =
	{ 0 };
	unsigned char function[6] =
	{ 0 };

	char *address = &BoardItem_data[35];
	char *lenTyp = &BoardItem_data[38];
	char size = lenTypToByte(lenTyp[0]);

	function[0] = 5;
	function[1] = 4;
	for (int var = 2; var < 5; var++)
	{
		function[var] = address[var - 2];
	}
	function[5] = size;
	encode(sendData, function);
	controlBoardCommWR(client,sendData,receivedDataBuffer,16,size);
	memcpy(defaultValue, &receivedDataBuffer[1], 4);
}


/**
 * Checks and updates the status of the control board via a socket.
 *
 * Sends a request to the control board and sets `boardStatus` to indicate readiness.
 * Logs the board's status.
 *
 * @param socket Socket used for communication.
 * @param boardStatus Pointer to store the board's status (0 for ready, 1 for not ready).
 * @return 1 on communication issues or if not ready, 0 if ready.
 */
int getSocketStatus(SOCKET socket, int *boardStatus){
	char getStatus[] ={ 1, 0 };
	encode((unsigned char*)sendData, (unsigned char*)getStatus);
	if(controlBoardCommWR(socket,sendData,recieved_data,16,2)==1){
		return 1;
	}
	if(recieved_data[0] == '\0'){
		logz("Socket status: OK.");
		*boardStatus = 0;
		return 0;
	}
	logz("Socket status: NOT READY.");
	*boardStatus = 1;
	return 1;
}


/**
 * Retrieves the number of items available on the control board.
 *
 * Sends a request to the control board to get the total item count, updates `itemCount`
 * with the number of items available, and logs the result.
 *
 * @param socket Socket used for communication with the control board.
 * @param itemCount Pointer to store the number of items available on the board.
 * @return 0 on success, 1 on failure to communicate with the board.
 */
int getBoardItemCount(SOCKET socket, int *itemCount){
	char BoardItemCount[] ={ 1, 1 };
	encode((unsigned char*)sendData, (unsigned char*)BoardItemCount);
	size_t len = sizeof(sendData);
	if(controlBoardCommWR(socket,sendData,recieved_data,len,2)==1){
		return 1;
	}
	*itemCount = (int) (unsigned char) recieved_data[1];
	sprintf(message, "%d available Items on board",*itemCount);
	logz(message);
	return 0;
}


/**
 * Retrieves and stores information about items on the control board into a JSON file.
 *
 * Iterates through each item on the control board, fetching its information and default
 * value, and then serializes this information into a JSON array. The JSON representation
 * is saved to "boardItems.json".
 *
 * @param socket Socket used for communication with the control board.
 * @param itemCount The number of items to fetch from the board.
 * @return 0 on successful execution and creation of JSON file, 1 on communication failure.
 */
int getBoardItems(SOCKET socket, int itemCount){
	char getBoardItem[] ={ 5, 2, 0 };
	size_t len = sizeof(sendData);
	cJSON *output = cJSON_CreateObject();
	cJSON *objArray = cJSON_CreateArray();
	char defaultValue[4];
	for (int i = 0; i < itemCount; i++)
	{
		getBoardItem[2] = i;
		encode((unsigned char*)sendData, (unsigned char*)getBoardItem);

		if(controlBoardCommWR(socket,sendData,recieved_data,len,70)==1){
			return 1;
		}
		extractInformationFromData(recieved_data,80);
		getDefaultValue(socket, recieved_data, defaultValue);

		char size = lenTypToByte(recieved_data[38]);
		BoardItemstoJson(recieved_data, defaultValue, size, objArray);
	}
	cJSON_AddItemToObject(output, "VLItems", objArray);
	char *jsonString = cJSON_Print(output);

	FILE *jsonFile;
	createFileStream(&jsonFile, "boardItems.json", "w",0);
	fprintf(jsonFile, "%s\n", jsonString);
	closeFileStream(jsonFile,0);
	cJSON_Delete(output);
	free(jsonString);
    logz("Creating boarditem.json succeeded");
	return 0;
}


/**
 * Creates a bitmask based on the provided BitItem properties.
 *
 * This function modifies the provided bitmask (`bitMask`) by setting bits in a range
 * defined by the `startBit` and `size` fields of a `BitItem` structure. The range
 * is set to 1s within the bitmask, leaving other bits unchanged.
 *
 * @param bitMask Pointer to the bitmask to be modified.
 * @param bitItem Pointer to a BitItem structure containing the start bit and size
 *        for the bitmask modification.
 * @return Always returns 0 to indicate success.
 */
int createBitMask(uint32_t *bitMask,BitItem *bitItem){

	*bitMask |=  (((1<<(bitItem->size))-1)<< bitItem->startBit);
	return 0;
}


/**
 * Inserts a value into a data word at a specified bit position.
 *
 * This function modifies the data pointed to by `data` by inserting `value` into it,
 * starting at `startBit`. The `value` is shifted left by `startBit` positions before
 * being OR'ed with the original data, effectively embedding `value` into `data`
 * starting at the specified bit.
 *
 * @param data Pointer to the data word to be modified.
 * @param value The value to insert into the data word.
 * @param startBit The bit position at which to start inserting `value`.
 * @return Always returns 0 to indicate success.
 */
int assembleData(uint32_t *data, uint32_t value, uint32_t startBit){

	*data |=  (value<< startBit);
	return 0;
}


/**
 * Updates bits in a VLItem's data on the board using a bitmask.
 *
 * Modifies the data of a VLItem based on a bitmask and writes the updated data back.
 * Adjusts for item data size and ensures only specified bits are altered.
 *
 * @param socket Communication socket.
 * @param data New bit values to apply.
 * @param bitMask Mask defining which bits to update.
 * @param item Item to be updated.
 * @return 0 if successful, 1 on error.
 */
int setupBitData(SOCKET socket,uint32_t data,uint32_t bitMask,VLItem *item){
	char receivedData[DEFAULT_BUFLEN] = {0};
	char senddata[4] = {0};
	readFromBoard(socket, item->name, receivedData);

	size_t requiredSize = lenTypToByte(item->LenTyp[0]);
	uint32_t boardData;
	charArrayToUint32(receivedData,requiredSize, &boardData);
	boardData = (boardData & ~bitMask) | (data & bitMask);

	if(requiredSize == 2){
		senddata[0] = (char)((boardData >> 8) & 0xFF);
		senddata[1] = (char)(boardData  & 0xFF);
	}else{
		senddata[0] = (char)((boardData >> 24) & 0xFF);
		senddata[1] = (char)((boardData >> 16) & 0xFF);
		senddata[2] = (char)((boardData >> 8) & 0xFF);
		senddata[3] = (char)(boardData & 0xFF);
	}
	uint32ToCharArray(boardData, senddata, requiredSize);
	if(writeRamF(socket, senddata, requiredSize, item)==1){
		fprintf(stderr,"Error writing to board occurred while writing to Item : %s",item->name);
		return 1;
	}
	return 0;
}


/**
 * Writes data to a specified item or bit item on the control board.
 *
 * Parses the input string to extract the item and optionally the bit item names,
 * retrieves the corresponding item or bit item structure, and writes the provided
 * data to the control board. Supports writing to both full items and individual bits
 * within items, handling bit masks and data shifting as needed.
 *
 * @param socket Socket used for communication with the control board.
 * @param input A string specifying the item and optionally the bit item (e.g., "item.bit").
 * @param data The data to write to the specified item or bit item.
 * @return 0 on success, 1 on failure due to various reasons like memory allocation failure,
 *         item or bit item not found, or write operation failure.
 */
int writeToBoard(SOCKET socket, char *input, uint32_t data){
	size_t length = strlen(input);
	char *itemName = malloc(length*sizeof(char));
	char *bitName = NULL;

	int i;
	for (i = 0; i < length && input[i] != '.'; i++) {
		itemName[i] = input[i];
	}

	itemName[i] = '\0'; // Null-terminate the first part

	if (input[i] == '.') {
		bitName = malloc(length*sizeof(char));
		if(bitName == NULL)return 1;
		strcpy(bitName, input + i + 1);
	}

	VLItem item;
	if(getVLItem(&item,itemName)==1){
		return 1;
	}

	BitItem *bitItem = NULL;
	if(bitName != NULL){
		for(int i=0;i<item.BitItemCount;i++){
			if(0==strcmp(item.BitItems[i].bitName,bitName)){
				bitItem = &item.BitItems[i];
			}
		}
		if(item.BitItemCount == 0){
			sprintf(message,"Board Write Operation : failed. Error: Item (%s) does not have BitItems.",itemName);
			logz(message);
			fprintf(stderr,"Item (%s) does not have BitItems",itemName);
			fflush(stderr);
			return 1;
		}
		if(bitItem == NULL){
			sprintf(message,"Board Write Operation : failed. Error: BitItem with the name : (%s) not found in Item (%s)",bitName,itemName);
			logz(message);
			fprintf(stderr,"BitItem with the name : (%s) not found in Item (%s)",bitName,itemName);
			fflush(stderr);
			return 1;
		}
	}else{
		if(item.BitItemCount > 0){
			sprintf(message,"Board Write Operation : failed. Error: Item (%s) consists of BitItems. Provide BitItem like \"itemName.bitName\"",itemName);
			logz(message);
			fprintf(stderr,"Item (%s) consists of BitItems. Provide BitItem like \"itemName.bitName\"",itemName);
			fflush(stderr);
			return 1;
		}
	}
	char *dataToSend;
	if(item.BitItemCount <=0){
		size_t size;
		if(lenTypToByte(item.LenTyp[0])==2){
			dataToSend = malloc(sizeof(char)*2);
			if(dataToSend == NULL)return 1;

			uint32ToCharArray(data, dataToSend, 2);
			size = 2;
		}else{
			dataToSend = malloc(sizeof(char)*4);
			if(dataToSend == NULL)return 1;
			uint32ToCharArray(data, dataToSend, 4);
			size = 4;
		}
		if(writeRamF(socket, dataToSend, size,&item)==1){
			sprintf(message,"Board Write Operation : failed. Value \"%d\" for Item (%s) could'nt be written",data,itemName);
			logz(message);
			return 1;
		}

		if(item.LenTyp[0]== 9 || item.LenTyp[0]==11){
			sprintf(message,"Board Write Operation : Item '%s' , Value= %u",itemName, data);
			logz(message);
		}
		return 0;
	}else{
		if(data>(1<<(bitItem->size -1))){
			sprintf(message,"Board Write Operation : failed. Error: Data not possible to send. Data to big. "
					"MaxSize for %s.%s is: %d. Data size tried to send %d\n",itemName,bitName,(1<<bitItem->size),data);
			logz(message);
			fprintf(stderr,"Data not possible to send. Data to big. MaxSize for %s.%s is: %d. Data size tried to send %d\n",
			itemName,bitName,(1<<bitItem->size),data);
			fflush(stderr);
			return 1;
		}

		uint32_t bitMask = 0;
		createBitMask(&bitMask, bitItem);
		//data need to be shifted to startbit position
		data = data<<bitItem->startBit;
		if(setupBitData(socket,data,bitMask,&item)==1){
			sprintf(message,"Board Write Operation : failed. Value \"%d\" for BitItem (%s) in Item (%s) could'nt be written",data,bitName,itemName);
			logz(message);
			return 1;
		}
	}
	free(itemName);
	free(bitName);
	return 0;
}


/**
 * Writes a floating-point value to a specified item on the control board.
 *
 * Converts the floating-point `data` to a `uint32_t` representation and then
 * writes this value to the control board using the `writeToBoard` function. It
 * logs the operation indicating the item being written to and the float value.
 *
 * @param socket Socket used for communication with the control board.
 * @param input A string specifying the item (and optionally the bit item) to write to.
 * @param data The floating-point data to write to the specified item.
 * @return Returns the result from `writeToBoard`: 0 on success, 1 on failure.
 */
int writeToBoardFloat(SOCKET socket, char *input, float data){
	uint32_t dataint;
	memcpy(&dataint, &data, sizeof(unsigned int));
	sprintf(message,"Board Write Operation : Item '%s' , Value= %f ",input, data);
	logz(message);
	return writeToBoard(socket, input, dataint);
}


/**
 * Writes a 16-bit integer value to a specified item on the control board.
 *
 * Converts the 16-bit `data` to a `uint32_t` representation to match the expected
 * input format of the `writeToBoard` function. It logs the operation, indicating
 * the item being written to and the integer value.
 *
 * @param socket Socket used for communication with the control board.
 * @param input A string specifying the item (and optionally the bit item) to write to.
 * @param data The 16-bit integer data to write to the specified item.
 * @return Returns the result from `writeToBoard`: 0 on success, 1 on failure.
 */
int writeToBoardInt16(SOCKET socket, char *input, int16_t data){
	uint32_t unsignedData = (uint32_t)data;
	sprintf(message,"Board Write Operation : Item '%s' , Value= %d ",input, data);
	logz(message);
	return writeToBoard(socket, input, unsignedData);
}


/**
 * Writes a 32-bit integer value to a specified item on the control board.
 *
 * Directly uses the 32-bit `data` as a `uint32_t` for compatibility with the
 * `writeToBoard` function. Logs the operation, showing the item targeted and
 * the integer value being written.
 *
 * @param socket Socket used for communication with the control board.
 * @param input String identifying the item (and optionally a bit item) to be written.
 * @param data The 32-bit integer data to write to the specified item.
 * @return Returns the result from `writeToBoard`: 0 if successful, 1 on failure.
 */
int writeToBoardInt32(SOCKET socket, char *input, int32_t data){
	uint32_t unsignedData = (uint32_t)data;
	sprintf(message,"Board Write Operation : Item '%s' , Value= %d ",input, data);
	logz(message);
	return writeToBoard(socket, input, unsignedData);
}


/**
 * Writes an item's initial value to the control board based on its data type.
 *
 * Fetches an item's details using its name, checks for a valid initial value,
 * and writes this value to the board according to the item's data type. Handles
 * various data types by calling the appropriate function for each type.
 *
 * @param socket Socket used for communication with the control board.
 * @param itemName The name of the item whose initial value is to be written.
 * @return Returns 0 on successful write, 1 on error such as item not found, initial
 *         value not set, or unsupported data type.
 */
int writeToBoardInitValue(SOCKET socket, char *itemName){
	VLItem item;
	if(getVLItem(&item,itemName)==1){
		return 1;
	}
	if(item.Value == NAN){
		fprintf(stderr,"Initial Value is null. Value cannot be written");
	}
	size_t datatyp = (size_t) item.LenTyp[0];
	if(datatyp == 8){
		return writeToBoardInt16(socket,itemName,(int)item.Value);
	}else if(datatyp == 9){
		return writeToBoard(socket,itemName,(uint16_t)item.Value);
	}else if(datatyp == 10){
		return writeToBoardInt32(socket,itemName,(int32_t) item.Value);
	}else if(datatyp == 11){
		return writeToBoard(socket,itemName,(uint32_t) item.Value);
	}else if(datatyp == 12){
		return writeToBoardFloat(socket,itemName,item.Value);
	}else{
		fprintf(stderr,"Error. Given Data type not known. Value cannot not written");
		return 1;
	}
	return 0;
}


/**
 * Clears all bit items within a specified VLItem on the control board.
 *
 * Retrieves the VLItem specified by `vlitemName` and constructs a bitmask
 * to clear (set to 0) all associated bit items. It then calls `setupBitData`
 * to apply the bitmask, effectively clearing the bit items on the board.
 *
 * @param socket Socket used for communication with the control board.
 * @param vlitemName The name of the VLItem whose bit items are to be cleared.
 * @return Returns 0 if the bit items were successfully cleared, 1 on error, such
 *         as failure to retrieve the VLItem or to create the bitmask.
 */
int clearBoardBitItems(SOCKET socket,char *vlitemName){
	VLItem item;
	uint32_t bitMask = 0;
	uint32_t data = 0;
	if(getVLItem(&item,vlitemName)==1){
		return 1;
	}
	for(int j=0; j<item.BitItemCount; j++){
		if(createBitMask(&bitMask,&(item.BitItems[j]))==1)return 1;
	}
	return setupBitData(socket,data,bitMask,&item);
}


/**
 * Writes values to specified bit items within a VLItem on the control board.
 *
 * For the given VLItem identified by `vlitemName`, this function writes provided
 * values to specified bit items. It constructs a bitmask and data value based on
 * the bit items' positions and the given values, then applies these to the board.
 *
 * @param socket Socket used for communication with the control board.
 * @param vlitemName Name of the VLItem containing the bit items to be written.
 * @param bitItemName Array of names for the bit items to be written.
 * @param values Array of values corresponding to each bit item.
 * @param size Number of bit items (and values) to be written.
 * @return Returns 0 on success, 1 on error such as failure to retrieve the VLItem,
 *         to find a specified bit item, or to apply the data and bitmask.
 */
int writeToBoardBitItems(SOCKET socket,char *vlitemName, char *bitItemName[], uint32_t values[],size_t size){
	VLItem item;
	if(getVLItem(&item,vlitemName)==1){
		return 1;
	}
	uint32_t bitMask = 0;
	uint32_t data = 0;
	BitItem *bitItem;
	for(int i=0;i<size;i++){
			getBitItemFromVlItem(item, bitItemName[i], &bitItem);
			createBitMask(&bitMask, bitItem);
			assembleData(&data, values[i], bitItem->startBit);
	}
	return setupBitData(socket,data,bitMask,&item);
}


/**
 * Sets up the control board by initializing VLItems with their default values.
 *
 * Loops through VLItems based on count, applying default values or configurations. For items
 * with bit fields, it uses a bitmask to set specific bits. For regular items, it directly writes
 * the default value. Handles both cases with appropriate data preparation and communication functions.
 *
 * @param socket Communication socket with the control board.
 * @param vLItemCount Number of VLItems to initialize.
 * @return 0 if setup is successful for all items, 1 on any failure.
 */
int setupBoard(SOCKET socket,int vLItemCount){
	VLItem item;
	for(int i=0;i<vLItemCount;i++){
		if(getVLItembyNr(&item, i)==1){
			printf("Search of ItemNr %d failed.\n",i);
			return 1;
		}
		if(item.BitItemCount>0){
			uint32_t bitMask = 0;
			uint32_t data = 0;

			for(int j=0; j<item.BitItemCount; j++){
				if(item.BitItems[j].value!=-1){
					createBitMask(&bitMask,&(item.BitItems[j]));
					assembleData(&data,item.BitItems[j].value,item.BitItems[j].startBit);
				}
			}
			setupBitData(socket, data, bitMask, &item);
		}else{
			char *dataToSend;
			if(item.Value!=NAN){
				size_t size;
				if(lenTypToByte(item.LenTyp[0])==2){
					dataToSend = malloc(sizeof(char)*2);
					size = 2;
				}else{
					dataToSend = malloc(sizeof(char)*4);
					size = 4;
				}
				doubleToCharArray(item.Value, dataToSend,size);
				writeRamF(socket, dataToSend, size, &item);
			}
		}
	}
	return 0;
}


