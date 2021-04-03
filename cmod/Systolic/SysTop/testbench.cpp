#include "SysTop.h"
#include "../include/SysSpec.h"
#include "../include/AxiSpec.h"
#include "../include/utils.h"
//#include "testbench/Master.h"

#include <systemc.h>
#include <mc_scverify.h>
#include <nvhls_int.h>

#include <vector>

#define NVHLS_VERIFY_BLOCKS (SysTop)
#include <nvhls_verify.h>
using namespace::std;

#include <testbench/nvhls_rand.h>
//#include <testbench/Master.h>

#include "testbench/Slave.h"

// W/I/O dimensions
const static int N = spec::N;
const static int M = N;

// bias left shift, 
const static bool  IsRelu = 1;
const static int   BiasShift = 6;
const static int   AccumShift = 10;
const static int   AccumMul   = 93;

// base address
const static unsigned w_rd_base = 0x4000;
const static unsigned d_rd_base = 0x8000;
const static unsigned d_wr_base = 0xC000;

int ERROR = 0;

vector<int>   Count(N, 0);
SC_MODULE (Master) {
  sc_in<bool> clk;
  sc_in<bool> rst;
  sc_in<bool> interrupt;
  typename spec::AxiConf::axi4_conf::read::template master<> if_rd;
  typename spec::AxiConf::axi4_conf::write::template master<> if_wr;

  SC_CTOR(Master)
        : if_rd("if_rd"), 
          if_wr("if_wr")
  {
    SC_THREAD(Run);
    sensitive << clk.pos();
    async_reset_signal_is(rst, false);
  }

  void MasterAccess(const bool rw, NVUINT32 addr, NVUINT32& data) {
    typename spec::AxiConf::axi4_conf::AddrPayload    a_pld;
    typename spec::AxiConf::axi4_conf::ReadPayload   rd_pld;
    typename spec::AxiConf::axi4_conf::WritePayload  wr_pld;
    typename spec::AxiConf::axi4_conf::WRespPayload   b_pld;

    a_pld.len = 0;
    a_pld.addr = addr;

    if (rw == 0) {  // read and set data 
      if_rd.ar.Push(a_pld);
      rd_pld = if_rd.r.Pop();
      data = rd_pld.data;
      wait();
    }
    else {          // write
      if_wr.aw.Push(a_pld);
      wait();
      wr_pld.data = data;
      wr_pld.wstrb = ~0;
      wr_pld.last = 1;
      if_wr.w.Push(wr_pld);
      wait();
      if_wr.b.Pop();
      wait();
    }
  }
  void Run() {
    if_rd.reset();
    if_wr.reset();

    wait(100);

    cout << "@" << sc_time_stamp() << " Start Axi Config[2-5] " << endl;
    NVUINT32 data=0, data_read=0;

  
    // Config_reg[2-5]
    data += (M-1);  // M_1
    data += IsRelu << 8;
    data += BiasShift << 16;
    data += AccumShift << 20;
    data += AccumMul << 24;
    MasterAccess(1, 0x08, data);   
    MasterAccess(0, 0x08, data_read);
    assert (data == data_read);
  
  
    data = w_rd_base;
    MasterAccess(1, 0x0C, data);
    MasterAccess(0, 0x0C, data_read);
    assert (data == data_read);

    data = d_rd_base;
    MasterAccess(1, 0x10, data);
    MasterAccess(0, 0x10, data_read);
    assert (data == data_read);

    data = d_wr_base;
    MasterAccess(1, 0x14, data);
    MasterAccess(0, 0x14, data_read);
    assert (data == data_read);

    cout << "@" << sc_time_stamp() << " Finish Axi Config[2-5] " << endl;
    
    // Start Master Input Read/Write test
    // * make sure slave memory for master read is already set before simulation starts
    // Start input master read 
    cout << "@" << sc_time_stamp() << " Start Input Master Read " << endl;
    data = 0x02;
    MasterAccess(1, 0x04, data);
    MasterAccess(0, 0x04, data_read);
    assert (data == data_read); 
    // wait for IRQ
    while (interrupt.read() == 0) wait();
    cout << "@" << sc_time_stamp() << " Finish Input Master Read " << endl;
    wait();
  

    /*
    cout << "@" << sc_time_stamp() << " Flip Input Memory" << endl;
    // Flip memory 
    //   0x00: mem[0]: input, mem[1]: output
    //   0x01: mem[0]: output, mem[1]: input
    data = 0x01;
    MasterAccess(1, 0x18, data);
    MasterAccess(0, 0x18, data_read);
    assert (data == data_read);
    wait();

    cout << "@" << sc_time_stamp() << " Start Input Master Write" << endl;
    // Start input master write
    data = 0x03;
    MasterAccess(1, 0x04, data);
    MasterAccess(0, 0x04, data_read);
    assert (data == data_read);
    // wait for IRQ
    while (interrupt.read() == 0) wait();
    cout << "@" << sc_time_stamp() << " Finish Input Master Write" << endl;
    */
  
  
    // weight master read
    cout << "@" << sc_time_stamp() << " Start Weight Master Read " << endl;
    data = 0x01;
    MasterAccess(1, 0x04, data);
    MasterAccess(0, 0x04, data_read);
    assert (data == data_read); 
    // wait for IRQ
    while (interrupt.read() == 0) wait();
    cout << "@" << sc_time_stamp() << " Finish Weight Master Read " << endl;
    wait();
   
    // start computation 
    cout << "@" << sc_time_stamp() << " Start Computation" << endl;
    data = 0x04;
    MasterAccess(1, 0x04, data);
    MasterAccess(0, 0x04, data_read);
    assert (data == data_read); 
    // wait for IRQ
    while (interrupt.read() == 0) wait();
    cout << "@" << sc_time_stamp() << " Finish Computation" << endl;
    wait();

    cout << "@" << sc_time_stamp() << " Start Input Master Write" << endl;
    // Start input master write
    data = 0x03;
    MasterAccess(1, 0x04, data);
    MasterAccess(0, 0x04, data_read);
    assert (data == data_read);
    // wait for IRQ
    while (interrupt.read() == 0) wait();
    cout << "@" << sc_time_stamp() << " Finish Input Master Write" << endl;
   
    wait();
  }
};

