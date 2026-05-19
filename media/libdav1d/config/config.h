/* Mozilla build configuration for bundled dav1d. */
#ifndef MOZ_DAV1D_CONFIG_H
#define MOZ_DAV1D_CONFIG_H

#ifdef __APPLE__
#define PREFIX 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
#  define ARCH_X86 1
#  define ARCH_X86_64 1
#  define ARCH_X86_32 0
#elif defined(__i386__) || defined(_M_IX86)
#  define ARCH_X86 1
#  define ARCH_X86_64 0
#  define ARCH_X86_32 1
#else
#  define ARCH_X86 0
#  define ARCH_X86_64 0
#  define ARCH_X86_32 0
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#  define ARCH_AARCH64 1
#else
#  define ARCH_AARCH64 0
#endif

#if defined(__arm__) || defined(_M_ARM)
#  define ARCH_ARM 1
#else
#  define ARCH_ARM 0
#endif

#if defined(__loongarch64)
#  define ARCH_LOONGARCH 1
#  define ARCH_LOONGARCH64 1
#else
#  define ARCH_LOONGARCH 0
#  define ARCH_LOONGARCH64 0
#endif

#if defined(__powerpc64__) && \
    (defined(_LITTLE_ENDIAN) || defined(__LITTLE_ENDIAN__) || \
     (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
#  define ARCH_PPC64LE 1
#else
#  define ARCH_PPC64LE 0
#endif

#if defined(__riscv)
#  define ARCH_RISCV 1
#else
#  define ARCH_RISCV 0
#endif

#if defined(IS_BIG_ENDIAN) || defined(__BIG_ENDIAN__)
#  define ENDIANNESS_BIG 1
#else
#  define ENDIANNESS_BIG 0
#endif

#define CONFIG_8BPC 1
#define CONFIG_16BPC 1
#define CONFIG_LOG 0
#define CONFIG_MACOS_KPERF 0
#if ARCH_AARCH64 || ARCH_ARM || ARCH_LOONGARCH64 || ARCH_PPC64LE || ARCH_X86
#  define HAVE_ASM 1
#else
#  define HAVE_ASM 0
#endif
#define TRIM_DSP_FUNCTIONS 0

#define HAVE_AS_FUNC 0
#if ARCH_AARCH64
#  define HAVE_AS_ARCH_DIRECTIVE 1
#  define AS_ARCH_LEVEL armv8-a
#  define HAVE_AS_ARCHEXT_DOTPROD_DIRECTIVE 1
#  define HAVE_AS_ARCHEXT_I8MM_DIRECTIVE 1
#  define HAVE_AS_ARCHEXT_SVE_DIRECTIVE 1
#  define HAVE_AS_ARCHEXT_SVE2_DIRECTIVE 1
#  define HAVE_DOTPROD 1
#  define HAVE_I8MM 1
#  define HAVE_SVE 1
#  define HAVE_SVE2 1
#else
#  define HAVE_AS_ARCH_DIRECTIVE 0
#  define HAVE_AS_ARCHEXT_DOTPROD_DIRECTIVE 0
#  define HAVE_AS_ARCHEXT_I8MM_DIRECTIVE 0
#  define HAVE_AS_ARCHEXT_SVE_DIRECTIVE 0
#  define HAVE_AS_ARCHEXT_SVE2_DIRECTIVE 0
#  define HAVE_DOTPROD 0
#  define HAVE_I8MM 0
#  define HAVE_SVE 0
#  define HAVE_SVE2 0
#endif

#define HAVE_ALIGNED_ALLOC 0
#define HAVE_DLSYM 0
#define HAVE_ELF_AUX_INFO 0
#if defined(__linux__)
#  define HAVE_GETAUXVAL 1
#else
#  define HAVE_GETAUXVAL 0
#endif
#define HAVE_MEMALIGN 0
#define HAVE_POSIX_MEMALIGN 0
#define HAVE_PTHREAD_GETAFFINITY_NP 0
#define HAVE_PTHREAD_NP_H 0
#define HAVE_PTHREAD_SETNAME_NP 0
#define HAVE_PTHREAD_SET_NAME_NP 0

#ifdef _WIN32
#  define HAVE_IO_H 1
#  define HAVE_SYS_TYPES_H 1
#  define HAVE_UNISTD_H 0
#else
#  define HAVE_IO_H 0
#  define HAVE_SYS_TYPES_H 1
#  define HAVE_UNISTD_H 1
#endif

#endif /* MOZ_DAV1D_CONFIG_H */
