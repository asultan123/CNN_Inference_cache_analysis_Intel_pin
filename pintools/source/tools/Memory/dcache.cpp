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

/*! @file
 *  This file contains an ISA-portable cache simulator
 *  data cache hierarchies
 */


#include "pin.H"

#include <iostream>
#include <fstream>
#include <cassert>
#include <string>

#include "cache.H"
#include "pin_profile.H"
using std::cerr;
using std::endl;

#define USE_L2_CACHE
// #define ECOLCACHE
#define PREFETCH_SIZE 64

UINT32 Load_Hits;
UINT32 Load_Misses;
UINT32 Store_Hits;
UINT32 Store_Misses;

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,    "pintool",
    "o", "dcache.out", "specify dcache file name");

KNOB<FLT32> Knobl1CacheSize(KNOB_MODE_WRITEONCE, "pintool",
    "l1c","32", "cache size in kilobytes");
KNOB<UINT32> Knobl1LineSize(KNOB_MODE_WRITEONCE, "pintool",
    "l1b","32", "cache block size in bytes");
KNOB<UINT32> Knobl1Associativity(KNOB_MODE_WRITEONCE, "pintool",
    "l1a","4", "cache associativity (1 for direct mapped)");

#ifdef USE_L2_CACHE
KNOB<FLT32> Knobl2CacheSize(KNOB_MODE_WRITEONCE, "pintool",
    "l2c","32", "cache size in kilobytes");
KNOB<UINT32> Knobl2LineSize(KNOB_MODE_WRITEONCE, "pintool",
    "l2b","32", "cache block size in bytes");
KNOB<UINT32> Knobl2Associativity(KNOB_MODE_WRITEONCE, "pintool",
    "l2a","4", "cache associativity (1 for direct mapped)");
#endif    


/* ===================================================================== */

INT32 Usage()
{
    cerr <<
        "This tool represents a cache simulator.\n"
        "\n";


    cerr << KNOB_BASE::StringKnobSummary();

    cerr << endl;


    return -1;


}

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */
// wrap configuation constants into their own name space to avoid name clashes

#ifdef ECOLCACHE


namespace DL1
{
    const UINT32 max_sets = KILO; // cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = 256; // associativity;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    typedef CACHE_COLUMN_ASSOC(max_sets, allocation) CACHE;
}

DL1::CACHE* dl1 = NULL;

#else

namespace DL1
{
    const UINT32 max_sets = KILO; // cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = 256; // associativity;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}

DL1::CACHE* dl1 = NULL;

#endif

#ifdef USE_L2_CACHE

namespace DL2
{

    const UINT32 max_sets = KILO; // cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = 256; // associativity;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;

