#ifndef __CONTROL_H__
#define __CONTROL_H__

/* Controller SC Module for recieving configurations
 * 
 * Update 0530: implement AxiSlave Read/Write
 *   add interruptRun sc_out (1-bit)
 *
 */

#include <systemc.h>
#include <nvhls_int.h>
#include <nvhls_types.h>
#include <nvhls_vector.h>
#include <nvhls_module.h>

#include "../SysArray/SysArray.h"

#include "../InputAxi/InputAxi.h"
#include "../WeightAxi/WeightAxi.h"
#include "../Memory/Memory.h"

#include "../include/SysSpec.h"
#include "../include/AxiSpec.h"

SC_MODULE(Control) {
 public:
  sc_in<bool>  clk;
  sc_in<bool>  rst;
  
  sc_out<bool> interrupt; 
   
  // AXI slave read/write ports
  typename spec::AxiConf::axi4_conf::read::template slave<>   if_axi_rd;
  typename spec::AxiConf::axi4_conf::write::template slave<>  if_axi_wr;

  // Output: Config to SysArray
  Connections::Out<spec::SysConfig> sys_config;
  // Output: Trigger
  // --WeightAxi Read (load), start 1
  // --InputAxi Read (load), start 2  
  // --InputAxi Write (store), start 3
  // --InputSet (start computation), start 4
  Connections::Out<NVUINT32> weight_axi_start;             
  Connections::Out<InputAxi::MasterTrig> input_rd_axi_start;
  Connections::Out<InputAxi::MasterTrig> input_wr_axi_start;
  Connections::Out<Memory::IndexType>    com_start;
  // Flip mem command 
  Connections::Out<bool> flip_mem;

  // Input: IRQs (= #triggers)
  Connections::In<bool> IRQs[4]; 

  // AXI rv channels
  Connections::Combinational<spec::AxiConf::SlaveToRV::Write> rv_in;
  Connections::Combinational<spec::AxiConf::SlaveToRV::Read>  rv_out;
  
  //Connections::Combinational<bool>  IRQ_inter;
  
  // AXI slave to ready/valid/address format
  spec::AxiConf::SlaveToRV   rv_inst;

  // 32-bit config regs, need only 8 entries
  // 0--0x00: dummy
  // 1--0x04: start signal (write only), fire interrupt upon completion 
  // ----data=1 -> master weight read
  // ----data=2 -> master input read
  // ----data=3 -> master input write
  // ----data=4 -> start systolic array
  // 2--0x08: computation config 
  // ----data[00-07]= "M-1" (8-bit, M: 1~256)
  // ----data[08-15]= use relu (1 bit)
  // ----data[16-19]= Bias left shift (4 bit)
  // ----data[20-23]= Accum right shift (4-bit)
  // ----data[24-31]= Accum mul (8-bit)
  // 3--0x0C: weight_read_base
  // 4--0x10: input_read_base
  // 5--0x14: input_write_base
  // 6--0x18: flip memory buffer (no IRQ, write only)
  // 7--0x1C: dummy
  NVUINT32 config_regs[8]; 

  SC_HAS_PROCESS(Control);
  Control(sc_module_name name)
     : sc_module(name),
       clk("clk"),
       rst("rst"),
       if_axi_rd("if_axi_rd"),
       if_axi_wr("if_axi_wr"),
       rv_inst("rv_inst")
  { 
    rv_inst.clk(clk);
    rv_inst.reset_bar(rst);
    rv_inst.if_axi_rd(if_axi_rd);
    rv_inst.if_axi_wr(if_axi_wr);
    rv_inst.if_rv_rd(rv_out);
    rv_inst.if_rv_wr(rv_in);

    SC_THREAD(ControlRun);
      sensitive << clk.pos();
      async_reset_signal_is(rst, false);
    
    SC_THREAD(InterruptRun);
      sensitive << clk.pos();
      async_reset_signal_is(rst, false);
  }

  void ControlRun() {
    // Reset 
    // FIXME: remove reset of config to reduce hardware ?
    #pragma hls_unroll yes 
    for (unsigned i = 0; i < 7; i++) {
      config_regs[i] = 0;
    }

    // Reset AXI alave RV
    rv_out.ResetWrite(); // Output  
    rv_in.ResetRead();   // Input
    
    // Reset AXI slave 
    //if_axi_rd.reset();   // read config, debug
    //if_axi_wr.reset();   // write config, command

    // Reset 
    sys_config.Reset();
    weight_axi_start.Reset();
    input_rd_axi_start.Reset();
    input_wr_axi_start.Reset();
    com_start.Reset();
    flip_mem.Reset();

    // This is controller module, no need II=1
    while(1) {
      spec::AxiConf::SlaveToRV::Write rv_in_reg;
      spec::AxiConf::SlaveToRV::Read  rv_out_reg;
      NVUINT3 index; // 0-7
      bool is_read = 0;
      bool is_write = 0;
      
      // TODO: implement hardware to block config when acc is running
      if (rv_in.PopNB(rv_in_reg)) {
        // use bit [4-2] for 32-bit = 4-byte word size
        index = nvhls::get_slc<3>(rv_in_reg.addr, 2);
         
        //cout << "debug index, data: " << index << "\t" << rv_in_reg.data << endl;
	      
        if (rv_in_reg.rw == 1) { // Write mode
          is_write = 1;
          config_regs[index] = rv_in_reg.data;
        } 
	      else { // Read mode
          is_read = 1;
          rv_out_reg.data = config_regs[index];
	      }
        wait();
      }

      // Flip memory command (no IRQ)
      if ((is_write==1) && (index==6)) {
        bool tmp = (config_regs[6] > 0);
        flip_mem.Push(tmp);
      }

      // Systolic array config
      if ((is_write==1) && (index==2)) {
        spec::SysConfig sys_config_reg;
        sys_config_reg.is_relu            = nvhls::get_slc<1>(config_regs[2], 8); // is_relu
        sys_config_reg.bias_left_shift    = nvhls::get_slc<4>(config_regs[2], 16);// bias_left_shift
        sys_config_reg.accum_right_shift  = nvhls::get_slc<4>(config_regs[2], 20);// accum_right_shift
        sys_config_reg.accum_multiplier   = nvhls::get_slc<8>(config_regs[2], 24);// accum_multiplier
        sys_config.Push(sys_config_reg);
      }

      // Start command, write only 
      if ((is_write==1) && (index==1)) {
        switch (config_regs[1]) {
          case 1: { // Weight Master Read (load)
            NVUINT32 base_addr = config_regs[3]; // base address of weight 
            weight_axi_start.Push(base_addr);
          }
          break; 
          case 2: { // Input Master Read (load from outside)
            InputAxi::MasterTrig trig_reg;
            trig_reg.M_1  = nvhls::get_slc<8>(config_regs[2], 0);
            trig_reg.base_addr = config_regs[4]; // base address of input read 
            input_rd_axi_start.Push(trig_reg);
          }  
          break;
          case 3: { // Input Master Write (Store to outside)
            InputAxi::MasterTrig trig_reg;
            trig_reg.M_1  = nvhls::get_slc<8>(config_regs[2], 0);
            trig_reg.base_addr = config_regs[5]; // base address of input write
            input_wr_axi_start.Push(trig_reg);
          }  
          break;
          case 4: { // Start computation (systolic array config should be issued )
            Memory::IndexType com_start_reg =   nvhls::get_slc<8>(config_regs[2], 0); // M_1
            com_start.Push(com_start_reg);
          }  
          break;
          default: 
            break; // no IRQ, may lead to error
        }
      }
      wait();
   
      // Read response
      if (is_read) {
        //cout << "rv_out.data: " << rv_out_reg.data << endl;
        rv_out.Push(rv_out_reg);
      }
      wait(); 
    }
  }

  // Simple IRQ handler for sending IRQ trigger
  // IRQ[0]: master weight read   
  // IRQ[1]: master input read
  // IRQ[2]: master input weight
  // IRQ[3]: computation
  void InterruptRun() {
    #pragma hls_unroll yes 
    for (unsigned i = 0; i < 4; i++) {
      IRQs[i].Reset();
    }
    // Reset IRO
    interrupt.write(false);

    while(1) {
      NVUINTW(4) irq_valids = 0;
      #pragma hls_unroll yes
      for (int i = 0; i < 4; i++) {
        bool tmp;
        irq_valids[i] = IRQs[i].PopNB(tmp);
      }
      if (irq_valids != 0) {
        interrupt.write(true);
        #pragma hls_pipeline_init_interval 1
        for (unsigned i = 0; i < 10; i++) {
          wait();
        }
        interrupt.write(false);
        wait();
      }
      wait();
    }
  }
};

#endif
