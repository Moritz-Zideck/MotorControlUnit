//MORITZ ZIDECK

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <unistd.h>
#include <string.h>
#include "cJSON.h"
#include <sys/time.h>
#include <windows.h>
#include "math.h"

#include "json_utils.h"
#include "socket_utils.h"
#include "vlitem_handler.h"
#include "logz.h"
#include "common_utils.h"
#include "axis_controller.h"


//thread global variable
__thread int axleNum;
__thread SOCKET clientSocket;
__thread char axleIPAdress[16];
__thread int axlePort;

int kbhit() {
    if ((GetAsyncKeyState('q') & 0x8001) || (GetAsyncKeyState('Q') & 0x8001)) {
        return 1;
    }
    return 0;
}

int visualiseGraph(char *fileName) {
    FILE *gnupipe = NULL;
    char *GnuCommands = "load 'sysidplot.gp'";

    gnupipe = _popen("gnuplot -persistent", "w");  // Fix the typo in "persistent"
    if (gnupipe == NULL) {
        return 1;
    }

	fprintf(gnupipe, "%s\n", GnuCommands);
    _pclose(gnupipe);  // Close the gnupipe
    sleep(20);
    return 0;
}

int readPosFromBoard(SOCKET socket,char* item, double* angle){
	int32_t readValue;
	float fval;
	VLItem vlitem;
	if(getVLItem(&vlitem,item)==1){
		return 1;
	}
	if(vlitem.LenTyp[0]==10){
		if(readFromBoardInt32(socket, item, &readValue)==1){
			fprintf(stderr,"Reading %s from Board failed",item);
			fflush(stderr);
			return 1;
		}
	}else if(vlitem.LenTyp[0]==12){
		if(readFromBoardFloat(socket, item, &fval)==1){
			fprintf(stderr,"Reading %s from Board failed",item);
			fflush(stderr);
			return 1;
		}
		readValue = (float) fval;
	}
	int32_t calcValue;
	calcValue = readValue & 0x3FFFFFFF;
	double incToDec = 360.0 / pow(2, 30);

	*angle = (double)calcValue * incToDec;
	*angle = *angle - 215.9;
	if(readValue >> 30==0)*angle += 360;
	if(*angle < 0)*angle = 0;
	if(*angle > 348)*angle = 348;
	return 0;
}

void startMotor(SOCKET *clientSocket){

	writeToBoard(*clientSocket,"state_2.motionmode",0);
	writeToBoard(*clientSocket,"state_1.motionmode",0);
	writeToBoard(*clientSocket,"state_1.run",0);
	writeToBoard(*clientSocket,"state_2.run",0);
	printf("Start\n");
//	clearBoardBitItems(*clientSocket, "errorAction_1");
//	clearBoardBitItems(*clientSocket, "errorAction_2");
//	sleep_us(800000);
	writeToBoard(*clientSocket,"peripherial.aux",1);
	writeToBoard(*clientSocket,"state_2.motionmode",2);
	writeToBoard(*clientSocket,"state_1.motionmode",2);
	writeToBoard(*clientSocket,"state_2.motionmode",2);
	writeToBoard(*clientSocket,"state_1.motionmode",2);
	//sleep_us(200000);
	writeToBoard(*clientSocket,"state_2.run",1);
	writeToBoard(*clientSocket,"state_2.run",1);

}

