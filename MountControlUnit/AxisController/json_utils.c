/*
 * json_utils.c
 *
 *  Created on: 03.11.2023
 *      Author: morit
 */
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <unistd.h>
#include <string.h>
#include "json_utils.h"
#include "cJSON.h"
#include <math.h>
#include "common_utils.h"

/**
 * @brief  Function to create a file stream.
 *
 * This function creates a file stream with the specified file name and privileges.
 * Optionally, it can create the file in a shared directory if the 'shared' parameter is set to 1.
 *
 * @param filePointer Pointer to a FILE pointer where the created file stream will be stored.
 * @param fileName    Name of the file to be created.
 * @param privileges  String indicating the privileges for opening the file (e.g., "r", "w", "a", etc.).
 * @param shared      Flag indicating whether the file is in a shared directory (1 for shared, 0 for not shared).
 * @return            Returns 0 on success, 1 if the file cannot be opened.
 */
int createFileStream(FILE **filePointer, char *fileName, char *privileges,int shared) {
	char path[256];
	if(shared == 1){
		if (sem_wait(&sharedDir) == -1) {
			perror("sem_wait");
			pthread_exit(NULL);
		}
		sprintf(path, "./shared/%s",fileName);
	}else{
		sprintf(path, "./axle_%d/%s",axleNum,fileName);
	}
    *filePointer = fopen(path, privileges);

    if (!*filePointer) {
    	return 1;
    }
    return 0;
}

/**
 * @brief  Function to close a file stream.
 *
 * This function closes the specified file stream. If the file was opened from a shared directory
 * and 'shared' is set to 1, it releases the semaphore associated with the shared directory.
 *
 * @param filePointer Pointer to the FILE stream to be closed.
 * @param shared      Flag indicating whether the file was opened from a shared directory (1 for shared, 0 for not shared).
 * @return            Returns 0 on success, EOF if an error occurs while closing the file stream.
 */
int closeFileStream(FILE *filePointer,int shared){
	if(shared == 1){
		if (sem_post(&sharedDir) == -1) {
			perror("sem_wait");
			pthread_exit(NULL);
		}
	}
	return fclose(filePointer);
}

/**
 * @brief Function to parse JSON data from a file and obtain the root cJSON object.
 *
 * This function reads JSON data from the provided file stream and parses it to obtain the root cJSON object.
 * It dynamically allocates memory for a buffer to read the JSON data from the file.
 * The memory for the buffer is freed after parsing is complete.
 *
 * @param root         Pointer to a pointer to a cJSON object where the root cJSON object will be stored.
 * @param filePointer  Pointer to the FILE stream from which JSON data will be read.
 * @return             Returns 0 on success, 1 if an error occurs during memory allocation or JSON parsing.
 */
int getJsonRoot(cJSON **root, FILE *filePointer) {
    // Initialize buffer
    char *json_buffer = NULL;
    size_t buffer_size = 128;
    size_t current_size = 0;
    json_buffer = (char *)malloc(buffer_size);
    if(json_buffer == NULL){
    	fprintf(stderr,"Memory allocation error\n");
    	return 1;
    }
    // Read the file character by character
    int current_char;
    while ((current_char = fgetc(filePointer)) != EOF) {
        // Expand buffer if needed
        if (current_size == buffer_size) {
            buffer_size += 128;  // You can adjust this value based on your needs
            json_buffer = (char *)realloc(json_buffer, buffer_size);
            if (json_buffer == NULL) {
            	closeFileStream(filePointer,0);
                fprintf(stderr,"Memory allocation error\n");
                return 1;
            }
        }

        // Append character to buffer
        json_buffer[current_size++] = (char)current_char;
    }
    //WARNING NO SEMAPHORE
    closeFileStream(filePointer,0);

    // Null-terminate the buffer
    json_buffer[current_size] = '\0';

    // Parse the JSON data
    *root = cJSON_Parse(json_buffer);

    // Check if parsing was successful
    if (*root == NULL) {
    	free(json_buffer);
        fprintf(stderr, "Error parsing JSON.\n");
        return 1;
    }

    // Clean up
    free(json_buffer);
    return 0;
}

/**
 * @brief Function to create a JSON array and add it to a cJSON object.
 *
 * This function creates a JSON array from the provided data and adds it to the specified cJSON object.
 *
 * @param json  Pointer to the cJSON object to which the JSON array will be added.
 * @param data  Pointer to the data array.
 * @param size  Size of the data array.
 * @param name  Name of the JSON array.
 */
void createJsonArray(cJSON *json, char *data, int size, char *name)
{
	cJSON *addressArray = cJSON_CreateArray();
	for (int i = 0; i < size; i++)
	{
		cJSON_AddItemToArray(addressArray,
				cJSON_CreateNumber((unsigned char) data[i]));
	}
	cJSON_AddItemToObject(json, name, addressArray);
}


/**
 * @brief Convert an INI file to JSON format.
 *
 * This function reads an INI file, parses its contents, and converts it to JSON format.
 * Each section in the INI file becomes an object in the JSON output.
 * Key-value pairs within each section become properties of the corresponding JSON object.
 * The resulting JSON is written to a new file named "CI-Servo.json".
 *
 * @return Returns 0 on success, 1 if there was an error during file operations or JSON creation.
 */
