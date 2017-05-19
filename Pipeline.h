/*
 * Pipeline.h
 *
 *  Created on: Mar 7, 2017
 *      Author: Yiming Wang
 */

#ifndef PIPELINE_H_
#define PIPELINE_H_

#define INS_LENGTH 8
#define CACHE_SIZE 2048

typedef struct IFID{
	uint8_t Opcode;
	uint8_t Rs;
	uint8_t Rt;
	uint8_t Rd;
	uint8_t shamt;
	uint8_t funct;
	uint16_t imm;
	uint32_t jumpaddress;
	uint32_t PCplus4;
}IFID_Reg;

typedef struct IDEX{
	uint8_t RegWrite;
	uint8_t MemToReg;
	uint8_t Branch;
	uint8_t RegDst;
	uint8_t ALUCtrl;
	uint8_t ALUOp;
	uint8_t ALUSrc;
	uint8_t MemRead;
	uint8_t MemWrite;
	uint32_t PCnext;
	int32_t RegRsValue;
	//uint32_t RegRsValue_u;
	int32_t RegRtValue;
	//uint32_t RegRtValue_u;
	uint32_t SignExtImm;
	uint8_t shamt;
	uint8_t RegRs;
	uint8_t RegRt;
	uint8_t RegRd;
//	uint8_t MDUOp;
}IDEX_Reg;

typedef struct EXMEM{
	uint8_t RegWrite;
	uint8_t MemToReg;
	uint8_t Branch;
	uint8_t MemRead;
	uint8_t MemWrite;
	uint32_t PCnext;
	uint32_t ALUresult;
	uint8_t WBReg;
	uint32_t RegRtValue;
	uint8_t RegRt;
	//uint32_t Hi;
	//uint32_t Lo;
}EXMEM_Reg;

typedef struct MEMWB{
	uint8_t RegWrite;
	uint8_t MemToReg;
	uint32_t ALUresult;
	uint8_t WBReg;
	uint32_t DataMemResult;
}MEMWB_Reg;

void Advance_pipeline();
void PC_increment(uint32_t machine_code);
void Pipeline_IF();
void Pipeline_ID();
void Pipeline_EX();
void Pipeline_MEM();
void Pipeline_WB();
void move_shadow_to_reg();
void Pipeline_Initialization();
void branch_flush();

#endif /* PIPELINE_H_ */
