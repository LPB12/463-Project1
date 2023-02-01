#include "sim_pipe.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <iomanip>
#include <map>



//#define DEBUG

using namespace std;

//used for debugging purposes
static const char *reg_names[NUM_SP_REGISTERS] = {"PC", "NPC", "IR", "A", "B", "IMM", "COND", "ALU_OUTPUT", "LMD"};
static const char *stage_names[NUM_STAGES] = {"IF", "ID", "EX", "MEM", "WB"};
static const char *instr_names[NUM_OPCODES] = {"LW", "SW", "ADD", "ADDI", "SUB", "SUBI", "XOR", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "NOP"};

/* =============================================================

   HELPER FUNCTIONS

   ============================================================= */


/* converts integer into array of unsigned char - little indian */
inline void int2char(unsigned value, unsigned char *buffer){
	memcpy(buffer, &value, sizeof value);
}

/* converts array of char into integer - little indian */
inline unsigned char2int(unsigned char *buffer){
	unsigned d;
	memcpy(&d, buffer, sizeof d);
	return d;
}

/* implements the ALU operations */
unsigned alu(unsigned opcode, unsigned a, unsigned b, unsigned imm, unsigned npc){
	switch(opcode){
			case ADD:
				return (a+b);
			case ADDI:
				return(a+imm);
			case SUB:
				return(a-b);
			case SUBI:
				return(a-imm);
			case XOR:
				return(a ^ b);
			case LW:
			case SW:
				return(a + imm);
			case BEQZ:
			case BNEZ:
			case BGTZ:
			case BGEZ:
			case BLTZ:
			case BLEZ:
			case JUMP:
				return(npc+imm);
			default:	
				return (-1);
	}
}

/* =============================================================

   CODE PROVIDED - NO NEED TO MODIFY FUNCTIONS BELOW

   ============================================================= */

/* loads the assembly program in file "filename" in instruction memory at the specified address */
void sim_pipe::load_program(const char *filename, unsigned base_address){

   /* initializing the base instruction address */
   instr_base_address = base_address;

   PCR = instr_base_address;
   /* creating a map with the valid opcodes and with the valid labels */
   map<string, opcode_t> opcodes; //for opcodes
   map<string, unsigned> labels;  //for branches
   for (int i=0; i<NUM_OPCODES; i++)
	 opcodes[string(instr_names[i])]=(opcode_t)i;

   /* opening the assembly file */
   ifstream fin(filename, ios::in | ios::binary);
   if (!fin.is_open()) {
      cerr << "error: open file " << filename << " failed!" << endl;
      exit(-1);
   }

   /* parsing the assembly file line by line */
   string line;
   unsigned instruction_nr = 0;
   while (getline(fin,line)){
	// set the instruction field
	char *str = const_cast<char*>(line.c_str());

  	// tokenize the instruction
	char *token = strtok (str," \t");
	map<string, opcode_t>::iterator search = opcodes.find(token);
        if (search == opcodes.end()){
		// this is a label for a branch - extract it and save it in the labels map
		string label = string(token).substr(0, string(token).length() - 1);
		labels[label]=instruction_nr;
                // move to next token, which must be the instruction opcode
		token = strtok (NULL, " \t");
		search = opcodes.find(token);
		if (search == opcodes.end()) cout << "ERROR: invalid opcode: " << token << " !" << endl;
	}
	instr_memory[instruction_nr].opcode = search->second;

	//reading remaining parameters
	char *par1;
	char *par2;
	char *par3;
	switch(instr_memory[instruction_nr].opcode){
		case ADD:
		case SUB:
		case XOR:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].src2 = atoi(strtok(par3, "R"));
			break;
		case ADDI:
		case SUBI:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].immediate = strtoul (par3, NULL, 0); 
			break;
		case LW:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
			break;
		case SW:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src2 = atoi(strtok(NULL, "R"));
			break;
		case BEQZ:
		case BNEZ:
		case BLTZ:
		case BGTZ:
		case BLEZ:
		case BGEZ:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].label = par2;
			break;
		case JUMP:
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].label = par2;
		default:
			break;

	} 

	/* increment instruction number before moving to next line */
	instruction_nr++;
   }
   //reconstructing the labels of the branch operations
   int i = 0;
   while(true){
   	instruction_t instr = instr_memory[i];
	if (instr.opcode == EOP) break;
	if (instr.opcode == BLTZ || instr.opcode == BNEZ ||
            instr.opcode == BGTZ || instr.opcode == BEQZ ||
            instr.opcode == BGEZ || instr.opcode == BLEZ ||
            instr.opcode == JUMP
	 ){
		instr_memory[i].immediate = (labels[instr.label] - i - 1) << 2;
	}
        i++;
   }

}

