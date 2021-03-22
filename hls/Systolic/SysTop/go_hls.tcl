# Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
# 
# Licensed under the Apache License, Version 2.0 (the "License")
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

source ../../nvhls_exec.tcl
namespace eval nvhls {
    proc set_bup_blocks {BUP_BLOCKS} { 
      upvar 1 $BUP_BLOCKS MY_BLOCKS
      set MY_BLOCKS {"Control" "InputAxi" "InputSetup" "MemoryCore" "SysBias" "SysPE" "WeightAxi"} 
    }   
    #proc usercmd_post_assembly {} {
    #  directive set /NLPTop/ActUnit/ActUnitRun/act_mem.banks.bank.array_impl.data0:rsc -MAP_TO_MODULE {sram_64x128}
    #  directive set /NLPTop/OutBuffer/Run/output_mem.banks.bank.array_impl.data0:rsc -MAP_TO_MODULE {sram_256x128}
    #  
    #  for {set k 0} {$k < 16} {incr k} {
    #      directive set /NLPTop/PECore/PECoreRun/weight_mem.banks.bank.array_impl.data$k:rsc -MAP_TO_MODULE {sram_2048x128}
    #  }
    #  directive set /NLPTop/PECore/PECoreRun/input_mem.banks.bank.array_impl.data0:rsc -MAP_TO_MODULE {sram_1024x128}
    #}
}

nvhls::run
