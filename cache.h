/*
 * cache.h
 *
 *  Created on: Mar 24, 2017
 *      Author: Yiming Wang
 */

#ifndef CACHE_H_
#define CACHE_H_
#include "msp.h"

void Cache_process();

void Cache_Initialization(uint16_t i_cache_size, uint16_t d_cache_size, uint16_t block_size, uint8_t write_thourgh);

void i_cache_config(uint16_t cache_size,uint16_t block_size);

uint32_t i_cache_read();

uint8_t i_cache_fill(uint32_t current_PC);

void i_cache_process();

void d_cache_config(uint16_t cache_size,uint16_t block_size, uint8_t WT);

uint32_t d_cache_read(uint32_t addr);

void d_cache_write(uint32_t data, uint32_t addr);

void d_cache_fill(uint32_t addr);

void d_cache_process();

void write_buffer_fill(uint16_t idx, uint32_t base_addr);

void write_buffer2memory(uint16_t idx);

typedef struct i_cache_t{
	uint16_t index;
	uint32_t data;
	uint32_t tag;
	uint8_t valid;
	uint8_t block_offset;
	uint8_t byte_offset;
	uint32_t block[8*(4+2)];
	uint16_t early_start_ready;
	//uint32_t *block;
}i_cache;

typedef struct d_cache_t{
	uint16_t index;
	uint32_t data;
	uint32_t tag;
	uint8_t valid;
	uint8_t block_offset;
	uint8_t byte_offset;
	uint32_t block[16*(4+2)];
	//uint32_t *block;
	uint8_t WT;
	uint8_t dirty[64];
	uint32_t write_buffer[64*2];
}d_cache;

/******************************************************
			   cache structure
	--------------------------------------
	|valid| tag  |data0 |data1 |...
	--------------------------------------
	block0|block1|block2|block3|block4|...
******************************************************/

typedef struct CACHE_REQUEST_T{
	uint8_t request;		//request signal. 1:read, 2:write
	uint8_t ready;	//cache ready signal
	uint32_t addr;			//address to read/write
	uint32_t data_O;		//data to write back to cache/memory
	uint32_t data_I;		//data to receive from cache/memory
}cache_request;

#endif /* CACHE_H_ */