/* writes an integer value to data memory at the specified address (use little-endian format: https://en.wikipedia.org/wiki/Endianness) */
void sim_pipe::write_memory(unsigned address, unsigned value){
	int2char(value,data_memory+address);
}

/* prints the content of the data memory within the specified address range */
void sim_pipe::print_memory(unsigned start_address, unsigned end_address){
	cout << "data_memory[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') <<  end_address << "]" << endl;
	for (unsigned i=start_address; i<end_address; i++){
		if (i%4 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": "; 
		cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
		if (i%4 == 3) cout << endl;
	} 
}

/* prints the values of the registers */
void sim_pipe::print_registers(){
        cout << "Special purpose registers:" << endl;
        unsigned i, s;
        for (s=0; s<NUM_STAGES; s++){
                cout << "Stage: " << stage_names[s] << endl;
                for (i=0; i< NUM_SP_REGISTERS; i++)
                        if ((sp_register_t)i != IR && (sp_register_t)i != COND && get_sp_register((sp_register_t)i, (stage_t)s)!=UNDEFINED) cout << reg_names[i] << " = " << dec <<  get_sp_register((sp_register_t)i, (stage_t)s) << hex << " / 0x" << get_sp_register((sp_register_t)i, (stage_t)s) << endl;
        }
        cout << "General purpose registers:" << endl;
        for (i=0; i< NUM_GP_REGISTERS; i++)
                if (get_gp_register(i)!=(int)UNDEFINED) cout << "R" << dec << i << " = " << get_gp_register(i) << hex << " / 0x" << get_gp_register(i) << endl;
}

/* initializes the pipeline simulator */
sim_pipe::sim_pipe(unsigned mem_size, unsigned mem_latency){
	data_memory_size = mem_size;
	data_memory_latency = mem_latency;
	data_memory = new unsigned char[data_memory_size];
	reset();
}
	
/* deallocates the pipeline simulator */
sim_pipe::~sim_pipe(){
	delete [] data_memory;
}

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */



//Define Counters

