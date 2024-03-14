/*
 * common_utils.c
 *
 *  Created on: 26.01.2024
 *      Author: morit
 */

#include <stdio.h>
#include <conio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>
#include "common_utils.h"

void error_handler(char* error){
	fprintf(stderr,"Error: \"%s\" occurred\n",error);
	fflush(stderr);
	exit(EXIT_FAILURE);
}

int charArrayToUint32(const char* charArray, size_t size, uint32_t* result) {
    if (size > 4) {
        return 1; // Error: Input size exceeds 4 bytes
    }
    *result = 0;
    // Copy up to 4 bytes into the result
    for (size_t i = 0; i < size; i++) {
        *result |= (uint32_t)(charArray[i] & 0xFF) << (i * 8);
    }
    return 0; // Success
}

int uint32ToCharArray(uint32_t value, char* charArray, size_t size) {
    if (size > 4) {
        return 1; // Error: Output size exceeds 4 bytes
    }

    for (size_t i = 0; i < size; ++i) {
        charArray[i] = (char)((value >> (i * 8)) & 0xFF);
    }

    return 0; // Success
}

int byteArrayToInt(const char* byteArray, size_t size, int* result) {
    if (size > 4) {
        return 1; // Error: Input size exceeds 4 bytes
    }
    *result = 0;
    // Copy up to 4 bytes into the result
    for (size_t i = 0; i < size; i++) {
        *result |= (int)(byteArray[i] & 0xFF) << (i * 8);
    }
    return 0; // Success
}

int byteArrayToInt32(const char* byteArray, size_t size, int32_t* result) {
    if (size > 4) {
        return 1; // Error: Input size exceeds 4 bytes
    }
    *result = 0;
    // Copy up to 4 bytes into the result
    for (size_t i = 0; i < size; i++) {
        *result |= (int32_t)(byteArray[i] & 0xFF) << (i * 8);
    }
    return 0; // Success
}

int byteArrayToFloat(const char* byteArray, float* result) {
    if (byteArray == NULL || result == NULL) {
        return 1; // Error: Null pointer
    }

    // Use memcpy to copy the bits from the byte array into the float
    memcpy(result, byteArray, sizeof(float));

    return 0; // Success
}

void doubleToCharArray(double value, char* charArray,size_t size) {
	union {
		double d;
		uint32_t u[2];
	} temp;

	temp.d = value;
	uint32_t tempi = temp.u[0];
	uint32ToCharArray(tempi, charArray, size);
}

void sleep_us(int microseconds) {
	// Use busy-wait loop for microsecond precision
	LARGE_INTEGER frequency, start, current;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);

	do {
		QueryPerformanceCounter(&current);
	} while ((current.QuadPart - start.QuadPart) * 1000000 / frequency.QuadPart < microseconds);
}


void printBitsInt(int16_t num) {
    int i;
    for (i = 15; i >= 0; i--) {
        uint32_t mask = 1u << i;
        uint32_t bit = (num & mask) >> i;
        printf("%u", bit);

        if (i % 8 == 0) {
            printf(" "); // Add a space every 8 bits for better readability
        }
    }
    printf("\n");
}

void printBits(uint32_t num) {
    int i;
    for (i = 31; i >= 0; i--) {
        uint32_t mask = 1u << i;
        uint32_t bit = (num & mask) >> i;
        printf("%u", bit);

        if (i % 8 == 0) {
            printf(" "); // Add a space every 8 bits for better readability
        }
    }
    printf("\n");
}