    typedef CACHE_ROUND_ROBIN(max_sets, max_associativity, allocation) CACHE;
}

DL2::CACHE* dl2 = NULL;

#endif


// UINT32 multi_called_times = 0;
/* ===================================================================== */

VOID LoadMultiFast(ADDRINT addr, UINT32 size, UINT32 isprefetch)
{
    // multi_called_times++;
#ifdef USE_L2_CACHE
    const BOOL dl1Hit = dl1->Access(addr, size, CACHE_BASE::ACCESS_TYPE_LOAD);
    BOOL dl2Hit = false;
    if(!dl1Hit)
    {
        dl2Hit = dl2->Access(addr, size, CACHE_BASE::ACCESS_TYPE_LOAD);
    }
    Load_Hits = (dl1Hit || dl2Hit) ? Load_Hits + 1 : Load_Hits;
    Load_Misses = (!dl1Hit && !dl2Hit) ? Load_Misses + 1 : Load_Misses;
#else
    const BOOL dl1Hit = dl1->Access(addr, size, CACHE_BASE::ACCESS_TYPE_LOAD);
    if(dl1Hit)
    {
        Load_Hits++;
    }
    else
    {
        Load_Misses++;
    }
#endif
}

/* ===================================================================== */

VOID LoadSingleFast(ADDRINT addr)
{
    
#ifdef USE_L2_CACHE
    const BOOL dl1Hit = dl1->AccessSingleLine(addr, CACHE_BASE::ACCESS_TYPE_LOAD);    
    BOOL dl2Hit = false;
    if(!dl1Hit)
    {
        dl2Hit = dl2->AccessSingleLine(addr, CACHE_BASE::ACCESS_TYPE_LOAD);
    }
    Load_Hits = (dl1Hit || dl2Hit) ? Load_Hits + 1 : Load_Hits;
    Load_Misses = (!dl1Hit && !dl2Hit) ? Load_Misses + 1 : Load_Misses;
#else
    const BOOL dl1Hit = dl1->AccessSingleLine(addr, CACHE_BASE::ACCESS_TYPE_LOAD);
    if(dl1Hit)
    {
        Load_Hits++;
    }
    else
    {
        Load_Misses++;
    }
#endif
}


/* ===================================================================== */

VOID StoreMultiFast(ADDRINT addr, UINT32 size)
{
    // multi_called_times++;
#ifdef USE_L2_CACHE
    const BOOL dl1Hit = dl1->Access(addr, size, CACHE_BASE::ACCESS_TYPE_STORE);
    BOOL dl2Hit = false;
    
    if(!dl1Hit)
    {
        dl2Hit = dl2->Access(addr, size, CACHE_BASE::ACCESS_TYPE_STORE);
    }
    Store_Hits = (dl1Hit || dl2Hit) ? Store_Hits + 1 : Store_Hits;
    Store_Misses = (!dl1Hit && !dl2Hit) ? Store_Misses + 1 : Store_Misses;

#else
    const BOOL dl1Hit = dl1->Access(addr, size, CACHE_BASE::ACCESS_TYPE_STORE);
    if(dl1Hit)
    {
        Store_Hits++;
    }
    else
    {
        Store_Misses++;
    }
#endif
}

/* ===================================================================== */

VOID StoreSingleFast(ADDRINT addr)
{
#ifdef USE_L2_CACHE
    const BOOL dl1Hit = dl1->AccessSingleLine(addr, CACHE_BASE::ACCESS_TYPE_STORE);   
    BOOL dl2Hit = false;
    if(!dl1Hit)
    {
        dl2Hit = dl2->AccessSingleLine(addr, CACHE_BASE::ACCESS_TYPE_STORE);
    }
    Store_Hits = (dl1Hit || dl2Hit) ? Store_Hits + 1 : Store_Hits;
    Store_Misses = (!dl1Hit && !dl2Hit) ? Store_Misses + 1 : Store_Misses;

#else
    const BOOL dl1Hit = dl1->AccessSingleLine(addr, CACHE_BASE::ACCESS_TYPE_STORE);
    if(dl1Hit)
    {
        Store_Hits++;
    }
    else
    {
        Store_Misses++;
    }
#endif
}


/* ===================================================================== */

VOID Instruction(INS ins, void * v)
{
    ADDRINT curr_addr = INS_Address(ins);
    std::string type = INS_Mnemonic(ins);
    if (curr_addr >= 0x4767b7 && curr_addr <= 0x476911)
    {
        UINT32 memOperands = INS_MemoryOperandCount(ins);

        // Instrument each memory operand. If the operand is both read and written
        // it will be processed twice.
        // Iterating over memory operands ensures that instructions on IA-32 with
        // two read operands (such as SCAS and CMPS) are correctly handled.
        for (UINT32 memOp = 0; memOp < memOperands; memOp++)
        {
            UINT32 size = INS_MemoryOperandSize(ins, memOp);
            const BOOL   single = (size <= 4);
            
            if (INS_MemoryOperandIsRead(ins, memOp))
            {
                if( single )
                {
                    INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) LoadSingleFast,
                        IARG_MEMORYOP_EA, memOp,
                        IARG_END);
                        
                }
                else
                {
                    UINT32 isprefetch;
                    if(type == "PREFETCHT0")
                    {
                        isprefetch = 1;
                        size = PREFETCH_SIZE;
                    }
                    else
                    {
                        isprefetch = 0;
                    }
                    INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) LoadMultiFast,
                        IARG_MEMORYOP_EA, memOp,
                        IARG_UINT32, size,
                        IARG_UINT32, isprefetch,
                        IARG_END);
                }
            }
            if (INS_MemoryOperandIsWritten(ins, memOp))
            {
                if( single )
                {
                    INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) StoreSingleFast,
                        IARG_MEMORYOP_EA, memOp,
                        IARG_END);
                        
                }
                else
                {
                    INS_InsertPredicatedCall(
                        ins, IPOINT_BEFORE,  (AFUNPTR) StoreMultiFast,
                        IARG_MEMORYOP_EA, memOp,
                        IARG_UINT32, size,
                        IARG_END);
                }
            }
        }
    }
}

