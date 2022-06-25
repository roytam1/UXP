// This file is generated. Do not edit.
#ifndef AOM_DSP_RTCD_H_
#define AOM_DSP_RTCD_H_

#ifdef RTCD_C
#define RTCD_EXTERN
#else
#define RTCD_EXTERN extern
#endif

/*
 * DSP
 */

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/enums.h"
#include "av1/common/blockd.h"


#ifdef __cplusplus
extern "C" {
#endif

void aom_blend_a64_hmask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);
void aom_blend_a64_hmask_neon(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);
RTCD_EXTERN void (*aom_blend_a64_hmask)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);

void aom_blend_a64_mask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh);
#define aom_blend_a64_mask aom_blend_a64_mask_c

void aom_blend_a64_vmask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);
void aom_blend_a64_vmask_neon(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);
RTCD_EXTERN void (*aom_blend_a64_vmask)(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h);

void aom_convolve8_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const InterpKernel *filter, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h);
#define aom_convolve8 aom_convolve8_c

void aom_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define aom_convolve8_horiz aom_convolve8_horiz_c

void aom_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h);
#define aom_convolve8_vert aom_convolve8_vert_c

void aom_convolve_copy_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int w, int h);
void aom_convolve_copy_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int w, int h);
RTCD_EXTERN void (*aom_convolve_copy)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, int w, int h);

void aom_dc_128_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_128_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_16x32 aom_dc_128_predictor_16x32_c

void aom_dc_128_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_16x4 aom_dc_128_predictor_16x4_c

void aom_dc_128_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_16x64 aom_dc_128_predictor_16x64_c

void aom_dc_128_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_16x8 aom_dc_128_predictor_16x8_c

void aom_dc_128_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_32x16 aom_dc_128_predictor_32x16_c

void aom_dc_128_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_128_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_32x64 aom_dc_128_predictor_32x64_c

void aom_dc_128_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_32x8 aom_dc_128_predictor_32x8_c

void aom_dc_128_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_4x16 aom_dc_128_predictor_4x16_c

void aom_dc_128_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_128_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_4x8 aom_dc_128_predictor_4x8_c

void aom_dc_128_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_64x16 aom_dc_128_predictor_64x16_c

void aom_dc_128_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_64x32 aom_dc_128_predictor_64x32_c

void aom_dc_128_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_64x64 aom_dc_128_predictor_64x64_c

void aom_dc_128_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_8x16 aom_dc_128_predictor_8x16_c

void aom_dc_128_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_8x32 aom_dc_128_predictor_8x32_c

void aom_dc_128_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_128_predictor_8x4 aom_dc_128_predictor_8x4_c

void aom_dc_128_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_128_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_128_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_16x32 aom_dc_left_predictor_16x32_c

void aom_dc_left_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_16x4 aom_dc_left_predictor_16x4_c

void aom_dc_left_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_16x64 aom_dc_left_predictor_16x64_c

void aom_dc_left_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_16x8 aom_dc_left_predictor_16x8_c

void aom_dc_left_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_32x16 aom_dc_left_predictor_32x16_c

void aom_dc_left_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_32x64 aom_dc_left_predictor_32x64_c

void aom_dc_left_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_32x8 aom_dc_left_predictor_32x8_c

void aom_dc_left_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_4x16 aom_dc_left_predictor_4x16_c

void aom_dc_left_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_left_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_4x8 aom_dc_left_predictor_4x8_c

void aom_dc_left_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_64x16 aom_dc_left_predictor_64x16_c

void aom_dc_left_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_64x32 aom_dc_left_predictor_64x32_c

void aom_dc_left_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_64x64 aom_dc_left_predictor_64x64_c

void aom_dc_left_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_8x16 aom_dc_left_predictor_8x16_c

void aom_dc_left_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_8x32 aom_dc_left_predictor_8x32_c

void aom_dc_left_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_left_predictor_8x4 aom_dc_left_predictor_8x4_c

void aom_dc_left_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_left_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_left_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_16x32 aom_dc_predictor_16x32_c

void aom_dc_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_16x4 aom_dc_predictor_16x4_c

void aom_dc_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_16x64 aom_dc_predictor_16x64_c

void aom_dc_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_16x8 aom_dc_predictor_16x8_c

void aom_dc_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_32x16 aom_dc_predictor_32x16_c

void aom_dc_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_32x64 aom_dc_predictor_32x64_c

void aom_dc_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_32x8 aom_dc_predictor_32x8_c

void aom_dc_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_4x16 aom_dc_predictor_4x16_c

void aom_dc_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_4x8 aom_dc_predictor_4x8_c

void aom_dc_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_64x16 aom_dc_predictor_64x16_c

void aom_dc_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_64x32 aom_dc_predictor_64x32_c

void aom_dc_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_64x64 aom_dc_predictor_64x64_c

void aom_dc_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_8x16 aom_dc_predictor_8x16_c

void aom_dc_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_8x32 aom_dc_predictor_8x32_c

void aom_dc_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_predictor_8x4 aom_dc_predictor_8x4_c

void aom_dc_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_16x32 aom_dc_top_predictor_16x32_c

void aom_dc_top_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_16x4 aom_dc_top_predictor_16x4_c

void aom_dc_top_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_16x64 aom_dc_top_predictor_16x64_c

void aom_dc_top_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_16x8 aom_dc_top_predictor_16x8_c

void aom_dc_top_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_32x16 aom_dc_top_predictor_32x16_c

void aom_dc_top_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_32x64 aom_dc_top_predictor_32x64_c

void aom_dc_top_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_32x8 aom_dc_top_predictor_32x8_c

void aom_dc_top_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_4x16 aom_dc_top_predictor_4x16_c

void aom_dc_top_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dc_top_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_4x8 aom_dc_top_predictor_4x8_c

void aom_dc_top_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_64x16 aom_dc_top_predictor_64x16_c

void aom_dc_top_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_64x32 aom_dc_top_predictor_64x32_c

void aom_dc_top_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_64x64 aom_dc_top_predictor_64x64_c

void aom_dc_top_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_8x16 aom_dc_top_predictor_8x16_c

void aom_dc_top_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_8x32 aom_dc_top_predictor_8x32_c

void aom_dc_top_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_dc_top_predictor_8x4 aom_dc_top_predictor_8x4_c

void aom_dc_top_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_dc_top_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_dc_top_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_h_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_h_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_h_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_16x32 aom_h_predictor_16x32_c

void aom_h_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_16x4 aom_h_predictor_16x4_c

void aom_h_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_16x64 aom_h_predictor_16x64_c

void aom_h_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_16x8 aom_h_predictor_16x8_c

void aom_h_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_32x16 aom_h_predictor_32x16_c

void aom_h_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_h_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_h_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_32x64 aom_h_predictor_32x64_c

void aom_h_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_32x8 aom_h_predictor_32x8_c

void aom_h_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_4x16 aom_h_predictor_4x16_c

void aom_h_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_h_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_h_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_4x8 aom_h_predictor_4x8_c

void aom_h_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_64x16 aom_h_predictor_64x16_c

void aom_h_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_64x32 aom_h_predictor_64x32_c

void aom_h_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_64x64 aom_h_predictor_64x64_c

void aom_h_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_8x16 aom_h_predictor_8x16_c

void aom_h_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_8x32 aom_h_predictor_8x32_c

void aom_h_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_h_predictor_8x4 aom_h_predictor_8x4_c

void aom_h_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_h_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_h_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_highbd_blend_a64_d16_mask_c(uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh, ConvolveParams *conv_params, const int bd);
#define aom_highbd_blend_a64_d16_mask aom_highbd_blend_a64_d16_mask_c

void aom_highbd_blend_a64_hmask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd);
#define aom_highbd_blend_a64_hmask aom_highbd_blend_a64_hmask_c

void aom_highbd_blend_a64_mask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh, int bd);
#define aom_highbd_blend_a64_mask aom_highbd_blend_a64_mask_c

void aom_highbd_blend_a64_vmask_c(uint8_t *dst, uint32_t dst_stride, const uint8_t *src0, uint32_t src0_stride, const uint8_t *src1, uint32_t src1_stride, const uint8_t *mask, int w, int h, int bd);
#define aom_highbd_blend_a64_vmask aom_highbd_blend_a64_vmask_c

void aom_highbd_convolve8_horiz_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bd);
#define aom_highbd_convolve8_horiz aom_highbd_convolve8_horiz_c

void aom_highbd_convolve8_vert_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bd);
#define aom_highbd_convolve8_vert aom_highbd_convolve8_vert_c

void aom_highbd_convolve_copy_c(const uint16_t *src, ptrdiff_t src_stride, uint16_t *dst, ptrdiff_t dst_stride, int w, int h);
#define aom_highbd_convolve_copy aom_highbd_convolve_copy_c

void aom_highbd_dc_128_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x16 aom_highbd_dc_128_predictor_16x16_c

void aom_highbd_dc_128_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x32 aom_highbd_dc_128_predictor_16x32_c

void aom_highbd_dc_128_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x4 aom_highbd_dc_128_predictor_16x4_c

void aom_highbd_dc_128_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x64 aom_highbd_dc_128_predictor_16x64_c

void aom_highbd_dc_128_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_16x8 aom_highbd_dc_128_predictor_16x8_c

void aom_highbd_dc_128_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_32x16 aom_highbd_dc_128_predictor_32x16_c

