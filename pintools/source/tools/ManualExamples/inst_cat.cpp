/*
 * Copyright 2002-2019 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

//TEST

#include <iostream>
#include <cstdio>
#include <fstream>
#include "pin.H"
#include <string>
#include <map>
#include <algorithm>
using std::cerr;
using std::ofstream;
using std::ios;
using std::string;
using std::endl;
using std::cout;

ofstream OutFile;
std::string arr[] = {"CMOVB","CMOVBE","CMOVNBE","CMOVNS","CMOVNZ","CMOVZ","CMPXCHG","MOV","MOVD","MOVDQA","MOVDQU","MOVHPD","MOVLPD","MOVQ","MOVSX","MOVSXD","MOVZX","XCHG"};
static std::map<string, int> inst_count;
static std::map<string, int> inst_cat_count;
static std::vector<string> mov_inst(arr, arr + sizeof(arr)/sizeof(std::string));


// The running count of instructions is kept here
// make it static to help the compiler optimize docount

bool isCMPXCHG(const string *type)
{
    return *type=="CMPXCHG";
}

bool isMov(const string *type)
{
    return (std::find(mov_inst.begin(),mov_inst.end(), *type)!=mov_inst.end());
}

VOID map_inc(std::map<string, int> *logMap, const string *key)
{
    if(logMap->find(*key) == logMap->end())
    {
        logMap->insert(std::pair<string,int>(*key,1));
    }
    else
    {
        (*logMap)[*key]++;
    }
}



VOID printip(const string *cat, const string *type) { 
    map_inc(&inst_cat_count, cat);
    map_inc(&inst_count, type);
}

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v)
{
    string catString = CATEGORY_StringShort(INS_Category(ins)); 
    // string catString = "HELLO"; 
    string typeString = INS_Mnemonic(ins); 

    if(isMov(new string(typeString)))
    {
        if(INS_OperandCount(ins)==2){

            if(INS_OperandIsMemory(ins,0))
            {
                typeString = string("S" + typeString);
            }
            else if(INS_OperandIsMemory(ins,1))
            {
                typeString = string("L" + typeString);
            }

        }
    }

    if(isCMPXCHG(new string(typeString)))
    {
        if(INS_OperandIsMemory(ins,0))
        {
            typeString = string("S" + typeString);
        }
    }

    // Insert a call to printip before every instruction, and pass it the IP
    INS_InsertCall(ins, IPOINT_BEFORE, 
      (AFUNPTR)printip, 
      IARG_PTR,new string(catString),IARG_PTR,new string(typeString),
      IARG_END);
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "mytool.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    // Write to a file since cout and cerr maybe closed by the application
    std::map<string, int>::iterator it;
    OutFile.setf(ios::showbase);

    OutFile<<"\n\n\n\nCATEGORY COUNT:\n\n\n\n"<<endl;

    for ( it = inst_cat_count.begin(); it != inst_cat_count.end(); it++ )
    {
        OutFile << it->first  // string (key)
        << ':'
                  << it->second   // string's value 
                  << std::endl ;
              }
    //     // Write to a file since cout and cerr maybe closed by the application
              OutFile<<"\n\n\n\nTYPE COUNT:\n\n\n\n"<<endl;

              for ( it = inst_count.begin(); it != inst_count.end(); it++ )
              {
        OutFile << it->first  // string (key)
        << ':'
                  << it->second   // string's value 
                  << std::endl ;
              }
              OutFile.close();
          }

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

          INT32 Usage()
          {
            cerr << "This tool counts the number of dynamic instructions executed" << endl;
            cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
            return -1;
        }

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*   argc, argv are the entire command line: pin -t <toolname> -- ...    */
/* ===================================================================== */

        int main(int argc, char * argv[])
        {
    // Initialize pin
            if (PIN_Init(argc, argv)) return Usage();

            OutFile.open(KnobOutputFile.Value().c_str());

    // Register Instruction to be called to instrument instructions
            INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
            PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
            PIN_StartProgram();

            return 0;
        }
