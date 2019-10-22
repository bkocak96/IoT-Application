/*M
 * Copyright (c) 2015-2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== httpget.c ========
 *  HTTP Client GET example application
 */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
/* XDCtools Header files */
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include "driverlib/adc.h"
/* TI-RTOS Header files */
#include <sys/socket.h>
#include <ti/sysbios/knl/Swi.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/interrupt.h"
#include "driverlib/timer.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include <stdlib.h> /* for atoi() and exit() */
#include <string.h> /* for memset() */
#include <unistd.h>


#include <time.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/GPIO.h>
#include <ti/net/http/httpcli.h>
#include<arpa/inet.h>
#include <stdio.h>
/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/cfg/global.h>

#include <xdc/runtime/Memory.h>

#include <netinet/in.h>

/* Example/Board Header file */
#include "Board.h"

#include <sys/socket.h>

#define HOSTNAME          "api.openweathermap.org"
#define REQUEST_URI       "/data/2.5/weather?q=Eskisehir&units=metric&APPID=86564f18f4ea69b98df9c8df82c1200d"
#define USER_AGENT        "HTTPCli (ARM; TI-RTOS)"
#define HTTPTASKSTACKSIZE 4096
#define TASKSTACKSIZE   2048


String        Temparature;
String        Weather_Cond;



Task_Struct task0Struct;
Char task0Stack[TASKSTACKSIZE];
int a=0;


Clock_Struct clk0Struct, clk1Struct;	//clock cfg
Clock_Handle clk2Handle;


Semaphore_Struct semStruct;
Semaphore_Handle semHandle;
Semaphore_Struct semStruct1;
Semaphore_Handle semHandle1;


Void clk0Fxn(UArg arg0);
Void sendData2Server(UArg arg0, UArg arg1);
Void httpTask(UArg arg0,UArg arg1);
Void timerISR(UArg arg0);
Void swi0ISR(UArg arg0);

int serverPort = 5011;
const char *serverIP = "10.10.96.61";	//IP


char my_temp_value[10];		//alinacak olan temp arrayi
char weather_info[30];			//alinacak olan hava arrayi
uint32_t ADCValues[20];
uint32_t TempValueC[20];
float avgtemp;
Void timerISR(UArg arg0)
{
    //
    // Trigger the ADC conversion.
    //
    ADCProcessorTrigger(ADC0_BASE, 3);
    //


    Swi_post(swi0H);


}

Void swi0ISR(UArg arg0)
{
    // Wait for conversion to be completed.
    //
    while(!ADCIntStatus(ADC0_BASE,3, false)) { }
    //
    // Clear the ADC interrupt flag.
    //
    ADCIntClear(ADC0_BASE, 3);
    //
    // Read ADC Value.
    //
    ADCSequenceDataGet(ADC0_BASE, 3, ADCValues);
    TempValueC[a] = (uint32_t)(147.5-((75.0*3.3 *(float)ADCValues[0])) / 4096.0);
    //
    // Convert to Centigrade
    //
    a++;
    if(a==20){
        uint32_t sum;
        sum=0;
        int i;
        for(i=0;i<=19;i++){
           sum+= TempValueC[i];
        }
        avgtemp=(float)(sum)/20.0;

        a=0;

     }



}




/*
 *  ======== printError ========
 */
void printError(char *errString, int code)
{
    System_printf("Error! code = %d, desc = %s\n", code, errString);		//hatalar tek tek hazirlandi asagida
    BIOS_exit(code);
}

/*
 *  ======== httpTask ========
 *  Makes a HTTP GET request
 */
