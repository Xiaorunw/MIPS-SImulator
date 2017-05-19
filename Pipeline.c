/*
 * Pipeline.c
 *
 *  Created on: Mar 7, 2017
 *      Author: Yiming Wang
 */
#include <stdio.h>
#include "msp.h"
#include "Pipeline.h"
#include "Load_Program.h"
#include "cache.h"

IFID_Reg IFID_REG_shadow;
IDEX_Reg IDEX_REG_shadow;
EXMEM_Reg EXMEM_REG_shadow;
MEMWB_Reg MEMWB_REG_shadow;
IFID_Reg IFID_REG;
IDEX_Reg IDEX_REG;
EXMEM_Reg EXMEM_REG;
MEMWB_Reg MEMWB_REG;
uint64_t clock_counter;		//system clock counter
uint32_t $PC;				//Program counter
uint32_t Reg_file[32];		//CPU register file
uint64_t IF_counter;
uint64_t read_memory_counter;
uint8_t lw_stall_state;
uint32_t $PC_br;
uint8_t br;
uint8_t stall;
//extern uint32_t memory_instruction[INS_LENGTH];
//extern uint32_t memory_data[256];
extern uint32_t memory[memory_size];
extern i_cache I_cache ;
extern uint64_t i_hit;
extern uint64_t i_miss;
extern uint64_t d_hit;
extern cache_request i_cache_request_handler;
extern cache_request d_cache_request_handler;
extern i_cache I_cache;
extern uint8_t i_cache_state;

//Advance_clock:simulate one clock sysle.
//Every pipeline stage finish its process
//and pass the result in its result:shadow registers
//Then the clock counter adds one.
void Advance_pipeline(){
    	if(!stall){
		Pipeline_IF();
		Pipeline_EX();
		Pipeline_ID();
		Pipeline_MEM();
		Pipeline_WB();
		move_shadow_to_reg();
		PC_increment(i_cache_request_handler.data_O);
    	}
    	clock_counter++;
}

void PC_increment(uint32_t machine_code){
	uint16_t opcode;
	opcode = (uint8_t)((0xFC000000&machine_code)>>26);
	if((opcode!=0x23)&(opcode!=0x24)&(opcode!=0x20)){		//if it's not load instruction
		$PC ++;
	}

	if(lw_stall_state==1){
		machine_code = 0;		//clear the second load instruction
		$PC ++;
		lw_stall_state = 0;
	}
	else if(((opcode==0x23)|(opcode==0x24)|(opcode==0x20))&(!lw_stall_state)){
		lw_stall_state=1;
	}
	if((br==1)&i_cache_request_handler.ready){
		$PC = $PC_br;
		br=0;
	}
}

//Seperate the input machine code(stored in memory array)
//into Opcode, Rs, Rd, Rt, Imm, shamt, funct
void Pipeline_IF(){
	uint32_t machine_code;
	if ($PC < 190){
		machine_code = memory[$PC];	//fetch instructions if not the end
		//machine_code = i_cache_read();
		i_cache_request_handler.request = 1;	//send request to cache
		if(i_cache_request_handler.ready){
			machine_code = i_cache_request_handler.data_O;
			//PC_increment(machine_code);
		}
		else machine_code = 0;
		 if(machine_code == 0x00000008){
			float CPI = (float)clock_counter/(i_hit+i_miss);
			printf("CPI: %f\n",CPI);
			float i_hit_rate = (float)i_hit/(i_hit+i_miss);
			printf("i hit rate: %f\n",i_hit_rate);
			float d_hit_rate = (float)d_hit/read_memory_counter;
			printf("d hit rate: %f\n",d_hit_rate);
			__delay_cycles(10);
		}
	}
	else{ machine_code = 0x00000000;}				//if the end of .text, do nothing
	IFID_REG_shadow.Opcode = (uint8_t)((0xFC000000&machine_code)>>26);
	IFID_REG_shadow.Rs = (uint8_t)((0x03E00000&machine_code)>>21);
	IFID_REG_shadow.Rt = (uint8_t)((0x001F0000&machine_code)>>16);
	IFID_REG_shadow.Rd = (uint8_t)((0x0000F800&machine_code)>>11);
	//IFID_REG_shadow.Rt = (uint8_t)((0x000007C0&machine_code)>>6);
	IFID_REG_shadow.funct = (uint8_t)((0x0000003F&machine_code));
	IFID_REG_shadow.imm = 	(uint16_t)((0x0000FFFF&machine_code));
	IFID_REG_shadow.jumpaddress = (uint32_t)((0x03FFFFFF&machine_code));
	IFID_REG_shadow.shamt = (uint8_t)((0x000007C0&machine_code)>>6);
	IFID_REG_shadow.PCplus4 = $PC+1;
}

