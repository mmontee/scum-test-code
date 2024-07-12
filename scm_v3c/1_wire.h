#ifndef __1_WIRE_H
#define __1_WIRE_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

//=========================== defines =========================================

#define READROM 0x33
#define SKIPROM 0xCC
#define MATCHROM 0x55
#define SEARCHROM 0xF0

#define DELAY_A 6
#define DELAY_B 64
#define DELAY_C 60
#define DELAY_D 10
#define DELAY_E 9
#define DELAY_F 55
#define DELAY_G 0 
#define DELAY_H 480
#define DELAY_I 70
#define DELAY_J 410

#define TX_PIN 0
#define RX_PIN 1

// Function Macros
#define BUS_LOW() GPIO_REG__OUTPUT |= (1 << TX_PIN); // Pulls bus LOW
#define BUS_RELEASE() GPIO_REG__OUTPUT &= ~(1 << TX_PIN); // Release bus to Pull-up
#define BUS_READ() (GPIO_REG__INPUT &= (1 << RX_PIN)) ? 1 : 0;
//===================================
// 1-wire search rom, code taken from;
// https://www.analog.com/en/resources/app-notes/1wire-search-algorithm.html
#define FALSE 0
#define TRUE 1

//=========================== variables =======================================
// ROMs found on the bus are stored in a linked list structure
typedef struct rom_list_root{
	uint32_t item_count;
	struct rom_list* next;
} rom_list_root_t,	*bus_roms_root_prt_t;

typedef struct rom_list{
	uint64_t rom;
	struct rom_list* next;
} rom_list_t,	*bus_roms_list_prt_t;
	
//=========================== prototypes ======================================
// User
uint8_t OWRead_byte(void);// Reads a byte from bus
void OWRead_bytes(uint8_t* buffer, int size);// Reads n bytes from bus
int OWIsolate_device(uint64_t device_rom); // Isolate a device
void OWWrite_rom(uint64_t device_rom); // Write rom to bus
bus_roms_root_prt_t OWSearch_bus(void);// Read all roms on bus, returns base to linked list
void OWGet_rom_array(uint64_t* array, bus_roms_root_prt_t root);// Create array of ROMs from linked list
int OWCRC_bytes(uint8_t* byte_array, int size);
	
// Driver
void OWDelay_us(int us);
void OWStore_rom(unsigned char* rom_bytes, bus_roms_list_prt_t base);
void OWWriteBit(uint8_t bit);
uint8_t OWReadBit(void);
int OWReset(void);
void OWWriteByte(uint8_t byte);
//===================================
// 1-wire search rom, code taken from;
// https://www.analog.com/en/resources/app-notes/1wire-search-algorithm.html
int OWFirst(void);
int OWNext(void);
int OWVerify(void);
void OWTargetSetup(unsigned char family_code);
void OWFamilySkipSetup(void);
int OWSearch(void);
unsigned char docrc8(unsigned char value);
#endif
