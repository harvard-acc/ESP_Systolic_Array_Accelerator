# ESP_Systolic_Array_Accelerator

* 8-bit integer weight stationary systolic array 
* Reconfigurable array size N = 8, 16, or 32

  /ESP_Systolic_Array_Accelerator/cmod/Systolic/include/SysSpec.h

### AXI Configuration 
* 0--0x00: dummy
* 1--0x04: start signal (write only), fire interrupt upon completion
* ----data=1 -> master weight read
* ----data=2 -> master input read
* ----data=3 -> master input write
* ----data=4 -> start systolic array
* 2--0x08: computation config
* ----data[00-07]= "M-1" (8-bit, M: 1~256)
* ----data[08-15]= use relu (1 bit)
* ----data[16-19]= Bias left shift (4 bit)
* ----data[20-23]= Accum right shift (4-bit)
* ----data[24-31]= Accum mul (8-bit)
* 3--0x0C: weight_read_base
* 4--0x10: input_read_base
* 5--0x14: input_write_base
* 6--0x18: flip memory buffer (no IRQ, write only)
* 7--0x1C: dummy
