/*
 * ProblemDemo.c
 *
 * Created: 20.03.2018 18:32:07
 * Author : chaos
 */ 

//#include <avr/io.h>
#include "avr_compiler.h"
#include "pmic_driver.h"
#include "TC_driver.h"
#include "clksys_driver.h"
#include "sleepConfig.h"
#include "port_driver.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "stack_macros.h"

#include "mem_check.h"

#include "init.h"
#include "utils.h"
#include "errorHandler.h"
#include "NHD0420Driver.h"

#include "ButtonHandler.h"

SemaphoreHandle_t meinMutex; //A-Ressource
uint32_t meineVariable = 0; //P-Ressource

EventGroupHandle_t meineEventGroup;
#define LOCK_DATA	1 << 0
#define DATA_READY	1 << 1
#define LOCK_CLEARED 1 << 2
#define LED1ENABLE	1 << 3
#define LED2ENABLE	1 << 4
#define RESET1		1 << 5
#define RESET2		1 << 6
#define ALGO1		1 << 8
#define ALGO2		1 << 9
uint32_t meineZweiteVariable = 0;

void vButtonTask(void *pvParameters);

void task1(void *pvParameters);
void task2(void *pvParameters);

void task3(void *pvParameters);
void task4(void *pvParameters);

void ledTask1(void *pvParameters);
void ledTask2(void *pvParameters);


TaskHandle_t ledTask;

int main(void)
{
    resetReason_t reason = getResetReason();

	vInitClock();
	vInitDisplay();
	
	meinMutex = xSemaphoreCreateMutex();
	meineEventGroup = xEventGroupCreate();
	xEventGroupSetBits(meineEventGroup, ALGO1);
	
	xTaskCreate(vButtonTask, (const char *) "buttonTask", configMINIMAL_STACK_SIZE+50, NULL, 2, NULL);
	
	xTaskCreate( task1, (const char *) "task1", configMINIMAL_STACK_SIZE+50, NULL, 1, NULL);
	xTaskCreate( task2, (const char *) "task2", configMINIMAL_STACK_SIZE+50, NULL, 1, NULL);
	
	xTaskCreate( task3, (const char *) "task3", configMINIMAL_STACK_SIZE+50, NULL, 1, NULL);
	xTaskCreate( task4, (const char *) "task4", configMINIMAL_STACK_SIZE+50, NULL, 1, NULL);
	
	xTaskCreate(ledTask1, (const char *) "ledtask1", configMINIMAL_STACK_SIZE+50, NULL, 1, NULL);
	xTaskCreate(ledTask2, (const char *) "ledtask2", configMINIMAL_STACK_SIZE+50, NULL, 1, NULL);

	vDisplayClear();
	vTaskStartScheduler();
	return 0;
}

//////////////////////////////////////////////////////////////////////////
// Task um Variable 1 hochzuzählen. Gesichert mit Mutex
//////////////////////////////////////////////////////////////////////////
void task1(void *pvParameters) {
	uint32_t localx = 0;
	for(;;) {
		localx++;
		if((xEventGroupGetBits(meineEventGroup) & RESET1) == RESET1) {
			xEventGroupClearBits(meineEventGroup, RESET1);
			localx = 0;
		}
		if(xSemaphoreTake(meinMutex, 10/portTICK_RATE_MS) == pdTRUE) {
			meineVariable = localx;
			xSemaphoreGive(meinMutex);
		}		
		vTaskDelay(10/portTICK_RATE_MS);
	}	
}

//////////////////////////////////////////////////////////////////////////
// Task um Variable 1 auf dem Display anzuzeigen. Gesichert mit Mutex
//////////////////////////////////////////////////////////////////////////
void task2(void *pvParameters) {
	uint32_t localy = 0;
	for(;;) {
		if(xSemaphoreTake(meinMutex, 10/portTICK_RATE_MS) == pdTRUE) {
			localy = meineVariable;
			xSemaphoreGive(meinMutex);
		}
		vDisplayClear();
		vDisplayWriteStringAtPos(0,0,"Variable1: %d", localy);
		vTaskDelay(200/portTICK_RATE_MS);
	}
}