int iniToJson()
{
	FILE *iniFile = NULL;
	if(createFileStream(&iniFile, "Cl-Servos.ini", "r",1)==1){
		fprintf(stderr,"Error CI-Servos.ini couldn't be opened\n");
		return 1;
	}
	cJSON *root = cJSON_CreateObject();
	cJSON *currentGroup = NULL;

	char line[256];
	char group[256] = "";

	while (fgets(line, sizeof(line), iniFile))
	{
		// Remove trailing newline character
		line[strcspn(line, "\n")] = '\0';
		if (strlen(line) > 0)
		{
			if (line[0] == '[' && line[strlen(line) - 1] == ']')
			{
				if (currentGroup)
				{
					cJSON_AddItemToObject(root, group, currentGroup);
				}
				strncpy(group, &line[1], strlen(line) - 2);
				group[strlen(line) - 2] = '\0';
				currentGroup = cJSON_CreateObject();
			}
			else if (currentGroup)
			{
				char *description = strtok(line, "=");
				char *value = strtok(NULL, "=");

				if (description && value)
				{
					cJSON_AddItemToObject(currentGroup, description,
							cJSON_CreateString(value));
				}
			}
		}
	}
	if (currentGroup)
	{
		cJSON_AddItemToObject(root, group, currentGroup);
	}
	closeFileStream(iniFile,1);
	FILE *servoFile = NULL;
	if(createFileStream(&servoFile, "CI-Servo.json", "w",0)==1){
		perror("Failed to create output file");
		cJSON_Delete(root);
		return 1;
	}

	char *jsonString = cJSON_Print(root);
	fprintf(servoFile, "%s\n", jsonString);
	closeFileStream(servoFile,0);
	free(jsonString);
	cJSON_Delete(root);
	return 0;
}

/**
 * @brief Finds a board item by its name in a cJSON root object.
 *
 * This function searches for a board item with the specified name in a cJSON root object.
 * If found, it creates a BoardItem structure and populates it with the details of the found board item.
 *
 * @param root The cJSON root object containing the board items.
 * @param name The name of the board item to find.
 * @return Returns a pointer to the BoardItem structure if found, or NULL if not found.
 */
BoardItem* findBoardItemByName(cJSON *root, char *name)
{
	cJSON *BoardItems = cJSON_GetObjectItem(root, "VLItems");

	if (BoardItems == NULL || !cJSON_IsArray(BoardItems))
	{
		fprintf(stderr,"BoardItems not found\n");
		return NULL;
	}

	for (int i = 0; i < cJSON_GetArraySize(BoardItems); i++)
	{
		cJSON *jBoardItem = cJSON_GetArrayItem(BoardItems, i);
		cJSON *nameItem = cJSON_GetObjectItem(jBoardItem, "name");

		if (nameItem != NULL && cJSON_IsString(nameItem)
				&& strcmp(nameItem->valuestring, name) == 0)
		{

			BoardItem *BoardItem_struct = (BoardItem*) malloc(sizeof(BoardItem));

			BoardItem_struct->name[sizeof(BoardItem_struct->name) - 1] = '\0';

			strncpy(BoardItem_struct->name, nameItem->valuestring,
					sizeof(BoardItem_struct->name));

			cJSON *addressItem = cJSON_GetObjectItem(jBoardItem, "Address");
			for (int j = 0; j < 3; j++)
			{
				cJSON *byteItem = cJSON_GetArrayItem(addressItem, j);
				BoardItem_struct->Address[j] = (char) (byteItem->valueint & 0xFF); // Extract the lower 8 bits
			}
			cJSON *lenTypItem = cJSON_GetObjectItem(jBoardItem, "LenTyp");
			for (int j = 0; j < 2; j++)
			{
				cJSON *byteItem = cJSON_GetArrayItem(lenTypItem, j);
				BoardItem_struct->LenTyp[j] = (char) (byteItem->valueint & 0xFF); // Extract the lower 8 bits
			}

			cJSON *flagsItem = cJSON_GetObjectItem(jBoardItem, "Flags");
			for (int j = 0; j < 2; j++)
			{
				cJSON *byteItem = cJSON_GetArrayItem(flagsItem, j);
				BoardItem_struct->Flags[j] = (char) (byteItem->valueint & 0xFF); // Extract the lower 8 bits
			}

			cJSON *symbolItem = cJSON_GetObjectItem(jBoardItem, "Symbol");
			for (int j = 0; j < 10; j++)
			{
				cJSON *byteItem = cJSON_GetArrayItem(symbolItem, j);
				BoardItem_struct->Symbol[j] = (char) (byteItem->valueint & 0xFF); // Extract the lower 8 bits
			}

			cJSON *scaleFactorItem = cJSON_GetObjectItem(jBoardItem, "ScaleFactor");
			for (int j = 0; j < 4; j++)
			{
				cJSON *byteItem = cJSON_GetArrayItem(scaleFactorItem, j);
				BoardItem_struct->ScaleFactor[j] = (char) (byteItem->valueint
						& 0xFF); // Extract the lower 8 bits
			}

			cJSON *unitItem = cJSON_GetObjectItem(jBoardItem, "Unit");
			for (int j = 0; j < 6; j++)
			{
				cJSON *byteItem = cJSON_GetArrayItem(unitItem, j);
				BoardItem_struct->Unit[j] = (char) (byteItem->valueint & 0xFF); // Extract the lower 8 bits
			}

			cJSON *minValItem = cJSON_GetObjectItem(jBoardItem, "MinVal");
			for (int j = 0; j < 4; j++)
			{
				cJSON *byteItem = cJSON_GetArrayItem(minValItem, j);
				BoardItem_struct->MinVal[j] = (char) (byteItem->valueint & 0xFF); // Extract the lower 8 bits
			}

			cJSON *maxValItem = cJSON_GetObjectItem(jBoardItem, "MaxVal");
			for (int j = 0; j < 4; j++)
			{
				cJSON *byteItem = cJSON_GetArrayItem(maxValItem, j);
				BoardItem_struct->MaxVal[j] = (char) (byteItem->valueint & 0xFF); // Extract the lower 8 bits
			}

			cJSON *defaultValueItem = cJSON_GetObjectItem(jBoardItem,
					"DefaultValue");
			int arrayLength = cJSON_GetArraySize(defaultValueItem);

			for (int j = 0; j < arrayLength; j++)
			{
				cJSON *byteItem = cJSON_GetArrayItem(defaultValueItem, j);
				BoardItem_struct->DefaultValue[j] = (char) (byteItem->valueint & 0xFF); // Extract the lower 8 bits
			}

			return BoardItem_struct;
		}
	}
	fprintf(stderr,"Item (%s) not found\n",name);
	return NULL; // BoardItem not found
}

