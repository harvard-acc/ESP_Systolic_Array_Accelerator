/*
 * Systolic Array Module to include N*N of SysPEs
 * Update 0531 extend function with bias add, shift mul 
 *   and related config
 */

#ifndef __SYSARRAY_H__
#define __SYSARRAY_H__

#include <systemc.h>
#include <nvhls_int.h>
#include <nvhls_connections.h>

#include "../SysPE/SysPE.h"
#include "string"
#include "../include/SysSpec.h"
#include "../SysBias/SysBias.h"

SC_MODULE(SysArray)
{
  public:
  sc_in_clk     clk;
  sc_in<bool>   rst;
  
  // Array Size N*N
  const static int N = spec::N;
   
  SysPE* pe_array[N][N];    // SysPE instantiation 
  SysBias* bias_vec[N];     // SysBias instantiation

  Connections::In<spec::SysConfig>  sys_config;

  // I/O
  // FIXME: another weight input module to reduce input channels
  Connections::In<spec::InputType>   weight_in_vec[N];
  Connections::In<spec::InputType>   act_in_vec[N];
  Connections::Out<spec::InputType>  act_out_vec[N];
  //Connections::Out<spec::AccumType>  accum_out_vec[N];

  // Output (0623 update )

  //Connections::Combinational<spec::AccumType> accum_out_vec[N];
  // --Connect to output post processing
  Connections::Combinational<spec::SysConfig> sys_config_out[N];
  
  
  // Interconnection Channels
  Connections::Combinational<spec::InputType> weight_inter[N][N];
  
  Connections::Combinational<spec::InputType> act_inter[N][N]; 
  
  Connections::Combinational<spec::AccumType> accum_inter[N][N];
  
  Connections::Combinational<spec::AccumType> accum_out_vec[N];

  // Bias vector (treated row N of weight, so we have N+1 rows total)
  // --8-bit
  //spec::InputType bias_vec[N];

  SC_HAS_PROCESS(SysArray);
  SysArray(sc_module_name name_) : sc_module(name_) {
    for (int i = 0; i < N; i++) {    // rows
      for (int j = 0; j < N; j++) {  // cols
        pe_array[i][j] = new SysPE(sc_gen_unique_name("pe"));
        pe_array[i][j]->clk(clk);
        pe_array[i][j]->rst(rst);
        
        // Weight Connections
        if (i == 0) {    // from Array weight (row 0)
          pe_array[i][j]->weight_in(weight_in_vec[j]);
          pe_array[i][j]->weight_out(weight_inter[i][j]);
        }
        else {           // from Array weight (row 1, 2, N-1)
          pe_array[i][j]->weight_in(weight_inter[i-1][j]);
          pe_array[i][j]->weight_out(weight_inter[i][j]);
        }
        // Act Connections
        if (j == 0) {  // from Array input (col 0)
          pe_array[i][j]->act_in(act_in_vec[i]);
          pe_array[i][j]->act_out(act_inter[i][j]);
        }
        else {         // from Array input (col 1, 2...)
          pe_array[i][j]->act_in(act_inter[i][j-1]);
          pe_array[i][j]->act_out(act_inter[i][j]);
        }
        // Accum Connections
        if (i != N-1) {  // from Array Accum (row 0, 1, N-2)
          pe_array[i][j]->accum_in(accum_inter[i][j]);
          pe_array[i][j]->accum_out(accum_inter[i+1][j]);
        }
        else {           // from Array Accum (row N-1)
          pe_array[i][j]->accum_in(accum_inter[i][j]);
          pe_array[i][j]->accum_out(accum_out_vec[j]);
        }
      }
      bias_vec[i] = new SysBias(sc_gen_unique_name("bias"));
      bias_vec[i]->clk(clk);
      bias_vec[i]->rst(rst);
      bias_vec[i]->sys_config(sys_config_out[i]);    
      bias_vec[i]->weight_in(weight_inter[N-1][i]);
      bias_vec[i]->accum_out(accum_out_vec[i]);
      bias_vec[i]->act_out(act_out_vec[i]);
    }
 
//    SC_THREAD (WeightAccumOutRun);
//    sensitive << clk.pos();
//    NVHLS_NEG_RESET_SIGNAL_IS(rst);

    SC_THREAD (SysConfigRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);
    

    SC_THREAD (ActOutRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);
    
    SC_THREAD (AccumInRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);
  }
   
  // Receive act_out from col N-1 (PopNB, every cycle)
  // use ResetRead to indicate the output port of channel
  // --last row of weight_inter
  // --accum_out_vec
  // --sys_config
  // --act_out_vec
  void SysConfigRun() {
    sys_config.Reset();
    #pragma hls_unroll yes
    for (int i = 0; i < N; i++) {
       sys_config_out[i].ResetWrite();
    }
    
    #pragma hls_pipeline_init_interval 1
    while(1) {
      spec::SysConfig sys_config_reg;
      if (sys_config.PopNB(sys_config_reg)) {
        #pragma hls_unroll yes
        for (int i = 0; i < N; i++) {      
          sys_config_out[i].Push(sys_config_reg);
        }
      }     
      wait();
    }
  }
  // Receive act_out from col N-1 (PopNB, every cycle)
  // use ResetRead to indicate the output port of channel
  void ActOutRun() {
    #pragma hls_unroll yes
    for (int i = 0; i < N; i++) {
       act_inter[i][N-1].ResetRead();
    }
    
    #pragma hls_pipeline_init_interval 1
    while(1) {
      #pragma hls_unroll yes
      for (int i = 0; i < N; i++) {
        spec::InputType act_tmp; 
        act_inter[i][N-1].PopNB(act_tmp);
      }
      wait();
    }
  }

  // Send 0 to accum_in of row 0 (PushNB, every cycle)
  // use ResetWrite to indicate the input port of channel
  void AccumInRun() {
    #pragma hls_unroll yes
    for (int j = 0; j < N; j++) {
       accum_inter[0][j].ResetWrite();
    }

    #pragma hls_pipeline_init_interval 1
    while(1) {
      #pragma hls_unroll yes
      for (int j = 0; j < N; j++) {
        accum_inter[0][j].PushNB(0);
      }
      wait();
    }
  }
  
};

#endif