//////////////////////////////////////////////////////////////////////////
// Task um Variable2 hochzuzählen. Gesichert mit EventGroup
//////////////////////////////////////////////////////////////////////////
void task3(void* pvParameters) {
	uint32_t localx = 0;
	for(;;) {
		localx++;
		if((xEventGroupGetBits(meineEventGroup) & RESET2) == RESET2) {
			xEventGroupClearBits(meineEventGroup, RESET2);
			localx = 0;
		}
		EventBits_t bits = xEventGroupGetBits(meineEventGroup);
		if((bits & LOCK_DATA) != 0) {
			xEventGroupSetBits(meineEventGroup, DATA_READY);
			xEventGroupWaitBits(meineEventGroup, LOCK_CLEARED, pdTRUE, pdFALSE, portMAX_DELAY);
		}
		meineZweiteVariable = localx;		
		vTaskDelay(10/portTICK_RATE_MS);
	}
}

//////////////////////////////////////////////////////////////////////////
// Task um Variable2 auf dem Display anzuzeigen. Gesichert mit EventGroup
//////////////////////////////////////////////////////////////////////////
void task4(void* pvParameters) {
	uint32_t localy = 0;
	for(;;) {
		xEventGroupSetBits(meineEventGroup, LOCK_DATA);
		xEventGroupWaitBits(meineEventGroup, DATA_READY, pdTRUE, pdFALSE, portMAX_DELAY);
		localy = meineZweiteVariable;
		xEventGroupClearBits(meineEventGroup, LOCK_DATA);
		xEventGroupSetBits(meineEventGroup, LOCK_CLEARED);
		vDisplayClear();
		vDisplayWriteStringAtPos(1,0,"Variable2: %d", localy);
		vTaskDelay(200/portTICK_RATE_MS);
	}
}

//////////////////////////////////////////////////////////////////////////
// Task um LED1 blinken zu lassen. Gesteuert mit EventGroup
//////////////////////////////////////////////////////////////////////////
void ledTask1(void *pvParameters) {
	PORTF.DIRSET = PIN0_bm; /*LED1*/
	PORTF.OUT = 0x01;
	for(;;) {
		xEventGroupWaitBits(meineEventGroup, LED1ENABLE, pdFALSE, pdFALSE, portMAX_DELAY);
		PORTF.OUTTGL = 0x01;
		vTaskDelay(100/portTICK_RATE_MS);
	}
}

//////////////////////////////////////////////////////////////////////////
// Task um LED2 blinken zu lassen. Gesteuert mit EventGroup
//////////////////////////////////////////////////////////////////////////
void ledTask2(void *pvParameters) {
	PORTF.DIRSET = PIN1_bm; /*LED1*/
	PORTF.OUT = 0x02;
	for(;;) {
		xEventGroupWaitBits(meineEventGroup, LED2ENABLE, pdFALSE, pdFALSE, portMAX_DELAY);
		PORTF.OUTTGL = 0x02;
		vTaskDelay(100/portTICK_RATE_MS);
	}
}

//////////////////////////////////////////////////////////////////////////
// Task um Buttons abzufragen und auszuwerten. Steuert EventGroups
//////////////////////////////////////////////////////////////////////////
void vButtonTask(void * pvParameters) {
	initButtons();
	for(;;) {
		updateButtons();
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {
			xEventGroupSetBits(meineEventGroup, LED1ENABLE);
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {
			xEventGroupClearBits(meineEventGroup, LED1ENABLE);
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {
			xEventGroupSetBits(meineEventGroup, LED2ENABLE);
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {
			xEventGroupClearBits(meineEventGroup, LED2ENABLE);
		}
		if(getButtonPress(BUTTON1) == LONG_PRESSED) {
			xEventGroupSetBits(meineEventGroup, RESET1);
		}
		if(getButtonPress(BUTTON2) == LONG_PRESSED) {			
			xEventGroupSetBits(meineEventGroup, RESET2);
		}
		if(getButtonPress(BUTTON3) == LONG_PRESSED) {
			// Beispiel für Wechseln zwischen zwei Algorithmen, gespeichert in EventGroup
			uint8_t algostatus = (xEventGroupGetBits(meineEventGroup) & 0x0300 ) >> 8;
			if(algostatus == 0x01) {
				xEventGroupClearBits(meineEventGroup, ALGO1);
				xEventGroupSetBits(meineEventGroup, ALGO2);
			} else {
				xEventGroupClearBits(meineEventGroup, ALGO2);
				xEventGroupSetBits(meineEventGroup, ALGO1);
			}
		}
		if(getButtonPress(BUTTON4) == LONG_PRESSED) {
		}
		vTaskDelay(10/portTICK_RATE_MS);
	}
}