/**
 * @brief Retrieve a BoardItem from a JSON file.
 *
 * This function reads a JSON file containing BoardItem data and retrieves the BoardItem
 * with the specified item name.
 *
 * @param itemName The name of the BoardItem to retrieve.
 * @return         A pointer to the retrieved BoardItem if found, or NULL if not found or an error occurs.
 */
BoardItem* getBoardItemFromJson(char *itemName)
{
	FILE *boardItemJsonFile = NULL;
	if(createFileStream(&boardItemJsonFile, "boardItems.json", "r",0)==1){
		fprintf(stderr,"Error boardItems.json couldn't be opened\n");
		return NULL;
	}
	// Get the file size
	fseek(boardItemJsonFile, 0, SEEK_END);
	long file_size = ftell(boardItemJsonFile);
	fseek(boardItemJsonFile, 0, SEEK_SET);
	// Read the file into a buffer
	char *json_buffer = (char*) malloc(file_size + 1);
	if (json_buffer == NULL)
	{
		closeFileStream(boardItemJsonFile,0);
		fprintf(stderr, "Memory allocation error.\n");
		return NULL;
	}

	fread(json_buffer, 1, file_size, boardItemJsonFile);
	closeFileStream(boardItemJsonFile,0);

	// Null-terminate the buffer
	json_buffer[file_size] = '\0';
	// Parse the JSON data
	cJSON *root = cJSON_Parse(json_buffer);

	// Check if parsing was successful
	if (root == NULL)
	{
		fprintf(stderr, "Error parsing JSON.\n");
		fflush(stdout);
		return NULL;
	}


	// Find the VLItem by name
	BoardItem *foundBoardItem = findBoardItemByName(root, itemName);

	// Free cJSON objects
	return foundBoardItem;
}

/**
 * @brief Retrieve BitItem data from a cJSON root object.
 *
 * This function extracts BitItem data from a cJSON root object and populates a BitItem structure.
 *
 * @param root Pointer to the cJSON root object containing BitItem data.
 * @param item Pointer to the BitItem structure to be populated.
 * @return Returns 0 on success.
 */
int getBitItemFromJson(cJSON* root,BitItem *item){
	cJSON *bitNameField = cJSON_GetObjectItem(root, "BitName");
	item->bitName[sizeof(bitNameField->valuestring)] = '\0';
	strncpy(item->bitName, bitNameField->valuestring,sizeof(item->bitName));

	cJSON *CIField = cJSON_GetObjectItem(root, "CI-Field");
	item->CIField[sizeof(CIField->valuestring)] = '\0';
	strncpy(item->CIField, CIField->valuestring,sizeof(item->CIField));

	cJSON *CIKey = cJSON_GetObjectItem(root, "CI-Key");
	item->CIKey[sizeof(CIKey->valuestring)] = '\0';
	strncpy(item->CIKey, CIKey->valuestring,sizeof(item->CIKey));

	cJSON *startBit = cJSON_GetObjectItem(root, "StartBit");
	item->startBit = startBit->valueint;

	cJSON *size = cJSON_GetObjectItem(root, "Size");
	item->size = size->valueint;

	cJSON *value = cJSON_GetObjectItem(root, "Value");
	if(cJSON_IsNull(value)){
		item->value=-1;
	}else{
		item->value = value->valueint;
	}
	return 0;
}

/**
 * @brief Retrieve VLItem data from a cJSON root object.
 *
 * This function extracts VLItem data from a cJSON root object and populates a VLItem structure.
 *
 * @param vlItem Pointer to the cJSON object containing VLItem data.
 * @param item   Pointer to the VLItem structure to be populated.
 * @return       Returns 0 on success.
 */
