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

#include <iostream>
#include <cstdio>
#include <fstream>
#include "pin.H"
#include <string>
#include <map>
#include <algorithm>
#include <utility>      
using std::cerr;
using std::ofstream;
using std::ios;
using std::string;
using std::endl;
using std::cout;
using std::map;
using std::pair;


// The running count of instructions is kept here
// make it static to help the compiler optimize docount

FILE * trace;
FILE * pages;
map<unsigned long,int> page_tracker;
map<unsigned long,int> read_tracker;
map<unsigned long,int> write_tracker;

VOID AddPageTrackerEntry(VOID * addr)
{

    if (page_tracker.find((unsigned long) addr & ~4095) == page_tracker.end())
    {
        page_tracker[((unsigned long) addr & ~4095)] = 1;   
    }
    else
    {
        page_tracker[((unsigned long) addr & ~4095)] += 1;   
    }
}

// Pin calls this function every time a new instruction is encountered
// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr)
{
    // fprintf(trace,"%p: R %p\n", ip, addr);
    if (read_tracker.find((unsigned long) addr) == read_tracker.end())
    {
        read_tracker[((unsigned long) addr)] = 1;   
    }
    else
    {
        read_tracker[((unsigned long) addr)] += 1;   
    }
    AddPageTrackerEntry(addr);
}

// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr)
{
    // fprintf(trace,"%p: W %p\n", ip, addr);
    if (write_tracker.find((unsigned long) addr) == write_tracker.end())
    {
        write_tracker[(unsigned long)addr] = 1;   
    }
    else
    {
        write_tracker[(unsigned long)addr] += 1;   
    }
    AddPageTrackerEntry(addr);
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v)
{
    // Instruments memory accesses using a predicated call, i.e.
    // the instrumentation is called iff the instruction will actually be executed.
    //
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    // Iterate over each memory operand of the instruction.
    for (UINT32 memOp = 0; memOp < memOperands; memOp++)
    {
        if (INS_MemoryOperandIsRead(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemRead,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_END);
        }
        // Note that in some architectures a single memory operand can be 
        // both read and written (for instance incl (%eax) on IA-32)
        // In that case we instrument it once for read and once for write.
        if (INS_MemoryOperandIsWritten(ins, memOp))
        {
            INS_InsertPredicatedCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordMemWrite,
                IARG_INST_PTR,
                IARG_MEMORYOP_EA, memOp,
                IARG_END);
        }
    }
}
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "mytool.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{

    fprintf(pages, "%s\n", "Begin Page Tracker Dump");
    fprintf(pages, "%s : %lu\n", "Total Unique Pages Referenced", (unsigned long)page_tracker.size());
    fprintf(pages, "%s\n", "Page Address : Reference Count");

    for (std::map<unsigned long,int>::iterator it=page_tracker.begin(); it!=page_tracker.end(); ++it)
    {
        fprintf(pages, "0x%lx:%d\n", it->first, it->second);
    }
       
    // Write to a file since cout and cerr maybe closed by the application
    fprintf(pages, "#eof\n");
    fclose(pages);


    fprintf(trace, "%s\n", "Begin Mem Tracker Read Dump");
    fprintf(trace, "%s\n", "pc address : Reference Count");

    for (std::map<unsigned long,int>::iterator it=read_tracker.begin(); it!=read_tracker.end(); ++it)
    {
        fprintf(trace, "0x%lx:%d\n", it->first, it->second);
    }

    fprintf(trace, "%s\n", "Begin Mem Tracker Write Dump");
    fprintf(trace, "%s\n", "pc address : Reference Count");

    for (std::map<unsigned long,int>::iterator it=write_tracker.begin(); it!=write_tracker.end(); ++it)
    {
        fprintf(trace, "0x%lx:%d\n", it->first, it->second);
    }
       
    // Write to a file since cout and cerr maybe closed by the application
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR( "This Pintool prints a trace of memory addresses\n" 
              + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*   argc, argv are the entire command line: pin -t <toolname> -- ...    */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    if (PIN_Init(argc, argv)) return Usage();

    trace = fopen("pinatrace.out", "w");
    pages = fopen("pagestrace.out", "w");

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}
