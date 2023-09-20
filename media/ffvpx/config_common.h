#ifndef MOZ_FFVPX_CONFIG_COMMON_H
#define MOZ_FFVPX_CONFIG_COMMON_H
#include "defaults_disabled.h"

#ifdef MOZ_LIBAV_FFT
#undef CONFIG_FFT
#undef CONFIG_RDFT
#define CONFIG_FFT 0
#define CONFIG_RDFT 1
#endif

#endif