int getVLItemFromRoot(cJSON *vlItem,VLItem *item){

	cJSON *vlItemName= cJSON_GetObjectItem(vlItem, "name");
	cJSON *vlItemCIField= cJSON_GetObjectItem(vlItem, "CIField");
	cJSON *vlItemCIKey= cJSON_GetObjectItem(vlItem, "CIKey");

	item->name[sizeof(vlItemName->valuestring)] = '\0';
	strncpy(item->name, vlItemName->valuestring,sizeof(item->name));

	item->CIField[sizeof(vlItemCIField->valuestring)] = '\0';
	strncpy(item->CIField, vlItemCIField->valuestring,sizeof(item->CIField));

	item->CIKey[sizeof(vlItemCIKey->valuestring)] = '\0';
	strncpy(item->CIKey, vlItemCIKey->valuestring,sizeof(item->CIKey));

	cJSON *addressItem = cJSON_GetObjectItem(vlItem, "Address");
	for (int j = 0; j < 3; j++) {
		cJSON *byteItem = cJSON_GetArrayItem(addressItem, j);
		item->Address[j] = (char)(byteItem->valueint & 0xFF); // Extract the lower 8 bits
	}

	cJSON *lenTypItem = cJSON_GetObjectItem(vlItem, "LenTyp");
	for (int j = 0; j < 2; j++) {
		cJSON *byteItem = cJSON_GetArrayItem(lenTypItem, j);
		item->LenTyp[j] = (char)(byteItem->valueint & 0xFF); // Extract the lower 8 bits
	}

	cJSON *flagsItem = cJSON_GetObjectItem(vlItem, "Flags");
	for (int j = 0; j < 2; j++) {
		cJSON *byteItem = cJSON_GetArrayItem(flagsItem, j);
		item->Flags[j] = (char)(byteItem->valueint & 0xFF); // Extract the lower 8 bits
	}

	cJSON *symbolItem = cJSON_GetObjectItem(vlItem, "Symbol");
	for (int j = 0; j < 10; j++) {
		cJSON *byteItem = cJSON_GetArrayItem(symbolItem, j);
		item->Symbol[j] = (char)(byteItem->valueint & 0xFF); // Extract the lower 8 bits
	}

	cJSON *scaleFactorItem = cJSON_GetObjectItem(vlItem, "ScaleFactor");
	for (int j = 0; j < 4; j++) {
		cJSON *byteItem = cJSON_GetArrayItem(scaleFactorItem, j);
		item->ScaleFactor[j] = (char)(byteItem->valueint & 0xFF); // Extract the lower 8 bits
	}

	cJSON *unitItem = cJSON_GetObjectItem(vlItem, "Unit");
	for (int j = 0; j < 6; j++) {
		cJSON *byteItem = cJSON_GetArrayItem(unitItem, j);
		item->Unit[j] = (char)(byteItem->valueint & 0xFF); // Extract the lower 8 bits
	}

	cJSON *minValItem = cJSON_GetObjectItem(vlItem, "MinVal");
	for (int j = 0; j < 4; j++) {
		cJSON *byteItem = cJSON_GetArrayItem(minValItem, j);
		item->MinVal[j] = (char)(byteItem->valueint & 0xFF); // Extract the lower 8 bits
	}

	cJSON *maxValItem = cJSON_GetObjectItem(vlItem, "MaxVal");
	for (int j = 0; j < 4; j++) {
		cJSON *byteItem = cJSON_GetArrayItem(maxValItem, j);
		item->MaxVal[j] = (char)(byteItem->valueint & 0xFF); // Extract the lower 8 bits
	}

	cJSON *bitItems = cJSON_GetObjectItem(vlItem, "BitItems");
	int s = cJSON_GetArraySize(bitItems);
	item ->BitItemCount = s;
	item->BitItems = malloc(s*sizeof(BitItem));
	for(int i = 0;i < s; i++){
		cJSON *bitItem = cJSON_GetArrayItem(bitItems, i);
		getBitItemFromJson(bitItem, &(item->BitItems[i]));
	}

	cJSON *defaultValueItem = cJSON_GetObjectItem(vlItem,"DefaultValue");
	int arrayLength = cJSON_GetArraySize(defaultValueItem);
	for (int j = 0; j < arrayLength; j++)
	{
		cJSON *byteItem = cJSON_GetArrayItem(defaultValueItem, j);
		item->DefaultValue[j] = (char) (byteItem->valueint & 0xFF); // Extract the lower 8 bits
	}

	cJSON *value = cJSON_GetObjectItem(vlItem,"Value");
	if(cJSON_IsNull(value)){
		item->Value = NAN;
	}else{
		item->Value = value->valuedouble;
	}

	return 0;
}

/**
 * @brief Retrieve VLItem data from a JSON file based on the provided name.
 *
 * This function reads a JSON file containing VLItem data, searches for a VLItem with the specified name,
 * and populates a VLItem structure with the found data.
 *
 * @param item   Pointer to the VLItem structure to be populated.
 * @param input  Name of the VLItem to search for.
 * @return       Returns 0 on success, 1 if an error occurs.
 */
int getVLItemFromJson(VLItem *item,char *input){

	cJSON *vlItemRoot = NULL;
	FILE *vlItemFile = NULL;

	if(createFileStream(&vlItemFile, "vlItem.json", "r",0)==1){
		fprintf(stderr,"Error vlItems.json couldn't be opened\n");
		return 1;
	}
	if(getJsonRoot(&vlItemRoot, vlItemFile)==1){
		return 1;
	}
	closeFileStream(vlItemFile,0);

	cJSON *vlitems = cJSON_GetObjectItem(vlItemRoot, "ItemData");
	if(vlitems == NULL)return 1;
	for (int i = 0; i < cJSON_GetArraySize(vlitems); i++) {
		cJSON *vlItem = cJSON_GetArrayItem(vlitems, i);
		cJSON *vlItemName= cJSON_GetObjectItem(vlItem, "name");

		if (vlItemName != NULL && cJSON_IsString(vlItemName) && strcmp(vlItemName->valuestring, input) == 0) {
			if(getVLItemFromRoot(vlItem, item)==1)return 1;
		}
	}

	return 0;
}

