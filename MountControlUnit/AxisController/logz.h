/*
 * logz.h
 *
 *  Created on: 19.01.2024
 *      Author: morit
 */

#ifndef LOGZ_H_
#define LOGZ_H_

#define DEFAULT_LOG_PATH ".\\logs"
#define DEFAULT_LOG_FILENAME "log"

void initLogger(char *fileName);
void logz(char *logMessage);
void closeLogger();

#endif /* LOGZ_H_ */
