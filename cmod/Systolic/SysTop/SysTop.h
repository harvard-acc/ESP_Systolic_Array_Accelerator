#ifndef __SYSTOP_H__
#define __SYSTOP_H__

#include <systemc.h>
#include <nvhls_int.h>
#include <nvhls_connections.h>
#include <string>

#include "../include/SysSpec.h"
#include "../include/AxiSpec.h"
#include "../SysPE/SysPE.h"
#include "../SysArray/SysArray.h"
#include "../InputSetup/InputSetup.h"
#include "../Control/Control.h"
#include "../WeightAxi/WeightAxi.h"
#include "../InputAxi/InputAxi.h"

SC_MODULE(SysTop)
{
  public:
  sc_in_clk     clk;
  sc_in<bool>   rst;

  // Array Size 
  const static int N = spec::N;

  // IRQ
  sc_out<bool> interrupt;

  // Axi Slave Config
  typename spec::AxiConf::axi4_conf::read::template slave<>   if_axi_rd;
  typename spec::AxiConf::axi4_conf::write::template slave<>  if_axi_wr;

  // Axi Master Data
  typename spec::AxiData::axi4_data::read::template master<>   if_data_rd;
  typename spec::AxiData::axi4_data::write::template master<>  if_data_wr;


  // Axi Master Weight
  typename spec::AxiData::axi4_data::read::template chan<>   if_weight_rd;
  typename spec::AxiData::axi4_data::write::template chan<>  if_weight_wr;

  // Axi Master Input
  typename spec::AxiData::axi4_data::read::template chan<>   if_input_rd;
  typename spec::AxiData::axi4_data::write::template chan<>  if_input_wr;
  // old I/O 
  //Connections::In<InputSetup::WriteReq>   write_req;
  //Connections::In<InputSetup::IndexType>  start; // Push input #col M as start signal
  //Connections::In<spec::InputType>      weight_in_vec[N];
  //Connections::Out<spec::AccumType>       accum_out_vec[N];  

  // Systolic array Interconnect
  Connections::Combinational<spec::InputType>  act_in_vec[N];
  Connections::Combinational<spec::InputType>  act_out_vec[N];
  Connections::Combinational<spec::InputType>  weight_in_vec[N];

  // Memory Inteconnect
  Connections::Combinational<Memory::req_t>  mem_req[4];
  Connections::Combinational<Memory::rsp_t>  mem_rsp[2];

  // Control Config
  Connections::Combinational<spec::SysConfig> sys_config;
  Connections::Combinational<bool> flip_mem;
  // Control Trigger
  Connections::Combinational<NVUINT32> weight_axi_start;
  Connections::Combinational<InputAxi::MasterTrig> input_rd_axi_start;
  Connections::Combinational<InputAxi::MasterTrig> input_wr_axi_start;
  Connections::Combinational<Memory::IndexType>    com_start;
  // Control IRQ
  Connections::Combinational<bool> IRQs[4];

  SysArray    sa_inst; 
  InputSetup  is_inst;
  Control     ct_inst; 
  WeightAxi   wa_inst;
  InputAxi    ia_inst;
  Memory      mm_inst;
  // TODO, need an AXI arbiter for 2 Masters
  spec::AxiData::ArbiterData axi_arbiter;
  
  SC_HAS_PROCESS(SysTop);
  SysTop(sc_module_name name_) : sc_module(name_),
    if_axi_rd("if_axi_rd"),
    if_axi_wr("if_axi_wr"),
    if_data_rd("if_data_rd"),
    if_data_wr("if_data_wr"),
    if_weight_rd("if_weight_rd"),
    if_weight_wr("if_weight_wr"),
    if_input_rd("if_input_rd"),
    if_input_wr("if_input_wr"),
    sa_inst("sa_inst"),
    is_inst("is_inst"),
    ct_inst("ct_inst"),
    wa_inst("wa_inst"),
    ia_inst("ia_inst"),
    mm_inst("mm_inst"),
    axi_arbiter("axi_arbiter")
  {
    // Control unit 
    ct_inst.clk(clk);
    ct_inst.rst(rst); 
    ct_inst.interrupt(interrupt); // IRQ sc_out
    ct_inst.if_axi_rd(if_axi_rd); // axi slave
    ct_inst.if_axi_wr(if_axi_wr); // axi slave
    ct_inst.sys_config(sys_config);                 // Config
    ct_inst.weight_axi_start(weight_axi_start);     // weight master read
    ct_inst.input_rd_axi_start(input_rd_axi_start); // input master read
    ct_inst.input_wr_axi_start(input_wr_axi_start); // input master write
    ct_inst.com_start(com_start);     // compute startg 
    ct_inst.flip_mem(flip_mem);       // flip mem
    for (int i = 0; i < 4; i++)       // irq in
      ct_inst.IRQs[i](IRQs[i]);

    // Systolic array
    sa_inst.clk(clk);
    sa_inst.rst(rst);
    sa_inst.sys_config(sys_config);
    for (int i=0; i < N; i++) {
      sa_inst.weight_in_vec[i](weight_in_vec[i]);
      sa_inst.act_out_vec[i](act_out_vec[i]);
      sa_inst.act_in_vec[i](act_in_vec[i]);
    }

    // Memory
    mm_inst.clk(clk);
    mm_inst.rst(rst);
    mm_inst.flip_mem(flip_mem);
    for (int i = 0; i < 4; i++) mm_inst.mem_req[i](mem_req[i]);
    for (int i = 0; i < 2; i++) mm_inst.mem_rsp[i](mem_rsp[i]);

    // Weight Axi Master IRQs[0]
    wa_inst.clk(clk);
    wa_inst.rst(rst); 
    wa_inst.start(weight_axi_start);
    wa_inst.rd_IRQ(IRQs[0]);
    wa_inst.if_data_rd(if_weight_rd);
    wa_inst.if_data_wr(if_weight_wr);
    for (int i=0; i < N; i++) {
      wa_inst.weight_in_vec[i](weight_in_vec[i]);
    }

    // Input Axi Master  read: IRQs[1] write: IRQs[2]
    ia_inst.clk(clk);
    ia_inst.rst(rst); 
    ia_inst.master_read(input_rd_axi_start);
    ia_inst.master_write(input_wr_axi_start);
    ia_inst.rd_IRQ(IRQs[1]);
    ia_inst.wr_IRQ(IRQs[2]);
    ia_inst.if_data_rd(if_input_rd);
    ia_inst.if_data_wr(if_input_wr);
    ia_inst.mem_rd_req(mem_req[3]);
    ia_inst.mem_wr_req(mem_req[0]);
    ia_inst.mem_rd_rsp(mem_rsp[1]);

    // Input Setup compute IRQs[3]
    is_inst.clk(clk);
    is_inst.rst(rst);
    is_inst.start(com_start);
    is_inst.com_IRQ(IRQs[3]);
    is_inst.rd_req(mem_req[1]);
    is_inst.wr_req(mem_req[2]);
    is_inst.rd_rsp(mem_rsp[0]);
    for (int i=0; i < N; i++) {
      is_inst.act_out_vec[i](act_out_vec[i]);
      is_inst.act_in_vec[i](act_in_vec[i]);
    }

    // TODO: axi arbiter
    axi_arbiter.clk(clk);
    axi_arbiter.reset_bar(rst);

    axi_arbiter.axi_rd_m_ar[0](if_weight_rd.ar);
    axi_arbiter.axi_rd_m_r [0](if_weight_rd.r);
    axi_arbiter.axi_wr_m_aw[0](if_weight_wr.aw);
    axi_arbiter.axi_wr_m_w [0](if_weight_wr.w);
    axi_arbiter.axi_wr_m_b [0](if_weight_wr.b);


    axi_arbiter.axi_rd_m_ar[1](if_input_rd.ar);
    axi_arbiter.axi_rd_m_r [1](if_input_rd.r);
    axi_arbiter.axi_wr_m_aw[1](if_input_wr.aw);
    axi_arbiter.axi_wr_m_w [1](if_input_wr.w);
    axi_arbiter.axi_wr_m_b [1](if_input_wr.b);

    axi_arbiter.axi_rd_s(if_data_rd);
    axi_arbiter.axi_wr_s(if_data_wr);

  }
};


#endif