void Pipeline_ID(){
	switch (IFID_REG.Opcode){
	/****************************************************************************/
	case 0b000000:					//R type
		//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];	//store Rs value
		//IDEX_REG_shadow.RegRtValue = Reg_file[IFID_REG.Rt];	//store Rt value
		IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;	//store Rs index
		IDEX_REG_shadow.RegRtValue = IFID_REG.Rt;	//store Rt index
		IDEX_REG_shadow.RegRd = IFID_REG.Rd;				//write back to Rd
		IDEX_REG_shadow.ALUSrc = 0b0;
		IDEX_REG_shadow.ALUOp = 0b10;
		IDEX_REG_shadow.RegDst = 0b1;
		IDEX_REG_shadow.Branch = 0;
		IDEX_REG_shadow.MemRead = 0;
		IDEX_REG_shadow.MemWrite = 0;
		IDEX_REG_shadow.MemToReg = 0;
		IDEX_REG_shadow.RegWrite = 1;
		IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;			//pass PCnext
		switch(IFID_REG.funct){
		case 0x20:										//add
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			break;
		case 0x21:										//addu
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			break;
		case 0b100010:										//sub
			IDEX_REG_shadow.ALUCtrl = 0b0110;
			break;
		case 0x23:										//subu
			IDEX_REG_shadow.ALUCtrl = 0b0110;
			break;
		case 0b100100:										//and
			IDEX_REG_shadow.ALUCtrl = 0b0000;
			break;
		case 0b100101:										//or
			IDEX_REG_shadow.ALUCtrl = 0b0001;
			break;
		case 0b101010:
			IDEX_REG_shadow.ALUCtrl = 0b0111;				//slt
			//IDEX_REG_shadow.RegRsValue = IDEX_REG_shadow.RegRsValue;
			//IDEX_REG_shadow.RegRtValue = IDEX_REG_shadow.RegRtValue;
			break;
		case 0x2b:
			IDEX_REG_shadow.ALUCtrl = 0b1111;				//sltu
			//IDEX_REG_shadow.RegRsValue = IDEX_REG_shadow.RegRsValue;
			//IDEX_REG_shadow.RegRtValue = IDEX_REG_shadow.RegRtValue;
			break;
		case 0b100111:
			IDEX_REG_shadow.ALUCtrl = 0b1100;				//nor
			break;
		case 0b000000:										//sll
			//IDEX_REG_shadow.MDUOp = 0b0101;
			IDEX_REG_shadow.ALUCtrl = 0b1000;				//pseudo ALUctrl signal
			IDEX_REG_shadow.shamt = IFID_REG.shamt;
			break;
		case 0b000010:
			//IDEX_REG_shadow.MDUOp = 0b0110;				//srl
			IDEX_REG_shadow.ALUCtrl = 0b1001;				//pseudo ALUctrl signal
			IDEX_REG_shadow.shamt = IFID_REG.shamt;
			break;
		case 0x03:
			//IDEX_REG_shadow.MDUOp = 0b0100;				//sra
			IDEX_REG_shadow.ALUCtrl = 0b1010;				//pseudo ALUctrl signal
			IDEX_REG_shadow.shamt = IFID_REG.shamt;
			break;
		case 0x08:											//jr
			IDEX_REG_shadow.ALUCtrl = 0b0011;
			//branch_flush();
			break;
		case 0x0b:											//movn
			IDEX_REG_shadow.ALUCtrl = 0b1110;
			break;
		case 0x0a:											//movz
			IDEX_REG_shadow.ALUCtrl = 0b1101;
			break;
		case 0x26:
			IDEX_REG_shadow.ALUCtrl = 0b1011;				//xor
			break;
/*		case 0x1a:
			IDEX_REG_shadow.MDUOp = 0b0111;				//div
			break;
		case 0x1b:
			IDEX_REG_shadow.MDUOp = 0b0001;				//divu
			break;
		case 0x10:
			IDEX_REG_shadow.RegRd = Hi;				//mfhi
			break;
		case 0x12:
			IDEX_REG_shadow.RegRd = Lo;				//mflo
			break;
		//case 0x1a:
			//IDEX_REG_shadow.MDUOp = 0b1100;				//mfc0
			//break;
		case 0x18:
			IDEX_REG_shadow.MDUOp = 0b0010;				//mult
			break;
		case 0x19:
			IDEX_REG_shadow.MDUOp = 0b0011;				//multu
			break;
		case 0x03:
			IDEX_REG_shadow.MDUOp = 0b0100;				//sra
			IDEX_REG_shadow.shamt = IFID_REG.shamt;
			break;*/
		}
		break;
	/****************************************************************************/
	case 0b000010:					//J type: j
		$PC = (0xFC000000&IFID_REG.PCplus4)+(IFID_REG.jumpaddress)-1;
		//branch_flush();
		break;
	case 0b000011:					//J type: jal
		$PC = (0xFC000000&IFID_REG.PCplus4)+(IFID_REG.jumpaddress)-1;
		Reg_file[31] = IFID_REG.PCplus4 ++;				//save ra
		//branch_flush();
		break;
	/****************************************************************************/
	default:						//I type
		//IDEX_REG_shadow.MDUOp = 0;							//if it's I type, don't use MDU
		;
		int32_t temp;
		int32_t temp1;
		switch(IFID_REG.Opcode){
		case 0x8:											//addi
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			IDEX_REG_shadow.ALUOp = 0b10;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
				IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
			}
			else{
				IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
			}
			IDEX_REG_shadow.ALUSrc = 0b1;
			break;
		case 0x9:											//addiu
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			IDEX_REG_shadow.ALUOp = 0b10;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
				IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
			}
			else{
				IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
			}
			IDEX_REG_shadow.ALUSrc = 0b1;
			break;
		case 0xC:											//andi
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b0000;
			IDEX_REG_shadow.ALUOp = 0b10;
			IDEX_REG_shadow.ALUSrc = 0b1;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			IDEX_REG_shadow.SignExtImm = IFID_REG.imm;
			break;
		case 0xD:											//ori
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b0001;
			IDEX_REG_shadow.ALUOp = 0b10;
			IDEX_REG_shadow.ALUSrc = 0b1;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			IDEX_REG_shadow.SignExtImm = IFID_REG.imm;
			break;
		case 0xA:											//slti
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b0111;
			IDEX_REG_shadow.ALUOp = 0b10;
			IDEX_REG_shadow.ALUSrc = 0b1;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			IDEX_REG_shadow.SignExtImm = IFID_REG.imm;
			break;
		case 0xB:											//sltiu
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b1111;
			IDEX_REG_shadow.ALUOp = 0b10;
			IDEX_REG_shadow.ALUSrc = 0b1;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			IDEX_REG_shadow.SignExtImm = IFID_REG.imm;
			break;
		case 0x23:											//lw
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			IDEX_REG_shadow.ALUOp = 0b00;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b1;
			IDEX_REG_shadow.MemToReg = 0b1;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
				IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
			}
			else{
				IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
			}
			IDEX_REG_shadow.ALUSrc = 0b1;
			/********Flush and refetch next ins: instert a stall*******/
			branch_flush();

			break;
		case 0x24:											//lbu
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			IDEX_REG_shadow.ALUOp = 0b00;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 3;
			IDEX_REG_shadow.MemToReg = 0b1;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
				IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
			}
			else{
				IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
			}
			IDEX_REG_shadow.ALUSrc = 0b1;
			/********Flush and refetch next ins: instert a stall*******/
			branch_flush();
			//$PC -= 1;
			break;
		case 0x20:											//lb
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			IDEX_REG_shadow.ALUOp = 0b00;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 3;
			IDEX_REG_shadow.MemToReg = 0b1;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
				IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
			}
			else{
				IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
			}
			IDEX_REG_shadow.ALUSrc = 0b1;
			/********Flush and refetch next ins: instert a stall*******/
			branch_flush();
			//$PC -= 1;
			break;
		case 0x25:											//lhu
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			IDEX_REG_shadow.ALUOp = 0b00;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 2;
			IDEX_REG_shadow.MemToReg = 0b1;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
				IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
			}
			else{
				IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
			}
			IDEX_REG_shadow.ALUSrc = 0b1;
			/********Flush and refetch next ins: instert a stall*******/
			branch_flush();
			//$PC -= 1;
			break;
		case 0x2b:											//sw
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			IDEX_REG_shadow.RegRtValue = IFID_REG.Rt;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			//IDEX_REG_shadow.RegRtValue = Reg_file[IFID_REG.Rt];
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			IDEX_REG_shadow.ALUOp = 0b00;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 0b1;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegWrite = 0b0;
			if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
				IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
			}
			else{
				IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
			}
			IDEX_REG_shadow.ALUSrc = 0b1;
			break;
		case 0x29:											//sh
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			IDEX_REG_shadow.RegRtValue = IFID_REG.Rt;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			//IDEX_REG_shadow.RegRtValue = Reg_file[IFID_REG.Rt];
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			IDEX_REG_shadow.ALUOp = 0b00;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 2;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegWrite = 0b0;
			if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
				IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
			}
			else{
				IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
			}
			IDEX_REG_shadow.ALUSrc = 0b1;
			break;
		case 0x28:											//sb
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			IDEX_REG_shadow.RegRtValue = IFID_REG.Rt;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			//IDEX_REG_shadow.RegRtValue = Reg_file[IFID_REG.Rt];
			IDEX_REG_shadow.ALUCtrl = 0b0010;
			IDEX_REG_shadow.ALUOp = 0b00;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 3;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegWrite = 0b0;
			if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
				IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
			}
			else{
				IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
			}
			IDEX_REG_shadow.ALUSrc = 0b1;
			break;
		case 0x4:											//beq
			//int32_t temp;
			temp = Reg_file[IFID_REG.Rs];
			if((IFID_REG.Rs == EXMEM_REG.WBReg)&(EXMEM_REG.WBReg!=0)&(EXMEM_REG.RegWrite==1)){
				temp = EXMEM_REG.ALUresult;	//forwarding:data hazard
			}
			if((IFID_REG.Rs == MEMWB_REG.WBReg)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.RegWrite==1)){
				temp = MEMWB_REG.ALUresult;					//from register to register
				if (MEMWB_REG.MemToReg){
					temp = MEMWB_REG.DataMemResult;			//from memory to register
				}
			}
			if((IFID_REG.Rs == EXMEM_REG_shadow.WBReg)&(EXMEM_REG_shadow.WBReg!=0)&(EXMEM_REG_shadow.RegWrite==1)){			//forwarding
				temp = EXMEM_REG_shadow.ALUresult;
			}

			//int32_t temp1;
			temp1 = Reg_file[IFID_REG.Rt];
			if((IFID_REG.Rt == EXMEM_REG.WBReg)&(EXMEM_REG.WBReg!=0)&(EXMEM_REG.RegWrite==1)){
				temp1 = EXMEM_REG.ALUresult;	//forwarding:data hazard
			}
			if((IFID_REG.Rt == MEMWB_REG.WBReg)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.RegWrite==1)){
				temp1 = MEMWB_REG.ALUresult;					//from register to registe
				if (MEMWB_REG.MemToReg){
					temp1 = MEMWB_REG.DataMemResult;			//from memory to register
				}
			}
			if((IFID_REG.Rt == EXMEM_REG_shadow.WBReg)&(EXMEM_REG_shadow.WBReg!=0)&(EXMEM_REG_shadow.RegWrite==1)){			//forwarding
				temp1 = EXMEM_REG_shadow.ALUresult;
			}

			if(temp == temp1){
				if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
					IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
				}
				else{
					IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
				}
				IDEX_REG_shadow.ALUSrc = 0b1;
				IDEX_REG_shadow.ALUCtrl = 0b0010;
				IDEX_REG_shadow.ALUOp = 0b01;
				IDEX_REG_shadow.Branch = 0b1;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
				IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
				IDEX_REG_shadow.RegDst = 0b0;
				IDEX_REG_shadow.RegWrite = 0b0;
				//branch_flush();					//Branch taken. Flush the instruction under the branch
			}
			else{
				IDEX_REG_shadow.Branch = 0;
				IDEX_REG_shadow.RegWrite = 0b0;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
			}
			break;
		case 0x5:											//bne
			//int32_t temp;
			temp = Reg_file[IFID_REG.Rs];

			if((IFID_REG.Rs == EXMEM_REG.WBReg)&(EXMEM_REG.WBReg!=0)&(EXMEM_REG.RegWrite==1)){
				temp = EXMEM_REG.ALUresult;	//forwarding:data hazard
			}
			if((IFID_REG.Rs == MEMWB_REG.WBReg)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.RegWrite==1)){
				temp = MEMWB_REG.ALUresult;					//from register to registe
				if (MEMWB_REG.MemToReg){
					temp = MEMWB_REG.DataMemResult;			//from memory to register
				}
			}
			if((IFID_REG.Rs == EXMEM_REG_shadow.WBReg)&(EXMEM_REG_shadow.WBReg!=0)&(EXMEM_REG_shadow.RegWrite==1)){			//forwarding
				temp = EXMEM_REG_shadow.ALUresult;
			}

			//int32_t temp1;
			temp1 = Reg_file[IFID_REG.Rt];
			if((IFID_REG.Rt == EXMEM_REG.WBReg)&(EXMEM_REG.WBReg!=0)&(EXMEM_REG.RegWrite==1)){
				temp1 = EXMEM_REG.ALUresult;	//forwarding:data hazard
			}
			if((IFID_REG.Rt == MEMWB_REG.WBReg)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.RegWrite==1)){
				temp1 = MEMWB_REG.ALUresult;					//from register to registe
				if (MEMWB_REG.MemToReg){
					temp1 = MEMWB_REG.DataMemResult;			//from memory to register
				}
			}
			if((IFID_REG.Rt == EXMEM_REG_shadow.WBReg)&(EXMEM_REG_shadow.WBReg!=0)&(EXMEM_REG_shadow.RegWrite==1)){			//forwarding
				temp1 = EXMEM_REG_shadow.ALUresult;
			}

			if(temp != temp1){
				if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
					IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
				}
				else{
					IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
				}
				IDEX_REG_shadow.ALUSrc = 0b1;
				IDEX_REG_shadow.ALUCtrl = 0b0010;
				IDEX_REG_shadow.ALUOp = 0b01;
				IDEX_REG_shadow.Branch = 0b1;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
				IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
				IDEX_REG_shadow.RegDst = 0b0;
				IDEX_REG_shadow.RegWrite = 0b0;
				//branch_flush();
			}
			else{
				IDEX_REG_shadow.Branch = 0;
				IDEX_REG_shadow.RegWrite = 0b0;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
			}
		}
		break;
		case 0x7:											//bgtz
			//if ((IDEX_REG.RegRd == IFID_REG.Rs)& !IDEX_REG.ALUSrc){
			//	branch_flush_current();					//if hazard, flush current branch
			//	ch_flush();
			//	$PC -= 1;
			//}
			//int32_t temp;
			temp = Reg_file[IFID_REG.Rs];
			if((IFID_REG.Rs == EXMEM_REG.WBReg)&(EXMEM_REG.WBReg!=0)&(EXMEM_REG.RegWrite==1)){
				temp = EXMEM_REG.ALUresult;	//forwarding:data hazard
			}
			if((IFID_REG.Rs == MEMWB_REG.WBReg)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.RegWrite==1)){
				temp = MEMWB_REG.ALUresult;					//from register to registe
				if (MEMWB_REG.MemToReg){
					temp = MEMWB_REG.DataMemResult;			//from memory to register
				}
			}
			if((IFID_REG.Rs == EXMEM_REG_shadow.WBReg)&(EXMEM_REG_shadow.WBReg!=0)&(EXMEM_REG_shadow.RegWrite==1)){			//forwarding
				temp = EXMEM_REG_shadow.ALUresult;
			}

			if((int32_t)temp > 0){
				if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
					IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
				}
				else{
					IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
				}
				IDEX_REG_shadow.ALUSrc = 0b1;
				IDEX_REG_shadow.ALUCtrl = 0b0010;
				IDEX_REG_shadow.ALUOp = 0b01;
				IDEX_REG_shadow.Branch = 0b1;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
				IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
				IDEX_REG_shadow.RegDst = 0b0;
				IDEX_REG_shadow.RegWrite = 0b0;
				//branch_flush();					//Branch taken. Flush the instruction under the branch
			}
			else{
				IDEX_REG_shadow.Branch = 0;
				IDEX_REG_shadow.RegWrite = 0b0;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
			}
			break;
		case 0x6:											//blez
			//int32_t temp;
			temp = Reg_file[IFID_REG.Rs];
			if((IFID_REG.Rs == EXMEM_REG.WBReg)&(EXMEM_REG.WBReg!=0)&(EXMEM_REG.RegWrite==1)){
				temp = EXMEM_REG.ALUresult;	//forwarding:data hazard
			}
			if((IFID_REG.Rs == MEMWB_REG.WBReg)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.RegWrite==1)){
				temp = MEMWB_REG.ALUresult;					//from register to registe
				if (MEMWB_REG.MemToReg){
					temp = MEMWB_REG.DataMemResult;			//from memory to register
				}
			}
			if((IFID_REG.Rs == EXMEM_REG_shadow.WBReg)&(EXMEM_REG_shadow.WBReg!=0)&(EXMEM_REG_shadow.RegWrite==1)){			//forwarding
				temp = EXMEM_REG_shadow.ALUresult;
			}

			if((int32_t)temp <= 0){
				if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
					IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
				}
				else{
					IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
				}
				IDEX_REG_shadow.ALUSrc = 0b1;
				IDEX_REG_shadow.ALUCtrl = 0b0010;
				IDEX_REG_shadow.ALUOp = 0b01;
				IDEX_REG_shadow.Branch = 0b1;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
				IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
				IDEX_REG_shadow.RegDst = 0b0;
				IDEX_REG_shadow.RegWrite = 0b0;

				//branch_flush();					//Branch taken. Flush the instruction under the branch
			}
			else{
				IDEX_REG_shadow.Branch = 0;
				IDEX_REG_shadow.RegWrite = 0b0;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
			}
			break;
		case 0x1:											//bltz
			//int32_t temp;
			temp = Reg_file[IFID_REG.Rs];
			if((IFID_REG.Rs == EXMEM_REG.WBReg)&(EXMEM_REG.WBReg!=0)&(EXMEM_REG.RegWrite==1)){
				temp = EXMEM_REG.ALUresult;	//forwarding:data hazard
			}
			if((IFID_REG.Rs == MEMWB_REG.WBReg)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.RegWrite==1)){
				temp = MEMWB_REG.ALUresult;					//from register to registe
				if (MEMWB_REG.MemToReg){
					temp = MEMWB_REG.DataMemResult;			//from memory to register
				}
			}
			if((IFID_REG.Rs == EXMEM_REG_shadow.WBReg)&(EXMEM_REG_shadow.WBReg!=0)&(EXMEM_REG_shadow.RegWrite==1)){			//forwarding
				temp = EXMEM_REG_shadow.ALUresult;
			}

			if((int32_t)temp < 0){
				if(IFID_REG.imm&0x8000){							//pass imm value. all I type instructions need imm value
					IDEX_REG_shadow.SignExtImm = 0xFFFF0000 + IFID_REG.imm;	//if negative, extend 1s
				}
				else{
					IDEX_REG_shadow.SignExtImm = IFID_REG.imm;				//if positive, extend 0s
				}
				IDEX_REG_shadow.ALUSrc = 0b1;
				IDEX_REG_shadow.ALUCtrl = 0b0010;
				IDEX_REG_shadow.ALUOp = 0b01;
				IDEX_REG_shadow.Branch = 0b1;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
				IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
				IDEX_REG_shadow.RegDst = 0b0;
				IDEX_REG_shadow.RegWrite = 0b0;
				//branch_flush();					//Branch taken. Flush the instruction under the branch
			}
			else{
				IDEX_REG_shadow.Branch = 0;
				IDEX_REG_shadow.RegWrite = 0b0;
				IDEX_REG_shadow.MemRead = 0b0;
				IDEX_REG_shadow.MemToReg = 0b0;
				IDEX_REG_shadow.MemWrite = 0b0;
			}
			break;
		case 0xE:											//xori
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.ALUCtrl = 0b1011;
			IDEX_REG_shadow.ALUOp = 0b10;
			IDEX_REG_shadow.ALUSrc = 0b1;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b0;
			IDEX_REG_shadow.RegRsValue = IFID_REG.Rs;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			IDEX_REG_shadow.SignExtImm = IFID_REG.imm;
			break;
		case 0x1F:									//seb
			IDEX_REG_shadow.RegRt = IFID_REG.Rt;
			IDEX_REG_shadow.RegRd = IFID_REG.Rd;
			IDEX_REG_shadow.ALUCtrl = 0b10000;
			IDEX_REG_shadow.ALUOp = 0b10;
			IDEX_REG_shadow.ALUSrc = 0b1;
			IDEX_REG_shadow.Branch = 0b0;
			IDEX_REG_shadow.MemRead = 0b0;
			IDEX_REG_shadow.MemToReg = 0b0;
			IDEX_REG_shadow.MemWrite = 0b0;
			IDEX_REG_shadow.PCnext = IFID_REG.PCplus4;
			IDEX_REG_shadow.RegDst = 0b1;
			//IDEX_REG_shadow.RegRsValue = Reg_file[IFID_REG.Rs];
			IDEX_REG_shadow.RegWrite = 0b1;
			IDEX_REG_shadow.SignExtImm = IFID_REG.imm;
			break;
	}
}

