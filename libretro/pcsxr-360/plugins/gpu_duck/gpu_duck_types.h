/*
 * gpu_duck_types.h
 *
 * VS2010-compatible rewrite of SwanStation's src/common/types.h.
 *
 * Changes from upstream:
 *   - `using X = Y;` aliases converted to typedefs (C++11 alias decls
 *     are accepted by VS2010 in most contexts but not uniformly; use
 *     typedefs for safety).
 *   - `std::make_signed_t` / `std::make_unsigned_t` replaced with the
 *     DUCK_MAKE_SIGNED_T / DUCK_MAKE_UNSIGNED_T macros from the shim.
 *   - `constexpr` is left in place — the shim strips the keyword on
 *     VS2010 so the same source still compiles there.
 *   - The enum-class bitwise-operators macro is kept; VS2010 supports
 *     scoped enums since MSC_VER >= 1700. For 1600 (XDK) we still
 *     declare `enum class` usage — if the downstream XDK actually
 *     compiles as MSC_VER 1600, the ported code will have to be
 *     adjusted there (most upstream enum classes become plain enums
 *     wrapped in a namespace). We cross that bridge when wiring up.
 */
#pragma once

#include "gpu_duck_compat.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

/* Force-inline helpers. */
#ifndef ALWAYS_INLINE
#if defined(_MSC_VER)
#define ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define ALWAYS_INLINE inline
#endif
#endif

#ifdef _DEBUG
#define ALWAYS_INLINE_RELEASE inline
#else
#define ALWAYS_INLINE_RELEASE ALWAYS_INLINE
#endif

#ifndef UNREFERENCED_VARIABLE
#if defined(_MSC_VER)
#define UNREFERENCED_VARIABLE(P) (P)
#elif defined(__GNUC__) || defined(__clang__) || defined(__EMSCRIPTEN__)
#define UNREFERENCED_VARIABLE(P) (void)(P)
#else
#define UNREFERENCED_VARIABLE(P) (P)
#endif
#endif

#ifndef countof
#ifdef _countof
#define countof _countof
#else
template<typename T, size_t N>
char (&__countof_ArraySizeHelper(T (&array)[N]))[N];
#define countof(array) (sizeof(__countof_ArraySizeHelper(array)))
#endif
#endif

#ifndef offsetof
#define offsetof(st, m) ((size_t)((char*)&((st*)(0))->m - (char*)0))
#endif

#ifdef __GNUC__
#ifndef _WIN32
#define printflike(n,m) __attribute__((format(printf,n,m)))
#else
#define printflike(n,m)
#endif
#else
#define printflike(n,m)
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4201)
#pragma warning(disable : 4100)
#pragma warning(disable : 4355)
#endif

/* Fixed-width integer typedefs. Upstream uses `using`; VS2010 accepts
 * alias-declarations in some contexts but not reliably in templates,
 * so we keep them as old-style typedefs here. */
typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;

/* Zero-extending helper.
 * NOTE: constexpr is dropped on VS2010 by the shim; the inline hint
 * plus __forceinline is what actually matters at this scale. */
template<typename TReturn, typename TValue>
ALWAYS_INLINE constexpr TReturn ZeroExtend(TValue value)
{
  return static_cast<TReturn>(static_cast<typename std::make_unsigned<TReturn>::type>(
    static_cast<typename std::make_unsigned<TValue>::type>(value)));
}

template<typename TReturn, typename TValue>
ALWAYS_INLINE constexpr TReturn SignExtend(TValue value)
{
  return static_cast<TReturn>(
    static_cast<typename std::make_signed<TReturn>::type>(static_cast<typename std::make_signed<TValue>::type>(value)));
}

template<typename TValue>
ALWAYS_INLINE constexpr u16 ZeroExtend16(TValue value)
{
  return ZeroExtend<u16, TValue>(value);
}
template<typename TValue>
ALWAYS_INLINE constexpr u32 ZeroExtend32(TValue value)
{
  return ZeroExtend<u32, TValue>(value);
}
template<typename TValue>
ALWAYS_INLINE constexpr u64 ZeroExtend64(TValue value)
{
  return ZeroExtend<u64, TValue>(value);
}
template<typename TValue>
ALWAYS_INLINE constexpr u16 SignExtend16(TValue value)
{
  return SignExtend<u16, TValue>(value);
}
template<typename TValue>
ALWAYS_INLINE constexpr u32 SignExtend32(TValue value)
{
  return SignExtend<u32, TValue>(value);
}
template<typename TValue>
ALWAYS_INLINE constexpr u64 SignExtend64(TValue value)
{
  return SignExtend<u64, TValue>(value);
}
template<typename TValue>
ALWAYS_INLINE constexpr u8 Truncate8(TValue value)
{
  return static_cast<u8>(static_cast<typename std::make_unsigned<TValue>::type>(value));
}
template<typename TValue>
ALWAYS_INLINE constexpr u16 Truncate16(TValue value)
{
  return static_cast<u16>(static_cast<typename std::make_unsigned<TValue>::type>(value));
}
template<typename TValue>
ALWAYS_INLINE constexpr u32 Truncate32(TValue value)
{
  return static_cast<u32>(static_cast<typename std::make_unsigned<TValue>::type>(value));
}