uint32_t toggleTrajectorySYSID(SOCKET *clientSocket, float vel, double pos_start, double pos_end, uint32_t busyFlag, uint32_t doneFlag){
	int32_t pos2;
	float velpos= vel;
	float velneg= -vel;
	int32_t calcValue;
	double angle;
	double incToDec;
	int print_ctr = 0;
	int send_ctr_end = 0;
	int send_ctr_start = 0;
	int checkFlag_timer = 0;

	// ----- limit values in degrees | change value for toggle path here -----
//	pos_start = 290;
//	pos_end = 180;
	double limit_start = 270;
	double limit_end = 260;
	// -----------------------------------------------------------------------

	writeToBoardFloat(*clientSocket,"vel_targ_2", vel);

	while(1){

		if (kbhit()){
			writeToBoardFloat(*clientSocket,"vel_targ_2",0);
			writeToBoard(*clientSocket,"state_2.run",0);
			writeToBoard(*clientSocket, "sysid_control.resetBit", 1);
			writeToBoard(*clientSocket, "sysid_control.resetBit", 1);
			//break;
			return 0;
		}

		readFromBoardInt32(*clientSocket, "pos_2", &pos2);
		sleep_us(10000);
		calcValue = pos2 & 0x3FFFFFFF;
		incToDec = 360.0 / pow(2, 30);
		angle = (double)calcValue * incToDec;

		if(print_ctr > 33){
			printf("%.1f\n",angle);
			print_ctr = 0;
		}

		if(angle >= pos_end && angle <= limit_end){
			if(send_ctr_start < 6){
				printf("Send negative vel_targ\n");
				writeToBoardFloat(*clientSocket,"vel_targ_2",velneg);
				send_ctr_start++;
				send_ctr_end=0;
			}
		}
		if(angle <= pos_start && angle >= limit_start){
			if(send_ctr_end < 6){
				printf("Send positive vel_targ\n");
				writeToBoardFloat(*clientSocket,"vel_targ_2",velpos);
				send_ctr_end++;
				send_ctr_start=0;
			}
		}

		if(checkFlag_timer > 100){
			readFromBoardBitItem(*clientSocket,"sysid_status.doneFlag",&doneFlag);
			readFromBoardBitItem(*clientSocket,"sysid_status.busyFlag",&busyFlag);
			printf("DoneFlag: %d\n",doneFlag);
			printf("BusyFlag: %d\n",busyFlag);
			fflush(stdout);
			if(doneFlag==1){
				return 1;
			}
			checkFlag_timer=0;
		}
		print_ctr++;
		checkFlag_timer++;
	}
}


int sysIdentification(SOCKET socket,int values, char *mode, char*fileName){
	/*if(strcmp(mode,"Current")!= 0 || strcmp(mode,"Velocity")!= 0 || strcmp(mode,"Position")!= 0 ){
		fprintf(stderr,"Wrong input type. Only Current, Velocity, Position");
		return 1;
	}*/
	writeToBoard(socket, "sysid_control.resetBit", 1);
	sleep(1);
	float min_freq = 10;
	float max_freq = 3000;
	float excitation_amp = 0;//0.25;//1;
	uint32_t index = 1;
	uint32_t length = 256;// 511 = max. length
	uint32_t busyFlag;
	float vel_factor = 0.002777;
	float vel_in_degree_per_second = 2;
	float vel = vel_in_degree_per_second * vel_factor;
	double pos_start = 350;
	double pos_end = 20;
	writeToBoard(socket, "sysid_length", length);
	writeToBoardFloat(socket, "sysid_min_fre", min_freq);
	writeToBoardFloat(socket, "sysid_max_fre", max_freq);
	writeToBoardFloat(socket, "sysid_excitat", excitation_amp);

	startMotor(&socket);

//	char *items[] = {"current", "startBit"};
//	int val[] = {1, 1};
	uint32_t data = 5;//3;//5;//9;
	if(data == (uint32_t)5){
		writeToBoardFloat(socket,"vel_targ_2", vel);
	}
	char datac [2];
	uint32ToCharArray(data, datac, 2);
	VLItem item;
	getVLItem(&item, "sysid_control");
	writeRamF(socket, datac, 2, &item);
	//writeToBoardBitItems(socket, "sysid_control", items, val, 2);
	fflush(stderr);
	printf("Startup\n");
	fflush(stdout);
	//MAYBE WAIT FOR RECIEVE
	float amplitude;
	float phase;
	float frequency;
	//
	uint32_t doneFlag;
	if(data == ((uint32_t)5)){
		if(toggleTrajectorySYSID(&socket, vel, pos_start, pos_end, busyFlag, doneFlag) == 0){
			return -1;
		}
	}
	else{
		while(1){
			readFromBoardBitItem(socket,"sysid_status.doneFlag",&doneFlag);
			readFromBoardBitItem(socket,"sysid_status.busyFlag",&busyFlag);
			printf("DoneFlag: %d\n",doneFlag);
			printf("BusyFlag: %d\n\n",busyFlag);
			fflush(stdout);
			sleep(1);
			if(doneFlag==1)break;

			if (kbhit()){
				writeToBoardFloat(socket,"vel_targ_2",0);
				writeToBoard(socket,"state_2.run",0);
				writeToBoard(socket, "sysid_control.resetBit", 1);
				writeToBoard(socket, "sysid_control.resetBit", 1);
				return 0;
			}
		}
	}

	printf("Messung abgeschlossen\n");
	fflush(stdout);
	FILE *fd = fopen(fileName, "w");
	if (fd == NULL) {
		return 1;
	}
	while(index <= length){
		writeToBoard(socket,"sysid_index",index);
		while(1){
			uint32_t indexToRead;
			readFromBoardBitItem(socket, "sysid_status.indexToRead", &indexToRead);
			if(indexToRead == index)break;
			usleep(40000);
		}

		readFromBoardFloat(socket, "sysid_amp",&(amplitude));
		readFromBoardFloat(socket, "sysid_phase",&(phase));
		readFromBoardFloat(socket, "sysid_freq",&(frequency));
		printf("Index %d read\n",index);
		fflush(stdout);
		fprintf(fd, "%f %f %f\n", log10(amplitude), phase*(180/3.1415), frequency);
		index+=1;
	}
	writeToBoard(socket, "sysid_control.resetBit", 1);
	writeToBoard(socket, "sysid_control.resetBit", 1);
	fclose(fd);
	visualiseGraph(fileName);
	return 0;
}