void Pipeline_EX(){
	EXMEM_REG_shadow.RegWrite = IDEX_REG.RegWrite;		//copy the unused registers
	EXMEM_REG_shadow.MemToReg = IDEX_REG.MemToReg;
	EXMEM_REG_shadow.Branch = IDEX_REG.Branch;
	EXMEM_REG_shadow.MemRead = IDEX_REG.MemRead;
	EXMEM_REG_shadow.MemWrite = IDEX_REG.MemWrite;

	if(IDEX_REG.RegDst){				//writeback to Rd or Rt
		EXMEM_REG_shadow.WBReg = IDEX_REG.RegRd;
	}
	else{
		EXMEM_REG_shadow.WBReg = IDEX_REG.RegRt;
	}

	if(IDEX_REG.ALUSrc){				//R type or I type:true-I type, false-R type
		int32_t temp = IDEX_REG.SignExtImm;
		int32_t temp1 = Reg_file[IDEX_REG.RegRsValue];
		if((IDEX_REG.RegRsValue == EXMEM_REG.WBReg)&(IDEX_REG.RegRsValue != 0)&(IDEX_REG.MemToReg == 0)&(EXMEM_REG.RegWrite==1)&(EXMEM_REG.WBReg!=0)){
			temp1 = EXMEM_REG.ALUresult;	//forwarding:data hazard
		}
		if((IDEX_REG.RegRsValue == MEMWB_REG.WBReg)&(IDEX_REG.RegRsValue != 0)&(MEMWB_REG.RegWrite==1)&(MEMWB_REG.WBReg!=0)){
			temp1 = MEMWB_REG.ALUresult;					//from register to registe
			if (MEMWB_REG.MemToReg){
				temp1 = MEMWB_REG.DataMemResult;			//from memory to register
			}
		}
		if(IDEX_REG.ALUOp == 0b10){						//I type: arithmatic
			switch(IDEX_REG.ALUCtrl){
			case 0b0000:					//andi
				EXMEM_REG_shadow.ALUresult = temp & temp1;
				break;
			case 0b0001:					//ori
				EXMEM_REG_shadow.ALUresult = temp | temp1;
				break;
			case 0b0010:					//addi
				EXMEM_REG_shadow.ALUresult = temp + temp1;
				break;
			case 0b0111:					//slti
				EXMEM_REG_shadow.ALUresult = temp > temp1;
				break;
			case 0b1111:					//sltiu
				EXMEM_REG_shadow.ALUresult = IDEX_REG.SignExtImm > (uint32_t)temp1;
				break;
			case 0b1011:					//xori
				EXMEM_REG_shadow.ALUresult = (temp|temp1)&((!temp)|(!temp1));
				break;
			case 0b10000:
				temp1 = Reg_file[IDEX_REG.RegRt];
				if((IDEX_REG.RegRt == EXMEM_REG.WBReg)&(IDEX_REG.RegRt != 0)&(IDEX_REG.MemToReg == 0)){
					temp1 = EXMEM_REG.ALUresult;	//forwarding:data hazard
				}
				else if((IDEX_REG.RegRt == MEMWB_REG.WBReg)&(IDEX_REG.RegRt != 0)){
					temp1 = MEMWB_REG.ALUresult;					//from register to register
					if (MEMWB_REG.MemToReg){
						temp1 = MEMWB_REG.DataMemResult;			//from memory to register
					}
				}

				if(temp1&0x80){							//seb
					EXMEM_REG_shadow.ALUresult = 0xFFFFFF00 + temp1;	//if negative, extend 1s
				}
				else{
					EXMEM_REG_shadow.ALUresult = temp1;				//if positive, extend 0s
				}
				break;
			}
		}
		else if(IDEX_REG.ALUOp == 0b00){						//I type: sw,lw
			if(!IDEX_REG.MemRead){			//sw
				/*if((IDEX_REG.RegRtValue == EXMEM_REG.WBReg)&(EXMEM_REG.WBReg!=0)&(EXMEM_REG.RegWrite==1)){
					EXMEM_REG_shadow.RegRtValue = MEMWB_REG.ALUresult;					//from register to register
				}*/
				if((IDEX_REG.RegRtValue == MEMWB_REG.WBReg)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.RegWrite==1)){
				//if((IDEX_REG.RegRtValue == EXMEM_REG.WBReg)&(EXMEM_REG.WBReg!=0)&(EXMEM_REG.RegWrite==1)){
					if (MEMWB_REG.MemToReg){
						EXMEM_REG_shadow.RegRtValue = MEMWB_REG.DataMemResult;			//from memory to register
					}
					else{EXMEM_REG_shadow.RegRtValue = MEMWB_REG.ALUresult;}					//from register to register
				}
				else{
				EXMEM_REG_shadow.RegRtValue = Reg_file[IDEX_REG.RegRtValue];}
				EXMEM_REG_shadow.RegRt = IDEX_REG.RegRtValue;
				EXMEM_REG_shadow.ALUresult = temp + temp1;
				EXMEM_REG_shadow.WBReg = 0;	//no WB
			}
			else{							//lw
				EXMEM_REG_shadow.ALUresult = temp + temp1;
				//$PC -= 1;
			}
		}
		else if(IDEX_REG.ALUOp == 0b01){						//I type: branch
			//EXMEM_REG_shadow.PCnext = IDEX_REG.SignExtImm + IDEX_REG.PCnext;
			$PC_br = (IDEX_REG.SignExtImm) + IDEX_REG.PCnext;
			br = 1;
			branch_flush();
		}
	}
	else{								//R type
		int32_t Rt = Reg_file[IDEX_REG.RegRtValue];
		int32_t Rs = Reg_file[IDEX_REG.RegRsValue];

		if((IDEX_REG.RegRsValue == MEMWB_REG.WBReg)&(IDEX_REG.RegRsValue != 0)&(MEMWB_REG.RegWrite==1)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.MemToReg)){
			Rs = MEMWB_REG.DataMemResult;			//from memory to register
		}
		else{
		if((IDEX_REG.RegRsValue == EXMEM_REG.WBReg)&(IDEX_REG.RegRsValue != 0)&(IDEX_REG.MemToReg == 0)&(EXMEM_REG.RegWrite==1)&(EXMEM_REG.WBReg!=0)){
			Rs = EXMEM_REG.ALUresult;
		}//forwarding
		if((IDEX_REG.RegRsValue == MEMWB_REG.WBReg)&(IDEX_REG.RegRsValue != 0)&(MEMWB_REG.RegWrite==1)&(MEMWB_REG.WBReg!=0)&(!MEMWB_REG.MemToReg)){
			Rs = MEMWB_REG.ALUresult;					//from register to register
			//if (MEMWB_REG.MemToReg){
			//	Rs = MEMWB_REG.DataMemResult;			//from memory to register
			//}
		}
		}

		if((IDEX_REG.RegRsValue == MEMWB_REG.WBReg)&(IDEX_REG.RegRsValue != 0)&(MEMWB_REG.RegWrite==1)&(MEMWB_REG.WBReg!=0)&(MEMWB_REG.MemToReg)){
			Rs = MEMWB_REG.DataMemResult;			//from memory to register
		}
		else{
		if((IDEX_REG.RegRtValue == EXMEM_REG.WBReg)&(IDEX_REG.RegRtValue != 0)&(IDEX_REG.MemToReg == 0)&(EXMEM_REG.RegWrite==1)&(EXMEM_REG.WBReg!=0)){
			Rt = EXMEM_REG.ALUresult;
		}//forwarding
		if((IDEX_REG.RegRtValue == MEMWB_REG.WBReg)&(IDEX_REG.RegRtValue != 0)&(MEMWB_REG.RegWrite==1)&(MEMWB_REG.WBReg!=0)&(!MEMWB_REG.MemToReg)){
			Rt = MEMWB_REG.ALUresult;					//from register to register
			if (MEMWB_REG.MemToReg){
				Rt = MEMWB_REG.DataMemResult;			//from memory to register
			}
		}
		}
		//uint32_t temp = IDEX_REG.RegRtValue;

		switch(IDEX_REG.ALUCtrl){
		case 0b0000:					//and
			//EXMEM_REG_shadow.ALUresult = temp & IDEX_REG.RegRsValue;
			EXMEM_REG_shadow.ALUresult = Rt & Rs;
			break;
		case 0b0001:					//or
			//EXMEM_REG_shadow.ALUresult = temp | IDEX_REG.RegRsValue;
			EXMEM_REG_shadow.ALUresult = Rt | Rs;
			break;
		case 0b0010:					//add
			//EXMEM_REG_shadow.ALUresult = temp + IDEX_REG.RegRsValue;
			EXMEM_REG_shadow.ALUresult = Rt + Rs;
			break;
		case 0b0110:					//sub
			//EXMEM_REG_shadow.ALUresult = IDEX_REG.RegRsValue - temp;
			EXMEM_REG_shadow.ALUresult = Rs - Rt;
			break;
		case 0b0111:					//slt
			//EXMEM_REG_shadow.ALUresult = IDEX_REG.RegRtValue > IDEX_REG.RegRsValue;
			EXMEM_REG_shadow.ALUresult = Rt > Rs;
			break;
		case 0b1111:					//sltu
			//EXMEM_REG_shadow.ALUresult = IDEX_REG.RegRtValue_u > IDEX_REG.RegRsValue_u;
			//EXMEM_REG_shadow.ALUresult = Rt_u > Rs_u;
			EXMEM_REG_shadow.ALUresult = (uint32_t)Rt > (uint32_t)Rs;
			break;
		case 0b1100:					//nor
			//EXMEM_REG_shadow.ALUresult = ~(temp | IDEX_REG.RegRsValue);
			EXMEM_REG_shadow.ALUresult = ~(Rt | Rs);
			break;
		case 0b1000:					//sll
			//EXMEM_REG_shadow.ALUresult = IDEX_REG.RegRtValue << IDEX_REG.shamt;
			EXMEM_REG_shadow.ALUresult = Rt << IDEX_REG.shamt;
			break;
		case 0b1001:					//srl
			//EXMEM_REG_shadow.ALUresult = IDEX_REG.RegRtValue >> IDEX_REG.shamt;
			EXMEM_REG_shadow.ALUresult = Rt >> IDEX_REG.shamt;
			break;
		case 0b1010:					//sra
			//EXMEM_REG_shadow.ALUresult = (int8_t)IDEX_REG.RegRtValue >> IDEX_REG.shamt;
			EXMEM_REG_shadow.ALUresult = (int8_t)Rt >> IDEX_REG.shamt;
			break;
		case 0b1110:					//movn
			if (Rt != 0){EXMEM_REG_shadow.ALUresult = Rs;}
			else EXMEM_REG_shadow.RegWrite = 0;
		break;
		case 0b1101:					//movz
			if (Rt == 0){EXMEM_REG_shadow.ALUresult = Rs;}
			else EXMEM_REG_shadow.RegWrite = 0;
		break;
		case 0b1011:					//xori
			EXMEM_REG_shadow.ALUresult = (Rs|Rt)&((!Rs)|(!Rt));
		break;
		case 0b0011:					//jr
			$PC = Rs;
			branch_flush();
		break;
		}
	}