/* ===================================================================== */
// string mydecstr(UINT64 v, UINT32 w)
// {
//     ostringstream o;
//     o.width(w);
//     o << v;
//     string str(o.str());
//     return str;
// }

VOID Fini(int code, VOID * v)
{


    std::ofstream out(KnobOutputFile.Value().c_str());

    // print D-cache profile
    
    out << "PIN:MEMLATENCIES 1.0. 0x0\n";
            
    out <<
        "#\n"
        "# L1 DCACHE stats\n"
        "#\n";
    
    out << dl1->StatsLong("# ", CACHE_BASE::CACHE_TYPE_DCACHE);
#ifdef USE_L2_CACHE

    out <<
        "#\n"
        "# L2 DCACHE stats\n"
        "#\n";
    
    out << dl2->StatsLong("# ", CACHE_BASE::CACHE_TYPE_DCACHE);

    const UINT64 headerWidth = 19U;
    const UINT64 numberWidth = 12U;
    const UINT64 Load_accesses = Load_Hits + Load_Misses;
    const UINT64 Store_accesses = Store_Hits + Store_Misses;
    const UINT64 Total_accesses = Load_accesses + Store_accesses;
    const UINT64 Total_hits = Load_Hits + Store_Hits;
    const UINT64 Total_misses = Load_Misses + Store_Misses;

    out << "#\n# Total Stats\n#\n";

    out << "# " + ljstr("Total-L-Hits:      ", headerWidth)
           + mydecstr(Load_Hits, numberWidth) +
           "  " +fltstr(100.0 * Load_Hits / Load_accesses, 2, 6) + "%\n";

    out << "# " + ljstr("Total-L-Misses:    ", headerWidth)
           + mydecstr(Load_Misses, numberWidth) +
           "  " +fltstr(100.0 * Load_Misses / Load_accesses, 2, 6) + "%\n";

    out << "# " + ljstr("Total-S-Hits:      ", headerWidth)
           + mydecstr(Store_Hits, numberWidth) +
           "  " +fltstr(100.0 * Store_Hits / Store_accesses, 2, 6) + "%\n";

    out << "# " + ljstr("Total-S-Misses:    ", headerWidth)
           + mydecstr(Store_Misses, numberWidth) +
           "  " +fltstr(100.0 * Store_Misses / Store_accesses, 2, 6) + "%\n";

    out << "# " + ljstr("Hits-Rate:  ", headerWidth)
           + mydecstr(Total_hits, numberWidth) +
           "  " +fltstr(100.0 * Total_hits / Total_accesses, 2, 6) + "%\n";

    out << "# " + ljstr("Miss-Rate:  ", headerWidth)
           + mydecstr(Total_misses, numberWidth) +
           "  " +fltstr(100.0 * Total_misses / Total_accesses, 2, 6) + "%\n";


#endif
    // out << " multi called times : " << multi_called_times << "\n";

    out.close();


}

/* ===================================================================== */

int main(int argc, char *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    
    Load_Hits = 0;


    Load_Misses = 0;
    Store_Hits = 0;
    Store_Misses = 0;

#ifdef ECOLCACHE
    UINT32 l1cacheSize = Knobl1CacheSize.Value() * KILO;

    dl1 = new DL1::CACHE("L1 Col Data Cache", 
                         l1cacheSize,
                         Knobl1LineSize.Value(),
                         1);


#else
    UINT32 l1cacheSize = Knobl1CacheSize.Value() * KILO;

    dl1 = new DL1::CACHE("L1 Data Cache", 
                         l1cacheSize,
                         Knobl1LineSize.Value(),
                         Knobl1Associativity.Value());

#endif
#ifdef USE_L2_CACHE

    UINT32 l2cacheSize = Knobl2CacheSize.Value() * KILO;

    dl2 = new DL2::CACHE("L2 Data Cache", 
                        l2cacheSize,
                        Knobl2LineSize.Value(),
                        Knobl2Associativity.Value());
    
#endif

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);


    // Never returns

    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