/* body of the simulator */
void sim_pipe::run(unsigned cycles){
	//Init simulator, update counters for number of instrs, # of clock cycles and so on

	/*clockCount = 0;


	//Init IF/ID Reg to 0
	IFID.pc = instr_base_address;
	IFID.npc = UNDEFINED;
	IFID.a = UNDEFINED;
	IFID.b = UNDEFINED;
    IFID.imm = UNDEFINED;
	IFID.ALUOut = UNDEFINED;
	IFID.cond = UNDEFINED;
	IFID.LMD = UNDEFINED;

	//Init ID/EX Reg to 0
	IDEX.pc = UNDEFINED;
	IDEX.npc = UNDEFINED;
	IDEX.a = UNDEFINED;
	IDEX.b = UNDEFINED;
    IDEX.imm = UNDEFINED;
	IDEX.ALUOut = UNDEFINED;
	IDEX.cond = UNDEFINED;
	IDEX.LMD = UNDEFINED;
	
	//Init EX/MEM Reg to 0
	EXMEM.pc = UNDEFINED;
	EXMEM.npc = UNDEFINED;
	EXMEM.a = UNDEFINED;
	EXMEM.b = UNDEFINED;
    EXMEM.imm = UNDEFINED;
	EXMEM.ALUOut = UNDEFINED;
	EXMEM.cond = UNDEFINED;
	EXMEM.LMD = UNDEFINED;
	
	//Init MEM/WB Reg to 0
	MEMWB.pc = UNDEFINED;
	MEMWB.npc = UNDEFINED;
	MEMWB.a = UNDEFINED;
	MEMWB.b = UNDEFINED;
    MEMWB.imm = UNDEFINED;
	MEMWB.ALUOut = UNDEFINED;
	MEMWB.cond = UNDEFINED;
	MEMWB.LMD = UNDEFINED;
	 */


    for(int i=0; (i < cycles || cycles == 0); i++) // represents 1 clock cycle
	{
		//WB stage
        if(k >= 4)
        {
            switch(MEMWB.IR.opcode) {
                case BEQZ:
                case BNEZ:
                case BLTZ:
                case BGTZ:
                case BLEZ:
                case BGEZ:
                case JUMP:
                    iCount++;
                    break;
                case ADD:
                case SUB:
                case XOR:
                case ADDI:
                case SUBI:
                    iCount++;
                    regFile[MEMWB.IR.dest] = MEMWB.ALUOut;
                    break;
                case LW:
                    if(memFlag == 0 || data_memory_latency == 0)
                    {
                        iCount++;
                        regFile[MEMWB.IR.dest] = MEMWB.LMD;
                    }
                    break;
                case SW:
                    if(memFlag == 0 || data_memory_latency == 0)
                    {
                        iCount++;
                    }
                    break;
                case EOP:
                    EOPflag = true;
                    break;
                case NOP:
                    break;
                default:
                    break;
            }

            if(EOPflag)
            {
                break;
            }
        }


		
		//MEM Stage

        if(k >= 3)
        {
            switch(EXMEM.IR.opcode) {
                case BEQZ:
                case BNEZ:
                case BLTZ:
                case BGTZ:
                case BLEZ:
                case BGEZ:
                case JUMP:
                case ADD:
                case SUB:
                case XOR:
                case ADDI:
                case SUBI:
                    MEMWB.IR = EXMEM.IR;
                    MEMWB.ALUOut = EXMEM.ALUOut;
                    MEMWB.LMD = UNDEFINED;
                    break;
                case LW:

                    if(memFlag == 0 && data_memory_latency != 0)
                    {
                        memFlag = data_memory_latency+1;
                        break;
                    }
                    else if(memFlag == 1 || data_memory_latency == 0)
                    {
                        MEMWB.IR = EXMEM.IR;
                        MEMWB.LMD = char2int(&data_memory[EXMEM.ALUOut]);
                        MEMWB.ALUOut = EXMEM.ALUOut;
                        break;
                    }
                    else
                    {
                        break;
                    }

                    break;
                case SW:
                    if(memFlag == 0 && data_memory_latency != 0)
                    {

                        memFlag = data_memory_latency+1;
                        break;
                    }
                    else if(memFlag == 1 || data_memory_latency == 0)
                    {
                        MEMWB.IR = EXMEM.IR;
                        write_memory(EXMEM.ALUOut, EXMEM.b);
                        MEMWB.ALUOut = EXMEM.ALUOut;
                        MEMWB.LMD = UNDEFINED;
                        break;
                    }
                    else
                    {
                        break;
                    }

                case EOP:
                    MEMWB.IR = EXMEM.IR;
                    MEMWB.ALUOut = UNDEFINED;
                    MEMWB.LMD = UNDEFINED;
                    break;

                case NOP:
                    MEMWB.IR = NOPI;
                    MEMWB.ALUOut = UNDEFINED;
                    MEMWB.LMD = UNDEFINED;
                    break;
                default:
                    break;
            }
        }



        if(memFlag > 1 && data_memory_latency != 0)
        {
            memFlag--;
            stallCount++;
            clockCount++;
            k++;
        }
        else
        {
            memFlag = 0;
            //EXE stage
            if(k >= 2)
            {
                switch(IDEX.IR.opcode) {
                    case ADD:
                    case SUB:
                    case XOR:
                    case ADDI:
                    case SUBI:
                        EXMEM.IR = IDEX.IR;
                        EXMEM.ALUOut = alu(IDEX.IR.opcode, IDEX.a, IDEX.b, IDEX.imm, IDEX.npc);
                        EXMEM.b = IDEX.b;
                        EXMEM.cond = UNDEFINED;
                        break;
                    case LW:
                    case SW:
                        EXMEM.IR = IDEX.IR;
                        EXMEM.ALUOut = alu(IDEX.IR.opcode, IDEX.a, IDEX.b, IDEX.imm, IDEX.npc);
                        EXMEM.b = IDEX.b;
                        EXMEM.cond = UNDEFINED;
                        break;
                    case BEQZ:
                        EXMEM.IR = IDEX.IR;
                        EXMEM.ALUOut = alu(IDEX.IR.opcode, IDEX.a, IDEX.b, IDEX.imm, IDEX.npc);
                        if(IDEX.a == 0)
                        {
                            EXMEM.cond = 1;


                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                        }
                        else
                        {
                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                            EXMEM.cond = 0;
                        }
                        EXMEM.b = IDEX.b;
                        break;
                    case BNEZ:
                        EXMEM.IR = IDEX.IR;
                        EXMEM.ALUOut = alu(IDEX.IR.opcode, IDEX.a, IDEX.b, IDEX.imm, IDEX.npc);
                        if(IDEX.a != 0)
                        {
                            EXMEM.cond = 1;


                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                        }
                        else
                        {
                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;



                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                            EXMEM.cond = 0;
                        }
                        EXMEM.b = IDEX.b;
                        break;
                    case BLTZ:
                        EXMEM.IR = IDEX.IR;
                        EXMEM.ALUOut = alu(IDEX.IR.opcode, IDEX.a, IDEX.b, IDEX.imm, IDEX.npc);
                        if(IDEX.a < 0)
                        {
                            EXMEM.cond = 1;


                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                        }
                        else
                        {
                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;



                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                            EXMEM.cond = 0;
                        }
                        EXMEM.b = IDEX.b;
                        break;
                    case BGTZ:
                        EXMEM.IR = IDEX.IR;
                        EXMEM.ALUOut = alu(IDEX.IR.opcode, IDEX.a, IDEX.b, IDEX.imm, IDEX.npc);
                        if(IDEX.a > 0)
                        {
                            EXMEM.cond = 1;


                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                        }
                        else
                        {
                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                            EXMEM.cond = 0;
                        }
                        EXMEM.b = IDEX.b;
                        break;
                    case BLEZ:
                        EXMEM.IR = IDEX.IR;
                        EXMEM.ALUOut = alu(IDEX.IR.opcode, IDEX.a, IDEX.b, IDEX.imm, IDEX.npc);
                        if(IDEX.a <= 0)
                        {
                            EXMEM.cond = 1;


                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                        }
                        else
                        {
                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                            EXMEM.cond = 0;
                        }
                        EXMEM.b = IDEX.b;
                        break;
                    case BGEZ:
                        EXMEM.IR = IDEX.IR;
                        EXMEM.ALUOut = alu(IDEX.IR.opcode, IDEX.a, IDEX.b, IDEX.imm, IDEX.npc);
                        if(IDEX.a >= 0)
                        {
                            EXMEM.cond = 1;


                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                        }
                        else
                        {
                            stallCount = stallCount+2;
                            IFID.IR = NOPI;
                            IFID.b = UNDEFINED;
                            IFID.a = UNDEFINED;
                            IFID.ALUOut = UNDEFINED;
                            IFID.imm = UNDEFINED;
                            IFID.LMD = UNDEFINED;

                            IDEX.IR=NOPI;
                            IDEX.b = UNDEFINED;
                            IDEX.a = UNDEFINED;
                            IDEX.ALUOut = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            IDEX.LMD = UNDEFINED;
                            EXMEM.cond = 0;
                        }
                        EXMEM.b = IDEX.b;
                        break;
                    case JUMP:

                        //EXMEM.ALUOut = UNDEFINED;

                        break;

                    case EOP:
                        EXMEM.IR = IDEX.IR;
                        EXMEM.b = IDEX.b;
                        EXMEM.ALUOut = UNDEFINED;
                        EXMEM.cond = UNDEFINED;
                        break;

                    case NOP:
                        EXMEM.IR = NOPI;
                        EXMEM.ALUOut = UNDEFINED;
                        EXMEM.cond = UNDEFINED;
                        EXMEM.b = UNDEFINED;
                        break;
                    default:
                        break;
                }
            }


            //ID STAGE
            if(k >= 1)
            {
                switch(IFID.IR.opcode) {
                    case ADD:
                    case SUB:
                    case XOR:
                        if((MEMWB.IR.dest == IFID.IR.src1 || MEMWB.IR.dest == IFID.IR.src2) || (EXMEM.IR.dest == IFID.IR.src1 || EXMEM.IR.dest == IFID.IR.src2))
                        {
                            stallCount++;
                            IDEX.IR = NOPI;
                            IDEX.a = UNDEFINED;
                            IDEX.b = UNDEFINED;
                            IDEX.npc = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            PCR = PCR - 4;
                        }
                        else
                        {
                            IDEX.a = regFile[IFID.IR.src1];
                            IDEX.b = regFile[IFID.IR.src2];
                            IDEX.npc = IFID.npc;
                            IDEX.IR = IFID.IR;
                            IDEX.imm = UNDEFINED;
                        }
                        break;
                    case ADDI:
                    case SUBI:
                    case LW:
                        if((MEMWB.IR.dest == IFID.IR.src1) || (EXMEM.IR.dest == IFID.IR.src1))
                        {
                            stallCount++;
                            IDEX.IR = NOPI;
                            IDEX.a = UNDEFINED;
                            IDEX.b = UNDEFINED;
                            IDEX.npc = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            PCR = PCR - 4;
                        }
                        else
                        {
                            IDEX.a = regFile[IFID.IR.src1];
                            IDEX.b = UNDEFINED;
                            IDEX.npc = IFID.npc;
                            IDEX.IR = IFID.IR;
                            IDEX.imm = IFID.IR.immediate;
                        }
                        break;

                    case SW:
                        if((MEMWB.IR.dest == IFID.IR.src1 || MEMWB.IR.dest == IFID.IR.src2) || (EXMEM.IR.dest == IFID.IR.src1 || EXMEM.IR.dest == IFID.IR.src2))
                        {
                            if(MEMWB.IR.dest == UNDEFINED)
                            {
                                IDEX.a = regFile[IFID.IR.src2];
                                IDEX.b = regFile[IFID.IR.src1];
                                IDEX.npc = IFID.npc;
                                IDEX.IR = IFID.IR;
                                IDEX.imm = IFID.IR.immediate;
                            }
                            stallCount++;
                            IDEX.IR = NOPI;
                            IDEX.a = UNDEFINED;
                            IDEX.b = UNDEFINED;
                            IDEX.npc = UNDEFINED;
                            IDEX.imm = UNDEFINED;
                            PCR = PCR - 4;
                        }
                        else
                        {
                            IDEX.a = regFile[IFID.IR.src2];
                            IDEX.b = regFile[IFID.IR.src1];
                            IDEX.npc = IFID.npc;
                            IDEX.IR = IFID.IR;
                            IDEX.imm = IFID.IR.immediate;
                        }
                        break;
                    case BEQZ:
                    case BNEZ:
                    case BLTZ:
                    case BGTZ:
                    case BLEZ:
                    case BGEZ:
                        if((MEMWB.IR.src1 == IFID.IR.src1 || MEMWB.IR.src1 == IFID.IR.src2) || (EXMEM.IR.src1 == IFID.IR.src1 || EXMEM.IR.src1 == IFID.IR.src2))
                        {
                            IDEX.a = UNDEFINED;
                            IDEX.b = UNDEFINED;
                            IDEX.npc = UNDEFINED;
                            IDEX.IR = NOPI;
                            IDEX.imm = UNDEFINED;

                        }
                        else
                        {
                            IDEX.a = regFile[IFID.IR.src1];
                            IDEX.b = UNDEFINED;
                            IDEX.npc = IFID.npc;
                            IDEX.IR = IFID.IR;
                            IDEX.imm = IFID.IR.immediate;
                            
                        }

                        break;
                    case JUMP:
                        IDEX.a = UNDEFINED;
                        IDEX.b = UNDEFINED;
                        IDEX.npc = IFID.npc;
                        IDEX.IR = IFID.IR;
                        IDEX.imm = IFID.IR.immediate;
                        break;

                    case EOP:
                        IDEX.a = UNDEFINED;
                        IDEX.b = UNDEFINED;
                        IDEX.npc = IFID.npc;
                        IDEX.IR = IFID.IR;
                        IDEX.imm = UNDEFINED;
                        break;
                    case NOP:
                        IDEX.a = UNDEFINED;
                        IDEX.b = UNDEFINED;
                        IDEX.npc = IFID.npc;
                        IDEX.IR = IFID.IR;
                        IDEX.imm = UNDEFINED;
                    default:
                        break;
                }
                /*IDEX.a = regFile[IFID.IR.src1];
                IDEX.b = regFile[IFID.IR.src2];
                IDEX.npc = IFID.npc;
                IDEX.IR = IFID.IR;
                IDEX.imm = IFID.IR.immediate;*/
            }


            //IF Stage
            IFID.IR = instr_memory[(PCR - instr_base_address)/4];
            switch(IFID.IR.opcode) {
                case EOP:

                    if (EXMEM.IR.opcode == BEQZ || EXMEM.IR.opcode == BGEZ || EXMEM.IR.opcode == BGTZ ||
                        EXMEM.IR.opcode == BLEZ || EXMEM.IR.opcode == BLTZ || EXMEM.IR.opcode == BNEZ) {
                        IFID.IR = NOPI;
                        PCR = PCR;
                        IFID.npc = UNDEFINED;
                        if (EXMEM.cond == 1) {
                            PCR = EXMEM.ALUOut;
                            IFID.npc = EXMEM.ALUOut;
                        }

                    } else {

                        PCR = PCR;
                        IFID.npc = PCR;
                    }
                    break;


                default:
                    if (EXMEM.cond == 1) {
                        PCR = EXMEM.ALUOut;
                        IFID.npc = EXMEM.ALUOut;

                    } else if (IFID.IR.opcode == SW) {
                        unsigned tmp = PCR;
                        PCR = PCR + 4;
                        IFID.npc = tmp + 4;
                        IFID.IR.dest = 50;
                    }
                    else if(EXMEM.cond != UNDEFINED )
                    {

                        PCR = PCR;

                        IFID.npc = UNDEFINED;
                        IFID.IR = NOPI;
                        IFID.b = UNDEFINED;
                        IFID.a = UNDEFINED;
                        IFID.ALUOut = UNDEFINED;
                        IFID.imm = UNDEFINED;
                        IFID.LMD = UNDEFINED;

                    }
                    else {
                        unsigned tmp = PCR;
                        PCR = PCR + 4;
                        IFID.npc = tmp + 4;
                    }

                    break;
            }
            clockCount++;
            k++;
        }







		//IF stage
			//Fetches from memory based by program and puts that instruction in the IR of IF/ID pipeline reg
			//IF/ID.IR = Instr_mem[PC]
			//IF/ID.NPC = PC + 4;

		//ID stage
			//Populate A B Imm
			//ID/EX.A = Regfile[IF/ID.IR.SRC1]
			//ID/EX.B = Regfile[IF/ID.IR.SRC2]
			//
		//EX stage
		//Mem stage
		//WB stage
		//Increment # of cycles couter
		//clockCount++;
        //k++;
		//Do reverse order so as to move stuff along
	}
		
}
	