/**
 * @brief Retrieve a VLItem by index from a JSON file.
 *
 * This function reads a JSON file containing VLItem data, retrieves the VLItem at the specified index,
 * and populates a VLItem structure with the data.
 *
 * @param item Pointer to the VLItem structure to be populated.
 * @param i    Index of the VLItem to retrieve.
 * @return     Returns 0 on success, 1 if an error occurs.
 */
int getVLItembyNr(VLItem *item,int i){
	cJSON *vlItemRoot = NULL;
	FILE *vlItemFile = NULL;
	if(createFileStream(&vlItemFile, "vlItem.json", "r",0)==1){
		fprintf(stderr,"Error vlItems.json couldn't be opened\n");
		return 1;
	}
	if(getJsonRoot(&vlItemRoot, vlItemFile)==1)return 1;
	closeFileStream(vlItemFile,0);
	cJSON *vlitems = cJSON_GetObjectItem(vlItemRoot, "ItemData");
	if(vlitems == NULL){
		fprintf(stderr,"Error ItemData in vlItems.json not found\n");
		return 1;
	}
	cJSON *vlItem = cJSON_GetArrayItem(vlitems, i);
	//NEED TO TEST
	if(getVLItemFromRoot(vlItem, item)==1){
		fprintf(stderr,"Item %d couldn't be extracted from vlItem.json\n",i);
		return 1;
	}
	return 0;
}


/**
 * @brief Convert BoardItem data to JSON format and append it to an array.
 *
 * This function takes BoardItem data in a byte array format and converts it into JSON format.
 * The resulting JSON object is appended to the specified cJSON array.
 *
 * @param BoardItems    Byte array containing BoardItem data.
 * @param defaultValue  Default value of the BoardItem.
 * @param defSize       Size of the default value.
 * @param objArray      cJSON array to which the JSON object will be appended.
 */
void BoardItemstoJson(char *BoardItems, char *defaultValue, char defSize,
		cJSON *objArray)
{
	int size = 0;
	char name[35];
	char address[3];
	char lenTyp[2];
	char flags[2];
	char symbol[10];
	char scaleFactor[4];
	char unit[6];
	char minVal[4];
	char maxVal[4];

	memcpy(name, BoardItems, 32);
	memcpy(address, BoardItems + 32, 3);
	memcpy(lenTyp, BoardItems + 35, 2);
	memcpy(flags, BoardItems + 37, 2);
	memcpy(symbol, BoardItems + 39, 10);
	memcpy(scaleFactor, BoardItems + 49, 4);
	memcpy(unit, BoardItems + 53, 6);
	memcpy(minVal, BoardItems + 59, 4);
	memcpy(maxVal, BoardItems + 63, 4);

	//NAME
	name[34] = '\0';
	cJSON *json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "name", name);

	//ADDRESS
	size = sizeof(address);
	createJsonArray(json, address, size, "Address");

	//LENTYP
	size = sizeof(lenTyp);
	createJsonArray(json, lenTyp, size, "LenTyp");

	//FLAGS
	size = sizeof(flags);
	createJsonArray(json, flags, size, "Flags");

	//SYMBOL
	size = sizeof(symbol);
	createJsonArray(json, symbol, size, "Symbol");

	//SCALEFACTOR
	size = sizeof(scaleFactor);
	createJsonArray(json, scaleFactor, size, "ScaleFactor");

	//UNIT
	size = sizeof(unit);
	createJsonArray(json, unit, size, "Unit");

	//MINVAL
	size = sizeof(minVal);
	createJsonArray(json, minVal, size, "MinVal");

	//MAXVAL
	size = sizeof(maxVal);
	createJsonArray(json, maxVal, size, "MaxVal");

	createJsonArray(json, defaultValue, defSize, "Default Value");

	cJSON_AddItemToArray(objArray, json);
}

/**
 * @brief Retrieves a BoardItem object by its index from a cJSON root object.
 *
 * This function extracts a BoardItem from a cJSON root object based on the specified index.
 *
 * @param root The root cJSON object containing VLItems array.
 * @param i The index of the desired BoardItem in the VLItems array.
 * @return A pointer to the extracted BoardItem. NULL if extraction fails or index out of bounds.
 */
