/* Maxon RE30 Encoder reader
made by Szuyu Lin R05631009, August 2017
Compilation Instructions:
~ $ gcc -std=c99 -o Encoder170831 /home/pi/Desktop/Encoder170831.c -l bcm2835 -l pthread
~ $ sudo ./Encoder170831
*/

#define _GNU_SOURCE
#include<stdio.h>
#include<sched.h>
#include<bcm2835.h>
#include<pthread.h>
#include<time.h>
//Pins definition: Switch A and B pins if direction reversed. See end of code for layout diagram.
#define FXA RPI_GPIO_P1_11
#define FXB RPI_GPIO_P1_15
#define FYA RPI_GPIO_P1_16
#define FYB RPI_GPIO_P1_18

#define RXA RPI_GPIO_P1_10
#define RXB RPI_GPIO_P1_08
#define RYA RPI_GPIO_P1_19
#define RYB RPI_GPIO_P1_21

#define WXA RPI_GPIO_P1_22
#define WXB RPI_GPIO_P1_24
#define WYA RPI_GPIO_P1_07
#define WYB RPI_GPIO_P1_23

//Global variables: Actual encoder counts
int fxc, fyc;
int rxc, ryc;
int wxc, wyc;
int loopcount;
//Struct: contain pins(static), counts(->global), and binding CPU core ID
struct pinset{
	uint8_t XA;
	uint8_t XB;
	uint8_t YA;
	uint8_t YB;
	int xc;
	int yc;
	uint8_t Core_ID;
};

//PinsInitialize: Hardware Settings
uint8_t PinsInitialize(uint8_t PIN_A, uint8_t PIN_B)
{
	//Pin state initialization & pull-down resistors
	bcm2835_gpio_fsel(PIN_A, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN_B, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_set_pud(PIN_A, BCM2835_GPIO_PUD_DOWN);
	bcm2835_gpio_set_pud(PIN_B, BCM2835_GPIO_PUD_DOWN);
	//falling & rising edge detections enabled
	bcm2835_gpio_afen(PIN_A);
	bcm2835_gpio_afen(PIN_B);
	bcm2835_gpio_aren(PIN_A);
	bcm2835_gpio_aren(PIN_B);
	//return initialized local state variables
	uint8_t CHA = bcm2835_gpio_lev(PIN_A);
	uint8_t CHB = bcm2835_gpio_lev(PIN_B);
	return (CHA!=CHB) + CHB*2;
}//PinsInitialize

// *encoderCount: logical encoder countingprocess, in thread form
void *encoderCount(void *channel)
{
	//struct: package containing identifiers
	struct pinset *c = channel;
	//CPU affinity settings for indiv thread
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(c->Core_ID, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 1;

	uint8_t xCHA, xCHB, yCHA, yCHB;                  //digital pin state
	uint8_t xstt, ystt;                              //encoder state = 0, 1, 2, or 3
	uint8_t dxstt, dystt;                            //direction = 1(CCW) or 3(CW)
	uint8_t xstt_pre = PinsInitialize(c->XA, c->XB); //Initialize pins & state
	uint8_t ystt_pre = PinsInitialize(c->YA, c->YB);

	//Logic operation: GPIO edge trigger detection
	while(1)
	{
		loopcount++;
		if(bcm2835_gpio_eds(c->XA))
		{
			xCHA = bcm2835_gpio_lev(c->XA);
			xCHB = bcm2835_gpio_lev(c->XB);
			xstt = (xCHA!=xCHB) + 2*xCHB;
			dxstt = (xstt - xstt_pre + 4)%4;
			c->xc += (dxstt%2)*(dxstt-2);
			xstt_pre = xstt;
			bcm2835_gpio_set_eds(c->XA);
			//reset edge detection flag to untriggered state
		}
		if(bcm2835_gpio_eds(c->XB))
		{
			xCHA = bcm2835_gpio_lev(c->XA);
			xCHB = bcm2835_gpio_lev(c->XB);
			xstt = (xCHA!=xCHB) + 2*xCHB;
			dxstt = (xstt - xstt_pre + 4)%4;
			c->xc += (dxstt%2)*(dxstt-2);
			xstt_pre = xstt;
			bcm2835_gpio_set_eds(c->XB);
		}
		if(bcm2835_gpio_eds(c->YA)) //trigger YA|| trigger YB
		{
			yCHA = bcm2835_gpio_lev(c->YA);
			yCHB = bcm2835_gpio_lev(c->YB);
			ystt = (yCHA!=yCHB) + 2*yCHB;
			dystt = (ystt - ystt_pre + 4)%4;
			c->yc += (dystt%2)*(dystt-2);
			ystt_pre = ystt;
			bcm2835_gpio_set_eds(c->YA);
		}
		if(bcm2835_gpio_eds(c->YB))
		{
			yCHA = bcm2835_gpio_lev(c->YA);
			yCHB = bcm2835_gpio_lev(c->YB);
			ystt = (yCHA!=yCHB) + 2*yCHB;
			dystt = (ystt - ystt_pre + 4)% 4;
			c->yc += (dystt%2)*(dystt-2);
			ystt_pre = ystt;
			bcm2835_gpio_set_eds(c->YB);
		}
		//usleep(1);
		//clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
	}//while(1)
}//encoderCount

//main: log out global encoder readings
int main(int argc, char **argv)
{
	//wait for GPIO initialization
	if(!bcm2835_init()) return 1;

	//create thread objects to bypass tasks
	pthread_t thread_Front, thread_Rear, thread_Waist;

	//pass in parameters & attach function to thread
	struct pinset Front = {FXA, FXB, FYA, FYB, fxc, fyc, 1};
	struct pinset Rear = {RXA, RXB, RYA, RYB, rxc, ryc, 2};
	struct pinset Waist = {WXA, WXB, WYA, WYB, wxc, wyc, 3};

	pthread_create(&thread_Front, NULL, encoderCount, (void*)&Front);
	pthread_create(&thread_Rear, NULL, encoderCount, (void*)&Rear);
	pthread_create(&thread_Waist, NULL, encoderCount, (void*)&Waist);

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	sched_setaffinity(0, sizeof(cpuset), &cpuset);

	while(1)
	{
		//usleep(1000);
		sleep(1);
		printf("front_x_counts %d\n", Front.xc); //fxc? Front.xc?
		printf("loopcount %d\n", loopcount);
	}

	bcm2835_close();
	return 0;
}//main
