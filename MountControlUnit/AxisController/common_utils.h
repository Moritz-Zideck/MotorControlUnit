/*
 * common_utils.h
 *
 *  Created on: 26.01.2024
 *      Author: morit
 */

#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <stdio.h>
#include <conio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <windows.h>
#include "thread_utils.h"

// Define PATH_SEPARATOR based on the operating system
#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

// Error handling function
void error_handler(char* error);

//creates fp to the specified file
int createFileStream(FILE **filePointer, char *fileName, char *privileges, int shared);

//closes fp to the specified file
int closeFileStream(FILE *filePointer,int shared);

// Convert a char array to a uint32_t
int charArrayToUint32(const char* charArray, size_t size, uint32_t* result);

// Convert a uint32_t to a char array
int uint32ToCharArray(uint32_t value, char* charArray, size_t size);

// Convert a byte array to an int
int byteArrayToInt(const char* byteArray, size_t size, int* result);

// Convert a byte array to an int32_t
int byteArrayToInt32(const char* byteArray, size_t size, int32_t* result);

// Convert a byte array to a float
int byteArrayToFloat(const char* byteArray, float* result);

// Convert a double to a char array
void doubleToCharArray(double value, char* charArray, size_t size);

// Sleep for a specified number of microseconds
void sleep_us(int microseconds);

// Print the bits of an int16_t
void printBitsInt(int16_t num);

// Print the bits of a uint32_t
void printBits(uint32_t num);

#endif // COMMON_UTILS_H
