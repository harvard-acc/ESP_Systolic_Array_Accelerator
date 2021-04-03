#ifndef __MEMORYCORE_H__
#define __MEMORYCORE_H__

#include <systemc.h>
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <nvhls_vector.h>

#include "ArbitratedScratchpad.h"
#include <ArbitratedScratchpad/ArbitratedScratchpadTypes.h>

#include <string>
#include <one_hot_to_bin.h>

#include "../SysPE/SysPE.h"
#include "../SysArray/SysArray.h"
#include "../include/SysSpec.h"

//#include <scratchpad.h>
//#include <scratchpad/scratchpadtypes.h>

SC_MODULE(MemoryCore)
{
 public: 
  sc_in_clk     clk;
  sc_in<bool>   rst;

  typedef spec::InputType InputType;  
  static const int N = spec::N;               // # banks = N
  //static const int N = 2; //spec::N;               // # banks = N
  static const int Entries = spec::Entries;   // # of entries per bank
  static const int Capacity = N*Entries;

  // Memory type
  typedef ArbitratedScratchpad<InputType, Capacity, N, N, 0> MemType;

  static const int IndexBits = nvhls::nbits<Entries-1>::val;
  typedef NVUINTW(IndexBits) IndexType;
  //typedef typename nvhls::nv_scvector <InputType, N> VectorType;
  typedef NVUINTW(MemType::addr_width) AddrType;
  
  typedef MemType::req_t req_t;
  typedef MemType::rsp_t rsp_t;  
   
  MemType mem_inst;
  
  Connections::In<req_t> req_inter;
  Connections::Out<rsp_t> rsp_inter;
 
  
  SC_HAS_PROCESS(MemoryCore);
  MemoryCore(sc_module_name name_) : sc_module(name_) {

    SC_THREAD (MemoryRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst); 
  }

  void MemoryRun() {
    req_inter.Reset();
    rsp_inter.Reset();
    
    #pragma hls_pipeline_init_interval 1
    while(1) {
      
      req_t req_reg;
      rsp_t rsp_reg;
      bool input_ready[N];
      bool is_read = 0;      
      if (req_inter.PopNB(req_reg)) {
        is_read = (req_reg.type.val == CLITYPE_T::LOAD);
        mem_inst.load_store(req_reg, rsp_reg, input_ready);
        
        if (is_read) {
          rsp_inter.Push(rsp_reg);
        }
      }
      wait();
    }
  }
};

#endif