SC_MODULE (testbench) {
  sc_clock clk;
  sc_signal<bool> rst;
  
  sc_signal<bool> interrupt;

  // Testbench master, Control.h Slave 
  typename spec::AxiConf::axi4_conf::read::template chan<> axi_conf_rd;
  typename spec::AxiConf::axi4_conf::write::template chan<> axi_conf_wr;

  // Slave slave, InputAxi WeightAxi Master
  typename spec::AxiData::axi4_data::read::template chan<> axi_data_rd;
  typename spec::AxiData::axi4_data::write::template chan<> axi_data_wr;
  
  //vector<vector<spec::InputType>> W_mat;
  //vector<vector<spec::InputType>> I_mat;
  //vector<vector<spec::InputType>> B_mat;
  vector<vector<spec::AccumType>> O_mat;
  
  NVHLS_DESIGN(SysTop) top;
  Slave<spec::AxiData::axiCfg> slave;
  Master master;
  SC_HAS_PROCESS(testbench);
  testbench(sc_module_name name)
  : sc_module(name),
    clk("clk", 1, SC_NS, 0.5,0,SC_NS,true),
    rst("rst"),
    axi_conf_rd("axi_conf_rd"),
    axi_conf_wr("axi_conf_wr"),
    axi_data_rd("axi_data_rd"),
    axi_data_wr("axi_data_wr"),
    top("top"),
    slave("slave"),
    master("master")
  {
    top.clk(clk);
    top.rst(rst);
    top.if_axi_rd(axi_conf_rd);
    top.if_axi_wr(axi_conf_wr);
    top.if_data_rd(axi_data_rd);
    top.if_data_wr(axi_data_wr);
    top.interrupt(interrupt);
    
    slave.clk(clk);
    slave.reset_bar(rst);
    slave.if_rd(axi_data_rd);
    slave.if_wr(axi_data_wr);

    master.clk(clk);
    master.rst(rst);
    master.if_rd(axi_conf_rd);
    master.if_wr(axi_conf_wr);
    master.interrupt(interrupt);

    SC_THREAD(Run);
  }


  void Run() {
    //printf("check check\n");
    rst = 1;
    wait(10, SC_NS);
    rst = 0;
    cout << "@" << sc_time_stamp() << " Asserting Reset " << endl;
    wait(1, SC_NS);
    cout << "@" << sc_time_stamp() << " Deasserting Reset " << endl;
    rst = 1;
    
    
    wait(5000,SC_NS);
    cout << "@" << sc_time_stamp() << " Stop " << endl ;


    //cout << "Print Bias" << endl;
    //for (int i = 0; i < N; i++) {
    //  cout << top.sa_inst.bias_vec[i] << "\t";
    //}
    //cout << endl;

    //cout << "Print Weight" << endl;
    //for (int i = 0; i < N; i++) {
    //  for (int j = 0; j < N; j++) {
    //    cout << top.sa_inst.pe_array[i][j]->weight_reg << "\t";
    //  }
    //  cout << endl;
    //}    
    //cout << endl;


    // Result Checking, directly access slave memory

    // read input from slave (memory mapping format) 
    // a00 a10 a20 a30   -> col 0
    // a01 a11 a21 a31   -> col 1
    // ...
    // ...
    // ...
    
    int addr = d_wr_base;
    for (int j = 0; j < M; j++) {    // each col
      for (int i = 0; i < N; i+=8) {  // each row
        NVUINT64 data = 0;
        data = slave.localMem[addr];
        for (int k = 0; k < 8; k++) {
          NVINT8 result = nvhls::get_slc<8>(data, k*8);
          NVINT8 ref = O_mat[i+k][j];
          cout << dec << " i=" << i+k << " j=" << j 
                << "\t result=" << result 
                << "\t | ref=" << ref << hex
                << "\t | addr=" << addr << dec << endl;
          assert (result == ref);
        }
        addr += 8;
      }
    }
    
    //cout << hex << slave.localMem[0x0C000] << endl;
    //cout << hex << slave.localMem[0x0C008] << endl;

    sc_stop();
  }
};

