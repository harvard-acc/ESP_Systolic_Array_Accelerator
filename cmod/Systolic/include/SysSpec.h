#ifndef __SYSSPEC__
#define __SYSSPEC__

#include <nvhls_int.h>
#include <nvhls_types.h>
#include <nvhls_vector.h>

namespace spec {
  const int N = 32;         // array size N*N
  const int Entries = N*8; // number of entries per bank
  // Total memory with double buffering: 2*N*Entries bytes

  // Fixed  8-bit for input/weight/bias
  // Fixed 24-bit for accumulation
  typedef NVINT8   InputType;
  typedef NVINT24  AccumType;
  
  // 8 bit mul for accum
  typedef NVUINT8  AccumMulType;
  
  // 4 bit left shift for bias,
  // 4 bit right shift for accum
  typedef NVUINT4  ShiftType;
 
  // Config format 
  // NOTE: declare here for now, may move to include/SysSpec.h
  //   * is_relu: use relu activation or not
  //   * bias_left_shift: 0-15 left shift on bias values before add
  //   * accum_right_shift: right shift accum before truncation (int quant)
  //   * accum_multiplier: unsigned multiplier 0-255 (int quant)
  class SysConfig: public nvhls_message{
   public:
    bool    is_relu;
    NVUINT4 bias_left_shift;
    NVUINT4 accum_right_shift;
    NVUINT8 accum_multiplier;
    static const unsigned int width = 1+4+4+8;

    template <unsigned int Size>
    void Marshall(Marshaller<Size>& m) {
      m& is_relu;
      m& bias_left_shift;
      m& accum_right_shift;
      m& accum_multiplier;
    }
  };

}


#endif