int startMotorSelf(SOCKET *clientSocket){

	int32_t pos2;
	float velval = 0.00277*2;
	int32_t calcValue;
	double angle;
	double incToDec;
	int print_ctr = 0;
	int send_ctr_end = 0;
	int send_ctr_start = 0;

	// --- pos/limit values in degrees | change value for toggle path here ---
	double pos_start = 350;
	double pos_end = 80;
	double limit_start = 270;
	double limit_end = 260;
	// -----------------------------------------------------------------------

	writeToBoard(*clientSocket,"state_2.motionmode",0);
	writeToBoard(*clientSocket,"state_1.motionmode",0);
	writeToBoard(*clientSocket,"state_1.run",0);
	writeToBoard(*clientSocket,"state_2.run",0);
	printf("Start\n");
//	clearBoardBitItems(*clientSocket, "errorAction_1");
//	clearBoardBitItems(*clientSocket, "errorAction_2");
//	sleep_us(800000);
	writeToBoard(*clientSocket,"peripherial.aux",1);
	writeToBoard(*clientSocket,"state_2.motionmode",2);
	writeToBoard(*clientSocket,"state_1.motionmode",2);
	writeToBoard(*clientSocket,"state_2.motionmode",2);
	writeToBoard(*clientSocket,"state_1.motionmode",2);
	//sleep_us(200000);
	writeToBoard(*clientSocket,"state_2.run",1);
	writeToBoard(*clientSocket,"state_2.run",1);
	//sleep_us(100000);
	writeToBoardFloat(*clientSocket,"vel_targ_2", velval);
	uint32_t num;


	while(1){

		if (kbhit()){
			writeToBoardFloat(*clientSocket,"vel_targ_2",0);
			writeToBoard(*clientSocket,"state_2.run",0);
			break;
		}

		readFromBoardInt32(*clientSocket, "pos_2", &pos2);
		sleep_us(10000);
		calcValue = pos2 & 0x3FFFFFFF;
		incToDec = 360.0 / pow(2, 30);
		angle = (double)calcValue * incToDec;

		if(print_ctr > 30){
			readFromBoardBitItem(*clientSocket,"state_2.run", &num);
			printf("RUNBIT IS :%d\n",num);
			printf("%.1f\n",angle);
			fflush(stdout);
			print_ctr = 0;
		}

		if(angle >= pos_end && angle <= limit_end){
			if(send_ctr_start < 6){
				printf("Send negative vel_targ\n");
				velval = -0.00277*2;
				writeToBoardFloat(*clientSocket,"vel_targ_2",velval);
				send_ctr_start++;
				send_ctr_end=0;
			}
		}
		if(angle <= pos_start && angle >= limit_start){
			if(send_ctr_end < 6){
				printf("Send positive vel_targ\n");
				velval = 0.00277*2;
				writeToBoardFloat(*clientSocket,"vel_targ_2",velval);
				send_ctr_end++;
				send_ctr_start=0;
			}
		}
		print_ctr++;

		//sleep_us(10000);
	}
	return 0;
}


