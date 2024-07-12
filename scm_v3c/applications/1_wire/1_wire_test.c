#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "memory_map.h"
#include "optical.h"
#include "scm3c_hw_interface.h"
#include "gpio.h"
#include "rftimer.h"
#include "1_wire.h"

//=========================== defines =========================================
// SCuM
#define CRC_VALUE (*((unsigned int*)0x0000FFFC))
#define CODE_LENGTH (*((unsigned int*)0x0000FFF8))

// DS18B20
#define DS18B20 
#define READ_SCRATCH 0xBE
#define WRITE_SCRATCH 0x4E
#define COPY_SCRATCH 0x48
#define RECALL_E2 0xB8
#define READ_POWER_SUPPLY 0xB4
#define CONVERT_T 0x44
#define T_CONV_9 94
#define T_CONV_10 188
#define T_CONV_11 375
#define T_CONV_12 750
//=========================== variables =======================================
// SCuM
typedef struct {
    uint8_t count;
} app_vars_t;

app_vars_t app_vars;

// DS18B20 - valid values are only {9,10,11,12}
int resolution = 9;
//=========================== prototypes ======================================
// 1-Wire
void OW_gpio_config(void); 

// DS18B20
void init_DS18B20(uint64_t device_rom, uint8_t T_h, uint8_t T_l, uint8_t config);
uint16_t get_temp(uint64_t device_rom);
void silence_callback(void);
//=========================== main ============================================

int main(void) {	
	// ===================init the mote============================================
	memset(&app_vars, 0, sizeof(app_vars_t)); 
	printf("Initializing...\r\n");
	initialize_mote();
	crc_check();
	perform_calibration();
	printf("Initialization Complete.\r\n");
	rftimer_set_callback_by_id(silence_callback, 1);
	
// Start -- 1-wire init.===========================================================
	/*
	GPIO 0, 1 are configured for bus using "OW_gpio_config();"
	Declare List root.
	Assign OWSearch_bus()'s output to the root. Returns NULL on fail.
	Declare array with List root member "item_count" elements.
	Pass both the root and the array to OWGet_rom_array tranfer() moves the list into an array.
	*/
	OW_gpio_config(); // config for GPIO_0 = input GPIO_1 = input
	bus_roms_root_prt_t rom_list_root; //Declaring List root.
	if((rom_list_root = OWSearch_bus()) == NULL)// Try to get the ROMs of all devices
	{
		printf("No ROMs Found.\r\n");
	}
	else //OWSearch_bus() found >= 1 ROM
	{
		uint64_t ROMS[rom_list_root->item_count]; // Declare array with root member "item_count" elements.
		OWGet_rom_array(ROMS, rom_list_root); // Store ROMs in array, frees linked list
// END -- 1-wire init.=============================================================	
		for(int i = 0; i < sizeof(ROMS) / sizeof(uint64_t); i++)// Print all ROMs
		{
			printf("0x%llX\r\n", ROMS[i]);
		}
		
	/*
	After 1-wire init. success all roms discovered on bus are stored in ROMS[].
	Individual devices can be isolated using OWIsolate_device(ROMS[n]);
	*/
		#ifdef DS18B20
		init_DS18B20(ROMS[0], 100, 10, 12);
		init_DS18B20(ROMS[1], 100, 10, 12);
		int count = 1;
		
		while(1)
		{
			printf("Sample #%d\r\n", count);
			OWIsolate_device(ROMS[0]);
			printf("Sensor 1 temp = %dC\r\n", get_temp(ROMS[0]));
			OWIsolate_device(ROMS[1]);
			printf("Sensor 2 temp = %dC\r\n", get_temp(ROMS[1]));
			count++;
			delay_milliseconds_synchronous(2000	, 1);
		}
		#endif
	}
}