int sc_main(int argc, char *argv[])
{
  //nvhls::set_random_seed();


  // Weight N*N 
  // Input N*M
  // Output N*M
  
  vector<vector<spec::InputType>> W_mat = GetMat<spec::InputType>(N, N); 
  vector<vector<spec::InputType>> I_mat = GetMat<spec::InputType>(N, M);  
  vector<vector<spec::InputType>> B_mat = GetMat<spec::InputType>(N, 1);

  vector<vector<spec::AccumType>> O_mat;
  O_mat = MatMul<spec::InputType, spec::AccumType>(W_mat, I_mat); 
 
  // Add bias/mul shift  for each column output
  // 
  for (int j=0; j<M; j++) {    // for each column
    for (int i=0; i<N; i++) {  // for each element in column
      //cout << O_mat[i][j] << "\t";
      spec::AccumType tmp = B_mat[i][0]; 
      O_mat[i][j] = O_mat[i][j] + (tmp << BiasShift);
      //cout << O_mat[i][j] << "\t";
      if (IsRelu && O_mat[i][j] < 0) O_mat[i][j] = 0;
      O_mat[i][j] = O_mat[i][j] * AccumMul;
      
      //cout << O_mat[i][j] << "\t";
      
      //cout << O_mat[i][j] << endl;
      O_mat[i][j] = O_mat[i][j] >> AccumShift;
      if (O_mat[i][j] > 127) O_mat[i][j] = 127;
      if (O_mat[i][j] < -128) O_mat[i][j] = -128;
      //cout << O_mat[i][j] << endl;
    }
    //cout << endl;
  }

  //cout << "Weight Matrix " << endl; 
  //PrintMat(W_mat);
  //cout << "Input Matrix " << endl; 
  //PrintMat(I_mat);
  //cout << "Reference Output Matrix " << endl; 
  //PrintMat(O_mat);

  testbench my_testbench("my_testbench");

  
  //my_testbench.W_mat = W_mat;
  //my_testbench.I_mat = I_mat;
  //my_testbench.B_mat = B_mat;
  my_testbench.O_mat = O_mat;
  
  //cout << "Weight Matrix " << endl; 
  //PrintMat(W_mat);
  //cout << "Input Matrix " << endl; 
  //PrintMat(I_mat);
  //cout << "Bias Matrix " << endl; 
  //PrintMat(B_mat);
  //cout << "Reference Output Matrix " << endl; 
  //PrintMat(O_mat);
  
  // write weight to source (make sure data pattern is correct)
  // bias,  row N-1 -> 0 of weight,
  // need to follow this format to store bias/weight into slave memory
  // b00 b10 b20 b30
  // w03 w13 w23 w33
  // w02 w12 w22 w32 
  // w01 w11 w21 w31
  // w00 w10 w20 w30
  // 
  int addr = w_rd_base;
  // store bias
  for (int j = 0; j < N; j+=8) {
    NVUINT64 data = 0;
    for (int k = 0; k < 8; k++) {
      data.set_slc<8>(k*8, B_mat[j+k][0]);
    }
    my_testbench.slave.localMem[addr] = data;
    //cout << hex << "slave weight: " << data << endl; 
    addr += 8;
  }

  // store weight
  for (int j = N-1; j >= 0; j--) {
    for (int i = 0; i < N; i+=8) {
      NVUINT64 data = 0;
      for (int k = 0; k < 8; k++) {
        data.set_slc<8>(k*8, W_mat[i+k][j]);
      }
      my_testbench.slave.localMem[addr] = data;
      //cout << hex << "slave weight: " << data << endl; 
      addr += 8;
    }   
  }

  // write input to slave (memory mapping format) 
  // a00 a10 a20 a30   -> col 0
  // a01 a11 a21 a31   -> col 1
  // ...
  // ...
  // ...
  addr = d_rd_base;
  for (int j = 0; j < M; j++) {    // each col
    for (int i = 0; i < N; i+=8) {  // each row
      NVUINT64 data = 0;
      for (int k = 0; k < 8; k++) {
        data.set_slc<8>(k*8, I_mat[i+k][j]);
      }
      my_testbench.slave.localMem[addr] = data;
      //cout << addr << "\t" << my_testbench.slave.localMem[addr] << endl;
      addr += 8;
    } 
  }   

  cout << "SC_START" << endl;
  sc_start();

  cout << "CMODEL PASS" << endl;
  return 0;
};

