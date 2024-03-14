/*
 * thread_utils.h
 *
 *  Created on: 16.02.2024
 *      Author: morit
 */

#ifndef THREAD_UTILS_H_
#define THREAD_UTILS_H


#include <semaphore.h> // Include the header file for sem_t

extern __thread int axleNum; // Thread specific variable defining the axle
extern __thread char axleIPAdress[16];
extern __thread int axlePort;
extern __thread SOCKET clientSocket;


sem_t sharedDir; //global semaphore for managing shared directory
sem_t schedule_sem_1;
sem_t schedule_sem_2;
sem_t axle_1_ready;
sem_t axle_2_ready;
int write_task_identify;
int read_task_identify;

#endif /* THREAD_UTILS_H_ */