void aom_highbd_dc_128_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_32x32 aom_highbd_dc_128_predictor_32x32_c

void aom_highbd_dc_128_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_32x64 aom_highbd_dc_128_predictor_32x64_c

void aom_highbd_dc_128_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_32x8 aom_highbd_dc_128_predictor_32x8_c

void aom_highbd_dc_128_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_4x16 aom_highbd_dc_128_predictor_4x16_c

void aom_highbd_dc_128_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_4x4 aom_highbd_dc_128_predictor_4x4_c

void aom_highbd_dc_128_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_4x8 aom_highbd_dc_128_predictor_4x8_c

void aom_highbd_dc_128_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_64x16 aom_highbd_dc_128_predictor_64x16_c

void aom_highbd_dc_128_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_64x32 aom_highbd_dc_128_predictor_64x32_c

void aom_highbd_dc_128_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_64x64 aom_highbd_dc_128_predictor_64x64_c

void aom_highbd_dc_128_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_8x16 aom_highbd_dc_128_predictor_8x16_c

void aom_highbd_dc_128_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_8x32 aom_highbd_dc_128_predictor_8x32_c

void aom_highbd_dc_128_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_8x4 aom_highbd_dc_128_predictor_8x4_c

void aom_highbd_dc_128_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_128_predictor_8x8 aom_highbd_dc_128_predictor_8x8_c

void aom_highbd_dc_left_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x16 aom_highbd_dc_left_predictor_16x16_c

void aom_highbd_dc_left_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x32 aom_highbd_dc_left_predictor_16x32_c

void aom_highbd_dc_left_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x4 aom_highbd_dc_left_predictor_16x4_c

void aom_highbd_dc_left_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x64 aom_highbd_dc_left_predictor_16x64_c

void aom_highbd_dc_left_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_16x8 aom_highbd_dc_left_predictor_16x8_c

void aom_highbd_dc_left_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_32x16 aom_highbd_dc_left_predictor_32x16_c

void aom_highbd_dc_left_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_32x32 aom_highbd_dc_left_predictor_32x32_c

void aom_highbd_dc_left_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_32x64 aom_highbd_dc_left_predictor_32x64_c

void aom_highbd_dc_left_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_32x8 aom_highbd_dc_left_predictor_32x8_c

void aom_highbd_dc_left_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_4x16 aom_highbd_dc_left_predictor_4x16_c

void aom_highbd_dc_left_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_4x4 aom_highbd_dc_left_predictor_4x4_c

void aom_highbd_dc_left_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_4x8 aom_highbd_dc_left_predictor_4x8_c

void aom_highbd_dc_left_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_64x16 aom_highbd_dc_left_predictor_64x16_c

void aom_highbd_dc_left_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_64x32 aom_highbd_dc_left_predictor_64x32_c

void aom_highbd_dc_left_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_64x64 aom_highbd_dc_left_predictor_64x64_c

void aom_highbd_dc_left_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_8x16 aom_highbd_dc_left_predictor_8x16_c

void aom_highbd_dc_left_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_8x32 aom_highbd_dc_left_predictor_8x32_c

void aom_highbd_dc_left_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_8x4 aom_highbd_dc_left_predictor_8x4_c

void aom_highbd_dc_left_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_left_predictor_8x8 aom_highbd_dc_left_predictor_8x8_c

void aom_highbd_dc_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_16x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_dc_predictor_16x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_dc_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_16x32 aom_highbd_dc_predictor_16x32_c

void aom_highbd_dc_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_16x4 aom_highbd_dc_predictor_16x4_c

void aom_highbd_dc_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_16x64 aom_highbd_dc_predictor_16x64_c

void aom_highbd_dc_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_16x8 aom_highbd_dc_predictor_16x8_c

void aom_highbd_dc_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_32x16 aom_highbd_dc_predictor_32x16_c

void aom_highbd_dc_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_32x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_dc_predictor_32x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_dc_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_32x64 aom_highbd_dc_predictor_32x64_c

void aom_highbd_dc_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_32x8 aom_highbd_dc_predictor_32x8_c

void aom_highbd_dc_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_4x16 aom_highbd_dc_predictor_4x16_c

void aom_highbd_dc_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_4x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_dc_predictor_4x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_dc_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_4x8 aom_highbd_dc_predictor_4x8_c

void aom_highbd_dc_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_64x16 aom_highbd_dc_predictor_64x16_c

void aom_highbd_dc_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_64x32 aom_highbd_dc_predictor_64x32_c

void aom_highbd_dc_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_64x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_dc_predictor_64x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_dc_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_8x16 aom_highbd_dc_predictor_8x16_c

void aom_highbd_dc_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_8x32 aom_highbd_dc_predictor_8x32_c

void aom_highbd_dc_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_predictor_8x4 aom_highbd_dc_predictor_8x4_c

void aom_highbd_dc_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_dc_predictor_8x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_dc_predictor_8x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_dc_top_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x16 aom_highbd_dc_top_predictor_16x16_c

void aom_highbd_dc_top_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x32 aom_highbd_dc_top_predictor_16x32_c

void aom_highbd_dc_top_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x4 aom_highbd_dc_top_predictor_16x4_c

void aom_highbd_dc_top_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x64 aom_highbd_dc_top_predictor_16x64_c

void aom_highbd_dc_top_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_16x8 aom_highbd_dc_top_predictor_16x8_c

void aom_highbd_dc_top_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_32x16 aom_highbd_dc_top_predictor_32x16_c

void aom_highbd_dc_top_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_32x32 aom_highbd_dc_top_predictor_32x32_c

void aom_highbd_dc_top_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_32x64 aom_highbd_dc_top_predictor_32x64_c

void aom_highbd_dc_top_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_32x8 aom_highbd_dc_top_predictor_32x8_c

void aom_highbd_dc_top_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_4x16 aom_highbd_dc_top_predictor_4x16_c

void aom_highbd_dc_top_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_4x4 aom_highbd_dc_top_predictor_4x4_c

void aom_highbd_dc_top_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_4x8 aom_highbd_dc_top_predictor_4x8_c

void aom_highbd_dc_top_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_64x16 aom_highbd_dc_top_predictor_64x16_c

void aom_highbd_dc_top_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_64x32 aom_highbd_dc_top_predictor_64x32_c

void aom_highbd_dc_top_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_64x64 aom_highbd_dc_top_predictor_64x64_c

void aom_highbd_dc_top_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_8x16 aom_highbd_dc_top_predictor_8x16_c

void aom_highbd_dc_top_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_8x32 aom_highbd_dc_top_predictor_8x32_c

void aom_highbd_dc_top_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_8x4 aom_highbd_dc_top_predictor_8x4_c

void aom_highbd_dc_top_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_dc_top_predictor_8x8 aom_highbd_dc_top_predictor_8x8_c

void aom_highbd_h_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x16 aom_highbd_h_predictor_16x16_c

void aom_highbd_h_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x32 aom_highbd_h_predictor_16x32_c

void aom_highbd_h_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x4 aom_highbd_h_predictor_16x4_c

void aom_highbd_h_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x64 aom_highbd_h_predictor_16x64_c

void aom_highbd_h_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_16x8 aom_highbd_h_predictor_16x8_c

void aom_highbd_h_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_32x16 aom_highbd_h_predictor_32x16_c

void aom_highbd_h_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_32x32 aom_highbd_h_predictor_32x32_c

void aom_highbd_h_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_32x64 aom_highbd_h_predictor_32x64_c

void aom_highbd_h_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_32x8 aom_highbd_h_predictor_32x8_c

void aom_highbd_h_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_4x16 aom_highbd_h_predictor_4x16_c

void aom_highbd_h_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_4x4 aom_highbd_h_predictor_4x4_c

void aom_highbd_h_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_4x8 aom_highbd_h_predictor_4x8_c

void aom_highbd_h_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_64x16 aom_highbd_h_predictor_64x16_c

void aom_highbd_h_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_64x32 aom_highbd_h_predictor_64x32_c

void aom_highbd_h_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_64x64 aom_highbd_h_predictor_64x64_c

void aom_highbd_h_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_8x16 aom_highbd_h_predictor_8x16_c

void aom_highbd_h_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_8x32 aom_highbd_h_predictor_8x32_c

void aom_highbd_h_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_8x4 aom_highbd_h_predictor_8x4_c

void aom_highbd_h_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
#define aom_highbd_h_predictor_8x8 aom_highbd_h_predictor_8x8_c

void aom_highbd_lpf_horizontal_14_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_horizontal_14_neon(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_14)(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);

void aom_highbd_lpf_horizontal_14_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1,int bd);
void aom_highbd_lpf_horizontal_14_dual_neon(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1,int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_14_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1,int bd);

void aom_highbd_lpf_horizontal_4_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_horizontal_4_neon(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_4)(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);