/*	if(IDEX_REG.MDUOp){					//R type: MDU used.
		switch(IDEX_REG.MDUOp){
		uint64_t temp_mult;
		case 0b0111:					//div
			EXMEM_REG_shadow.Lo = IDEX_REG.RegRsValue/IDEX_REG.RegRtValue;
			EXMEM_REG_shadow.Hi = IDEX_REG.RegRsValue%IDEX_REG.RegRtValue;
			break;
		case 0b0001:					//divu
			EXMEM_REG_shadow.Lo = IDEX_REG.RegRsValue/IDEX_REG.RegRtValue;
			EXMEM_REG_shadow.Hi = IDEX_REG.RegRsValue%IDEX_REG.RegRtValue;
			break;
		case 0b0101:					//sll
			EXMEM_REG_shadow.ALUresult = IDEX_REG.RegRtValue << IDEX_REG.shamt;
			break;
		case 0b0110:					//srl
			EXMEM_REG_shadow.ALUresult = IDEX_REG.RegRtValue >> IDEX_REG.shamt;
			break;
		case 0b0100:					//sra
			EXMEM_REG_shadow.ALUresult = (int8_t)IDEX_REG.RegRtValue >> IDEX_REG.shamt;
			break;
		case 0b0010:					//mult
			temp_mult = IDEX_REG.RegRsValue*IDEX_REG.RegRtValue;
			EXMEM_REG_shadow.Lo = temp_mult;
			EXMEM_REG_shadow.Hi = temp_mult >>32;
			break;
		case 0b0011:					//multu
			temp_mult = IDEX_REG.RegRsValue*IDEX_REG.RegRtValue;
			EXMEM_REG_shadow.Lo = temp_mult;
			EXMEM_REG_shadow.Hi = temp_mult >>32;
			break;
		}
	}*/
}

