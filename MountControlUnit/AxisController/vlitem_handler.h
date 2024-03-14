/*
 * vlitem_handler.h
 *
 *  Created on: 04.12.2023
 *      Author: Moritz Zideck
 */

#ifndef VLITEM_HANDLER_H_
#define VLITEM_HANDLER_H_

#include "json_utils.h"
#define MAXSIZE (256)

int addVlItemToArray(VLItem item,VLItem array[]);
VLItem* getVlItemFromArray(char *item_name, VLItem array[]);
int getBitItemFromVlItem(VLItem item, const char *bitItemName, BitItem **bitItem);

#endif /* SOCKET_UTILS_H_ */