BoardItem* getBoardItemByNr(cJSON *root, int i){

	cJSON *BoardItemsteam = cJSON_GetObjectItem(root, "VLItems");
	if (!BoardItemsteam) {
		fprintf(stderr, "Error: Could not find 'VLItems' array in JSON\n");
		return NULL;
	}
	BoardItem *returnItem = (BoardItem *)malloc(sizeof(BoardItem));
	cJSON *item = cJSON_GetArrayItem(BoardItemsteam, i);
	if (item != NULL)
	{
		strcpy(returnItem->name, cJSON_GetObjectItem(item, "name")->valuestring);

		cJSON *addressArray = cJSON_GetObjectItem(item, "Address");
		for (int j = 0; j < 3; j++)
		{
			returnItem->Address[j] = cJSON_GetArrayItem(addressArray, j)->valueint & 0xFF;
		}

		cJSON *lenTypArray = cJSON_GetObjectItem(item, "LenTyp");
		for (int j = 0; j < 2; j++)
		{
			returnItem->LenTyp[j] = cJSON_GetArrayItem(lenTypArray, j)->valueint & 0xFF;
		}

		cJSON *flagsArray = cJSON_GetObjectItem(item, "Flags");
		for (int j = 0; j < 2; j++)
		{
			returnItem->Flags[j] = cJSON_GetArrayItem(flagsArray, j)->valueint & 0xFF;
		}

		cJSON *symbolArray = cJSON_GetObjectItem(item, "Symbol");
		for (int j = 0; j < 10; j++)
		{
			returnItem->Symbol[j] = cJSON_GetArrayItem(symbolArray, j)->valueint & 0xFF;
		}

		cJSON *scaleFactorArray = cJSON_GetObjectItem(item, "ScaleFactor");
		for (int j = 0; j < 4; j++)
		{
			returnItem->ScaleFactor[j] = cJSON_GetArrayItem(scaleFactorArray, j)->valueint & 0xFF;
		}

		cJSON *unitArray = cJSON_GetObjectItem(item, "Unit");
		for (int j = 0; j < 6; j++)
		{
			returnItem->Unit[j] = cJSON_GetArrayItem(unitArray, j)->valueint & 0xFF;
		}

		cJSON *minValArray = cJSON_GetObjectItem(item, "MinVal");
		for (int j = 0; j < 4; j++)
		{
			returnItem->MinVal[j] = cJSON_GetArrayItem(minValArray, j)->valueint & 0xFF;
		}

		cJSON *maxValArray = cJSON_GetObjectItem(item, "MaxVal");
		for (int j = 0; j < 4; j++)
		{
			returnItem->MaxVal[j] = cJSON_GetArrayItem(maxValArray, j)->valueint & 0xFF;
		}


		cJSON *defaultValueArray = cJSON_GetObjectItem(item, "Default Value");
		int arrayLength = cJSON_GetArraySize(defaultValueArray);
		for (int j = 0; j < arrayLength; j++)
		{
			returnItem->DefaultValue[j] = cJSON_GetArrayItem(defaultValueArray, j)->valueint & 0xFF;
		}
	}
	return returnItem;
}

/**
 * @brief Retrieves a Match object by its VLItem Name from a cJSON root object.
 *
 * This function retrieves a Match object from a cJSON root object based on the specified VLItem Name.
 *
 * @param root The root cJSON object containing Matches array.
 * @param vLItemName The name of the VLItem to search for.
 * @return A pointer to the extracted Match object. NULL if extraction fails or VLItemName not found.
 */
Match *getMatchByItemName(cJSON *root, char* vLItemName) {
    cJSON *matchesArray = cJSON_GetObjectItem(root, "Matches");
    if (!matchesArray) {
        fprintf(stderr, "Error: Could not find 'Matches' array in JSON\n");
        return NULL;
    }

    cJSON *matchItem;
    int i;

    for (i = 0; i < cJSON_GetArraySize(matchesArray); i++) {
        matchItem = cJSON_GetArrayItem(matchesArray, i);
        cJSON *itemName = cJSON_GetObjectItemCaseSensitive(matchItem, "VLItemName");
        if (itemName && cJSON_IsString(itemName) && strcmp(itemName->valuestring, vLItemName) == 0) {
            break;
        }
    }
    if(i == cJSON_GetArraySize(matchesArray)){
    	return NULL;
    }

    Match *returnMatch = (Match *)malloc(sizeof(Match));
	if (!returnMatch) {
		fprintf(stderr, "Error: Memory allocation failed for Match\n");
		return NULL;
	}
    cJSON *bitItemArray = cJSON_GetObjectItem(matchItem, "BitItems");
	if (bitItemArray != NULL) {
		int bitItemCount = cJSON_GetArraySize(bitItemArray);
		returnMatch->bitItemCount = bitItemCount;
		returnMatch->BitItems = (BitItem *)malloc(bitItemCount*sizeof(BitItem));
		for (int i = 0; i < bitItemCount; i++) {
			cJSON *bitArrayEntry = cJSON_GetArrayItem(bitItemArray, i);
			if (bitArrayEntry != NULL) {
				strcpy(returnMatch->BitItems[i].bitName,cJSON_GetObjectItem(bitArrayEntry, "BitName")->valuestring);
				strcpy(returnMatch->BitItems[i].CIField,cJSON_GetObjectItem(bitArrayEntry, "CI-Field")->valuestring);
				strcpy(returnMatch->BitItems[i].CIKey,cJSON_GetObjectItem(bitArrayEntry, "CI-Key")->valuestring);
				returnMatch->BitItems[i].startBit = cJSON_GetObjectItem(bitArrayEntry, "StartBit")->valueint;
				returnMatch->BitItems[i].size = cJSON_GetObjectItem(bitArrayEntry, "Size")->valueint;
			}
		}
	}else{
		returnMatch->BitItems = NULL;
	}
	strcpy(returnMatch->VLItemName, cJSON_GetObjectItem(matchItem, "VLItemName")->valuestring);
	strcpy(returnMatch->CIField, cJSON_GetObjectItem(matchItem, "CI-Field")->valuestring);
	strcpy(returnMatch->CIKey, cJSON_GetObjectItem(matchItem, "CI-Key")->valuestring);
    return returnMatch;
}

/**
 * @brief Retrieves a CIEntry object by its field and key from a cJSON root object.
 *
 * This function extracts a CIEntry from a cJSON root object based on the specified field and key.
 *
 * @param root The root cJSON object containing the field and key entries.
 * @param field The field name to search for.
 * @param key The key name to search for within the specified field.
 * @return A pointer to the extracted CIEntry. NULL if extraction fails or field/key not found.
 */