int initBoard(SOCKET *clientSocket){
	writeToBoardFloat(*clientSocket, "cur_lim", 7);
	writeToBoardFloat(*clientSocket, "curpeak_lim", 12);
	writeToBoardFloat(*clientSocket, "curpeak_time", 2);
	writeToBoardFloat(*clientSocket, "curphase_lim", 13);
	writeToBoardFloat(*clientSocket, "temp_err", 80);

	clearBoardBitItems(*clientSocket, "state_1");
	writeToBoard(*clientSocket, "polnr_1", 11);
	writeToBoard(*clientSocket, "enc_1" , 15744); //WHATS THIS ?
	writeToBoard(*clientSocket, "angleconfig_1.el_dir", 1);
	writeToBoard(*clientSocket, "state_1.angleconfig", 1);
	writeToBoardFloat(*clientSocket, "kp_cur_1", 0.4);
	writeToBoardFloat(*clientSocket, "ki_cur_1", 15);
	writeToBoardFloat(*clientSocket, "kp_vel_1", 900);
	writeToBoardFloat(*clientSocket, "kp_pos_1", 20);
	writeToBoardFloat(*clientSocket, "ki_vel_1", 50000);
	writeToBoardFloat(*clientSocket, "Dz_filt_1", 0 );
	writeToBoardFloat(*clientSocket, "Wz_filt_1", 3553058);
	writeToBoardFloat(*clientSocket, "Tz_filt_1", 0);
	writeToBoardFloat(*clientSocket, "Dp_filt_1", 7540);
	writeToBoardFloat(*clientSocket, "Wp_filt_1", 3553058);
	writeToBoardFloat(*clientSocket, "Tp_filt_1", 1);
	writeToBoardFloat(*clientSocket, "K_filt_1", 1);
	writeToBoardFloat(*clientSocket, "f_velmeas_1", 1500);
	writeToBoard(*clientSocket, "state_1.motionmode", 2);
	writeToBoard(*clientSocket, "errorAction_1.all", 1);

	clearBoardBitItems(*clientSocket, "state_2");
	writeToBoard(*clientSocket, "polnr_2", 11);
	writeToBoard(*clientSocket, "enc_2", 15744); //WHATS THIS ?
	writeToBoard(*clientSocket, "angleconfig_2.el_dir", 1);
	writeToBoard(*clientSocket, "state_2.angleconfig", 1);
	writeToBoardFloat(*clientSocket, "kp_cur_2", 0.4);
	writeToBoardFloat(*clientSocket, "ki_cur_2", 15);
	writeToBoardFloat(*clientSocket, "kp_vel_2", 900);
	writeToBoardFloat(*clientSocket, "kp_pos_2", 20);
	writeToBoardFloat(*clientSocket, "ki_vel_2", 50000);
	writeToBoardFloat(*clientSocket, "Dz_filt_2", 0);
	writeToBoardFloat(*clientSocket, "Wz_filt_2", 1);
	writeToBoardFloat(*clientSocket, "Tz_filt_2", 0);
	writeToBoardFloat(*clientSocket, "Dp_filt_2", 0);
	writeToBoardFloat(*clientSocket, "Wp_filt_2", 1);
	writeToBoardFloat(*clientSocket, "Tp_filt_2", 0);
	writeToBoardFloat(*clientSocket, "K_filt_2", 1);
	writeToBoardFloat(*clientSocket, "f_velmeas_2", 1500);
	writeToBoard(*clientSocket, "state_2.motionmode", 2);
	writeToBoard(*clientSocket, "errorAction_2.all", 1);

	writeToBoard(*clientSocket, "pos_err_1", 179132);
	writeToBoardFloat(*clientSocket, "cur_err_1", 13);

	writeToBoardFloat(*clientSocket, "pos_min_1", -2);
	writeToBoardFloat(*clientSocket, "pos_max_1", 2);
	//written as one
	writeToBoard(*clientSocket,"errorAction_1.pos", 1); // Why?q
	writeToBoard(*clientSocket,"errorAction_1.ichouse", 1); // Why?

	writeToBoard(*clientSocket, "pos_err_2", 179132);
	writeToBoardFloat(*clientSocket, "cur_err_2", 13);

	writeToBoardFloat(*clientSocket, "pos_min_2", -2);
	writeToBoardFloat(*clientSocket, "pos_max_2", 2);
	//written as one
	writeToBoard(*clientSocket,"errorAction_2.pos", 1);	// Why?
	writeToBoard(*clientSocket,"errorAction_2.ichouse", 1); // Why?

	writeToBoard(*clientSocket,"epsilon0PU_2", 364);
	writeToBoard(*clientSocket,"epsilon0PU_1", 43872);

	writeToBoard(*clientSocket,"peripherial.aux" , 1);
	writeToBoardFloat(*clientSocket, "acc_lim_1", 0.013888);
	writeToBoardFloat(*clientSocket, "vel_lim_1", 0.027777);
	writeToBoardFloat(*clientSocket, "vel_targ_1", 0);
	writeToBoardFloat(*clientSocket, "acc_lim_2", 0.013888);
	writeToBoardFloat(*clientSocket, "vel_lim_2", 0.027777);
	writeToBoardFloat(*clientSocket, "vel_targ_2", 0);

	clearBoardBitItems(*clientSocket, "errorAction_1");
	clearBoardBitItems(*clientSocket, "errorAction_2");
	sleep_us(1000000);

	//startMotorSelf(clientSocket);
	return 0;
}