void aom_highbd_lpf_horizontal_4_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_horizontal_4_dual_neon(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_4_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_horizontal_6_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_horizontal_6_neon(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_6)(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);

void aom_highbd_lpf_horizontal_6_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_horizontal_6_dual_neon(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_6_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_horizontal_8_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_horizontal_8_neon(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_8)(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);

void aom_highbd_lpf_horizontal_8_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_horizontal_8_dual_neon(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_horizontal_8_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_vertical_14_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_vertical_14_neon(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_14)(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);

void aom_highbd_lpf_vertical_14_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_14_dual_neon(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_14_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_vertical_4_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_vertical_4_neon(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_4)(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);

void aom_highbd_lpf_vertical_4_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_4_dual_neon(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_4_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_vertical_6_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_vertical_6_neon(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_6)(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);

void aom_highbd_lpf_vertical_6_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_6_dual_neon(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_6_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_lpf_vertical_8_c(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
void aom_highbd_lpf_vertical_8_neon(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_8)(uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd);

void aom_highbd_lpf_vertical_8_dual_c(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
void aom_highbd_lpf_vertical_8_dual_neon(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);
RTCD_EXTERN void (*aom_highbd_lpf_vertical_8_dual)(uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd);

void aom_highbd_paeth_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_16x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_16x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_16x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_16x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_16x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_16x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_16x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_16x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_16x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_16x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_32x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_32x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_32x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_32x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_32x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_32x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_32x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_32x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_4x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_4x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_4x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_4x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_4x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_4x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_64x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_64x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_64x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_64x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_64x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_64x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_8x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_8x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_8x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_8x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_8x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_8x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_paeth_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_paeth_predictor_8x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_paeth_predictor_8x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_16x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_16x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_16x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_16x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_16x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_16x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_16x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_16x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_16x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_16x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_32x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_32x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_32x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_32x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_32x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_32x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_32x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_32x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_4x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_4x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_4x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_4x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_4x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_4x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_64x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_64x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_64x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_64x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_64x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_64x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_8x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_8x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_8x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_8x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_8x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_8x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_h_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_h_predictor_8x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_h_predictor_8x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_16x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_16x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_16x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_16x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_16x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_16x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_16x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_16x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_16x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_16x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_32x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_32x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_32x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_32x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_32x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_32x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_32x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_32x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_4x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_4x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_4x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_4x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_4x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_4x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_64x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_64x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_64x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_64x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_64x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_64x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_8x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_8x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_8x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_8x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_8x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_8x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_predictor_8x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_predictor_8x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_16x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_16x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_16x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_16x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_16x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_16x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_16x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_16x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_16x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_16x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_32x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_32x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_32x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_32x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_32x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_32x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_32x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_32x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_4x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_4x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_4x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_4x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_4x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_4x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_64x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_64x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_64x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_64x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_64x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_64x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_8x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_8x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_8x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_8x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_8x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_8x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_smooth_v_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_smooth_v_predictor_8x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_smooth_v_predictor_8x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_16x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_16x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_16x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_16x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_16x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_16x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_16x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_16x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_16x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_16x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_16x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_16x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_16x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_16x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_16x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_32x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_32x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_32x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_32x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_32x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_32x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_32x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_32x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_32x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_32x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_32x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_32x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_4x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_4x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_4x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_4x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_4x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_4x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_4x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_4x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_4x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_64x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_64x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_64x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_64x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_64x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_64x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_64x64_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_64x64_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_64x64)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_8x16_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_8x16_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_8x16)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_8x32_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_8x32_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_8x32)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_8x4_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_8x4_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_8x4)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_highbd_v_predictor_8x8_c(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
void aom_highbd_v_predictor_8x8_neon(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);
RTCD_EXTERN void (*aom_highbd_v_predictor_8x8)(uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd);

void aom_lowbd_blend_a64_d16_mask_c(uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh, ConvolveParams *conv_params);
void aom_lowbd_blend_a64_d16_mask_neon(uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh, ConvolveParams *conv_params);
RTCD_EXTERN void (*aom_lowbd_blend_a64_d16_mask)(uint8_t *dst, uint32_t dst_stride, const CONV_BUF_TYPE *src0, uint32_t src0_stride, const CONV_BUF_TYPE *src1, uint32_t src1_stride, const uint8_t *mask, uint32_t mask_stride, int w, int h, int subw, int subh, ConvolveParams *conv_params);

void aom_lpf_horizontal_14_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_horizontal_14_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*aom_lpf_horizontal_14)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void aom_lpf_horizontal_14_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_horizontal_14_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*aom_lpf_horizontal_14_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void aom_lpf_horizontal_14_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
void aom_lpf_horizontal_14_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
RTCD_EXTERN void (*aom_lpf_horizontal_14_quad)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);

void aom_lpf_horizontal_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_horizontal_4_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*aom_lpf_horizontal_4)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void aom_lpf_horizontal_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_horizontal_4_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*aom_lpf_horizontal_4_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void aom_lpf_horizontal_4_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
void aom_lpf_horizontal_4_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
RTCD_EXTERN void (*aom_lpf_horizontal_4_quad)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);

void aom_lpf_horizontal_6_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_horizontal_6_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*aom_lpf_horizontal_6)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void aom_lpf_horizontal_6_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_horizontal_6_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*aom_lpf_horizontal_6_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void aom_lpf_horizontal_6_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
void aom_lpf_horizontal_6_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
RTCD_EXTERN void (*aom_lpf_horizontal_6_quad)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);

void aom_lpf_horizontal_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_horizontal_8_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*aom_lpf_horizontal_8)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void aom_lpf_horizontal_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_horizontal_8_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*aom_lpf_horizontal_8_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void aom_lpf_horizontal_8_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
void aom_lpf_horizontal_8_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
RTCD_EXTERN void (*aom_lpf_horizontal_8_quad)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);

void aom_lpf_vertical_14_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_vertical_14_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*aom_lpf_vertical_14)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void aom_lpf_vertical_14_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_vertical_14_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*aom_lpf_vertical_14_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void aom_lpf_vertical_14_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
void aom_lpf_vertical_14_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
RTCD_EXTERN void (*aom_lpf_vertical_14_quad)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);

void aom_lpf_vertical_4_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_vertical_4_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*aom_lpf_vertical_4)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void aom_lpf_vertical_4_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_vertical_4_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*aom_lpf_vertical_4_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void aom_lpf_vertical_4_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
void aom_lpf_vertical_4_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
RTCD_EXTERN void (*aom_lpf_vertical_4_quad)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);

void aom_lpf_vertical_6_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_vertical_6_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*aom_lpf_vertical_6)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void aom_lpf_vertical_6_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_vertical_6_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*aom_lpf_vertical_6_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void aom_lpf_vertical_6_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
void aom_lpf_vertical_6_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
RTCD_EXTERN void (*aom_lpf_vertical_6_quad)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);

void aom_lpf_vertical_8_c(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
void aom_lpf_vertical_8_neon(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);
RTCD_EXTERN void (*aom_lpf_vertical_8)(uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh);

void aom_lpf_vertical_8_dual_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
void aom_lpf_vertical_8_dual_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);
RTCD_EXTERN void (*aom_lpf_vertical_8_dual)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1);

void aom_lpf_vertical_8_quad_c(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
void aom_lpf_vertical_8_quad_neon(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);
RTCD_EXTERN void (*aom_lpf_vertical_8_quad)(uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0);

void aom_paeth_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_16x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_16x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_32x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_32x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_4x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_4x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_4x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_4x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_64x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_64x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_64x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_8x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_8x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_8x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_8x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_8x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_8x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_paeth_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_paeth_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_paeth_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_scaled_2d_c(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const InterpKernel *filter, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h);
void aom_scaled_2d_neon(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const InterpKernel *filter, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h);
RTCD_EXTERN void (*aom_scaled_2d)(const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const InterpKernel *filter, int x0_q4, int x_step_q4, int y0_q4, int y_step_q4, int w, int h);

void aom_smooth_h_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_16x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_16x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_32x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_32x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_32x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_32x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_4x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_4x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_4x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_4x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_64x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_64x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_64x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_8x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_8x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_8x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_8x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_8x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_8x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_h_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_h_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_h_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_16x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_16x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_32x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_32x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_32x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_32x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_4x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_4x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_4x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_4x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_64x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_64x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_64x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_8x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_8x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_8x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_8x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_8x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_8x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_16x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_16x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_32x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_32x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_32x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_32x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_32x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_32x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_4x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_4x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_4x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_4x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_64x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_64x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_64x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_64x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_64x64_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_64x64)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_8x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_8x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_8x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_8x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_8x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_8x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_smooth_v_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_smooth_v_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_smooth_v_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_16x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_16x16_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_16x16)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_16x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_16x32 aom_v_predictor_16x32_c

void aom_v_predictor_16x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_16x4 aom_v_predictor_16x4_c

void aom_v_predictor_16x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_16x64 aom_v_predictor_16x64_c

void aom_v_predictor_16x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_16x8 aom_v_predictor_16x8_c

void aom_v_predictor_32x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_32x16 aom_v_predictor_32x16_c

void aom_v_predictor_32x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_32x32_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_32x32)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_32x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_32x64 aom_v_predictor_32x64_c

void aom_v_predictor_32x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_32x8 aom_v_predictor_32x8_c

void aom_v_predictor_4x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_4x16 aom_v_predictor_4x16_c

void aom_v_predictor_4x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_4x4_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_4x4)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_v_predictor_4x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_4x8 aom_v_predictor_4x8_c

void aom_v_predictor_64x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_64x16 aom_v_predictor_64x16_c

void aom_v_predictor_64x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_64x32 aom_v_predictor_64x32_c

void aom_v_predictor_64x64_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_64x64 aom_v_predictor_64x64_c

void aom_v_predictor_8x16_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_8x16 aom_v_predictor_8x16_c

void aom_v_predictor_8x32_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_8x32 aom_v_predictor_8x32_c