Void httpTask(UArg arg0, UArg arg1)
{
	while(1){

	Semaphore_pend(semHandle,BIOS_WAIT_FOREVER);		//semafor bekliyor clock 30 olunca task1 calisacak




    //http_get örneginden
    bool moreFlag = false;
    char data[500];			//siteden alinan büyük data
    int ret;
    int len;
    struct sockaddr_in addr;

    HTTPCli_Struct cli;
    HTTPCli_Field fields[3] = {
        { HTTPStd_FIELD_NAME_HOST, HOSTNAME },
        { HTTPStd_FIELD_NAME_USER_AGENT, USER_AGENT },
        { NULL, NULL }
    };

    System_printf("Sending a HTTP GET request to '%s'\n", HOSTNAME);
    System_flush();

    HTTPCli_construct(&cli);

    HTTPCli_setRequestFields(&cli, fields);

    ret = HTTPCli_initSockAddr((struct sockaddr *)&addr, HOSTNAME, 0);
    if (ret < 0) {
        printError("httpTask: address resolution failed", ret);
    }

    ret = HTTPCli_connect(&cli, (struct sockaddr *)&addr, 0, NULL);
    if (ret < 0) {
        printError("httpTask: connect failed", ret);
    }

    ret = HTTPCli_sendRequest(&cli, HTTPStd_GET, REQUEST_URI, false);
    if (ret < 0) {
        printError("httpTask: send failed", ret);
    }

    ret = HTTPCli_getResponseStatus(&cli);
    if (ret != HTTPStd_OK) {
        printError("httpTask: cannot get status", ret);
    }

    System_printf("HTTP Response Status Code: %d\n", ret);

    ret = HTTPCli_getResponseField(&cli, data, sizeof(data), &moreFlag);
    if (ret != HTTPCli_FIELD_ID_END) {
        printError("httpTask: response field processing failed", ret);
    }

    len = 0;	//alinan datanin boyutu
    do {
        ret = HTTPCli_readResponseBody(&cli, data, sizeof(data), &moreFlag);
        if (ret < 0) {
            printError("httpTask: response body processing failed", ret);
        }

        len += ret;
    } while (moreFlag);



    System_printf("Recieved %d bytes of payload\n", len);

    char *pStart;
    char *pEnd;
    //temp i bulup yanindaki deðeri alacak
    pStart = strstr(data, "temp\":");
           if (pStart != NULL) {
               pStart += 6;
               pEnd = strstr(pStart, ",");
               if (pEnd != NULL) {
                   *pEnd = 0;
                   System_printf("Temperature : %s*C\n", pStart);//ayrýca yazdýracak
                    strcpy(my_temp_value,pStart);	//sonra da bunu temparature ye kaydedecek
                   *pEnd = ',';
               }
           }

           //description(hava durumunu) i bulup yanindaki deðeri alacak

           pStart = strstr(data, "description\":");

           if (pStart != NULL) {

               pStart += 13;

               pEnd = strstr(pStart, ",");

               if (pEnd != NULL) {

                   *pEnd = 0;

                   //System_printf("Weather Condition : %s\n", pStart);//ayrýca yazdýracak

                    strcpy(weather_info,pStart);		//sonra da bunu weather a kaydedecek
                   *pEnd = ',';
               }
           }


    System_flush();



    HTTPCli_disconnect(&cli);
    HTTPCli_destruct(&cli);

        Weather_Cond=weather_info;
        Temparature=my_temp_value;
        Semaphore_post(semHandle1);
	}
}

/*
 *  ======== netIPAddrHook ========
 *  This function is called when IP Addr is added/deleted
 */
void netIPAddrHook(unsigned int IPAddr, unsigned int IfIdx, unsigned int fAdd)
{

	  static Task_Handle taskHandle;
	        			  Task_Params taskParams;
	        			    Error_Block eb;

	   if (fAdd && !taskHandle) {

            Clock_Handle clk0;
        	Clock_Params clkParams;
        	Clock_Params_init(&clkParams);
        	clkParams.period = 30000;			//30 sn de bir task1 i calistiriyor
        	clkParams.startFlag = TRUE;
        	clk0 = Clock_create(clk0Fxn,30000, &clkParams, NULL);
        	Clock_start(clk0);

			Error_init(&eb);
			Task_Params_init(&taskParams);
			taskParams.stackSize = HTTPTASKSTACKSIZE;
			taskParams.priority = 2;
			taskHandle = Task_create((Task_FuncPtr)httpTask, &taskParams, &eb);

			Task_Params TcptaskParams;
			Task_Params_init(&TcptaskParams);
			TcptaskParams.stackSize =HTTPTASKSTACKSIZE;
			TcptaskParams.priority = 2;
			taskHandle = Task_create((Task_FuncPtr) sendData2Server, &TcptaskParams, NULL);

	   if (taskHandle == NULL) {
		   printError("netIPAddrHook: Failed to create HTTP Task\n", -1);
	   }

	   /* Create a HTTP task when the IP address is added */

        }


}

