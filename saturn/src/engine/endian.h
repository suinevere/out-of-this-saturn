/* Raw - Another World Interpreter
 * Copyright (C) 2004 Gregory Montoir
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __SYS_H__
#define __SYS_H__

#include <stdint.h>

#if defined(__sh__) || defined(__SH2__) || defined(__sh2__)
  #define SYS_BIG_ENDIAN
  #define SYS_NO_UNALIGNED_ACCESS
#endif

#if defined(AUTO_DETECT_PLATFORM) && !defined(SYS_BIG_ENDIAN) && !defined(SYS_LITTLE_ENDIAN)
  #if defined(_WIN32) || defined(_WIN64)
  	#define SYS_LITTLE_ENDIAN
  #else
    #if defined(__i386__) || defined(i386) || defined(__i386) || \
        defined(_M_IX86) || defined(_X86_) || defined(_i386) || defined(__X86__)
       #define SYS_LITTLE_ENDIAN
    #elif defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64) || \
          defined(__amd64__) || defined(__amd64) || defined(__x86_64)
       #define SYS_LITTLE_ENDIAN
    #elif defined(__sparc__)
       #define SYS_BIG_ENDIAN
    #elif defined(__ia64__) || defined(_IA64) || defined(__IA64__) || \
          defined(__ia64) || defined(_M_IA64) || defined(__itanium__)
       #if defined(__linux__)
           #define SYS_LITTLE_ENDIAN
       #else 
           #warning "Unknown Itanium platform detected..."
           #define SYS_BIG_ENDIAN
       #endif
    #elif defined(__ppc__) || defined(__powerpc) || defined(__powerpc__) || \
          defined(__POWERPC__) || defined(_M_PPC) || defined(_ARCH_PPC) || \
          defined(__ppc64__)
       #define SYS_BIG_ENDIAN
    #elif defined(__arm__) || defined(__aarch64__)
       #define SYS_LITTLE_ENDIAN
    #elif defined(__mips__) || defined(__mips) || defined(__MIPS__)
       #define SYS_BIG_ENDIAN
    #else
       #warning "Unknown architecture detected..."
    #endif
  #endif
#endif

#if defined SYS_LITTLE_ENDIAN || defined SYS_NO_UNALIGNED_ACCESS
inline uint16_t READ_BE_UINT16(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 8) | b[1];
}

inline uint32_t READ_BE_UINT32(const void *ptr) {
	const uint8_t *b = (const uint8_t *)ptr;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

#elif defined SYS_BIG_ENDIAN

inline uint16_t READ_BE_UINT16(const void *ptr) {
	return *(const uint16_t *)ptr;
}

inline uint32_t READ_BE_UINT32(const void *ptr) {
	return *(const uint32_t *)ptr;
}

#else

#error No endianness defined

#endif

#endif
