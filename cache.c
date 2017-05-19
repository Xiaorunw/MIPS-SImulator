/*
 * cache.c
 *
 *  Created on: Mar 24, 2017
 *      Author: Yiming Wang
 */
#include <stdlib.h>
#include "msp.h"
#include "cache.h"
#include "Load_Program.h"

i_cache I_cache;
d_cache D_cache;
uint8_t row_size;
uint64_t i_hit;
uint64_t i_miss;
uint64_t d_hit;
cache_request i_cache_request_handler;
cache_request d_cache_request_handler;
uint8_t i_cache_state=1;
uint8_t fill_first_line;
uint64_t fill_start_clock_counter;
uint8_t fill_index;
uint8_t lw_first_time=1;

extern uint32_t $PC;				//Program counter
extern unsigned int memory[memory_size];
extern uint64_t clock_counter;		//system clock counter
extern uint64_t read_memory_counter;//read memory counter
extern uint8_t stall;

void Cache_process(){
	i_cache_process();
	//d_cache_process();
}

void i_cache_process(){
	switch (i_cache_state){
	/*case 0://Idle state
		i_cache_request_handler.ready = 0;
		if(i_cache_request_handler.request == 1){
			i_cache_state = 1;	//if request, switch to Compare Tag state
			clock_counter--;	//state machine doesn't waste cc
		}
		break;*/
	case 1://Compare Tag state
		if (i_cache_read() == 1){
			i_cache_request_handler.ready = 1; //ready to give value back to CPU.
			//i_cache_state = 0;
		}
		else {
			i_cache_state = 2;	//if needed, jump to Memory state
			i_cache_request_handler.ready = 0;
			i_cache_fill($PC);
		}
		break;
	case 2:
		if (i_cache_fill($PC) == 1) i_cache_state = 1;
		break;
	}
}

void Cache_Initialization(uint16_t i_cache_size, uint16_t d_cache_size, uint16_t block_size, uint8_t write_through){
	i_cache_config(i_cache_size,block_size);
	d_cache_config(d_cache_size,block_size,write_through);
}

void i_cache_config(uint16_t cache_size,uint16_t block_size){
	uint16_t size_index;
	size_index = cache_size/4/block_size;
	row_size = block_size+2;
	//I_cache.block = (uint32_t *)malloc(size_index*(row_size)*sizeof(uint32_t));
	I_cache.block_offset=block_size;
	I_cache.index=size_index;
}

uint32_t i_cache_read(){
	uint16_t idx;
		/**************************************************
			             cache structure
			------------------------------------------
			|valid| tag  |data0 |data1 |data2 |data3 |...
			------------------------------------------
			block0|block1|block2|block3|block4|block5|...
		**************************************************/
		uint16_t ofs;
		ofs = $PC % I_cache.block_offset;
		idx = ($PC / I_cache.block_offset) % I_cache.index;

		if(I_cache.block[idx*row_size+0] == 1){			//valid
			if (I_cache.block[idx*row_size+1] == (($PC/ I_cache.block_offset) / I_cache.index)){		//Tag matches. Fetch instructiuon from the cache
				i_hit ++;
				stall=0;
				i_cache_request_handler.data_O = I_cache.block[idx*row_size+2+ofs];
				return 1;
			}
			else{														//Tag miss. Replace this cache block.
				fill_start_clock_counter = clock_counter-1;
				i_miss++;
				return 0;
			}
		}
		else{											//not valid
			fill_start_clock_counter = clock_counter-1;
			i_miss++;
			return 0;
		}
}

uint8_t i_cache_fill(uint32_t current_PC){
	uint16_t idx;
		/**************************************************
			             cache structure
			------------------------------------------
			|valid| tag  |data0 |data1 |data2 |data3 |...
			------------------------------------------
			block0|block1|block2|block3|block4|block5|...
		**************************************************/
		uint16_t ofs;
		ofs = current_PC % I_cache.block_offset;
		idx = (current_PC / I_cache.block_offset) % I_cache.index;

		uint32_t PC_temp;
		PC_temp = current_PC - ofs;
		if(fill_first_line == 0){								//Fill the 1st line
			if((clock_counter-fill_start_clock_counter)>=7){	//Clock increment 8 cycles.
				I_cache.block[idx*row_size+2] = memory[PC_temp];
				fill_start_clock_counter = clock_counter;		//renew fill start reference
				fill_first_line = 1;
				if(I_cache.block_offset==1) {
					I_cache.block[idx*row_size+0] = 1;
					I_cache.block[idx*row_size+1] = (current_PC / I_cache.block_offset) / I_cache.index;
					fill_first_line = 0;
					return 1;			//just one line
				}
			}
		}
		else{													//Fill the rest lines
			if(I_cache.block_offset>1){
				if(fill_index < I_cache.block_offset-1){
					if((clock_counter-fill_start_clock_counter)>=2){
						I_cache.block[idx*row_size+3+fill_index] = memory[PC_temp+fill_index+1];
						fill_index ++;
						fill_start_clock_counter = clock_counter;		//renew fill start reference
					}
				}
				//if(fill_index == I_cache.block_offset-1) {
				else{
					I_cache.block[idx*row_size+0] = 1;
					I_cache.block[idx*row_size+1] = (current_PC / I_cache.block_offset) / I_cache.index;
					fill_first_line = 0;
					fill_index = 0;
					return 1;
				}
			}
		}
		stall = 1;
		return 0;
}

