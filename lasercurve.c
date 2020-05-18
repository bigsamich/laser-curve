//Marked for review
//in line comments 
/* laserCurve.c

Creadted 12-24-15

Authors:
Robert Santucci

The purpose of this program is to offer a command line interface to the laser notcher wave form generation

$ ./laserCurve <SaveFilename.txt> <Burstwidth> <NumberOfBursts> <Scenario> <PulseAmplitude> <PulseWidth> <PulseDelay> <KeepAliveShift> <PolyFit> <NotchSpace> <Notch %>

----------Help-----------
The purpose of this program is to create the input file for the laser notcher curve
Arguments needed include:
	-file (output file name)
	-bw (burst width 8-20 # of pulses)
	-bn (burst number 1-20 # of bursts including cleanup)
	-sc (scenario 1-2  1 has not leading burst. 2 has a leading burst)
	-pa (pulse amplitude 2047-4095)
	-pw (pulse width 1-2.5 ns)
	-nd (notch delay 0-2.218 us)
	-ks (keep alive shift 0-5 words)
	-or (polynomial order of pulse)
	-ns (notch spacing in words ~4440)
	-np (Begining notch percentage, pulse amp dependent)
------ End of Help-------


*/


//#include "stdafx.h"   /* Win32 Console App */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
	int bw;
	int bn;
	int sc;
	int pa;
	float pw;
	float nd;
	int ks;
	float order;
	int ns;
	float np;
} params;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float equationSlope(params input){
	//This function will calculate the constant/slope asociated with the beginning and max pulse for the given order of function.
	//Default to a quadratic for higher order
	int value = input.order;
	switch(value){
		case 0 :
			return 0;
		case 1 :
			return((float)(4095-input.pa)/(input.bw-1));
		case 2 :
			return((float)(4095-input.pa)/((input.bw-1)*(input.bw-1)));
		case 3 :
			return((float)(4095-input.pa)/((input.bw-1)*(input.bw-1)*(input.bw-1)));
		case -1 :
			return((float)(input.pa-4095)/(input.bw-1));
		case -2 :
			return((float)(input.pa-4095)/((input.bw-1)*(input.bw-1)));
		case -3 :
			return((float)(input.pa-4095)/((input.bw-1)*(input.bw-1)*(input.bw-1)));
		default :
			return((float)(4095-input.pa)/((input.bw-1)*(input.bw-1)));
	}
		
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void keepAlive(char file_name[], params input) {
	int value = 0;
	int tryCreate =0;
	//Open the laser file for read and write. Advance the file pointer by the shift value
	FILE *laser_file1 = fopen(file_name, "r");
	FILE *laser_file2 = fopen("temp.txt", "w");
	FILE *laser_file3 = fopen("temp2.txt","w");
	int flag = 1;
	int i = 0;
	int n = 0;
	int m = 0;
	char laser_train[50] ={'\0'};
	char laser_keepalive[50] ={'\0'};
	
	//set up our 450KHz keep alive mask by oring in 0x3000 to whatever value we land on until EOF
	i = 0;
	for (; i<input.ks; i++) {
		fscanf(laser_file1, "%d", &value);
		fprintf(laser_file2, "%d\n", value);
		fprintf(laser_file3,"%d\n", 0);
	}

	while (flag != EOF) {
		for (i = 0; i<4; i++) {
			flag = fscanf(laser_file1, "%d", &value);
			if (flag == EOF) break;
			value = value | 12288;
			fprintf(laser_file2, "%d\n", value);
			fprintf(laser_file3, "%d\n", 12288);
			for (n=0; n<3; n++) {
				flag = fscanf(laser_file1, "%d", &value);
				if (flag == EOF) break;
				fprintf(laser_file2, "%d\n", value);
				fprintf(laser_file3, "%d\n", 0);
			}
		}
		m = 0;
		//Need to lock in this numebras amount of time for each turn. This numebr plus 16 for the marker
		for (; m<2200; m++) {
			flag = fscanf(laser_file1, "%d", &value);
			if (flag == EOF) break;
			fprintf(laser_file2, "%d\n", value);
			fprintf(laser_file3, "%d\n", 0);
		}
	}
	fclose(laser_file1);
	fclose(laser_file2);
	fclose(laser_file3);
	
	strncpy(laser_train, file_name, (strlen(file_name) -4));
	strcat(laser_train, "_train.txt");
	
	strncpy(laser_keepalive, file_name, (strlen(file_name) -4));
	strcat(laser_keepalive, "_keepalive.txt");
	
	
	remove(laser_train);
	remove(laser_keepalive);
	
	//This is the raw data only
	rename(file_name, laser_train);
	//This is the keepalive pulse only
	rename("temp2.txt", laser_keepalive);
	//This is going to be the data imputted into the laser
	rename("temp.txt", file_name);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int curve(char file_name[], params input) {
	int xcord = 0;
	int flag = 0;
	int backflag = 1;
	int flagcord = 0;
	int start;
	char laser_train[50] ={'\0'};
	char laser_pulse[50] ={'\0'};
	int pad_delay, i, pad_it, pulse_it, pulse_number;
	int tryCreate1 = 0;
	int tryCreate2 = 0;
	int tryCreate3 = 0;
	float eq;
	int amp;
	FILE *laser_file = fopen(file_name, "w");
	
	////Do this x number of times depending on number of bursts as is deifned above
	int burst_it = 0; //Burst iterator

	//Put the notch pad here and then do a fixed 450 kHz b/w pulses with a tuneable couple words

	//Convert delay into padding lines. A 2 us delay will translate to 4000 lines.
	pad_delay =(input.nd * 2000);
	if(input.sc==2) pad_delay= pad_delay - (input.bw*10);
	for (pad_it = 1; pad_it <= pad_delay; pad_it++) {
			fprintf(laser_file, "%d\n", 2047);
			xcord++;
		}
	
	eq = equationSlope(input);
	//Put a leading pulse if scenario = 2
	if (input.sc == 2) goto PRE;
BACK:;
	backflag = 0;
	for (; burst_it < input.bn; burst_it++) {
		//Set 450khZ per burst
		//dont do this on the first one
		//4436 minus our burst width to set up the 450Khz structure
		//Changed 4436 to be a variable 7-26-16
		for (pad_it = 1; pad_it <= (input.ns -(input.bw*10)); pad_it++) {
			fprintf(laser_file, "%d\n", 2047);
			xcord++;
		}
		
		//We now iterate the number of pulses per burst as defined by burst_width
	PRE:;
		for (pulse_it = 0; pulse_it < input.bw; pulse_it++) {
			//We need to know our pulse width in bin size.
			//(This was entered as nano seconds) 1 word is equal to .5 ns.
			//For now we will round low for all arithmetic.
			pulse_number = (int)(input.pw / 0.5);

			if (((int)(input.pw * 10) % 5) >= 3) pulse_number++;

			//Calculate the pulse amplitude for the specific pulse using the equation slope prevoiusly calculated, we need to calculate our percenatge based on burst number
			if (input.order > 0){
				amp= ((((1-input.np)/input.bn)*burst_it)+input.np)*((eq*pow(pulse_it,input.order))+input.pa);
			}
			else if (input.order < 0){
				amp= ((((1-input.np)/input.bn)*burst_it)+input.np)*(4095+(eq*pow((pulse_it),(-input.order))));
			}
			else{
				amp= ((((1-input.np)/input.bn)*burst_it)+input.np)*input.pa;
			}
							
			for (i = 0; i <= 9; i++) {
				if (i < pulse_number) {
					fprintf(laser_file, "%d\n", amp);
				}
				else {
					fprintf(laser_file, "%d\n", 2047);
				}
				xcord++;
				flagcord++;
			}
			flag++;
		}
		if (input.sc == 2 && backflag == 1) goto BACK;
	}
	
	//Pad the end of the file to mod %64
	//since we are already incrementig xcrod, use this to know how many lines we have in the out file
	while (xcord % 64) {
		fprintf(laser_file, "%d\n",2047);
		xcord++;
	}
	//Close the output file
	fclose(laser_file);

	//Open the created file, and or in 0x3000 at 450KHz to mask the keep alive
	keepAlive(file_name, input);

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
int checkTol(params input) {

	if (input.bw<8 || input.bw > 20) return 0; //burst_width
	else if (input.bn<1 || input.bn > 20) return 0;//burst_num
	else if (input.sc<1 || input.sc > 4) return 0;//scenario
	else if (input.pa<2047 || input.pa > 4095) return 0;//pulse_amp
	else if (input.pw<1 || input.pw > 2.5) return 0;//pulse_width
	else if (input.nd<0 || input.nd > 2.218) return 0;//notch_delay
	else if (input.ks<0 || input.ks > 6) return 0;//keep alive shift
	else if (input.np * input.pa < 2047) return 0; //notch percentage
	//else if     notch spacing check
	else return 1;

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) {
	int check;
	char * file_name = malloc(sizeof(char) * 50);
	char c;
	params input;

	//Check argument count
	if (argc != 12 && argc != 2) {
		printf("Invalid number of options: Type help \n");
		return 0;
	}
	//printf("%s\n",argv[1]);

	if (!strcmp(argv[1], "help")) {
		printf("----------Help-----------\n");
		printf("The purpose of this program is to create the input file for the laser notcher curve\n");
		printf("Arguments needed include:\n");
		printf("-file (output file name)\n");
		printf("-bw (burst width 8-20 # of pulses)\n");
		printf("-bn (burst number 1-20 # of bursts including cleanup)\n");
		printf("-sc (scenario 1-2  1 has not leading burst. 2 has a leading burst)\n");
		printf("-pa (pulse amplitude 2047-4095)\n");
		printf("-pw (pulse width 1-2.5 ns)\n");
		printf("-nd (notch delay 0-2.218 us)\n");
		printf("-ks (keep alive shift 0-5 words)\n");
		printf("-or (polynomial order of pulse)\n");
		printf("-ns (notch spacing in words ~4440)\n");
		printf("-np (Begining notch percentage, pulse amp dependent)\n");
		printf("------ End of Help-------\n");
		return 0;
	}

	file_name = argv[1];

	input.bw = atoi(argv[2]);

	input.bn = atoi(argv[3]);

	input.sc = atoi(argv[4]);

	input.pa = atoi(argv[5]);

	input.pw = atof(argv[6]);

	input.nd = atof(argv[7]);

	input.ks = atoi(argv[8]);
	
	input.order =atoi(argv[9]);
	
	input.ns = atoi(argv[10]);
	
	input.np = atof(argv[11]);
	//Scale notch percentage to 1
	input.np = input.np/100;

	//printf("Done\n");

	// Check tolerance of each input
	check = checkTol(input);

	if (check != 1) {
		printf("Curve creation failed: Input argument out of range\n");
		return check;
	}

	//Create the curve
	check = curve(file_name, input);

	if (check != 1) {
		printf("Curve creation failed: Abort in curve function\n");
		return check;
	}
	//If here the program completed succesfully
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
