/*
 * json_utils.h
 *
 *  Created on: 03.11.2023
 *      Author: morit
 */

#ifndef JSON_UTILS_H_
#define JSON_UTILS_H_
#include "cJSON.h"
#include <stdio.h>
typedef struct {
    char name[35];
    char Address[3];
    char LenTyp[2];
    char Flags[2];
    char Symbol[10];
    char ScaleFactor[4];
    char Unit[6];
    char MinVal[4];
    char MaxVal[4];
    char DefaultValue[4];
} BoardItem;

typedef struct
{
	char bitName[35];
	char CIField[256];
	char CIKey[256];
	int  startBit;
	int  size;
	char defaultValue[4];
	int value;
} BitItem;

typedef struct
{
	char VLItemName[35];
	BitItem *BitItems;
	int bitItemCount;
	char CIField[256];
	char CIKey[256];

} Match;

typedef struct
{
	char CIField[256];
	char CIKey[256];
	char Value[16];
} CIEntry;



typedef struct
{
	char name[35];
	char CIField[256];
	char CIKey[256];
	char Address[3];
	char LenTyp[2];
	char Flags[2];
	char Symbol[10];
	char ScaleFactor[4];
	char Unit[6];
	char MinVal[4];
	char MaxVal[4];
	BitItem *BitItems;
	int BitItemCount;
	char DefaultValue[4];
	double Value;
} VLItem;

typedef struct
{
	VLItem *items;
	size_t size;
} VlItemMemory;


void BoardItemstoJson(char *VLItems, char *defaultValue, char defSize,
		cJSON *objArray);

BoardItem* findBoardItemByName(cJSON *root, char *name);
BoardItem* getBoardItemFromJson(char *itemName);

int getVLItemFromJson(VLItem *itemdata,char *input);
int getVLItembyNr(VLItem *item,int i);

void createJsonArray(cJSON *json, char *data, int size, char *name);

int iniToJson();

int createJson(cJSON *objArray);

int setvlistJsonFile();

void error_handler(char* error);
int createDataJson(int itemcount);



#endif /* JSON_UTILS_H_ */