CIEntry *getCIEntryByFieldAndKey(cJSON *root, const char *field, const char *key) {
    cJSON *currentEntry = cJSON_GetObjectItemCaseSensitive(root, field);
    if (!currentEntry || !cJSON_IsObject(currentEntry)) {
        //fprintf(stderr, "Error: Could not find '%s' object in JSON\n", field);
        return NULL;
    }

    cJSON *currentKey = cJSON_GetObjectItemCaseSensitive(currentEntry, key);
    if (!currentKey || !cJSON_IsString(currentKey)) {
        fprintf(stderr, "Error: Could not find '%s' key in '%s' object\n", key, field);
        return NULL;
    }

    CIEntry *entry = (CIEntry *)malloc(sizeof(CIEntry));
    if (!entry) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return NULL;
    }

    strncpy(entry->CIField, field, sizeof(entry->CIField) - 1);
    entry->CIField[sizeof(entry->CIField) - 1] = '\0';

    strncpy(entry->CIKey, key, sizeof(entry->CIKey) - 1);
    entry->CIKey[sizeof(entry->CIKey) - 1] = '\0';

    strncpy(entry->Value, currentKey->valuestring, sizeof(entry->Value) - 1);
    entry->Value[sizeof(entry->Value) - 1] = '\0';

    return entry;
}

/**
 * @brief Creates a cJSON object representing a BitItem and adds it to an array.
 *
 * This function creates a cJSON object representing a BitItem structure and adds it to the specified cJSON array.
 *
 * @param array Pointer to the cJSON array where the cJSON object will be added.
 * @param bitItem Pointer to the BitItem structure containing the data to be added to the cJSON object.
 * @return Returns 0 on success, indicating the cJSON object was successfully created and added to the array.
 */
int createBitItemJsonObject(cJSON *array,BitItem *bitItem){
	cJSON *bitItemJSON = cJSON_CreateObject();

	cJSON_AddStringToObject(bitItemJSON, "BitName", bitItem->bitName);
	cJSON_AddStringToObject(bitItemJSON, "CI-Field", bitItem->CIField);
	cJSON_AddStringToObject(bitItemJSON, "CI-Key", bitItem->CIKey);
	cJSON_AddNumberToObject(bitItemJSON, "StartBit", bitItem->startBit);
	cJSON_AddNumberToObject(bitItemJSON, "Size", bitItem->size);

	if(bitItem->value != -1){
		cJSON_AddNumberToObject(bitItemJSON, "Value", bitItem->value);
	}else{
		cJSON_AddNullToObject(bitItemJSON, "Value");
	}
	fflush(stdout);
	cJSON_AddItemToArray(array, bitItemJSON);
	return 0;
}

/**
 * @brief Writes item data to a JSON file.
 *
 * This function creates a cJSON object representing the item data, including board item details, match details, and entry details (if available),
 * and writes it to a JSON file.
 *
 * @param boardItem Pointer to the BoardItem structure containing the board item details.
 * @param match Pointer to the Match structure containing the match details. Can be NULL if no match is available.
 * @param entry Pointer to the CIEntry structure containing the entry details. Can be NULL if no entry is available.
 * @param file Pointer to the FILE structure representing the JSON file to which the item data will be written.
 * @param array Pointer to the cJSON array where the cJSON object representing the item data will be added.
 */
void writeItemData(BoardItem *boardItem ,Match *match, CIEntry *entry,FILE *file, cJSON *array){
	cJSON* jsonObject = cJSON_CreateObject();
	cJSON_AddStringToObject(jsonObject, "name", boardItem->name);
	if(match!=NULL){
		cJSON_AddStringToObject(jsonObject, "CIField", match->CIField);
		cJSON_AddStringToObject(jsonObject, "CIKey", match->CIKey);
	}else{
		cJSON_AddStringToObject(jsonObject, "CIField", "");
		cJSON_AddStringToObject(jsonObject, "CIKey", "");
	}

	int size;

	size = sizeof(boardItem->Address);
	createJsonArray(jsonObject, boardItem->Address, size, "Address");

	size = sizeof(boardItem->LenTyp);
	createJsonArray(jsonObject, boardItem->LenTyp, size, "LenTyp");

	size = sizeof(boardItem->Flags);
	createJsonArray(jsonObject, boardItem->Flags, size, "Flags");

	size = sizeof(boardItem->Symbol);
	createJsonArray(jsonObject, boardItem->Symbol, size, "Symbol");

	size = sizeof(boardItem->ScaleFactor);
	createJsonArray(jsonObject, boardItem->ScaleFactor, size, "ScaleFactor");

	size = sizeof(boardItem->Unit);
	createJsonArray(jsonObject, boardItem->Unit, size, "Unit");

	size = sizeof(boardItem->MinVal);
	createJsonArray(jsonObject, boardItem->MinVal, size, "MinVal");

	size = sizeof(boardItem->MaxVal);
	createJsonArray(jsonObject, boardItem->MaxVal, size, "MaxVal");

	cJSON* bitItemArrayJSON = cJSON_CreateArray();
	if(match!=NULL){
		size = (match->bitItemCount);
		if(match->BitItems!=NULL){
			for(int i = 0; i < match->bitItemCount; i++){
					createBitItemJsonObject(bitItemArrayJSON,&(match->BitItems[i]));
			}
		}
	}
	cJSON_AddItemToObject(jsonObject, "BitItems", bitItemArrayJSON);


	size = sizeof(boardItem->DefaultValue);
	createJsonArray(jsonObject, boardItem->DefaultValue, size, "DefaultValue");

	if(entry!=NULL){
		const char *val = entry->Value;
		char *endptr;
		char *endptr2;
		int intResult = strtol(val, &endptr, 10);
		float floatResult = strtof(val, &endptr2);
		if (strcmp(val, "True") == 0) {
			cJSON_AddNumberToObject(jsonObject, "Value", 1);
		} else if (strcmp(val, "False") == 0) {
			cJSON_AddNumberToObject(jsonObject, "Value", 0);
		} else if (*endptr2 == '\0'){
				cJSON_AddNumberToObject(jsonObject, "Value", floatResult);
		} else if (*endptr == '\0'){
			cJSON_AddNumberToObject(jsonObject, "Value", intResult);
		} else{
			cJSON_AddNullToObject(jsonObject, "Value");
		}
	}else{
		cJSON_AddNullToObject(jsonObject, "Value");
	}

	char* jsonString = cJSON_Print(jsonObject);
	if (!jsonString) {
		cJSON_Delete(jsonObject);
		error_handler("Error: cJSON_Print failed");
	}
	cJSON_AddItemToArray(array, jsonObject);
	free(jsonString);
	return;
}