void d_cache_config(uint16_t cache_size,uint16_t block_size, uint8_t WT){
	uint16_t size_index;
	size_index = cache_size/4/block_size;
	row_size = block_size+2;
	//D_cache.block = (uint32_t *)malloc(size_index*(row_size)*sizeof(uint32_t));
	D_cache.block_offset=block_size;
	D_cache.index=size_index;
	D_cache.WT=WT;
	//D_cache.write_buffer = (uint32_t *)malloc(block_size*sizeof(uint32_t));
}

uint32_t d_cache_read(uint32_t addr){
	read_memory_counter ++;
	uint16_t idx;
		/**************************************************
			             cache structure
			------------------------------------------
			|valid| tag  |data0 |data1 |data2 |data3 |...
			------------------------------------------
			block0|block1|block2|block3|block4|block5|...
		**************************************************/
		uint16_t ofs;
		ofs = addr % D_cache.block_offset;
		idx = (addr / D_cache.block_offset) % D_cache.index;

	if(D_cache.WT){								//write though
		if(D_cache.block[idx*row_size+0] == 1){			//valid
			if (D_cache.block[idx*row_size+1] == ((addr/ D_cache.block_offset) / D_cache.index)){		//Tag matches. Fetch instructiuon from the cache
				clock_counter ++;
				d_hit ++;
				return D_cache.block[idx*row_size+2+ofs];
			}
			else{														//Tag miss. Replace this cache block.
				d_cache_fill(addr);
				return D_cache.block[idx*row_size+2+ofs];
			}
		}
		else{											//not valid
			d_cache_fill(addr);
			return D_cache.block[idx*row_size+2+ofs];
		}
	}
	else{										//write back
		if(D_cache.block[idx*row_size+0] == 1){			//valid
			if (D_cache.block[idx*row_size+1] == ((addr/ D_cache.block_offset) / D_cache.index)){		//Tag matches. Fetch instructiuon from the cache
				clock_counter ++;
				d_hit ++;
				return D_cache.block[idx*row_size+2+ofs];
			}
			else{														//Tag miss. Replace this cache block.
				d_cache_fill(addr);
				write_buffer2memory(idx);
				return D_cache.block[idx*row_size+2+ofs];
			}
		}
		else{											//not valid
			d_cache_fill(addr);
			return D_cache.block[idx*row_size+2+ofs];
		}
	}
}

void d_cache_write(uint32_t data, uint32_t addr){
	uint16_t idx;
	if(D_cache.WT == 1){					//write through
			uint16_t ofs;
			ofs = addr % D_cache.block_offset;
			idx = (addr / D_cache.block_offset) % D_cache.index;
			if (D_cache.block[idx*row_size+1] == ((addr/ D_cache.block_offset) / D_cache.index)){		//Tag matches. Write through cache
				D_cache.block[idx*row_size+2+ofs] = data;
				memory[addr] = data;
				clock_counter += 6+2*(D_cache.block_offset-1);
			}
			else{																//Tag miss
				d_cache_fill(addr);
				D_cache.block[idx*row_size+2+ofs] = data;
				memory[addr] = data;
				clock_counter += 6+2*(D_cache.block_offset-1);
			}
	}
	else{									//write back
			uint16_t ofs;
			ofs = addr % D_cache.block_offset;
			idx = (addr / D_cache.block_offset) % D_cache.index;
			if (D_cache.block[idx*row_size+0] == 1){	//valid
				if (D_cache.block[idx*row_size+1] == ((addr/ D_cache.block_offset) / D_cache.index)){		//Tag matches. Write through cache
					D_cache.block[idx*row_size+2+ofs] = data;
					D_cache.dirty[idx] = 1;
					write_buffer_fill(idx, addr-ofs);
				}
				else{																						//Tag miss.
					write_buffer2memory(idx);					//write back to memory
					d_cache_fill(addr);
					D_cache.block[idx*row_size+2+ofs] = data;
					write_buffer_fill(idx, addr-ofs);
				}
			}
			else{										//not valid
				d_cache_fill(addr);
				D_cache.block[idx*row_size+2+ofs] = data;
				write_buffer_fill(idx, addr-ofs);
			}
	}
}

void d_cache_fill(uint32_t addr){
	clock_counter += 8+2*(I_cache.block_offset-1);
	uint16_t idx;
		/**************************************************
			             cache structure
			------------------------------------------
			|valid| tag  |data0 |data1 |data2 |data3 |...
			------------------------------------------
			block0|block1|block2|block3|block4|block5|...
		**************************************************/
		uint16_t ofs;
		ofs = addr % D_cache.block_offset;
		idx = (addr / D_cache.block_offset) % D_cache.index;
		//idx--;
		D_cache.block[idx*row_size+0] = 1;
		D_cache.block[idx*row_size+1] = (addr / D_cache.block_offset) / D_cache.index;

		uint8_t i;
		uint32_t addr_temp;
		addr_temp = addr - ofs;
		for (i=0;i<D_cache.block_offset;i++){
			D_cache.block[idx*row_size+2+i] = memory[addr_temp];
			addr_temp++;
		}
}

void write_buffer_fill(uint16_t idx, uint32_t base_addr){
	uint8_t i;
	for(i=0;i<D_cache.block_offset;i++){
		D_cache.write_buffer[idx*D_cache.block_offset*2+i*2] = base_addr+i;
		D_cache.write_buffer[idx*D_cache.block_offset*2+i*2+1] = D_cache.block[idx*row_size+2+i];
	}
}

void write_buffer2memory(uint16_t idx){
	uint8_t i;
	uint32_t addr_temp;
	for (i=0;i<D_cache.block_offset;i++){
		addr_temp = D_cache.write_buffer[idx*D_cache.block_offset*2+i*2];
		memory[addr_temp] = D_cache.write_buffer[idx*D_cache.block_offset*2+i*2+1];
	}
	clock_counter += 6+2*D_cache.block_offset;
}

