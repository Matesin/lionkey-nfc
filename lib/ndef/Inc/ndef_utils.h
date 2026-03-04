//
// Created by Maty Martan on 03.03.2026.

#ifndef LIONKEY_NDEF_UTILS_H
#define LIONKEY_NDEF_UTILS_H
#include <stddef.h>
#include <stdint.h>

#define GETU32(a)           (((uint32_t)(a)[0] << 24) | ((uint32_t)(a)[1] << 16) | ((uint32_t)(a)[2] << 8) | ((uint32_t)(a)[3])) /*!< Cast four Big Endian 8-bit byte array to 32-bit unsigned */
#define GETU16(a)           (((uint16_t)(a)[0] << 8) | ((uint16_t)(a)[1])) /*!< Cast two Big Endian 8-bit byte array to 16-bit unsigned */

#define SUPPRESS_WARNING(a)   ((void) (a)) /*!< Suppress unused parameter warning */

#define MIN(a,b) (((a) < (b)) ? (a) : (b)) /*!< Return the minimum of a and b */
#define MAX(a,b) (((a) > (b)) ? (a) : (b)) /*!< Return the maximum of a and b */

/* sizeof_array
 *
 * - In C: use as a macro on real arrays only: sizeof_array(arr)
 * - In C++: a type-safe constexpr function is provided that accepts only
 *   actual arrays (reference to array).
 *
 * Do not pass pointers to these helpers — they are intended for fixed-size
 * arrays known at compile time.
 */
#ifdef __cplusplus

template<typename T, size_t N>
static inline constexpr size_t sizeof_array(const T (&)[N]) noexcept { return N; }

#else

/* C version: valid only for actual arrays (not pointers) */
#define sizeof_array(a) (sizeof(a) / sizeof((a)[0]))

#endif

#endif //LIONKEY_NDEF_UTILS_H
