#ifndef __SYSBIAS_H__
#define __SYSBIAS_H__


#include <systemc.h>
#include <nvhls_int.h>
#include <nvhls_connections.h>

#include "../SysPE/SysPE.h"
#include "string"
#include "../include/SysSpec.h"


SC_MODULE(SysBias)
{
  public:
  sc_in_clk     clk;
  sc_in<bool>   rst;
  
  bool    is_relu;
  NVUINT4 bias_left_shift;
  NVUINT4 accum_right_shift;
  NVUINT8 accum_multiplier;
 
  spec::InputType bias;
 
  Connections::In<spec::SysConfig>   sys_config;
  Connections::In<spec::InputType>   weight_in;
  Connections::In<spec::AccumType>   accum_out;
  Connections::Out<spec::InputType>  act_out;

  SC_HAS_PROCESS(SysBias);
  SysBias(sc_module_name name_) : sc_module(name_) {
 
    SC_THREAD (SysBiasRun);
    sensitive << clk.pos();
    NVHLS_NEG_RESET_SIGNAL_IS(rst);
  }
   
  // Receive act_out from col N-1 (PopNB, every cycle)
  // use ResetRead to indicate the output port of channel
  // --last row of weight_inter
  // --accum_out_vec
  // --sys_config
  // --act_out_vec
  void SysBiasRun() {  
    sys_config.Reset();
    weight_in.Reset();
    accum_out.Reset();
    act_out.Reset();
    
    is_relu = 0;
    bias_left_shift = 0;
    accum_right_shift = 0;
    accum_multiplier = 1;
    
    bias = 0;
    
    #pragma hls_pipeline_init_interval 1
    while(1) {
	    spec::AccumType accum_tmp;
      spec::SysConfig sys_config_tmp;
      spec::InputType weight_tmp;
      
      if (sys_config.PopNB(sys_config_tmp)) {
        is_relu = sys_config_tmp.is_relu;
        bias_left_shift = sys_config_tmp.bias_left_shift;
        accum_right_shift = sys_config_tmp.accum_right_shift;
        accum_multiplier = sys_config_tmp.accum_multiplier;
        //cout << "Receive axi systolic array config" << endl;
        //cout << "is_relu: " << is_relu << "\tbias_l: " << bias_left_shift
        //     << "\taccum_r: " << accum_right_shift
        //     << "\taccum_ml: " << accum_multiplier
        //     << endl;
      }
      else if (accum_out.PopNB(accum_tmp)) {
	      spec::AccumType bias_tmp = bias;
	      bias_tmp = bias_tmp << bias_left_shift;
        accum_tmp = accum_tmp + bias_tmp;      
        if ((is_relu==1) && (accum_tmp < 0)) accum_tmp = 0;
	      NVINT32 accum_mul = accum_tmp; 
                accum_mul = accum_mul * accum_multiplier;

	      accum_mul = (accum_mul >> accum_right_shift);
	      if (accum_mul > 127)
	        accum_mul = 127;  // -128~127 truncation
	      else if (accum_mul < -128)
	        accum_mul = -128;
        // Truncation
        spec::InputType act_out_reg = accum_mul;
        // Push output
        act_out.Push(act_out_reg);
      }
      else if(weight_in.PopNB(weight_tmp)){
        bias = weight_tmp;
	    }
      wait();
	  }
  }
};


#endif 