void Pipeline_MEM(){
	MEMWB_REG_shadow.WBReg = EXMEM_REG.WBReg;
	MEMWB_REG_shadow.ALUresult = EXMEM_REG.ALUresult;
	MEMWB_REG_shadow.MemToReg = EXMEM_REG.MemToReg;
	MEMWB_REG_shadow.RegWrite = EXMEM_REG.RegWrite;

	/*uint32_t temp1;
	temp1 = EXMEM_REG.RegRtValue;
	if((EXMEM_REG.RegRt == MEMWB_REG.WBReg)&(EXMEM_REG.RegRt != 0)){
		if (MEMWB_REG.MemToReg){
			temp1 = MEMWB_REG.DataMemResult;			//from memory to register
		}
		else{temp1 = MEMWB_REG.ALUresult;}					//from register to register
	}*/

	switch (EXMEM_REG.MemRead){
	case 1:											//lw
		//MEMWB_REG_shadow.DataMemResult = memory[EXMEM_REG.ALUresult>>2];
		MEMWB_REG_shadow.DataMemResult = d_cache_read(EXMEM_REG.ALUresult>>2);
		break;
	case 2:											//lhu
		if(EXMEM_REG.ALUresult % 2 == 0){
			//MEMWB_REG_shadow.DataMemResult = 0x0000FFFF&memory[EXMEM_REG.ALUresult>>2];
			MEMWB_REG_shadow.DataMemResult = 0x0000FFFF&d_cache_read(EXMEM_REG.ALUresult>>2);
		}
		else if (EXMEM_REG.ALUresult % 2 == 1){
			//MEMWB_REG_shadow.DataMemResult = (0xFFFF0000&memory[EXMEM_REG.ALUresult>>2])>>16;
			MEMWB_REG_shadow.DataMemResult = (0xFFFF0000&d_cache_read(EXMEM_REG.ALUresult>>2))>>16;
		}
		break;
	case 3:											//lbu
		if(EXMEM_REG.ALUresult % 4 == 0){
			//MEMWB_REG_shadow.DataMemResult = 0x000000FF&memory[EXMEM_REG.ALUresult>>2];
			MEMWB_REG_shadow.DataMemResult = (0x000000FF&d_cache_read(EXMEM_REG.ALUresult>>2));
		}
		else if(EXMEM_REG.ALUresult % 4 == 1){
			//MEMWB_REG_shadow.DataMemResult = (0x0000FF00&memory[EXMEM_REG.ALUresult>>2])>>8;
			MEMWB_REG_shadow.DataMemResult = (0x0000FF00&d_cache_read(EXMEM_REG.ALUresult>>2))>>8;
		}
		else if(EXMEM_REG.ALUresult % 4 == 2){
			//MEMWB_REG_shadow.DataMemResult = (0x00FF0000&memory[EXMEM_REG.ALUresult>>2])>>16;
			MEMWB_REG_shadow.DataMemResult = (0x00FF0000&d_cache_read(EXMEM_REG.ALUresult>>2))>>16;
		}
		else if(EXMEM_REG.ALUresult % 4 == 3){
			//MEMWB_REG_shadow.DataMemResult = (0xFF000000&memory[EXMEM_REG.ALUresult>>2])>>24;
			MEMWB_REG_shadow.DataMemResult = (0xFF000000&d_cache_read(EXMEM_REG.ALUresult>>2))>>24;
		}
		break;
	}
	switch (EXMEM_REG.MemWrite){												//<<2 is for word alligned
	uint16_t hw_temp;
	uint32_t b_temp;

	case 1:											//sw
		//memory[EXMEM_REG.ALUresult>>2] = EXMEM_REG.RegRtValue;
		d_cache_write(EXMEM_REG.RegRtValue,EXMEM_REG.ALUresult>>2);
		break;
	case 2:											//sh
		if(EXMEM_REG.ALUresult % 2 == 0){
			hw_temp = 0xFFFF0000&memory[EXMEM_REG.ALUresult>>2];
			//memory[EXMEM_REG.ALUresult>>2] = EXMEM_REG.RegRtValue | hw_temp;
			d_cache_write(EXMEM_REG.RegRtValue | hw_temp,EXMEM_REG.ALUresult>>2);
		}
		else if (EXMEM_REG.ALUresult % 2 == 1){
			hw_temp = 0x0000FFFF&memory[EXMEM_REG.ALUresult>>2];
			//memory[EXMEM_REG.ALUresult>>2] = EXMEM_REG.RegRtValue<<16 | hw_temp;
			d_cache_write(EXMEM_REG.RegRtValue<<16 | hw_temp,EXMEM_REG.ALUresult>>2);
		}
		break;
	case 3:											//sb
		if(EXMEM_REG.ALUresult % 4 == 0){
			b_temp = 0xFFFFFF00&memory[EXMEM_REG.ALUresult>>2];
			//memory[EXMEM_REG.ALUresult>>2] = EXMEM_REG.RegRtValue | b_temp;
			d_cache_write(EXMEM_REG.RegRtValue | b_temp,EXMEM_REG.ALUresult>>2);
		}
		else if(EXMEM_REG.ALUresult % 4 == 1){
			b_temp = 0xFFFF00FF&memory[EXMEM_REG.ALUresult>>2];
			//memory[EXMEM_REG.ALUresult>>2] = EXMEM_REG.RegRtValue<<8 | b_temp;
			d_cache_write(EXMEM_REG.RegRtValue<<8 | b_temp,EXMEM_REG.ALUresult>>2);
		}
		else if(EXMEM_REG.ALUresult % 4 == 2){
			b_temp = 0xFF00FFFF&memory[EXMEM_REG.ALUresult>>2];
			//memory[EXMEM_REG.ALUresult>>2] = EXMEM_REG.RegRtValue <<16 | b_temp;
			d_cache_write(EXMEM_REG.RegRtValue<<16 | b_temp,EXMEM_REG.ALUresult>>2);
		}
		else if(EXMEM_REG.ALUresult % 4 == 3){
			b_temp = 0x00FFFFFF&memory[EXMEM_REG.ALUresult>>2];
			//memory[EXMEM_REG.ALUresult>>2] = EXMEM_REG.RegRtValue<<24 | b_temp;
			d_cache_write(EXMEM_REG.RegRtValue<<24 | b_temp,EXMEM_REG.ALUresult>>2);
		}
		break;
	}
}

