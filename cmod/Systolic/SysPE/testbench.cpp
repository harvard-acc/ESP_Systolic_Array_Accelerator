#include "SysPE.h"
#include <systemc.h>
#include <mc_scverify.h>
#include <nvhls_int.h>

#include <vector>

#define NVHLS_VERIFY_BLOCKS (SysPE)
#include <nvhls_verify.h>
using namespace::std;

#include <testbench/nvhls_rand.h>

#include <../include/SysSpec.h>

SC_MODULE (Source) {
  Connections::Out<spec::InputType> act_in;
  Connections::Out<spec::InputType> weight_in;
  Connections::Out<spec::AccumType> accum_in;
  Connections::In<spec::AccumType>  accum_out;
  Connections::In<spec::InputType>  act_out;
  Connections::In<spec::InputType>  weight_out;
  sc_in <bool> clk;
  sc_in <bool> rst;
  
  bool start_src;  
  vector<spec::InputType> act_list{0, -1, 3, -7, 15, -31, 63, -127};
  vector<spec::AccumType> accum_list{0, -10, 30, -70, 150, -310, 630, -1270};
  spec::InputType weight_data = 10;
  spec::AccumType accum_init = 0;
  spec::InputType act_out_src;
  spec::AccumType accum_out_src;

  SC_CTOR(Source) {
    SC_THREAD(run);
    sensitive << clk.pos();
    async_reset_signal_is(rst, false);
    SC_THREAD(pop_result);
    sensitive << clk.pos();
    async_reset_signal_is(rst, false);
  }

  void run() {
    spec::InputType _act;
    spec::AccumType _acc;
    act_in.Reset();
    weight_in.Reset();
    accum_in.Reset();

    // Wait for initial reset
    wait(20.0, SC_NS);
    wait();
    // Write wait to PE
    weight_in.Push(weight_data);
    wait();

    for (int i=0; i< (int) act_list.size(); i++) {
        _act = act_list[i];
        _acc = accum_list[i];
        act_in.Push(_act);
        accum_in.Push(_acc);
        wait(); 
    } // for  
    wait(5); 
  }// void run()

  void pop_result() {
    weight_out.Reset();
    wait();

    unsigned int i = 0, j = 0;
    bool correct = 1;
    while (1) {    
        spec::InputType tmp;
        if (weight_out.PopNB(tmp)) {
           cout << sc_time_stamp() << ": Received Output Weight:" << " \t " << tmp << endl;
        }
        if (act_out.PopNB(act_out_src)) { 
           cout << sc_time_stamp() << ": Received Output Activation:" << " \t " << act_out_src << "\t| Reference \t" << act_list[i] << endl;
           correct &= (act_list[i] == act_out_src);
	   i++;
        }
        if (accum_out.PopNB(accum_out_src)) {
           int acc_ref = accum_list[j] + act_list[j]*weight_data;
           cout << sc_time_stamp() << ": Received Accumulated Output:" << "\t " << accum_out_src << "\t| Reference \t" << acc_ref << endl;
           correct &= (acc_ref == accum_out_src);
	   j++;
        }
        wait();
      if (i == act_list.size() && j == act_list.size()) break;  
    }// while
    
   if (correct == 1) cout << "Implementation Correct :)" << endl;
   else  cout << "Implementation Incorrect (:" << endl;
  }// void pop_result()
};

SC_MODULE (testbench) {
  sc_clock clk;
  sc_signal<bool> rst;

  Connections::Combinational<spec::InputType> act_in;
  Connections::Combinational<spec::InputType> act_out;
  Connections::Combinational<spec::InputType> weight_in;
  Connections::Combinational<spec::InputType> weight_out;
  Connections::Combinational<spec::AccumType> accum_in;
  Connections::Combinational<spec::AccumType> accum_out;

  NVHLS_DESIGN(SysPE) PE;
  Source src;

  SC_HAS_PROCESS(testbench);
  testbench(sc_module_name name)
  : sc_module(name),
    clk("clk", 1, SC_NS, 0.5,0,SC_NS,true),
    rst("rst"),
    PE("PE"),
    src("src")
  {
    PE.clk(clk);
    PE.rst(rst);
    PE.act_in(act_in);
    PE.act_out(act_out);
    PE.weight_in(weight_in);
    PE.weight_out(weight_out);
    PE.accum_in(accum_in);
    PE.accum_out(accum_out);

    src.clk(clk);
    src.rst(rst);
    src.act_in(act_in);
    src.act_out(act_out);
    src.weight_in(weight_in);
    src.weight_out(weight_out);
    src.accum_in(accum_in);
    src.accum_out(accum_out);

    SC_THREAD(run);
  }

  void run() {
    rst = 1;
    wait(10.5, SC_NS);
    rst = 0;
    cout << "@" << sc_time_stamp() << " Asserting Reset " << endl ;
    wait(1, SC_NS);
    cout << "@" << sc_time_stamp() << " Deasserting Reset " << endl ;
    rst = 1;
    wait(10000,SC_NS);
    cout << "@" << sc_time_stamp() << " Stop " << endl ;
    sc_stop();
  }
};



int sc_main(int argc, char *argv[])
{
  nvhls::set_random_seed();
  testbench my_testbench("my_testbench");
  sc_start();
  //cout << "CMODEL PASS" << endl;
  return 0;
};

