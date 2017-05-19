//*****************************************************************************
//
// MSP432 main.c template - Empty main
//
//****************************************************************************

#include "msp.h"
#include "Pipeline.h"
#include "Load_Program.h"
#include "cache.h"

void debug_node(uint32_t node);
void debug_nodes();

extern uint64_t clock_counter;
extern uint32_t $PC;				//Program counter
extern uint32_t Reg_file[32];		//CPU register file
uint32_t instruction_num;
extern unsigned int memory[memory_size];

extern uint8_t i_cache_state;

void main(void){
	//Initialization
    WDTCTL = WDTPW | WDTHOLD;           // Stop watchdog timer
	//load_instructions();
	Initialize_Simulation_Memory();
	Pipeline_Initialization();			//Initialize clock counter and $PC
	Cache_Initialization(128,256,4,1);
    CSKEY = 0x695A;
    CSCTL0 = DCORES;
    CSCTL0 |= DCORSEL_4;
    CSCTL1 |= DIVS_3|DIVHS_3;
    //Infinite loop
    while(1){
    	Cache_process();
    	Advance_pipeline();
    	debug_nodes();
    }
}

void debug_nodes(){
	//debug_node(30);		//bubble sort entry
	//debug_node(43);
	debug_node(134);
	//debug_node(142);
   	debug_node(140);	//bubble sort end
    debug_node(144);
    //debug_node(94);
}

void debug_node(uint32_t node){
   	//if (clock_counter == node){
   	if (($PC == node)){
   		__delay_cycles(10);
    }
}