void aom_v_predictor_8x4_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
#define aom_v_predictor_8x4 aom_v_predictor_8x4_c

void aom_v_predictor_8x8_c(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
void aom_v_predictor_8x8_neon(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);
RTCD_EXTERN void (*aom_v_predictor_8x8)(uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left);

void aom_dsp_rtcd(void);

#include "config/aom_config.h"

#ifdef RTCD_C
#include "aom_ports/arm.h"
static void setup_rtcd_internal(void)
{
    int flags = aom_arm_cpu_caps();

    (void)flags;

    aom_blend_a64_hmask = aom_blend_a64_hmask_c;
    if (flags & HAS_NEON) aom_blend_a64_hmask = aom_blend_a64_hmask_neon;
    aom_blend_a64_vmask = aom_blend_a64_vmask_c;
    if (flags & HAS_NEON) aom_blend_a64_vmask = aom_blend_a64_vmask_neon;
    aom_convolve_copy = aom_convolve_copy_c;
    if (flags & HAS_NEON) aom_convolve_copy = aom_convolve_copy_neon;
    aom_dc_128_predictor_16x16 = aom_dc_128_predictor_16x16_c;
    if (flags & HAS_NEON) aom_dc_128_predictor_16x16 = aom_dc_128_predictor_16x16_neon;
    aom_dc_128_predictor_32x32 = aom_dc_128_predictor_32x32_c;
    if (flags & HAS_NEON) aom_dc_128_predictor_32x32 = aom_dc_128_predictor_32x32_neon;
    aom_dc_128_predictor_4x4 = aom_dc_128_predictor_4x4_c;
    if (flags & HAS_NEON) aom_dc_128_predictor_4x4 = aom_dc_128_predictor_4x4_neon;
    aom_dc_128_predictor_8x8 = aom_dc_128_predictor_8x8_c;
    if (flags & HAS_NEON) aom_dc_128_predictor_8x8 = aom_dc_128_predictor_8x8_neon;
    aom_dc_left_predictor_16x16 = aom_dc_left_predictor_16x16_c;
    if (flags & HAS_NEON) aom_dc_left_predictor_16x16 = aom_dc_left_predictor_16x16_neon;
    aom_dc_left_predictor_32x32 = aom_dc_left_predictor_32x32_c;
    if (flags & HAS_NEON) aom_dc_left_predictor_32x32 = aom_dc_left_predictor_32x32_neon;
    aom_dc_left_predictor_4x4 = aom_dc_left_predictor_4x4_c;
    if (flags & HAS_NEON) aom_dc_left_predictor_4x4 = aom_dc_left_predictor_4x4_neon;
    aom_dc_left_predictor_8x8 = aom_dc_left_predictor_8x8_c;
    if (flags & HAS_NEON) aom_dc_left_predictor_8x8 = aom_dc_left_predictor_8x8_neon;
    aom_dc_predictor_16x16 = aom_dc_predictor_16x16_c;
    if (flags & HAS_NEON) aom_dc_predictor_16x16 = aom_dc_predictor_16x16_neon;
    aom_dc_predictor_32x32 = aom_dc_predictor_32x32_c;
    if (flags & HAS_NEON) aom_dc_predictor_32x32 = aom_dc_predictor_32x32_neon;
    aom_dc_predictor_4x4 = aom_dc_predictor_4x4_c;
    if (flags & HAS_NEON) aom_dc_predictor_4x4 = aom_dc_predictor_4x4_neon;
    aom_dc_predictor_8x8 = aom_dc_predictor_8x8_c;
    if (flags & HAS_NEON) aom_dc_predictor_8x8 = aom_dc_predictor_8x8_neon;
    aom_dc_top_predictor_16x16 = aom_dc_top_predictor_16x16_c;
    if (flags & HAS_NEON) aom_dc_top_predictor_16x16 = aom_dc_top_predictor_16x16_neon;
    aom_dc_top_predictor_32x32 = aom_dc_top_predictor_32x32_c;
    if (flags & HAS_NEON) aom_dc_top_predictor_32x32 = aom_dc_top_predictor_32x32_neon;
    aom_dc_top_predictor_4x4 = aom_dc_top_predictor_4x4_c;
    if (flags & HAS_NEON) aom_dc_top_predictor_4x4 = aom_dc_top_predictor_4x4_neon;
    aom_dc_top_predictor_8x8 = aom_dc_top_predictor_8x8_c;
    if (flags & HAS_NEON) aom_dc_top_predictor_8x8 = aom_dc_top_predictor_8x8_neon;
    aom_h_predictor_16x16 = aom_h_predictor_16x16_c;
    if (flags & HAS_NEON) aom_h_predictor_16x16 = aom_h_predictor_16x16_neon;
    aom_h_predictor_32x32 = aom_h_predictor_32x32_c;
    if (flags & HAS_NEON) aom_h_predictor_32x32 = aom_h_predictor_32x32_neon;
    aom_h_predictor_4x4 = aom_h_predictor_4x4_c;
    if (flags & HAS_NEON) aom_h_predictor_4x4 = aom_h_predictor_4x4_neon;
    aom_h_predictor_8x8 = aom_h_predictor_8x8_c;
    if (flags & HAS_NEON) aom_h_predictor_8x8 = aom_h_predictor_8x8_neon;
    aom_highbd_dc_predictor_16x16 = aom_highbd_dc_predictor_16x16_c;
    if (flags & HAS_NEON) aom_highbd_dc_predictor_16x16 = aom_highbd_dc_predictor_16x16_neon;
    aom_highbd_dc_predictor_32x32 = aom_highbd_dc_predictor_32x32_c;
    if (flags & HAS_NEON) aom_highbd_dc_predictor_32x32 = aom_highbd_dc_predictor_32x32_neon;
    aom_highbd_dc_predictor_4x4 = aom_highbd_dc_predictor_4x4_c;
    if (flags & HAS_NEON) aom_highbd_dc_predictor_4x4 = aom_highbd_dc_predictor_4x4_neon;
    aom_highbd_dc_predictor_64x64 = aom_highbd_dc_predictor_64x64_c;
    if (flags & HAS_NEON) aom_highbd_dc_predictor_64x64 = aom_highbd_dc_predictor_64x64_neon;
    aom_highbd_dc_predictor_8x8 = aom_highbd_dc_predictor_8x8_c;
    if (flags & HAS_NEON) aom_highbd_dc_predictor_8x8 = aom_highbd_dc_predictor_8x8_neon;
    aom_highbd_lpf_horizontal_14 = aom_highbd_lpf_horizontal_14_c;
    if (flags & HAS_NEON) aom_highbd_lpf_horizontal_14 = aom_highbd_lpf_horizontal_14_neon;
    aom_highbd_lpf_horizontal_14_dual = aom_highbd_lpf_horizontal_14_dual_c;
    if (flags & HAS_NEON) aom_highbd_lpf_horizontal_14_dual = aom_highbd_lpf_horizontal_14_dual_neon;
    aom_highbd_lpf_horizontal_4 = aom_highbd_lpf_horizontal_4_c;
    if (flags & HAS_NEON) aom_highbd_lpf_horizontal_4 = aom_highbd_lpf_horizontal_4_neon;
    aom_highbd_lpf_horizontal_4_dual = aom_highbd_lpf_horizontal_4_dual_c;
    if (flags & HAS_NEON) aom_highbd_lpf_horizontal_4_dual = aom_highbd_lpf_horizontal_4_dual_neon;
    aom_highbd_lpf_horizontal_6 = aom_highbd_lpf_horizontal_6_c;
    if (flags & HAS_NEON) aom_highbd_lpf_horizontal_6 = aom_highbd_lpf_horizontal_6_neon;
    aom_highbd_lpf_horizontal_6_dual = aom_highbd_lpf_horizontal_6_dual_c;
    if (flags & HAS_NEON) aom_highbd_lpf_horizontal_6_dual = aom_highbd_lpf_horizontal_6_dual_neon;
    aom_highbd_lpf_horizontal_8 = aom_highbd_lpf_horizontal_8_c;
    if (flags & HAS_NEON) aom_highbd_lpf_horizontal_8 = aom_highbd_lpf_horizontal_8_neon;
    aom_highbd_lpf_horizontal_8_dual = aom_highbd_lpf_horizontal_8_dual_c;
    if (flags & HAS_NEON) aom_highbd_lpf_horizontal_8_dual = aom_highbd_lpf_horizontal_8_dual_neon;
    aom_highbd_lpf_vertical_14 = aom_highbd_lpf_vertical_14_c;
    if (flags & HAS_NEON) aom_highbd_lpf_vertical_14 = aom_highbd_lpf_vertical_14_neon;
    aom_highbd_lpf_vertical_14_dual = aom_highbd_lpf_vertical_14_dual_c;
    if (flags & HAS_NEON) aom_highbd_lpf_vertical_14_dual = aom_highbd_lpf_vertical_14_dual_neon;
    aom_highbd_lpf_vertical_4 = aom_highbd_lpf_vertical_4_c;
    if (flags & HAS_NEON) aom_highbd_lpf_vertical_4 = aom_highbd_lpf_vertical_4_neon;
    aom_highbd_lpf_vertical_4_dual = aom_highbd_lpf_vertical_4_dual_c;
    if (flags & HAS_NEON) aom_highbd_lpf_vertical_4_dual = aom_highbd_lpf_vertical_4_dual_neon;
    aom_highbd_lpf_vertical_6 = aom_highbd_lpf_vertical_6_c;
    if (flags & HAS_NEON) aom_highbd_lpf_vertical_6 = aom_highbd_lpf_vertical_6_neon;
    aom_highbd_lpf_vertical_6_dual = aom_highbd_lpf_vertical_6_dual_c;
    if (flags & HAS_NEON) aom_highbd_lpf_vertical_6_dual = aom_highbd_lpf_vertical_6_dual_neon;
    aom_highbd_lpf_vertical_8 = aom_highbd_lpf_vertical_8_c;
    if (flags & HAS_NEON) aom_highbd_lpf_vertical_8 = aom_highbd_lpf_vertical_8_neon;
    aom_highbd_lpf_vertical_8_dual = aom_highbd_lpf_vertical_8_dual_c;
    if (flags & HAS_NEON) aom_highbd_lpf_vertical_8_dual = aom_highbd_lpf_vertical_8_dual_neon;
    aom_highbd_paeth_predictor_16x16 = aom_highbd_paeth_predictor_16x16_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_16x16 = aom_highbd_paeth_predictor_16x16_neon;
    aom_highbd_paeth_predictor_16x32 = aom_highbd_paeth_predictor_16x32_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_16x32 = aom_highbd_paeth_predictor_16x32_neon;
    aom_highbd_paeth_predictor_16x4 = aom_highbd_paeth_predictor_16x4_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_16x4 = aom_highbd_paeth_predictor_16x4_neon;
    aom_highbd_paeth_predictor_16x64 = aom_highbd_paeth_predictor_16x64_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_16x64 = aom_highbd_paeth_predictor_16x64_neon;
    aom_highbd_paeth_predictor_16x8 = aom_highbd_paeth_predictor_16x8_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_16x8 = aom_highbd_paeth_predictor_16x8_neon;
    aom_highbd_paeth_predictor_32x16 = aom_highbd_paeth_predictor_32x16_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_32x16 = aom_highbd_paeth_predictor_32x16_neon;
    aom_highbd_paeth_predictor_32x32 = aom_highbd_paeth_predictor_32x32_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_32x32 = aom_highbd_paeth_predictor_32x32_neon;
    aom_highbd_paeth_predictor_32x64 = aom_highbd_paeth_predictor_32x64_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_32x64 = aom_highbd_paeth_predictor_32x64_neon;
    aom_highbd_paeth_predictor_32x8 = aom_highbd_paeth_predictor_32x8_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_32x8 = aom_highbd_paeth_predictor_32x8_neon;
    aom_highbd_paeth_predictor_4x16 = aom_highbd_paeth_predictor_4x16_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_4x16 = aom_highbd_paeth_predictor_4x16_neon;
    aom_highbd_paeth_predictor_4x4 = aom_highbd_paeth_predictor_4x4_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_4x4 = aom_highbd_paeth_predictor_4x4_neon;
    aom_highbd_paeth_predictor_4x8 = aom_highbd_paeth_predictor_4x8_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_4x8 = aom_highbd_paeth_predictor_4x8_neon;
    aom_highbd_paeth_predictor_64x16 = aom_highbd_paeth_predictor_64x16_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_64x16 = aom_highbd_paeth_predictor_64x16_neon;
    aom_highbd_paeth_predictor_64x32 = aom_highbd_paeth_predictor_64x32_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_64x32 = aom_highbd_paeth_predictor_64x32_neon;
    aom_highbd_paeth_predictor_64x64 = aom_highbd_paeth_predictor_64x64_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_64x64 = aom_highbd_paeth_predictor_64x64_neon;
    aom_highbd_paeth_predictor_8x16 = aom_highbd_paeth_predictor_8x16_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_8x16 = aom_highbd_paeth_predictor_8x16_neon;
    aom_highbd_paeth_predictor_8x32 = aom_highbd_paeth_predictor_8x32_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_8x32 = aom_highbd_paeth_predictor_8x32_neon;
    aom_highbd_paeth_predictor_8x4 = aom_highbd_paeth_predictor_8x4_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_8x4 = aom_highbd_paeth_predictor_8x4_neon;
    aom_highbd_paeth_predictor_8x8 = aom_highbd_paeth_predictor_8x8_c;
    if (flags & HAS_NEON) aom_highbd_paeth_predictor_8x8 = aom_highbd_paeth_predictor_8x8_neon;
    aom_highbd_smooth_h_predictor_16x16 = aom_highbd_smooth_h_predictor_16x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_16x16 = aom_highbd_smooth_h_predictor_16x16_neon;
    aom_highbd_smooth_h_predictor_16x32 = aom_highbd_smooth_h_predictor_16x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_16x32 = aom_highbd_smooth_h_predictor_16x32_neon;
    aom_highbd_smooth_h_predictor_16x4 = aom_highbd_smooth_h_predictor_16x4_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_16x4 = aom_highbd_smooth_h_predictor_16x4_neon;
    aom_highbd_smooth_h_predictor_16x64 = aom_highbd_smooth_h_predictor_16x64_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_16x64 = aom_highbd_smooth_h_predictor_16x64_neon;
    aom_highbd_smooth_h_predictor_16x8 = aom_highbd_smooth_h_predictor_16x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_16x8 = aom_highbd_smooth_h_predictor_16x8_neon;
    aom_highbd_smooth_h_predictor_32x16 = aom_highbd_smooth_h_predictor_32x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_32x16 = aom_highbd_smooth_h_predictor_32x16_neon;
    aom_highbd_smooth_h_predictor_32x32 = aom_highbd_smooth_h_predictor_32x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_32x32 = aom_highbd_smooth_h_predictor_32x32_neon;
    aom_highbd_smooth_h_predictor_32x64 = aom_highbd_smooth_h_predictor_32x64_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_32x64 = aom_highbd_smooth_h_predictor_32x64_neon;
    aom_highbd_smooth_h_predictor_32x8 = aom_highbd_smooth_h_predictor_32x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_32x8 = aom_highbd_smooth_h_predictor_32x8_neon;
    aom_highbd_smooth_h_predictor_4x16 = aom_highbd_smooth_h_predictor_4x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_4x16 = aom_highbd_smooth_h_predictor_4x16_neon;
    aom_highbd_smooth_h_predictor_4x4 = aom_highbd_smooth_h_predictor_4x4_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_4x4 = aom_highbd_smooth_h_predictor_4x4_neon;
    aom_highbd_smooth_h_predictor_4x8 = aom_highbd_smooth_h_predictor_4x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_4x8 = aom_highbd_smooth_h_predictor_4x8_neon;
    aom_highbd_smooth_h_predictor_64x16 = aom_highbd_smooth_h_predictor_64x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_64x16 = aom_highbd_smooth_h_predictor_64x16_neon;
    aom_highbd_smooth_h_predictor_64x32 = aom_highbd_smooth_h_predictor_64x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_64x32 = aom_highbd_smooth_h_predictor_64x32_neon;
    aom_highbd_smooth_h_predictor_64x64 = aom_highbd_smooth_h_predictor_64x64_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_64x64 = aom_highbd_smooth_h_predictor_64x64_neon;
    aom_highbd_smooth_h_predictor_8x16 = aom_highbd_smooth_h_predictor_8x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_8x16 = aom_highbd_smooth_h_predictor_8x16_neon;
    aom_highbd_smooth_h_predictor_8x32 = aom_highbd_smooth_h_predictor_8x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_8x32 = aom_highbd_smooth_h_predictor_8x32_neon;
    aom_highbd_smooth_h_predictor_8x4 = aom_highbd_smooth_h_predictor_8x4_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_8x4 = aom_highbd_smooth_h_predictor_8x4_neon;
    aom_highbd_smooth_h_predictor_8x8 = aom_highbd_smooth_h_predictor_8x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_h_predictor_8x8 = aom_highbd_smooth_h_predictor_8x8_neon;
    aom_highbd_smooth_predictor_16x16 = aom_highbd_smooth_predictor_16x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_16x16 = aom_highbd_smooth_predictor_16x16_neon;
    aom_highbd_smooth_predictor_16x32 = aom_highbd_smooth_predictor_16x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_16x32 = aom_highbd_smooth_predictor_16x32_neon;
    aom_highbd_smooth_predictor_16x4 = aom_highbd_smooth_predictor_16x4_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_16x4 = aom_highbd_smooth_predictor_16x4_neon;
    aom_highbd_smooth_predictor_16x64 = aom_highbd_smooth_predictor_16x64_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_16x64 = aom_highbd_smooth_predictor_16x64_neon;
    aom_highbd_smooth_predictor_16x8 = aom_highbd_smooth_predictor_16x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_16x8 = aom_highbd_smooth_predictor_16x8_neon;
    aom_highbd_smooth_predictor_32x16 = aom_highbd_smooth_predictor_32x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_32x16 = aom_highbd_smooth_predictor_32x16_neon;
    aom_highbd_smooth_predictor_32x32 = aom_highbd_smooth_predictor_32x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_32x32 = aom_highbd_smooth_predictor_32x32_neon;
    aom_highbd_smooth_predictor_32x64 = aom_highbd_smooth_predictor_32x64_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_32x64 = aom_highbd_smooth_predictor_32x64_neon;
    aom_highbd_smooth_predictor_32x8 = aom_highbd_smooth_predictor_32x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_32x8 = aom_highbd_smooth_predictor_32x8_neon;
    aom_highbd_smooth_predictor_4x16 = aom_highbd_smooth_predictor_4x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_4x16 = aom_highbd_smooth_predictor_4x16_neon;
    aom_highbd_smooth_predictor_4x4 = aom_highbd_smooth_predictor_4x4_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_4x4 = aom_highbd_smooth_predictor_4x4_neon;
    aom_highbd_smooth_predictor_4x8 = aom_highbd_smooth_predictor_4x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_4x8 = aom_highbd_smooth_predictor_4x8_neon;
    aom_highbd_smooth_predictor_64x16 = aom_highbd_smooth_predictor_64x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_64x16 = aom_highbd_smooth_predictor_64x16_neon;
    aom_highbd_smooth_predictor_64x32 = aom_highbd_smooth_predictor_64x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_64x32 = aom_highbd_smooth_predictor_64x32_neon;
    aom_highbd_smooth_predictor_64x64 = aom_highbd_smooth_predictor_64x64_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_64x64 = aom_highbd_smooth_predictor_64x64_neon;
    aom_highbd_smooth_predictor_8x16 = aom_highbd_smooth_predictor_8x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_8x16 = aom_highbd_smooth_predictor_8x16_neon;
    aom_highbd_smooth_predictor_8x32 = aom_highbd_smooth_predictor_8x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_8x32 = aom_highbd_smooth_predictor_8x32_neon;
    aom_highbd_smooth_predictor_8x4 = aom_highbd_smooth_predictor_8x4_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_8x4 = aom_highbd_smooth_predictor_8x4_neon;
    aom_highbd_smooth_predictor_8x8 = aom_highbd_smooth_predictor_8x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_predictor_8x8 = aom_highbd_smooth_predictor_8x8_neon;
    aom_highbd_smooth_v_predictor_16x16 = aom_highbd_smooth_v_predictor_16x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_16x16 = aom_highbd_smooth_v_predictor_16x16_neon;
    aom_highbd_smooth_v_predictor_16x32 = aom_highbd_smooth_v_predictor_16x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_16x32 = aom_highbd_smooth_v_predictor_16x32_neon;
    aom_highbd_smooth_v_predictor_16x4 = aom_highbd_smooth_v_predictor_16x4_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_16x4 = aom_highbd_smooth_v_predictor_16x4_neon;
    aom_highbd_smooth_v_predictor_16x64 = aom_highbd_smooth_v_predictor_16x64_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_16x64 = aom_highbd_smooth_v_predictor_16x64_neon;
    aom_highbd_smooth_v_predictor_16x8 = aom_highbd_smooth_v_predictor_16x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_16x8 = aom_highbd_smooth_v_predictor_16x8_neon;
    aom_highbd_smooth_v_predictor_32x16 = aom_highbd_smooth_v_predictor_32x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_32x16 = aom_highbd_smooth_v_predictor_32x16_neon;
    aom_highbd_smooth_v_predictor_32x32 = aom_highbd_smooth_v_predictor_32x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_32x32 = aom_highbd_smooth_v_predictor_32x32_neon;
    aom_highbd_smooth_v_predictor_32x64 = aom_highbd_smooth_v_predictor_32x64_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_32x64 = aom_highbd_smooth_v_predictor_32x64_neon;
    aom_highbd_smooth_v_predictor_32x8 = aom_highbd_smooth_v_predictor_32x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_32x8 = aom_highbd_smooth_v_predictor_32x8_neon;
    aom_highbd_smooth_v_predictor_4x16 = aom_highbd_smooth_v_predictor_4x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_4x16 = aom_highbd_smooth_v_predictor_4x16_neon;
    aom_highbd_smooth_v_predictor_4x4 = aom_highbd_smooth_v_predictor_4x4_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_4x4 = aom_highbd_smooth_v_predictor_4x4_neon;
    aom_highbd_smooth_v_predictor_4x8 = aom_highbd_smooth_v_predictor_4x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_4x8 = aom_highbd_smooth_v_predictor_4x8_neon;
    aom_highbd_smooth_v_predictor_64x16 = aom_highbd_smooth_v_predictor_64x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_64x16 = aom_highbd_smooth_v_predictor_64x16_neon;
    aom_highbd_smooth_v_predictor_64x32 = aom_highbd_smooth_v_predictor_64x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_64x32 = aom_highbd_smooth_v_predictor_64x32_neon;
    aom_highbd_smooth_v_predictor_64x64 = aom_highbd_smooth_v_predictor_64x64_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_64x64 = aom_highbd_smooth_v_predictor_64x64_neon;
    aom_highbd_smooth_v_predictor_8x16 = aom_highbd_smooth_v_predictor_8x16_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_8x16 = aom_highbd_smooth_v_predictor_8x16_neon;
    aom_highbd_smooth_v_predictor_8x32 = aom_highbd_smooth_v_predictor_8x32_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_8x32 = aom_highbd_smooth_v_predictor_8x32_neon;
    aom_highbd_smooth_v_predictor_8x4 = aom_highbd_smooth_v_predictor_8x4_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_8x4 = aom_highbd_smooth_v_predictor_8x4_neon;
    aom_highbd_smooth_v_predictor_8x8 = aom_highbd_smooth_v_predictor_8x8_c;
    if (flags & HAS_NEON) aom_highbd_smooth_v_predictor_8x8 = aom_highbd_smooth_v_predictor_8x8_neon;
    aom_highbd_v_predictor_16x16 = aom_highbd_v_predictor_16x16_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_16x16 = aom_highbd_v_predictor_16x16_neon;
    aom_highbd_v_predictor_16x32 = aom_highbd_v_predictor_16x32_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_16x32 = aom_highbd_v_predictor_16x32_neon;
    aom_highbd_v_predictor_16x4 = aom_highbd_v_predictor_16x4_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_16x4 = aom_highbd_v_predictor_16x4_neon;
    aom_highbd_v_predictor_16x64 = aom_highbd_v_predictor_16x64_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_16x64 = aom_highbd_v_predictor_16x64_neon;
    aom_highbd_v_predictor_16x8 = aom_highbd_v_predictor_16x8_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_16x8 = aom_highbd_v_predictor_16x8_neon;
    aom_highbd_v_predictor_32x16 = aom_highbd_v_predictor_32x16_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_32x16 = aom_highbd_v_predictor_32x16_neon;
    aom_highbd_v_predictor_32x32 = aom_highbd_v_predictor_32x32_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_32x32 = aom_highbd_v_predictor_32x32_neon;
    aom_highbd_v_predictor_32x64 = aom_highbd_v_predictor_32x64_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_32x64 = aom_highbd_v_predictor_32x64_neon;
    aom_highbd_v_predictor_32x8 = aom_highbd_v_predictor_32x8_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_32x8 = aom_highbd_v_predictor_32x8_neon;
    aom_highbd_v_predictor_4x16 = aom_highbd_v_predictor_4x16_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_4x16 = aom_highbd_v_predictor_4x16_neon;
    aom_highbd_v_predictor_4x4 = aom_highbd_v_predictor_4x4_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_4x4 = aom_highbd_v_predictor_4x4_neon;
    aom_highbd_v_predictor_4x8 = aom_highbd_v_predictor_4x8_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_4x8 = aom_highbd_v_predictor_4x8_neon;
    aom_highbd_v_predictor_64x16 = aom_highbd_v_predictor_64x16_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_64x16 = aom_highbd_v_predictor_64x16_neon;
    aom_highbd_v_predictor_64x32 = aom_highbd_v_predictor_64x32_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_64x32 = aom_highbd_v_predictor_64x32_neon;
    aom_highbd_v_predictor_64x64 = aom_highbd_v_predictor_64x64_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_64x64 = aom_highbd_v_predictor_64x64_neon;
    aom_highbd_v_predictor_8x16 = aom_highbd_v_predictor_8x16_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_8x16 = aom_highbd_v_predictor_8x16_neon;
    aom_highbd_v_predictor_8x32 = aom_highbd_v_predictor_8x32_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_8x32 = aom_highbd_v_predictor_8x32_neon;
    aom_highbd_v_predictor_8x4 = aom_highbd_v_predictor_8x4_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_8x4 = aom_highbd_v_predictor_8x4_neon;
    aom_highbd_v_predictor_8x8 = aom_highbd_v_predictor_8x8_c;
    if (flags & HAS_NEON) aom_highbd_v_predictor_8x8 = aom_highbd_v_predictor_8x8_neon;
    aom_lowbd_blend_a64_d16_mask = aom_lowbd_blend_a64_d16_mask_c;
    if (flags & HAS_NEON) aom_lowbd_blend_a64_d16_mask = aom_lowbd_blend_a64_d16_mask_neon;
    aom_lpf_horizontal_14 = aom_lpf_horizontal_14_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_14 = aom_lpf_horizontal_14_neon;
    aom_lpf_horizontal_14_dual = aom_lpf_horizontal_14_dual_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_14_dual = aom_lpf_horizontal_14_dual_neon;
    aom_lpf_horizontal_14_quad = aom_lpf_horizontal_14_quad_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_14_quad = aom_lpf_horizontal_14_quad_neon;
    aom_lpf_horizontal_4 = aom_lpf_horizontal_4_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_4 = aom_lpf_horizontal_4_neon;
    aom_lpf_horizontal_4_dual = aom_lpf_horizontal_4_dual_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_4_dual = aom_lpf_horizontal_4_dual_neon;
    aom_lpf_horizontal_4_quad = aom_lpf_horizontal_4_quad_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_4_quad = aom_lpf_horizontal_4_quad_neon;
    aom_lpf_horizontal_6 = aom_lpf_horizontal_6_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_6 = aom_lpf_horizontal_6_neon;
    aom_lpf_horizontal_6_dual = aom_lpf_horizontal_6_dual_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_6_dual = aom_lpf_horizontal_6_dual_neon;
    aom_lpf_horizontal_6_quad = aom_lpf_horizontal_6_quad_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_6_quad = aom_lpf_horizontal_6_quad_neon;
    aom_lpf_horizontal_8 = aom_lpf_horizontal_8_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_8 = aom_lpf_horizontal_8_neon;
    aom_lpf_horizontal_8_dual = aom_lpf_horizontal_8_dual_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_8_dual = aom_lpf_horizontal_8_dual_neon;
    aom_lpf_horizontal_8_quad = aom_lpf_horizontal_8_quad_c;
    if (flags & HAS_NEON) aom_lpf_horizontal_8_quad = aom_lpf_horizontal_8_quad_neon;
    aom_lpf_vertical_14 = aom_lpf_vertical_14_c;
    if (flags & HAS_NEON) aom_lpf_vertical_14 = aom_lpf_vertical_14_neon;
    aom_lpf_vertical_14_dual = aom_lpf_vertical_14_dual_c;
    if (flags & HAS_NEON) aom_lpf_vertical_14_dual = aom_lpf_vertical_14_dual_neon;
    aom_lpf_vertical_14_quad = aom_lpf_vertical_14_quad_c;
    if (flags & HAS_NEON) aom_lpf_vertical_14_quad = aom_lpf_vertical_14_quad_neon;
    aom_lpf_vertical_4 = aom_lpf_vertical_4_c;
    if (flags & HAS_NEON) aom_lpf_vertical_4 = aom_lpf_vertical_4_neon;
    aom_lpf_vertical_4_dual = aom_lpf_vertical_4_dual_c;
    if (flags & HAS_NEON) aom_lpf_vertical_4_dual = aom_lpf_vertical_4_dual_neon;
    aom_lpf_vertical_4_quad = aom_lpf_vertical_4_quad_c;
    if (flags & HAS_NEON) aom_lpf_vertical_4_quad = aom_lpf_vertical_4_quad_neon;
    aom_lpf_vertical_6 = aom_lpf_vertical_6_c;
    if (flags & HAS_NEON) aom_lpf_vertical_6 = aom_lpf_vertical_6_neon;
    aom_lpf_vertical_6_dual = aom_lpf_vertical_6_dual_c;
    if (flags & HAS_NEON) aom_lpf_vertical_6_dual = aom_lpf_vertical_6_dual_neon;
    aom_lpf_vertical_6_quad = aom_lpf_vertical_6_quad_c;
    if (flags & HAS_NEON) aom_lpf_vertical_6_quad = aom_lpf_vertical_6_quad_neon;
    aom_lpf_vertical_8 = aom_lpf_vertical_8_c;
    if (flags & HAS_NEON) aom_lpf_vertical_8 = aom_lpf_vertical_8_neon;
    aom_lpf_vertical_8_dual = aom_lpf_vertical_8_dual_c;
    if (flags & HAS_NEON) aom_lpf_vertical_8_dual = aom_lpf_vertical_8_dual_neon;
    aom_lpf_vertical_8_quad = aom_lpf_vertical_8_quad_c;
    if (flags & HAS_NEON) aom_lpf_vertical_8_quad = aom_lpf_vertical_8_quad_neon;
    aom_paeth_predictor_16x16 = aom_paeth_predictor_16x16_c;
    if (flags & HAS_NEON) aom_paeth_predictor_16x16 = aom_paeth_predictor_16x16_neon;
    aom_paeth_predictor_16x32 = aom_paeth_predictor_16x32_c;
    if (flags & HAS_NEON) aom_paeth_predictor_16x32 = aom_paeth_predictor_16x32_neon;
    aom_paeth_predictor_16x4 = aom_paeth_predictor_16x4_c;
    if (flags & HAS_NEON) aom_paeth_predictor_16x4 = aom_paeth_predictor_16x4_neon;
    aom_paeth_predictor_16x64 = aom_paeth_predictor_16x64_c;
    if (flags & HAS_NEON) aom_paeth_predictor_16x64 = aom_paeth_predictor_16x64_neon;
    aom_paeth_predictor_16x8 = aom_paeth_predictor_16x8_c;
    if (flags & HAS_NEON) aom_paeth_predictor_16x8 = aom_paeth_predictor_16x8_neon;
    aom_paeth_predictor_32x16 = aom_paeth_predictor_32x16_c;
    if (flags & HAS_NEON) aom_paeth_predictor_32x16 = aom_paeth_predictor_32x16_neon;
    aom_paeth_predictor_32x32 = aom_paeth_predictor_32x32_c;
    if (flags & HAS_NEON) aom_paeth_predictor_32x32 = aom_paeth_predictor_32x32_neon;
    aom_paeth_predictor_32x64 = aom_paeth_predictor_32x64_c;
    if (flags & HAS_NEON) aom_paeth_predictor_32x64 = aom_paeth_predictor_32x64_neon;
    aom_paeth_predictor_32x8 = aom_paeth_predictor_32x8_c;
    if (flags & HAS_NEON) aom_paeth_predictor_32x8 = aom_paeth_predictor_32x8_neon;
    aom_paeth_predictor_4x16 = aom_paeth_predictor_4x16_c;
    if (flags & HAS_NEON) aom_paeth_predictor_4x16 = aom_paeth_predictor_4x16_neon;
    aom_paeth_predictor_4x4 = aom_paeth_predictor_4x4_c;
    if (flags & HAS_NEON) aom_paeth_predictor_4x4 = aom_paeth_predictor_4x4_neon;
    aom_paeth_predictor_4x8 = aom_paeth_predictor_4x8_c;
    if (flags & HAS_NEON) aom_paeth_predictor_4x8 = aom_paeth_predictor_4x8_neon;
    aom_paeth_predictor_64x16 = aom_paeth_predictor_64x16_c;
    if (flags & HAS_NEON) aom_paeth_predictor_64x16 = aom_paeth_predictor_64x16_neon;
    aom_paeth_predictor_64x32 = aom_paeth_predictor_64x32_c;
    if (flags & HAS_NEON) aom_paeth_predictor_64x32 = aom_paeth_predictor_64x32_neon;
    aom_paeth_predictor_64x64 = aom_paeth_predictor_64x64_c;
    if (flags & HAS_NEON) aom_paeth_predictor_64x64 = aom_paeth_predictor_64x64_neon;
    aom_paeth_predictor_8x16 = aom_paeth_predictor_8x16_c;
    if (flags & HAS_NEON) aom_paeth_predictor_8x16 = aom_paeth_predictor_8x16_neon;
    aom_paeth_predictor_8x32 = aom_paeth_predictor_8x32_c;
    if (flags & HAS_NEON) aom_paeth_predictor_8x32 = aom_paeth_predictor_8x32_neon;
    aom_paeth_predictor_8x4 = aom_paeth_predictor_8x4_c;
    if (flags & HAS_NEON) aom_paeth_predictor_8x4 = aom_paeth_predictor_8x4_neon;
    aom_paeth_predictor_8x8 = aom_paeth_predictor_8x8_c;
    if (flags & HAS_NEON) aom_paeth_predictor_8x8 = aom_paeth_predictor_8x8_neon;
    aom_scaled_2d = aom_scaled_2d_c;
    if (flags & HAS_NEON) aom_scaled_2d = aom_scaled_2d_neon;
    aom_smooth_h_predictor_16x16 = aom_smooth_h_predictor_16x16_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_16x16 = aom_smooth_h_predictor_16x16_neon;
    aom_smooth_h_predictor_16x32 = aom_smooth_h_predictor_16x32_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_16x32 = aom_smooth_h_predictor_16x32_neon;
    aom_smooth_h_predictor_16x4 = aom_smooth_h_predictor_16x4_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_16x4 = aom_smooth_h_predictor_16x4_neon;
    aom_smooth_h_predictor_16x64 = aom_smooth_h_predictor_16x64_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_16x64 = aom_smooth_h_predictor_16x64_neon;
    aom_smooth_h_predictor_16x8 = aom_smooth_h_predictor_16x8_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_16x8 = aom_smooth_h_predictor_16x8_neon;
    aom_smooth_h_predictor_32x16 = aom_smooth_h_predictor_32x16_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_32x16 = aom_smooth_h_predictor_32x16_neon;
    aom_smooth_h_predictor_32x32 = aom_smooth_h_predictor_32x32_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_32x32 = aom_smooth_h_predictor_32x32_neon;
    aom_smooth_h_predictor_32x64 = aom_smooth_h_predictor_32x64_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_32x64 = aom_smooth_h_predictor_32x64_neon;
    aom_smooth_h_predictor_32x8 = aom_smooth_h_predictor_32x8_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_32x8 = aom_smooth_h_predictor_32x8_neon;
    aom_smooth_h_predictor_4x16 = aom_smooth_h_predictor_4x16_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_4x16 = aom_smooth_h_predictor_4x16_neon;
    aom_smooth_h_predictor_4x4 = aom_smooth_h_predictor_4x4_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_4x4 = aom_smooth_h_predictor_4x4_neon;
    aom_smooth_h_predictor_4x8 = aom_smooth_h_predictor_4x8_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_4x8 = aom_smooth_h_predictor_4x8_neon;
    aom_smooth_h_predictor_64x16 = aom_smooth_h_predictor_64x16_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_64x16 = aom_smooth_h_predictor_64x16_neon;
    aom_smooth_h_predictor_64x32 = aom_smooth_h_predictor_64x32_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_64x32 = aom_smooth_h_predictor_64x32_neon;
    aom_smooth_h_predictor_64x64 = aom_smooth_h_predictor_64x64_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_64x64 = aom_smooth_h_predictor_64x64_neon;
    aom_smooth_h_predictor_8x16 = aom_smooth_h_predictor_8x16_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_8x16 = aom_smooth_h_predictor_8x16_neon;
    aom_smooth_h_predictor_8x32 = aom_smooth_h_predictor_8x32_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_8x32 = aom_smooth_h_predictor_8x32_neon;
    aom_smooth_h_predictor_8x4 = aom_smooth_h_predictor_8x4_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_8x4 = aom_smooth_h_predictor_8x4_neon;
    aom_smooth_h_predictor_8x8 = aom_smooth_h_predictor_8x8_c;
    if (flags & HAS_NEON) aom_smooth_h_predictor_8x8 = aom_smooth_h_predictor_8x8_neon;
    aom_smooth_predictor_16x16 = aom_smooth_predictor_16x16_c;
    if (flags & HAS_NEON) aom_smooth_predictor_16x16 = aom_smooth_predictor_16x16_neon;
    aom_smooth_predictor_16x32 = aom_smooth_predictor_16x32_c;
    if (flags & HAS_NEON) aom_smooth_predictor_16x32 = aom_smooth_predictor_16x32_neon;
    aom_smooth_predictor_16x4 = aom_smooth_predictor_16x4_c;
    if (flags & HAS_NEON) aom_smooth_predictor_16x4 = aom_smooth_predictor_16x4_neon;
    aom_smooth_predictor_16x64 = aom_smooth_predictor_16x64_c;
    if (flags & HAS_NEON) aom_smooth_predictor_16x64 = aom_smooth_predictor_16x64_neon;
    aom_smooth_predictor_16x8 = aom_smooth_predictor_16x8_c;
    if (flags & HAS_NEON) aom_smooth_predictor_16x8 = aom_smooth_predictor_16x8_neon;
    aom_smooth_predictor_32x16 = aom_smooth_predictor_32x16_c;
    if (flags & HAS_NEON) aom_smooth_predictor_32x16 = aom_smooth_predictor_32x16_neon;
    aom_smooth_predictor_32x32 = aom_smooth_predictor_32x32_c;
    if (flags & HAS_NEON) aom_smooth_predictor_32x32 = aom_smooth_predictor_32x32_neon;
    aom_smooth_predictor_32x64 = aom_smooth_predictor_32x64_c;
    if (flags & HAS_NEON) aom_smooth_predictor_32x64 = aom_smooth_predictor_32x64_neon;
    aom_smooth_predictor_32x8 = aom_smooth_predictor_32x8_c;
    if (flags & HAS_NEON) aom_smooth_predictor_32x8 = aom_smooth_predictor_32x8_neon;
    aom_smooth_predictor_4x16 = aom_smooth_predictor_4x16_c;
    if (flags & HAS_NEON) aom_smooth_predictor_4x16 = aom_smooth_predictor_4x16_neon;
    aom_smooth_predictor_4x4 = aom_smooth_predictor_4x4_c;
    if (flags & HAS_NEON) aom_smooth_predictor_4x4 = aom_smooth_predictor_4x4_neon;
    aom_smooth_predictor_4x8 = aom_smooth_predictor_4x8_c;
    if (flags & HAS_NEON) aom_smooth_predictor_4x8 = aom_smooth_predictor_4x8_neon;
    aom_smooth_predictor_64x16 = aom_smooth_predictor_64x16_c;
    if (flags & HAS_NEON) aom_smooth_predictor_64x16 = aom_smooth_predictor_64x16_neon;
    aom_smooth_predictor_64x32 = aom_smooth_predictor_64x32_c;
    if (flags & HAS_NEON) aom_smooth_predictor_64x32 = aom_smooth_predictor_64x32_neon;
    aom_smooth_predictor_64x64 = aom_smooth_predictor_64x64_c;
    if (flags & HAS_NEON) aom_smooth_predictor_64x64 = aom_smooth_predictor_64x64_neon;
    aom_smooth_predictor_8x16 = aom_smooth_predictor_8x16_c;
    if (flags & HAS_NEON) aom_smooth_predictor_8x16 = aom_smooth_predictor_8x16_neon;
    aom_smooth_predictor_8x32 = aom_smooth_predictor_8x32_c;
    if (flags & HAS_NEON) aom_smooth_predictor_8x32 = aom_smooth_predictor_8x32_neon;
    aom_smooth_predictor_8x4 = aom_smooth_predictor_8x4_c;
    if (flags & HAS_NEON) aom_smooth_predictor_8x4 = aom_smooth_predictor_8x4_neon;
    aom_smooth_predictor_8x8 = aom_smooth_predictor_8x8_c;
    if (flags & HAS_NEON) aom_smooth_predictor_8x8 = aom_smooth_predictor_8x8_neon;
    aom_smooth_v_predictor_16x16 = aom_smooth_v_predictor_16x16_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_16x16 = aom_smooth_v_predictor_16x16_neon;
    aom_smooth_v_predictor_16x32 = aom_smooth_v_predictor_16x32_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_16x32 = aom_smooth_v_predictor_16x32_neon;
    aom_smooth_v_predictor_16x4 = aom_smooth_v_predictor_16x4_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_16x4 = aom_smooth_v_predictor_16x4_neon;
    aom_smooth_v_predictor_16x64 = aom_smooth_v_predictor_16x64_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_16x64 = aom_smooth_v_predictor_16x64_neon;
    aom_smooth_v_predictor_16x8 = aom_smooth_v_predictor_16x8_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_16x8 = aom_smooth_v_predictor_16x8_neon;
    aom_smooth_v_predictor_32x16 = aom_smooth_v_predictor_32x16_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_32x16 = aom_smooth_v_predictor_32x16_neon;
    aom_smooth_v_predictor_32x32 = aom_smooth_v_predictor_32x32_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_32x32 = aom_smooth_v_predictor_32x32_neon;
    aom_smooth_v_predictor_32x64 = aom_smooth_v_predictor_32x64_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_32x64 = aom_smooth_v_predictor_32x64_neon;
    aom_smooth_v_predictor_32x8 = aom_smooth_v_predictor_32x8_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_32x8 = aom_smooth_v_predictor_32x8_neon;
    aom_smooth_v_predictor_4x16 = aom_smooth_v_predictor_4x16_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_4x16 = aom_smooth_v_predictor_4x16_neon;
    aom_smooth_v_predictor_4x4 = aom_smooth_v_predictor_4x4_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_4x4 = aom_smooth_v_predictor_4x4_neon;
    aom_smooth_v_predictor_4x8 = aom_smooth_v_predictor_4x8_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_4x8 = aom_smooth_v_predictor_4x8_neon;
    aom_smooth_v_predictor_64x16 = aom_smooth_v_predictor_64x16_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_64x16 = aom_smooth_v_predictor_64x16_neon;
    aom_smooth_v_predictor_64x32 = aom_smooth_v_predictor_64x32_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_64x32 = aom_smooth_v_predictor_64x32_neon;
    aom_smooth_v_predictor_64x64 = aom_smooth_v_predictor_64x64_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_64x64 = aom_smooth_v_predictor_64x64_neon;
    aom_smooth_v_predictor_8x16 = aom_smooth_v_predictor_8x16_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_8x16 = aom_smooth_v_predictor_8x16_neon;
    aom_smooth_v_predictor_8x32 = aom_smooth_v_predictor_8x32_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_8x32 = aom_smooth_v_predictor_8x32_neon;
    aom_smooth_v_predictor_8x4 = aom_smooth_v_predictor_8x4_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_8x4 = aom_smooth_v_predictor_8x4_neon;
    aom_smooth_v_predictor_8x8 = aom_smooth_v_predictor_8x8_c;
    if (flags & HAS_NEON) aom_smooth_v_predictor_8x8 = aom_smooth_v_predictor_8x8_neon;
    aom_v_predictor_16x16 = aom_v_predictor_16x16_c;
    if (flags & HAS_NEON) aom_v_predictor_16x16 = aom_v_predictor_16x16_neon;
    aom_v_predictor_32x32 = aom_v_predictor_32x32_c;
    if (flags & HAS_NEON) aom_v_predictor_32x32 = aom_v_predictor_32x32_neon;
    aom_v_predictor_4x4 = aom_v_predictor_4x4_c;
    if (flags & HAS_NEON) aom_v_predictor_4x4 = aom_v_predictor_4x4_neon;
    aom_v_predictor_8x8 = aom_v_predictor_8x8_c;
    if (flags & HAS_NEON) aom_v_predictor_8x8 = aom_v_predictor_8x8_neon;
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