/**
 * @brief Creates a JSON file containing item data.
 *
 * This function reads board item details, match details, and CI entry details from separate JSON files,
 * creates a cJSON object representing the item data for each item, and writes the cJSON objects to a JSON file.
 *
 * @param itemcount The number of items for which data will be created.
 * @return Returns 0 on success, 1 on failure.
 */
int createDataJson(int itemcount){

	cJSON *boardItemRoot = NULL;
	FILE *boardItemFile = NULL;

	cJSON *matchRoot = NULL;
	FILE *matchFile = NULL;

	cJSON *CIEntryRoot = NULL;
	FILE *CIEntryFile = NULL;

	cJSON *itemArray = cJSON_CreateArray();
	cJSON *itemOutput = cJSON_CreateObject();
	FILE *itemFile = NULL;

	createFileStream(&boardItemFile, "boardItems.json", "r",0);
	if(getJsonRoot(&boardItemRoot, boardItemFile)==1){
		fprintf(stderr,"Error finding JSON root in boarditems.json\n");
		return 1;
	}
	closeFileStream(boardItemFile,0);

	createFileStream(&matchFile, "match.json", "r",0);
	if(getJsonRoot(&matchRoot, matchFile)==1){
		fprintf(stderr,"Error finding JSON root in match.json\n");
		return 1;
	}
	closeFileStream(matchFile,0);

	createFileStream(&CIEntryFile, "CI-Servo.json", "r",0);
	if(getJsonRoot(&CIEntryRoot, CIEntryFile)==1){
		fprintf(stderr,"Error finding JSON root in CI-Servo.json\n");
		return 1;
	}
	closeFileStream(CIEntryFile,0);

	if(createFileStream(&itemFile, "vlItem.json", "w",0)==1){
		fprintf(stderr,"Error vlItems.json couldn't be opened\n");
		return 1;
	}

	BoardItem *boardItem = NULL;
	Match *match = NULL;
	CIEntry *cientry = NULL;
	VLItem *itemdata = NULL;

	itemdata = (VLItem *)malloc(sizeof(VLItem));

	if(itemdata == NULL){
		fprintf(stderr,"Memory allocation for itemdata failed\n");
		return 1;
	}

	for(int i = 0;i<itemcount;i++){
		boardItem = getBoardItemByNr(boardItemRoot,i);
		if(boardItem == NULL){
			fprintf(stderr,"Board Item with number %d not found\n",i);
			return 1;
		}
		match = getMatchByItemName(matchRoot,boardItem->name);
		if(match != NULL){
			if(match->BitItems != NULL){
				for(int j = 0; j< match->bitItemCount;j++){
					cientry = NULL;
					cientry = getCIEntryByFieldAndKey(CIEntryRoot, match->BitItems[j].CIField, match->BitItems[j].CIKey);
					if(cientry != NULL){
						const char *val = cientry->Value;
						char *endptr;
						char *endptr2;
						int intResult = strtol(val, &endptr, 10);
						float floatResult = strtof(val, &endptr2);
						if (strcmp(val, "True") == 0) {
							match->BitItems[j].value =0;
						} else if (strcmp(val, "False") == 0) {
							match->BitItems[j].value =1;
						} else if (*endptr == '\0'){
							match->BitItems[j].value = intResult;
						} else if (*endptr2 == '\0'){
							match->BitItems[j].value = floatResult;
						} else{
							match->BitItems[j].value = -1;
						}

					}else{
						match->BitItems[j].value = -1;
					}
				}
			}
			cientry = getCIEntryByFieldAndKey(CIEntryRoot, match->CIField, match->CIKey);
		}
		writeItemData(boardItem, match, cientry, itemFile, itemArray);
		fflush(stdout);
		free(boardItem);
		if(match != NULL){
			free(match->BitItems);
			free(match);
		}
	}
	cJSON_AddItemToObject(itemOutput, "ItemData", itemArray);
	char *itemString = cJSON_Print(itemOutput);
	fprintf(itemFile, "%s\n", itemString);
	closeFileStream(itemFile,0);
	cJSON_Delete(itemArray);
	cJSON_Delete(boardItemRoot);
	cJSON_Delete(matchRoot);
	cJSON_Delete(CIEntryRoot);

	return 0;
}


