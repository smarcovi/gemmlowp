// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// kernel_SSE.h: a collection of Intel SSE optimized kernels.
// Check in kernel_default.h which one(s) are actually used by default.
// Others are mere experiments; they are still covered by tests
// in case they might be useful some day.
// Writen by: Sagi Marovich, mail: sagi.marcovich@intel.com

#ifndef GEMMLOWP_INTERNAL_KERNEL_SSE_H_
#define GEMMLOWP_INTERNAL_KERNEL_SSE_H_


#include "kernel.h"
#include <cassert>
#include <string.h>
#include <iostream>




namespace gemmlowp {


struct SSEKernel12x4Depth2 : KernelBase {
  typedef KernelFormat<KernelSideFormat<CellFormat<4, 2,CellOrder::WidthMajor>, 3>,
                       KernelSideFormat<CellFormat<4, 2,CellOrder::WidthMajor>, 1> > Format;					   


  const char* Name() const override { return "SSE, 12x4, depth 2"; }

  void Run(std::int32_t* dst_ptr, int dst_row_stride, int dst_col_stride,
           const std::uint8_t* lhs_ptr, const std::uint8_t* rhs_ptr,
           int start_depth, int run_depth) const override {
	ScopedProfilingLabel label("optimized kernel");
	assert(dst_row_stride == 1); 
	const std::int64_t run_depth_cells = run_depth / Format::kDepth;
	const std::int64_t dst_col_stride_q = dst_col_stride;
	
	/* Main loop */


	// A 2x4 cell of Rhs is stored in 16bit in xmm1 .
	// A 12x2 block of 3 4x2 cells Lhs is stored in 16bit in xmm0, replaced every Iteration.
	// A 12x4 block of accumulators is stored in 32bit in xmm4--xmm15.
	//
	//                   +-------+-------+-------+-------+
	//                   |xmm1[0]|xmm1[2]|xmm1[4]|xmm1[6]|
	//              Rhs  +-------+---------------+-------+
	//                   |xmm1[1]|xmm1[3]|xmm1[5]|xmm1[7]|
	//                   +-------+-------+-------+-------+
	//
	//                   |       |       |       |       |
	//                                                   
	//    Lhs            |       |       |       |       |
	//
	//  +--+--+ - - - -  +-------+-------+-------+-------+
	//  |xmm0 |          | xmm4  | xmm5  | xmm6  | xmm7  |
	//  |xmm0 | (Iter1)  | xmm4  | xmm5  | xmm6  | xmm7  |
	//  |xmm0 |          | xmm4  | xmm5  | xmm6  | xmm7  |
	//  |xmm0 |          | xmm4  | xmm5  | xmm6  | xmm7  |
	//  +--+--+ - - - -  +-------+-------+-------+-------+
	//  |xmm0 |          | xmm8  | xmm9  | xmm10 | xmm11 |
	//  |xmm0 | (Iter2)  | xmm8  | xmm9  | xmm10 | xmm11 |
	//  |xmm0 |          | xmm8  | xmm9  | xmm10 | xmm11 |
	//  |xmm0 |          | xmm8  | xmm9  | xmm10 | xmm11 |
	//  +--+--+ - - - -  +-------+-------+-------+-------+
	//  |xmm0 |          | xmm12 | xmm13 | xmm14 | xmm15 |
	//  |xmm0 | (Iter3)  | xmm12 | xmm13 | xmm14 | xmm15 |
	//  |xmm0 |          | xmm12 | xmm13 | xmm14 | xmm15 |
	//  |xmm0 |          | xmm12 | xmm13 | xmm14 | xmm15 |
	//  +--+--+ - - - -  +-------+-------+-------+-------+
	//
	//                              Accumulator

	
	asm volatile(
		
		//set accumulators to zero.
		"pxor %%xmm4  , %%xmm4 \n\t" 
		"pxor %%xmm5  , %%xmm5 \n\t"
		"pxor %%xmm6  , %%xmm6 \n\t" 
		"pxor %%xmm7  , %%xmm7 \n\t"
		"pxor %%xmm8  , %%xmm8 \n\t"
		"pxor %%xmm9  , %%xmm9 \n\t"
		"pxor %%xmm10 , %%xmm10\n\t"
		"pxor %%xmm11 , %%xmm11\n\t"
		"pxor %%xmm12 , %%xmm12\n\t"
		"pxor %%xmm13 , %%xmm13\n\t"
		"pxor %%xmm14 , %%xmm14\n\t"
		"pxor %%xmm15 , %%xmm15\n\t"
		
		"outerLoop%=:\n\t"
		
		//RHS cell to xmm1
		"pmovzxbw (%[rhs_ptr]), %%xmm1\n\t"
		
		//LHS cell
		"pmovzxbw 0x00(%[lhs_ptr]), %%xmm0\n\t"	
		"pshufd $0x00,%%xmm1,%%xmm2     \n\t"
		"pshufd $0x55,%%xmm1,%%xmm3     \n\t"
		"pmaddwd %%xmm0, %%xmm2         \n\t"
		"pmaddwd %%xmm0, %%xmm3         \n\t"
		"paddd %%xmm2, %%xmm4           \n\t"
		"paddd %%xmm3, %%xmm5           \n\t"
		"pshufd $0xaa,%%xmm1,%%xmm2     \n\t"
		"pshufd $0xff,%%xmm1,%%xmm3     \n\t"
		"pmaddwd %%xmm0, %%xmm2         \n\t"
		"pmaddwd %%xmm0, %%xmm3         \n\t"
		"paddd %%xmm2, %%xmm6           \n\t"
		"paddd %%xmm3, %%xmm7           \n\t"
		
		//next LHS cell
		"pmovzxbw 0x08(%[lhs_ptr]), %%xmm0\n\t"		
		"pshufd $0x00,%%xmm1,%%xmm2     \n\t"
		"pshufd $0x55,%%xmm1,%%xmm3     \n\t"
		"pmaddwd %%xmm0, %%xmm2         \n\t"
		"pmaddwd %%xmm0, %%xmm3         \n\t"
		"paddd %%xmm2, %%xmm8           \n\t"
		"paddd %%xmm3, %%xmm9           \n\t"
		"pshufd $0xaa,%%xmm1,%%xmm2     \n\t"
		"pshufd $0xff,%%xmm1,%%xmm3     \n\t"
		"pmaddwd %%xmm0, %%xmm2         \n\t"
		"pmaddwd %%xmm0, %%xmm3         \n\t"
		"paddd %%xmm2, %%xmm10          \n\t"
		"paddd %%xmm3, %%xmm11          \n\t"
		
		//next LHS cell
		"pmovzxbw 0x10(%[lhs_ptr]), %%xmm0\n\t"		
		"pshufd $0x00,%%xmm1,%%xmm2     \n\t"
		"pshufd $0x55,%%xmm1,%%xmm3     \n\t"
		"pmaddwd %%xmm0, %%xmm2         \n\t"
		"pmaddwd %%xmm0, %%xmm3         \n\t"
		"paddd %%xmm2, %%xmm12          \n\t"
		"paddd %%xmm3, %%xmm13          \n\t"
		"pshufd $0xaa,%%xmm1,%%xmm2     \n\t"
		"pshufd $0xff,%%xmm1,%%xmm3     \n\t"
		"pmaddwd %%xmm0, %%xmm2         \n\t"
		"pmaddwd %%xmm0, %%xmm3         \n\t"
		"paddd %%xmm2, %%xmm14          \n\t"
		"paddd %%xmm3, %%xmm15          \n\t"
		
		"addq $0x18, %[lhs_ptr]\n\t"
		"addq $0x08, %[rhs_ptr]\n\t"
		"decq %[run_depth_cells]\n\t"
		"jnz outerLoop%=\n\t"
		
		"movq  %[dst_col_stride_q], %%r12\n\t"
		"shlq $2, %%r12\n\t"
		"leaq (%%r12,%%r12,0x2), %%r13\n\t"
		"test %[start_depth], %[start_depth]\n\t"
		"jz storeDst%=\n\t"
		
		"paddd 0x00(%[dst_ptr])           , %%xmm4 \n\t"
		"paddd 0x10(%[dst_ptr])           , %%xmm8 \n\t"
		"paddd 0x20(%[dst_ptr])           , %%xmm12\n\t"
		"paddd 0x00(%[dst_ptr], %%r12, 1) , %%xmm5 \n\t"
		"paddd 0x10(%[dst_ptr], %%r12, 1) , %%xmm9 \n\t"
		"paddd 0x20(%[dst_ptr], %%r12, 1) , %%xmm13\n\t"
		"paddd 0x00(%[dst_ptr], %%r12, 2) , %%xmm6 \n\t"
		"paddd 0x10(%[dst_ptr], %%r12, 2) , %%xmm10\n\t"
		"paddd 0x20(%[dst_ptr], %%r12, 2) , %%xmm14\n\t"
		"paddd 0x00(%[dst_ptr], %%r13, 1) , %%xmm7 \n\t"
		"paddd 0x10(%[dst_ptr], %%r13, 1) , %%xmm11\n\t"
		"paddd 0x20(%[dst_ptr], %%r13, 1) , %%xmm15\n\t"
		
		"storeDst%=:\n\t"
		
		"movdqu %%xmm4  , 0x00(%[dst_ptr])          \n\t" 
		"movdqu %%xmm8  , 0x10(%[dst_ptr])          \n\t" 
		"movdqu %%xmm12 , 0x20(%[dst_ptr])          \n\t" 
		"movdqu %%xmm5  , 0x00(%[dst_ptr], %%r12, 1)\n\t" 
		"movdqu %%xmm9  , 0x10(%[dst_ptr], %%r12, 1)\n\t" 
		"movdqu %%xmm13 , 0x20(%[dst_ptr], %%r12, 1)\n\t" 
		"movdqu %%xmm6  , 0x00(%[dst_ptr], %%r12, 2)\n\t" 
		"movdqu %%xmm10 , 0x10(%[dst_ptr], %%r12, 2)\n\t" 
		"movdqu %%xmm14 , 0x20(%[dst_ptr], %%r12, 2)\n\t" 
		"movdqu %%xmm7  , 0x00(%[dst_ptr], %%r13, 1)\n\t" 
		"movdqu %%xmm11 , 0x10(%[dst_ptr], %%r13, 1)\n\t" 
		"movdqu %%xmm15 , 0x20(%[dst_ptr], %%r13, 1)\n\t" 
		
		:  // outputs
        [lhs_ptr] "+r"(lhs_ptr), [rhs_ptr] "+r"(rhs_ptr),
        [dst_ptr] "+r"(dst_ptr)
        :  // inputs
        [start_depth] "r"(start_depth),
        [dst_col_stride_q] "r"(dst_col_stride_q),
		[run_depth_cells] "r"(run_depth_cells)
        :  // clobbers
		"cc", "memory", "%xmm0", "%xmm1","%xmm3", "%xmm2", "%xmm4",
		"%xmm5","%xmm6","%xmm7","%xmm8","%xmm9","%xmm10", "%r12", "%r13",
		"%xmm11","%xmm12","%xmm13","%xmm14","%xmm15");
  }
};







}  // namespace gemmlowp

#endif  // GEMMLOWP_INTERNAL_KERNEL_SSE_H_