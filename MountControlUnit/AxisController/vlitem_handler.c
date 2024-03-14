#include "vlitem_handler.h"
#include <stdio.h>
#include <string.h>

// Function to add a VLItem to the array
int addVlItemToArray(VLItem item, VLItem array[]) {
    // Check if the item is already in the array
    for (int i = 0; i < MAXSIZE; i++) {
        if (strcmp(array[i].name, item.name) == 0) {
            // Item already exists in the array, return an error code
            return -2; // Duplicate item
        }
    }

    // If the item is not in the array, find an empty slot to add it
    for (int i = 0; i < MAXSIZE; i++) {
        if (array[i].name[0] == '\0') {
            // Found an empty slot in the array, add the item here
        	memcpy(&(array[i]),&item,sizeof(VLItem));
            return 0; // Success
        }
    }

    // If the array is full, return an error code (you may want to handle this case differently)
    return -1; // Array is full
}

// Function to get a VLItem by name from the array
VLItem* getVlItemFromArray(char *item_name, VLItem array[]) {
    for (int i = 0; i < MAXSIZE; i++) {
        if (strcmp(array[i].name, item_name) == 0) {
            // Found the item with the matching name, return a pointer to it
            return &array[i];
        }

    }
    // If the item is not found, return NULL (you may want to handle this case differently)
    return NULL;
}

int getBitItemFromVlItem(VLItem item, const char *bitItemName, BitItem **bitItem) {
    for (int i = 0; i < item.BitItemCount; i++) {
        if (strcmp(item.BitItems[i].bitName, bitItemName) == 0) {
            *bitItem = &(item.BitItems[i]);
            return 0;
        }
    }

    fprintf(stderr, "BitItem: %s not found in VLItem %s", bitItemName, item.name);
    return 1;
}

