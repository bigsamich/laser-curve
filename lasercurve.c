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
	int trainOrder;
	int xcord;
} params;

//Function Prototypes
float equationSlope(params *input);
int keepAlive(int *data, int *rawkeepalive, params *input);
int saveData(char file_name[], int *data, int *rawdata, int *rawkeepalive, params *input);
int curve(char file_name[], params *input);
int addPadding(int *data, int *rawdata, params *input);
int addPulse(int *data, int *rawdata, int burst_it, int pulse_it, params *input);
int amplitude(params *input, int burst_it, int pulse_it);
int mod64(int *data, int *rawdata, params *input);
int addBurst(int *data, int *rawdata, int burst_it, params *input);
int checkTol(params input);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float burstSlope(params *input){
	//This function will calculate the constant/slope asociated with the beginning and max pulse for the given order of function.
	//Default to a quadratic for higher order
	int value = (*input).order;
	switch(value){
		case 0 :
			return 0;
		case 1 :
			return((float)(4095- (*input).pa)/((*input).bw-1));
		case 2 :
			return((float)(4095- (*input).pa)/(((*input).bw-1)*((*input).bw-1)));
		case 3 :
			return((float)(4095- (*input).pa)/(((*input).bw-1)*((*input).bw-1)*((*input).bw-1)));
		case -1 :
			return((float)((*input).pa-4095)/((*input).bw-1));
		case -2 :
			return((float)((*input).pa-4095)/(((*input).bw-1)*((*input).bw-1)));
		case -3 :
			return((float)((*input).pa-4095)/(((*input).bw-1)*((*input).bw-1)*((*input).bw-1)));
		default :
			return((float)(4095- (*input).pa)/(((*input).bw-1)*((*input).bw-1)));
	}
		
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float trainSlope(params *input) {
	//Calculates the slope/order for the notch train.
	int value = (*input).trainOrder;
	switch (value) {
	case 0:
		return 1;
	case 1:
		return((float)(4095 - (*input).pa) / ((*input).bn - 1));
	case 2:
		return((float)(4095 - (*input).pa) / (((*input).bn - 1)*((*input).bn - 1)));
	case 3:
		return((float)(4095 - (*input).pa) / (((*input).bn - 1)*((*input).bn - 1)*((*input).bn - 1)));
	default:
		return((float)(4095 - (*input).pa) / (((*input).bn - 1)*((*input).bn - 1)));
	}

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int keepAlive(int *data,int *rawkeepalive, params *input) {
	//set up our 450KHz keep alive mask by oring in 0x3000 to whatever value we land on until EOF
	int value = 0;	
	int flag = 1;
	int i = 0;
	int n = 0;
	int m = 0;
	int localcord = 0;
	int size = (*input).xcord;
	
	for (i=0; i<(*input).ks; i++) {
		rawkeepalive[i]=0;
		localcord++;

	}
	
	while (localcord < size) {
		
		for (i = 0; i<4; i++) {
			if (localcord==size) break;
			
			value = data[localcord] | 12288;
			
			data[localcord] = value;
			rawkeepalive[localcord] = 12228;
			localcord++;
			
			for (n=0; n<3; n++) {
				if (localcord==size) break;
				rawkeepalive[localcord] = 0;
				localcord++;
			}
			
		}
		
		//Need to lock in this numebras amount of time for each turn. This numebr plus 16 for the marker
		for (m = 0; m<2200; m++) {
			if (localcord==size) break;
			rawkeepalive[localcord] = 0;
			localcord++;
		}
		
	}
	
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int saveData(char file_name[],int *data,int *rawdata,int *rawkeepalive, params *input){
	//Saves the array data from the data rawdata and keepalive arrays. Creates three seperate files for each data. Saves the data with the name filename+identifier 

	char laser_train[50] ={'\0'};
	char laser_keepalive[50] ={'\0'};	
	
	int index;
	int size = (*input).xcord;
	
	
	strncpy(laser_train, file_name, (strlen(file_name) -4));
	strcat(laser_train, "_train.txt");
	
	strncpy(laser_keepalive, file_name, (strlen(file_name) -4));
	strcat(laser_keepalive, "_keepalive.txt");
	

	FILE *laser_file1 = fopen(file_name, "w");
	FILE *laser_file2 = fopen(laser_train, "w");
	FILE *laser_file3 = fopen(laser_keepalive,"w");
   
	for(index=0; index<size;index++){
		fprintf(laser_file1, "%d\n", data[index]);
		fprintf(laser_file2, "%d\n", rawdata[index]);
		fprintf(laser_file3, "%d\n", rawkeepalive[index]);
	}

		
	fclose(laser_file1);
	fclose(laser_file2);
	fclose(laser_file3);
	
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
int curve(char file_name[],params *input){
	//Generates the curve data structure. Will create the 3 arrays asociated with the data, rawdata, and the keepalive only data. 
	// rawdata = curve data with no keep alive
	//keepalive = array of size equal to rawdata with the $3000 keep alive pulses
	//Data = rawdata || keepalive

	int sizescale = ((*input).bn*5000);
	int *data = (int*)malloc(sizescale *sizeof(int));
	int *rawdata = (int*)malloc(sizescale *sizeof(int));
	int *rawkeepalive = (int*)malloc(sizescale *sizeof(int));
		
	int pad_delay, i, pad_it, pulse_it, pulse_number;
	int xcord = 0;
	float eq;
	int burst_it = 0; //Burst iterator	
	
	memset(data, 0, sizescale * sizeof(int));


	if ((*input).sc==2)for (pulse_it = 0; pulse_it < (*input).bw; pulse_it++) addPulse(data,rawdata,burst_it,pulse_it,input);
	addPadding(data,rawdata,input);

	for (; burst_it <= (*input).bn; burst_it++) addBurst(data,rawdata,burst_it,input);

	mod64(data,rawdata,input);

	keepAlive(data,rawkeepalive,input);

	saveData(file_name,data, rawdata, rawkeepalive, input);
	free(data);
	free(rawdata);
	free(rawkeepalive);
	
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int addPadding(int *data,int *rawdata, params *input){
	//adds in 0 amplitude pulses (2047) of size notch delay * 2000

	int pad_it;
	int pad_delay =(int) ((*input).nd * 2000);
	
	for (pad_it = 1; pad_it <= pad_delay; pad_it++) {
		data[(*input).xcord]=2047;
		rawdata[(*input).xcord]=2047;
		(*input).xcord++;
	}
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////		
int addPulse(int *data,int *rawdata,int burst_it, int pulse_it, params *input){
	//Adds a pulse to the curve
	//A pulse has size ~10 words. 1 word = 0.5 nanoseconds

	int amp = amplitude(input, burst_it, pulse_it);;
	int pulse_number = (int)((*input).pw/ 0.5);
	int i;

	//Round off pulse width to the nearest word.
	if (((int)((*input).pw * 10) % 5) >= 3) pulse_number++;

	for (i = 0; i <= 9; i++) {
		if (i < pulse_number){
			data[(*input).xcord]=amp;
			rawdata[(*input).xcord]=amp;
		} 
		else{
			data[(*input).xcord]=2047;
			rawdata[(*input).xcord]=2047;
		}	
		(*input).xcord++;
	}
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
int amplitude(params *input, int burst_it , int pulse_it) {
	//Calculates the amplitude of the laser at a given burst/pulse.
	//Scales the amplitude to set up curve structure given input parameters. 

	////////////////////////IN DEVELPOMENT//////////////////////////

	int amp;
	float burstPower;
	float burstEquation = burstSlope(input);
	float trainEquation = trainSlope(input);

	//float pulseScalar = ((((1 - (*input).np) / (*input).bn)*burst_it) + (*input).np);

	float pulseScalar = pow(burst_it, (*input).trainOrder);

	if ((*input).order > 0) {
		burstPower = pow(pulse_it, (*input).order);
		amp = pulseScalar * ((burstEquation*burstPower) + (*input).pa);
	}
	else if ((*input).order < 0) {
		burstPower = pow(pulse_it, (-(*input).order));
		amp = pulseScalar * (4095 + (burstEquation*burstPower));
	}
	else
		amp = pulseScalar * (*input).pa;

	return amp;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int mod64(int *data,int *rawdata, params *input){
	//Insures the curve data is module 64

	while ((*input).xcord % 64 != 0) {
		(*input).xcord++;
		data[(*input).xcord]=2047;
		rawdata[(*input).xcord]=2047;
	}
	return 1;
}	
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////	
int addBurst(int *data,int *rawdata,int burst_it, params *input){
	//Adds a burst to the curve
	//A burst is made of 8-20 pulses
	//Parameter burstwidth controls the size

	int pad_it;
	int pulse_it;
	for (pad_it = 1; pad_it <= ((*input).ns -((*input).bw*10)); pad_it++) {
		data[(*input).xcord]=2047;
		rawdata[(*input).xcord]=2047;
		(*input).xcord++;
	}

	for (pulse_it = 0; pulse_it < (*input).bw; pulse_it++) {
		addPulse(data,rawdata,burst_it,pulse_it,input);
	}

	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv) {
	//Main entry point. Does argument checking, error bounds, contains help information, takes in user input


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
	if (input.np > 1)
		input.np = input.np/100;

	input.trainOrder = atoi(argv[12]);

	input.xcord=0;

	// Check tolerance of each input
	check = checkTol(input);

	if (check != 1) {
		printf("Curve creation failed: Input argument out of range\n");
		return check;
	}

	//Create the curve

	check = curve(file_name, &input);

	if (check != 1) {
		printf("Curve creation failed: Abort in curve function\n");
		return check;
	}
	//If here the program completed succesfully
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