int initialise(SOCKET *clientSocket){
	//visualiseGraph("sysidplot.txt");
	initLogger("log.txt");

	int boardItemCount = 0;

	iniToJson();
	if(createConnection((SOCKET *)clientSocket,axleIPAdress,axlePort)==1)exit(EXIT_FAILURE);

	int status = -1;
	if(getSocketStatus(*clientSocket,&status)==1)exit(EXIT_FAILURE);
	if(status != 0){
		error_handler("Board is not ready\n");
	}

	if(getBoardItemCount(*clientSocket,&boardItemCount)==1){
		fprintf(stderr,"ERROR: Important data transmission failed\n");
		exit(EXIT_FAILURE);
	}
	if(getBoardItems(*clientSocket, boardItemCount)==1){
		fprintf(stderr,"ERROR: boardItems.json creation failed\n");
		exit(EXIT_FAILURE);
	}
	if(createDataJson(boardItemCount)==1){
		fprintf(stderr,"ERROR: vlItem.json creation failed\n");
		exit(EXIT_FAILURE);
	}
	initBoard(clientSocket);
	return 0;
}



void readOutAngle(SOCKET *clientSocket){
// Implemented for Test Purposes
	int32_t pos2;
	int32_t calcValue;
	double angle;
	double incToDec;
	int print_ctr = 0;

	while(1){
		readFromBoardInt32(*clientSocket, "pos_2", &pos2);
		sleep_us(10000);
		calcValue = pos2 & 0x3FFFFFFF;
		incToDec = 360.0 / pow(2, 30);
		angle = (double)calcValue * incToDec;

		if(print_ctr > 30){
			printf("%.1f\n",angle);
			print_ctr = 0;
		}
		print_ctr++;
	}
}

unsigned int axleController(){
	
	/*AxleArguments* axArg = (AxleArguments*)arg;

	axleNum = axArg->axleNum;
	memcpy(axleIPAdress,axArg->ip_address,16);
	axlePort = axArg->port;

	printf("AXLENUM: %d, IP-Adress: %s, Port. %d\n",axArg->axleNum, axArg->ip_address, axArg->port);
	fflush(stdout);*/

	sysIdentification(clientSocket,128,"","test.txt");
	closesocket(clientSocket);
	WSACleanup();
	printf("Close Socket\n");
	fflush(stdout);
	closeLogger();
	return 0;
}