/* reset the state of the pipeline simulator */
void sim_pipe::reset(){
    k = 0;
    clockCount = 0;
    iCount = 0;
    stallCount = 0;
    for(int l = 0; l < data_memory_size; l++)
    {
        write_memory(l, 0xff);
    }
    for(int i = 0; i < NUM_GP_REGISTERS; i++)
    {
        regFile[i] = UNDEFINED;
    }

    //Init IF/ID Reg
    IFID.IR.src1 = UNDEFINED;
    IFID.IR.src2 = UNDEFINED;
    IFID.IR.dest = UNDEFINED;
    IFID.IR.immediate = UNDEFINED;
    IFID.pc = UNDEFINED;
    IFID.npc = UNDEFINED;
    IFID.a = UNDEFINED;
    IFID.b = UNDEFINED;
    IFID.imm = UNDEFINED;
    IFID.ALUOut = UNDEFINED;
    IFID.cond = UNDEFINED;
    IFID.LMD = UNDEFINED;

    //Init ID/EX Reg
    IDEX.IR.src1 = UNDEFINED;
    IDEX.IR.src2 = UNDEFINED;
    IDEX.IR.dest = UNDEFINED;
    IDEX.IR.immediate = UNDEFINED;
    IDEX.pc = UNDEFINED;
    IDEX.npc = UNDEFINED;
    IDEX.a = UNDEFINED;
    IDEX.b = UNDEFINED;
    IDEX.imm = UNDEFINED;
    IDEX.ALUOut = UNDEFINED;
    IDEX.cond = UNDEFINED;
    IDEX.LMD = UNDEFINED;

    //Init EX/MEM Reg
    EXMEM.IR.src1 = UNDEFINED;
    EXMEM.IR.src2 = UNDEFINED;
    EXMEM.IR.dest = UNDEFINED;
    EXMEM.IR.immediate = UNDEFINED;
    EXMEM.pc = UNDEFINED;
    EXMEM.npc = UNDEFINED;
    EXMEM.a = UNDEFINED;
    EXMEM.b = UNDEFINED;
    EXMEM.imm = UNDEFINED;
    EXMEM.ALUOut = UNDEFINED;
    EXMEM.cond = UNDEFINED;
    EXMEM.LMD = UNDEFINED;

    //Init MEM/WB Reg
    MEMWB.IR.src1 = UNDEFINED;
    MEMWB.IR.src2 = UNDEFINED;
    MEMWB.IR.dest = UNDEFINED;
    MEMWB.IR.immediate = UNDEFINED;
    MEMWB.pc = UNDEFINED;
    MEMWB.npc = UNDEFINED;
    MEMWB.a = UNDEFINED;
    MEMWB.b = UNDEFINED;
    MEMWB.imm = UNDEFINED;
    MEMWB.ALUOut = UNDEFINED;
    MEMWB.cond = UNDEFINED;
    MEMWB.LMD = UNDEFINED;
}