void Pipeline_WB(){
	if(MEMWB_REG.RegWrite){
		if(MEMWB_REG.MemToReg){
			Reg_file[MEMWB_REG.WBReg] = MEMWB_REG.DataMemResult;
		}
		else{
			Reg_file[MEMWB_REG.WBReg] = MEMWB_REG.ALUresult;
		}
		if(MEMWB_REG.WBReg == 0){Reg_file[MEMWB_REG.WBReg] = 0;}
	}
}

void move_shadow_to_reg(){
	IFID_REG = IFID_REG_shadow;
	IDEX_REG = IDEX_REG_shadow;
	EXMEM_REG = EXMEM_REG_shadow;
	MEMWB_REG = MEMWB_REG_shadow;
}

void Pipeline_Initialization(){
	clock_counter = 0;
	//$PC = 0;
	Reg_file[29] = memory[0];	//$sp = memory[0]
	Reg_file[30] = memory[1];	//$fp = memory[1]
	$PC = memory[5];
	//for testing
	//Reg_file[31] = 1;
	//Reg_file[18] = 2;
	//Reg_file[17] = 3;
	//Reg_file[16] = 4;
	//Reg_file[2] = 5;

}

void branch_flush(){
	IFID_REG_shadow.Opcode=0;
	IFID_REG_shadow.PCplus4=0;
	IFID_REG_shadow.Rd=0;
	IFID_REG_shadow.Rs=0;
	IFID_REG_shadow.Rt=0;
	IFID_REG_shadow.funct=0;
	IFID_REG_shadow.imm=0;
	IFID_REG_shadow.jumpaddress=0;
	IFID_REG_shadow.shamt=0;
}

void branch_flush_current(){
	IDEX_REG_shadow.ALUCtrl=0;
	IDEX_REG_shadow.ALUOp=0b10;
	IDEX_REG_shadow.ALUSrc=0;
	IDEX_REG_shadow.Branch=0;
	IDEX_REG_shadow.MemRead=0;
	IDEX_REG_shadow.MemToReg=0;
	IDEX_REG_shadow.MemWrite=0;
	IDEX_REG_shadow.SignExtImm=0;
	IDEX_REG_shadow.RegDst=0;
	IDEX_REG_shadow.RegRd=0;
	IDEX_REG_shadow.RegRs=0;
	IDEX_REG_shadow.RegRt=0;

}