ALWAYS_INLINE constexpr u8 BinaryToBCD(u8 value)   { return ((value / 10) << 4) + (value % 10); }
ALWAYS_INLINE constexpr u8 PackedBCDToBinary(u8 value) { return ((value >> 4) * 10) + (value % 16); }
ALWAYS_INLINE constexpr u8 IsValidBCDDigit(u8 digit)   { return (digit <= 9); }
ALWAYS_INLINE constexpr u8 IsValidPackedBCD(u8 value)
{
  return IsValidBCDDigit(value & 0x0F) && IsValidBCDDigit(value >> 4);
}

ALWAYS_INLINE constexpr u8  BoolToUInt8(bool value)  { return static_cast<u8>(value); }
ALWAYS_INLINE constexpr u16 BoolToUInt16(bool value) { return static_cast<u16>(value); }
ALWAYS_INLINE constexpr u32 BoolToUInt32(bool value) { return static_cast<u32>(value); }
ALWAYS_INLINE constexpr u64 BoolToUInt64(bool value) { return static_cast<u64>(value); }

template<typename TValue>
ALWAYS_INLINE constexpr bool ConvertToBool(TValue value)
{
  return static_cast<bool>(value);
}

template<typename TValue>
ALWAYS_INLINE bool ConvertToBoolUnchecked(TValue value)
{
  bool ret;
  std::memcpy(&ret, &value, sizeof(bool));
  return ret;
}

/* Generic N-bit sign extension.
 * Upstream: `static_cast<std::make_signed_t<T>>(value)`.
 * Here: DUCK_MAKE_SIGNED_T(T) -> `typename std::make_signed<T>::type` on VS2010. */
template<int NBITS, typename T>
ALWAYS_INLINE constexpr T SignExtendN(T value)
{
  const int shift = 8 * static_cast<int>(sizeof(T)) - NBITS;
  return static_cast<T>((static_cast<DUCK_MAKE_SIGNED_T(T)>(value) << shift) >> shift);
}

/* Enum-class bitwise operators. Kept identical to upstream since VS2010
 * / MSC_VER 1600 DOES support `enum class` (added in 1600 explicitly).
 * If an XDK variant of VS2010 rejects this, callers that declare enum
 * classes will need adjustment, not this macro. */
#define IMPLEMENT_ENUM_CLASS_BITWISE_OPERATORS(type_)                                                                  \
  ALWAYS_INLINE constexpr type_ operator&(type_ lhs, type_ rhs)                                                        \
  {                                                                                                                    \
    return static_cast<type_>(static_cast<std::underlying_type<type_>::type>(lhs) &                                    \
                              static_cast<std::underlying_type<type_>::type>(rhs));                                    \
  }                                                                                                                    \
  ALWAYS_INLINE constexpr type_ operator|(type_ lhs, type_ rhs)                                                        \
  {                                                                                                                    \
    return static_cast<type_>(static_cast<std::underlying_type<type_>::type>(lhs) |                                    \
                              static_cast<std::underlying_type<type_>::type>(rhs));                                    \
  }                                                                                                                    \
  ALWAYS_INLINE constexpr type_ operator^(type_ lhs, type_ rhs)                                                        \
  {                                                                                                                    \
    return static_cast<type_>(static_cast<std::underlying_type<type_>::type>(lhs) ^                                    \
                              static_cast<std::underlying_type<type_>::type>(rhs));                                    \
  }                                                                                                                    \
  ALWAYS_INLINE constexpr type_ operator~(type_ val)                                                                   \
  {                                                                                                                    \
    return static_cast<type_>(~static_cast<std::underlying_type<type_>::type>(val));                                   \
  }                                                                                                                    \
  ALWAYS_INLINE constexpr type_& operator&=(type_& lhs, type_ rhs)                                                     \
  {                                                                                                                    \
    lhs = static_cast<type_>(static_cast<std::underlying_type<type_>::type>(lhs) &                                     \
                             static_cast<std::underlying_type<type_>::type>(rhs));                                     \
    return lhs;                                                                                                        \
  }                                                                                                                    \
  ALWAYS_INLINE constexpr type_& operator|=(type_& lhs, type_ rhs)                                                     \
  {                                                                                                                    \
    lhs = static_cast<type_>(static_cast<std::underlying_type<type_>::type>(lhs) |                                     \
                             static_cast<std::underlying_type<type_>::type>(rhs));                                     \
    return lhs;                                                                                                        \
  }                                                                                                                    \
  ALWAYS_INLINE constexpr type_& operator^=(type_& lhs, type_ rhs)                                                     \
  {                                                                                                                    \
    lhs = static_cast<type_>(static_cast<std::underlying_type<type_>::type>(lhs) ^                                     \
                             static_cast<std::underlying_type<type_>::type>(rhs));                                     \
    return lhs;                                                                                                        \
  }