/*
 *  ======== main ========
 */
Void clk0Fxn(UArg arg0){


Semaphore_post(semHandle);


}

//2. task server a gönderecek olan
Void sendData2Server(UArg arg0, UArg arg1){

while(1){



    Semaphore_pend(semHandle1,BIOS_WAIT_FOREVER);

		System_printf("Gelen Temperature : %sF\n",Temparature);
		//System_printf("Gelen Hava Durumu : %s\n",Weather_Cond);
		char avg[5];
		sprintf(avg,"%.2f",avgtemp);


		char kata[100];
		memset(kata,'\0',100);
		int sockfd;
		struct sockaddr_in serverAddr;
		strcpy(kata, "\nTemp: ");
		strcat(kata, Temparature);
		//strcat(kata, "Cond: ");
		//strcat(kata, Weather_Cond);
        strcat(kata, "\naverage: ");
        strcat(kata, avg);



		sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sockfd == -1){
			System_printf("Socket not created");
		}
		else{
			System_printf("Socket is created\n");
		}
		 memset(&serverAddr, 0, sizeof(serverAddr));

		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(serverPort);
		inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

		int connStat = connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
		if(connStat < 0){
			System_printf("Error while connecting to server\n");
			if (sockfd > 0)
				close(sockfd);
		}

		int numSend = send(sockfd, kata, 80, 0);
		if(numSend < 0){
			System_printf("Error while sending data to server\n");
			if (sockfd > 0)
				close(sockfd);
		}

		if (sockfd > 0){
				close(sockfd);
				GPIO_write(Board_LED1, Board_LED_ON);
		}
		GPIO_write(Board_LED0, Board_LED_ON);


	}

}
void initialize_ADC()
{
    //
    // The ADC0 peripheral must be enabled for use.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlDelay(3);
    //
    // Enable sample sequence 3 with a processor signal trigger. Sequence 3
    // will do a single sample when the processor sends a singal to start the
    // conversion. Each ADC module has 4 programmable sequences, sequence 0
    // to sequence 3. This example is arbitrarily using sequence 3.
    //
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
    //
    // Configure step 0 on sequence 3. Sample the temperature sensor
    // (ADC_CTL_TS) and configure the interrupt flag (ADC_CTL_IE) to be set
    // when the sample is done. Tell the ADC logic that this is the last
    // conversion on sequence 3 (ADC_CTL_END). Sequence 3 has only one
    // programmable step. Sequence 1 and 2 have 4 steps, and sequence 0 has
    // 8 programmable steps. Since we are only doing a single conversion using
    // sequence 3 we will only configure step 0. For more information on the
    // ADC sequences and steps, reference the datasheet.
    //
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_TS | ADC_CTL_IE | ADC_CTL_END);
    //
    // Since sample sequence 3 is now configured, it must be enabled.
    //
    ADCSequenceEnable(ADC0_BASE, 3);
    //
    // Clear the interrupt status flag. This is done to make sure the
    // interrupt flag is cleared before we sample.
    //
    ADCIntClear(ADC0_BASE, 3);
}



int main(void)
{
    /* Call board init functions */
	//Mailbox_Params mbxParams;
	Semaphore_Params semParams;
	//Swi_Params swiParams;
    Board_initGeneral();
    Board_initGPIO();
    Board_initEMAC();




	Semaphore_Params_init(&semParams);
	Semaphore_construct(&semStruct, 0, &semParams);

	Semaphore_Params_init(&semParams);
    Semaphore_construct(&semStruct1, 0, &semParams);


	/* Obtain instance handle */
	semHandle = Semaphore_handle(&semStruct);
    semHandle1 = Semaphore_handle(&semStruct1);

    initialize_ADC();







    System_printf("Starting the HTTP GET example\nSystem provider is set to "
            "SysMin. Halt the target to view any SysMin contents in ROV.\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
