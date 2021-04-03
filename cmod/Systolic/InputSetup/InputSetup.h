#ifndef __INPUTSETUP_H__
#define __INPUTSETUP_H__

#include <systemc.h>
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <nvhls_vector.h>

#include <string>

#include "../Memory/Memory.h"
#include "../SysPE/SysPE.h"
#include "../SysArray/SysArray.h"
#include "../include/SysSpec.h"

SC_MODULE(InputSetup)
{
 public: 
  sc_in_clk     clk;
  sc_in<bool>   rst;

  typedef spec::InputType InputType;  
  static const int N = spec::N;     // # banks = N

  // Start with M_1 to start Sytolic array data read
  // after receive all outputs, fire interrupt (Use counter to count last col output)

  // I/O 
  Connections::In<Memory::IndexType>  start;      // push input "#col-1" m_1 as start signal
  Connections::Out<bool>              com_IRQ;    // computation IRQ
  
  Connections::In<InputType>      act_out_vec[N];  // from SysArray
  Connections::Out<InputType>     act_in_vec[N];   // to SysArray

  // Memory channels
  Connections::Out<Memory::req_t>   rd_req; 
  Connections::In<Memory::rsp_t>    rd_rsp; 
  Connections::Out<Memory::req_t>   wr_req; 

  // interconnect trigger
  Connections::Combinational<Memory::IndexType>   start_wr;

  SC_HAS_PROCESS(InputSetup);
  InputSetup(sc_module_name name_) : sc_module(name_) {

    SC_THREAD (ReadReqRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst); 
    
    SC_THREAD (ReadRspRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);

    SC_THREAD (WriteReqRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);
  
  }

  // The main process that generate read req that matches
  // systolic array data setup
  void ReadReqRun() {
    start.Reset();
    rd_req.Reset();   
    start_wr.ResetWrite();

    //#pragma hls_pipeline_init_interval 1
    while(1) {
      Memory::IndexType M_1;
      M_1 = start.Pop();
      //cout << "compute M_1 " << M_1 << endl; 
      start_wr.Push(M_1);
      // Now we want to generate sequencial read that 
      // matches SysArray setup
      // 1, 2, 3 ...  N-1, N ... N, N-1 .. 3, 2, 1
       
      //IndexType ctr[N];
      // Must explicitly initialize
      //#pragma hls_unroll yes
      //for (int i = 0; i < N; i++) {
      //  ctr[i] = 0;
      //}
      Memory::req_t req_reg;
      req_reg.type.val = CLITYPE_T::LOAD;
      //req_reg.opcode = LOAD;

      #pragma hls_pipeline_init_interval 1
      //for (int i = 0; i < N+M-1; i++) { 
      for (int i = 0; i < N+M_1; i++) { 
        #pragma hls_unroll yes
        for (int j = 0; j < N; j++) {
          //req_reg.addr[j] = ctr[j]*N + j;
          if (i >= j && (i-j) <= M_1) {
            req_reg.valids[j] = 1;
            req_reg.addr[j] = N*(i-j) + j;
          }
          else {
            req_reg.valids[j] = 0;
            req_reg.addr[j] = 0;
          }
          //if (i < N-1 && j <= i) {
          //  req_reg.valids[j] = true;
          //  ctr[j] += 1;
          //}
          //else if (i >= N-1 && i < M) {
          //else if (i >= N-1 && i < M_1+1) {
          //  req_reg.valids[j] = true;
          //  ctr[j] += 1;
          //}
          //else if (i >= M && j > i-M) {
          //else if (i >= M_1+1 && j > i-M_1-1) {
          //  req_reg.valids[j] = true;
          //  ctr[j] += 1;
          //}
          //else {
          //  req_reg.valids[j] = false;
          //}
   	    } 
       // cout << "@" << sc_time_stamp() << "\tm=" << i << endl;
        rd_req.Push(req_reg);
        wait();
      }
      wait();
    }
  }

  // Push data read from SRAM to SysArray
  // only push banks with valid rsp
  void ReadRspRun() {
    #pragma hls_unroll yes
    for (int i = 0; i < N; i++) {
      act_in_vec[i].Reset();
    }
    rd_rsp.Reset();

    #pragma hls_pipeline_init_interval 1
    while(1) {
      Memory::rsp_t rsp_reg; 
      if (rd_rsp.PopNB(rsp_reg)) {
        #pragma hls_unroll yes
        for (int i = 0; i < N; i++) {
          if (rsp_reg.valids[i] == true)
            act_in_vec[i].Push(rsp_reg.data[i]);
        }
      }
      wait();
    }
  }

  void WriteReqRun() {
    #pragma hls_unroll yes
    for (int i = 0; i < N; i++) {
      act_out_vec[i].Reset();
    }
    wr_req.Reset();
    start_wr.ResetRead();
    com_IRQ.Reset();
    while(1) {
      Memory::IndexType M_1;
      M_1 = start_wr.Pop();
      //cout << "compute write back M_1 " << M_1 << endl; 

      // FIXME: can use lower bit width for counters
      NVUINT16 ctr[N];
      #pragma hls_unroll yes
      for (int i = 0; i < N; i++) {
        ctr[i] = 0;
      }

      bool is_done = 0; 
      #pragma hls_pipeline_init_interval 1
      while (!is_done) {
        Memory::req_t req_reg;
        
        req_reg.type.val = CLITYPE_T::STORE;
        //req_reg.opcode = STORE;
        
        NVUINTW(N) valids = 0;
        InputType act_out_regs[N];
        
        #pragma hls_unroll yes
        for (int i = 0; i < N; i++) {
          valids[i] = act_out_vec[i].PopNB(act_out_regs[i]);
          req_reg.valids[i] = valids[i];
          req_reg.addr[i] = i + ctr[i]*N;
          req_reg.data[i] = act_out_regs[i];
          if (valids[i]) {
            ctr[i] += 1;
          }
        }
        
        if (valids != 0) {
          //cout << "\t\t" << req_reg.data[0] << "\t" << req_reg.data[31] << endl;
          wr_req.Push(req_reg);
        }
        //cout << "ctr[0], ctr[31]  "  << ctr[0] << "\t" << ctr[31] << endl;

        if (ctr[N-1] - 1 == M_1) {
          NVHLS_ASSERT(ctr[0]-1 == M_1);
          is_done = 1;
        }

        wait();
      }
      //cout << "compute IRQ" << endl;

      com_IRQ.Push(1);
      wait();
    }
  }
};

#endif

