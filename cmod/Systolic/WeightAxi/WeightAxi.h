/* Weight setup with master port to 
 * actively request data from other memory
 * --master read request is triggered by controller
 * --no master write request
 */


#ifndef __WEIGHT_AXI_H__
#define __WEIGHT_AXI_H__


#include <systemc.h>
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <nvhls_vector.h>

#include "../include/SysSpec.h"
#include "../include/AxiSpec.h"

SC_MODULE(WeightAxi)
{
  const static int N = spec::N;
  // number of read for 64-bit axi, 
  // should be smaller than maxBurstSize
  //
  // (N byte >> 3)*(N+1) row, 
  //   N rows of weight and 1 row of bias
  // N = 32 => 132 => 131
  // N = 16 => 34  => 33
  // N = 8  => 9   => 8
 
  // append -1 to match Axi Len format
  //  # of data = AxLEN[7:0] + 1
  //  rd_len should not exceed 255
  const static int rd_len = (spec::N >> 3)*(spec::N+1) - 1;
  const static int num_read_per_row = N >> 3;   // use one read to set 8 cols
 public:
  sc_in_clk     clk;
  sc_in<bool>   rst;
  
  // Axi Master Interface
  typename spec::AxiData::axi4_data::read::template master<> if_data_rd;
  typename spec::AxiData::axi4_data::write::template master<> if_data_wr;

  // 32 bit start signal to indicate base address
  Connections::In<NVUINT32> start;                    // start with base_addr
  Connections::Out<spec::InputType> weight_in_vec[N]; // output weight_in_vec or systolic array
  Connections::Out<bool> rd_IRQ;                      // rd_IRQ -> IRQ[0]
  
  typename spec::AxiData::axi4_data::AddrPayload addr_pld;
  typename spec::AxiData::axi4_data::ReadPayload data_pld;

  SC_HAS_PROCESS(WeightAxi);
  WeightAxi(sc_module_name name_) : sc_module(name_),
    if_data_rd("if_data_rd"),
    if_data_wr("if_data_wr")
  {
    SC_THREAD (WeightAxiRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);
  }

  void WeightAxiRun() {
    // Reset     
    #pragma hls_unroll yes
    for (int i = 0; i < N; i++) {
      weight_in_vec[i].Reset();  
    }
    start.Reset();
    if_data_rd.reset();
    if_data_wr.reset();
    rd_IRQ.Reset();

    while(1) {
      NVUINT32 base_addr;
      base_addr = start.Pop();   // Start signal with base addr

      // Master read request if_data_read.ar push
      addr_pld.addr = base_addr; // (1) base addr
      addr_pld.len  = rd_len;    // (2) read length

      // Blocking Push
      if_data_rd.ar.Push(addr_pld);

      // Needs to ensure that we can recieve weight every cycle 
      NVUINT4 col_ctr = 0;       // col counter  
      #pragma hls_pipeline_init_interval 1
      for (int i = 0; i <= rd_len; i++) {
        data_pld = if_data_rd.r.Pop();  // pop for each received data
        
        //cout << hex << data_pld.data << endl;

        // push to 8 8-bit weight to weight_in_vec
        //   only select 8 channels at a time, 
	      #pragma hls_unroll yes
	      for (int j = 0; j < 8; j++) {
	        spec::InputType tmp = nvhls::get_slc<8>(data_pld.data, 8*j);
          weight_in_vec[j+col_ctr*8].Push(tmp);
        }

        col_ctr++;
        if (col_ctr == num_read_per_row) 
	        col_ctr = 0;
        wait();
      }

      // Complete master read request, send IRQ back 
      rd_IRQ.Push(1);
      wait();
    }
  }
};

#endif