// Routine to configure gpio after mote_init.
// Taken from the mote_init routine just changed the GPI/O_enable values
void OW_gpio_config(void)
{
	// Select banks for GPIO inputs
	GPI_control(0, 0, 1, 0);  // 1 in 3rd arg connects GPI8 to EXT_INTERRUPT<1>
	// Select banks for GPIO outputs
	GPO_control(6, 6, 0, 6);  // 0 in 3rd arg connects clk_3wb to GPO8 for 3WB cal
	// Set GPI enables
	GPI_enables(0x0102); // enable GPIO 1 as RX for 1-wire
	// Set GPO enables
	GPO_enables(0x0FFD); // GPIO 0 is 1-wire TX
	// scan chain
	analog_scan_chain_write();
	analog_scan_chain_load();
	// Initialize all pins to be low.
	GPIO_REG__OUTPUT &= ~0xFFFF;
}

#ifdef DS18B20
// DS18B20 =======================================================================
// Isolates then configures a DS18B20's alarms and resolution registers.
// Write all changes to EEPROM
// config = 9 | 10 | 11 | 12, for resolution bits
void init_DS18B20(uint64_t device_rom, uint8_t T_h, uint8_t T_l, uint8_t resolution)
{
	int config; // resolution sets config
	switch(resolution)
	{
		case 9:
			config = 0x00;
			break;
		case 10:
			config = 0x20;
			break;
		case 11:
			config = 0x40;
			break;
		case 12:
			config = 0x60;
			break;
		default:
			config = 0x00;
			break;
	}

	uint8_t scratch_mem[9] = {0}; // Read the scratch
	int cnt = 0;
	do
	{
		// Write the config registers.
		OWIsolate_device(device_rom);
		OWWriteByte(WRITE_SCRATCH);
		OWWriteByte(T_h); // T_high alarm
		OWWriteByte(T_l); // T_low alarm
		OWWriteByte(config); // resolution config.
		
		// Copy scatch memory into device EEPROM 
		OWIsolate_device(device_rom);
		OWWriteByte(COPY_SCRATCH); // Copies to EEPROM
		delay_milliseconds_synchronous(10	, 1);

		// Read scatch memory for CRC check.
		OWIsolate_device(device_rom);
		OWWriteByte(READ_SCRATCH);
		OWRead_bytes(scratch_mem, sizeof(scratch_mem));
		cnt++;
		if(cnt > 10)
		{
			printf("\r\nCRC fail\r\n");
		}
	}while(((OWCRC_bytes(scratch_mem, sizeof(scratch_mem) + 1)) != 0) && (cnt <= 10));
}

// Returns temperature data from a given ROM
uint16_t get_temp(uint64_t device_rom)
{
	int delay; // Resolution selects delay
	switch(resolution)
	{
		case 9:
			delay = T_CONV_9;
			break;
		case 10:
			delay = T_CONV_10;
			break;
		case 11:
			delay = T_CONV_11;
			break;
		case 12:
			delay = T_CONV_12;
			break;
		default:
			delay = T_CONV_9;
			break;
	}
	uint8_t scratch_bytes[9] = {0};
	uint16_t temp_val = 0x00;
	int cnt = 0;
	do{
		OWIsolate_device(device_rom);
		OWWriteByte(CONVERT_T);// take a temperature reading
		delay_milliseconds_synchronous(delay, 1); // Let the sensor think

		OWIsolate_device(device_rom);
		OWWriteByte(READ_SCRATCH);
		OWRead_bytes(scratch_bytes, sizeof(scratch_bytes));
		temp_val |= scratch_bytes[1]; // MSB
		temp_val = (temp_val << 8);
		temp_val |= scratch_bytes[0]; // LSB
		temp_val = (temp_val >> 4); // Truncate the decimal for now.
		cnt++;
		if(cnt > 10)
		{
			printf("\r\nCRC fail\r\n");
		}
		//temp_val = (int)((temp_val * (9/5)) + 32); // convert to Freedom units ;)
	}while(((OWCRC_bytes(scratch_bytes, sizeof(scratch_bytes) + 1)) != 0) && (cnt <= 10));
	return temp_val;
}

void silence_callback(void)
{
	//SHHHHH!!!!
}
#endif

