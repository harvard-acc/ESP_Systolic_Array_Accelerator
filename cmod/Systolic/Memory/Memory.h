#ifndef __MEMORY_H__
#define __MEMORY_H__

#include <systemc.h>
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <nvhls_vector.h>

//#include "ArbitratedScratchpad.h"
//#include <ArbitratedScratchpad/ArbitratedScratchpadTypes.h>

#include <string>
#include <one_hot_to_bin.h>

#include "../SysPE/SysPE.h"
#include "../SysArray/SysArray.h"
#include "../include/SysSpec.h"
#include "../MemoryCore/MemoryCore.h"
//#include "Scratchpad.h"
//#include <Scratchpad/ScratchpadTypes.h>

SC_MODULE(Memory)
{
 public: 
  sc_in_clk     clk;
  sc_in<bool>   rst;

  typedef spec::InputType InputType;  
  static const int N = spec::N;               // # banks = N
  static const int Entries = spec::Entries;   // # of entries per bank
  static const int Capacity = N*Entries;

  typedef MemoryCore::MemType MemType;
//  typedef Scratchpad<InputType, N, Capacity> MemType;
//  MemType mem0, mem1;

  // Memory type
//  typedef ArbitratedScratchpad<InputType, Capacity, N, N, 0> MemType;

  static const int IndexBits = nvhls::nbits<Entries-1>::val;
  typedef NVUINTW(IndexBits) IndexType;
  typedef typename nvhls::nv_scvector <InputType, N> VectorType;
  typedef NVUINTW(MemType::addr_width) AddrType;
  //typedef NVUINTW(MemType::ADDR_WIDTH) AddrType;
 
  //1207 change interface to Scratchpad format
  typedef MemType::req_t req_t;
  typedef MemType::rsp_t rsp_t;  
//  typedef cli_req_t<InputType, MemType::ADDR_WIDTH, N> req_t;
//  typedef cli_rsp_t<InputType, N> rsp_t;  
   
  //MemType mem_inst0, mem_inst1; // Double buffering memory instance

  // Flip Memory instances
  Connections::In<bool> flip_mem;
    
  // Memory req/rsp
  // FIXME update: change order of axi master read/write
  //
  // req[0]: axi mem wr (from axi master read)
  // req[1]: com mem rd (computation input setup read)
  // req[2]: com mem wr (computation input setup write) 
  // req[3]: axi mem rd (from axi master write)
  //    
  // rsp[1]: axi mem rd response (axi master write)
  // rsp[0]: com mem rd response (Input setup rsp)
  Connections::In<req_t>  mem_req[4]; 
  Connections::Out<rsp_t> mem_rsp[2];


  Connections::Combinational<req_t> req_inter[2];
  Connections::Combinational<rsp_t> rsp_inter[2];
  
  Connections::Combinational<bool>  flip_inter[2];
  
  MemoryCore mem0, mem1;


  SC_HAS_PROCESS(Memory);
  Memory(sc_module_name name_) : 
    sc_module(name_),
    mem0("mem0"),
    mem1("mem1")
  {
    mem0.clk(clk);
    mem0.rst(rst);
    mem0.req_inter(req_inter[0]);
    mem0.rsp_inter(rsp_inter[0]);
    //mem0.cli_req(req_inter[0]);
    //mem0.cli_rsp(rsp_inter[0]);

    mem1.clk(clk);
    mem1.rst(rst);
    mem1.req_inter(req_inter[1]);
    mem1.rsp_inter(rsp_inter[1]);
    //mem1.cli_req(req_inter[1]);
    //mem1.cli_rsp(rsp_inter[1]);
    //SC_THREAD (MemoryRun0);
    //sensitive << clk.pos();
    //NVHLS_NEG_RESET_SIGNAL_IS(rst); 
    //SC_THREAD (MemoryRun1);
    //sensitive << clk.pos();
    //NVHLS_NEG_RESET_SIGNAL_IS(rst); 
  
    SC_THREAD (FlipRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst); 
    SC_THREAD (SplitReqRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst); 
    SC_THREAD (MergeRspRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst); 
  
  }
  /*
  void MemoryRun0() {
    req_inter[0].ResetRead();
    rsp_inter[0].ResetWrite();
    
    #pragma hls_pipeline_init_interval 1
    while(1) {
      
      req_t req_reg;
      rsp_t rsp_reg;
      bool input_ready[N];
      bool is_read = 0;      
      if (req_inter[0].PopNB(req_reg)) {
        is_read = (req_reg.type.val == CLITYPE_T::LOAD);
        mem_inst0.load_store(req_reg, rsp_reg, input_ready);
        
        if (is_read) {
          rsp_inter[0].Push(rsp_reg);
        }
      }
      wait();
    }
  }

  void MemoryRun1() {
    req_inter[1].ResetRead();
    rsp_inter[1].ResetWrite();
    
    #pragma hls_pipeline_init_interval 1
    while(1) {
      req_t req_reg;
      rsp_t rsp_reg;
      bool input_ready[N];
      bool is_read = 0;
      if (req_inter[1].PopNB(req_reg)) {
        is_read = (req_reg.type.val == CLITYPE_T::LOAD);
        mem_inst1.load_store(req_reg, rsp_reg, input_ready);
        
        if (is_read) {
          rsp_inter[1].Push(rsp_reg);
        }
      }
      wait();
    }
  }*/

  void FlipRun() {
    flip_mem.Reset();
    flip_inter[0].ResetWrite();
    flip_inter[1].ResetWrite();
 
    #pragma hls_pipeline_init_interval 1
    while(1) {
      bool tmp;
      if (flip_mem.PopNB(tmp)) {
        flip_inter[0].Push(tmp);
        flip_inter[1].Push(tmp);
      }   
      wait();
    }
  }

  // Split request to memory0 or memory 1
  // req[0]: axi mem wr (from axi master read)
  // req[1]: com mem rd (computation input setup read)
  // req[2]: com mem wr (computation input setup write) 
  // req[3]: axi mem rd (from axi master write)
  void SplitReqRun() {
    flip_inter[0].ResetRead();
    
    #pragma hls_unroll yes
    for (int i=0; i<4; i++) mem_req[i].Reset();
    #pragma hls_unroll yes
    for (int i=0; i<2; i++) req_inter[i].ResetWrite();

    bool mem_id = 0;
    #pragma hls_pipeline_init_interval 1
    while(1) {
      req_t req_reg[2];
      bool valids[2];
      
      valids[0] = 0;
      if (mem_req[0].PopNB(req_reg[0])) {
        valids[0] = 1;
        NVHLS_ASSERT(req_reg[0].type.val == CLITYPE_T::STORE);
        //NVHLS_ASSERT(req_reg[0].opcode == STORE);
      }
      else if (mem_req[1].PopNB(req_reg[0])) {
        valids[0] = 1;
        NVHLS_ASSERT(req_reg[0].type.val == CLITYPE_T::LOAD);
        //NVHLS_ASSERT(req_reg[0].opcode == LOAD);
      }
      
      valids[1] = 0;
      if (mem_req[2].PopNB(req_reg[1])) {
        valids[1] = 1;
        NVHLS_ASSERT(req_reg[1].type.val == CLITYPE_T::STORE);
        //NVHLS_ASSERT(req_reg[1].opcode == STORE);
      }
      else if (mem_req[3].PopNB(req_reg[1])) {
        valids[1] = 1;
        NVHLS_ASSERT(req_reg[1].type.val == CLITYPE_T::LOAD);
        //NVHLS_ASSERT(req_reg[1].opcode == LOAD);
      }
      
      if (valids[0] == 1 && mem_id == 0) 
        req_inter[0].Push(req_reg[0]);
      else if (valids[1] == 1 && mem_id == 1) 
        req_inter[0].Push(req_reg[1]);
      
      if (valids[0] == 1 && mem_id == 1) 
        req_inter[1].Push(req_reg[0]);
      else if (valids[1] == 1 && mem_id == 0) 
        req_inter[1].Push(req_reg[1]);
        
      bool tmp;
      if (flip_inter[0].PopNB(tmp)) {
        mem_id = tmp;
      }
      wait();
    }  
  }

  void MergeRspRun() {
    flip_inter[1].ResetRead();

    #pragma hls_unroll yes
    for (int i=0; i<2; i++) rsp_inter[i].ResetRead();
    #pragma hls_unroll yes
    for (int i=0; i<2; i++) mem_rsp[i].Reset();
    
    bool mem_id = 0;
    #pragma hls_pipeline_init_interval 1
    while(1) {
      rsp_t rsp_reg[2];
      
      if (rsp_inter[0].PopNB(rsp_reg[0])) {
        if (mem_id == 0)  mem_rsp[0].Push(rsp_reg[0]);
        else              mem_rsp[1].Push(rsp_reg[0]);
      }
      else if (rsp_inter[1].PopNB(rsp_reg[1])) {
        if (mem_id == 1)  mem_rsp[0].Push(rsp_reg[1]);
        else              mem_rsp[1].Push(rsp_reg[1]);     
      }

      bool tmp;
      if (flip_inter[1].PopNB(tmp)) {
        mem_id = tmp;
      }       
      wait();
    }
  }
};

#endif