//return value of special purpose register
unsigned sim_pipe::get_sp_register(sp_register_t reg, stage_t s){
    switch(s){
        case IF:
            switch(reg){
                case PC:
                    return PCR;
                case NPC:
                    return UNDEFINED;
                case A:
                    return IFID.a;
                case B:
                    return IFID.b;
                case IMM:
                    return IFID.imm;
                case COND:
                    return IFID.cond;
                case ALU_OUTPUT:
                    return IFID.ALUOut;
                case LMD:
                    return IFID.LMD;
                default:
                    break;
            }

        case ID:
            switch(reg){
                case PC:
                    return IFID.pc;
                case NPC:
                    return IFID.npc;
                case A:
                    return IFID.a;
                case B:
                    return IFID.b;
                case IMM:
                    return IFID.imm;
                case COND:
                    return IFID.cond;
                case ALU_OUTPUT:
                    return IFID.ALUOut;
                case LMD:
                    return IFID.LMD;
                default:
                    break;
            }

        case EXE:
            switch(reg){
                case PC:
                    return IDEX.pc;
                case NPC:
                    return IDEX.npc;
                case A:
                    return IDEX.a;
                case B:
                    return IDEX.b;
                case IMM:
                    return IDEX.imm;
                case COND:
                    return IDEX.cond;
                case ALU_OUTPUT:
                    return IDEX.ALUOut;
                case LMD:
                    return IDEX.LMD;
                default:
                    break;
            }

        case MEM:
            switch(reg) {
                case PC:
                    return EXMEM.pc;
                case NPC:
                    return EXMEM.npc;
                case A:
                    return EXMEM.a;
                case B:
                    return EXMEM.b;
                case IMM:
                    return EXMEM.imm;
                case COND:
                    return EXMEM.cond;
                case ALU_OUTPUT:
                    return EXMEM.ALUOut;
                case LMD:
                    return EXMEM.LMD;
                default:
                    break;
            }


        case WB:
            switch(reg) {
                case PC:
                    return MEMWB.pc;
                case NPC:
                    return MEMWB.npc;
                case A:
                    return MEMWB.a;
                case B:
                    return MEMWB.b;
                case IMM:
                    return MEMWB.imm;
                case COND:
                    return MEMWB.cond;
                case ALU_OUTPUT:
                    return MEMWB.ALUOut;
                case LMD:
                    return MEMWB.LMD;
                default:
                    break;
            }

        default:
            break;

    }
	return 0; //please modify
}

//returns value of general purpose register
int sim_pipe::get_gp_register(unsigned reg){
	return regFile[reg];
}

void sim_pipe::set_gp_register(unsigned reg, int value){
    regFile[reg] = value;
}

float sim_pipe::get_IPC(){
        float IPC = (float)iCount/(float)clockCount;
        return IPC;
}

unsigned sim_pipe::get_instructions_executed(){
        return iCount;
}

unsigned sim_pipe::get_stalls(){
        return stallCount;
}

unsigned sim_pipe::get_clock_cycles(){
        return clockCount;
}

