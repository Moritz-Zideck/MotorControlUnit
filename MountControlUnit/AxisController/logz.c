/*
 * logz.c
 *
 *  Created on: 19.01.2024
 *      Author: morit
 */

#include "logz.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <stddef.h>
#include <windows.h>

static FILE *logFile;
static int initDone = 0;
char fileName[100];

static char* get_datetime() {
    time_t t = time(NULL);
    struct tm *tm  = localtime(&t);
    const char *d_time = asctime(tm);
    const char *datetime = strchr(d_time, '\n');
    int length = datetime - d_time;

    char *date = malloc(length + 1);
    strncpy(date, d_time, length);
    date[length] = '\0';
    return date;
}

void getDateTimeString(char *buffer) {
    time_t now;
    struct tm *tm_info;

    time(&now);
    tm_info = localtime(&now);

    // Format: YYYYMMDD_HHMMSS
    strftime(buffer, 20, "%Y%m%d_%H%M%S_", tm_info);
}

static void convertWindowsPathToPOSIX(char *path) {
    if (path == NULL) return;

    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
}

void initLogger(char *fileNm){
	char path[100];
	strcpy(path,DEFAULT_LOG_PATH);
	convertWindowsPathToPOSIX(path);
	strcpy(fileName,path);
	strcat(fileName,"/");
	char buffer[20];
	getDateTimeString(buffer);
	strcat(fileName,buffer);
	if(fileNm != NULL){
		strcat(fileName,fileNm);
	}else{
		strcat(fileName,DEFAULT_LOG_FILENAME);
	}
	if (GetFileAttributes(DEFAULT_LOG_PATH) == INVALID_FILE_ATTRIBUTES) {
		CreateDirectory(DEFAULT_LOG_PATH, NULL);
	}
	logFile = fopen(fileName,"w");
	if(logFile == NULL){
		fprintf(stderr,"LOGFILE couldn't be created");
		return;
	}
	initDone = 1;
}

void logz(char *message){
	if(initDone == 0){
		fprintf(stderr,"Inizialise Logger first. (initLogger(char *logFileName)");
		return;
	}
	char *date = get_datetime();
	fprintf(logFile,"%s, %s\n",date,message);
	fflush(logFile);
	free(date);
}

void closeLogger(){
	if(initDone == 1){
		fclose(logFile);
	}
}

