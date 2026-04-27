/*
 * gpu_duck_compat.h
 *
 * VS2010 / XDK (MSC_VER==1600) compatibility shim for the SwanStation SW
 * renderer port. SwanStation targets C++17; this shim downgrades the
 * required surface to what Xbox 360 XDK (Visual Studio 2010) supports.
 *
 * Missing in VS2010 that we work around here:
 *   - constexpr                   -> stripped (no compile-time eval)
 *   - `using X = Y;` type alias   -> must be written as typedefs per file
 *   - `if constexpr`              -> must be replaced by tag dispatch
 *   - std::is_same_v / _t suffix  -> use ::value / ::type instead
 *   - std::make_signed_t          -> typename std::make_signed<T>::type
 *   - std::clamp                  -> provided below
 *   - std::tuple / make_tuple     -> use <tuple> (VS2010 has it)
 *
 * The strategy is to make the ported headers directly compilable by
 * VS2010; this shim centralises the polyfills. Keep the upstream header
 * in plugins/gpu_duck/upstream/ untouched for diffs.
 */
#pragma once

#include <algorithm>
#include <type_traits>

/* Force-inline helpers. Hoisted up from gpu_duck_types.h so that every
 * header in the port can use ALWAYS_INLINE without depending on the
 * integer-typedefs header just for the macro. */
#ifndef ALWAYS_INLINE
#if defined(_MSC_VER)
#define ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define ALWAYS_INLINE inline
#endif
#endif

#ifndef ALWAYS_INLINE_RELEASE
#ifdef _DEBUG
#define ALWAYS_INLINE_RELEASE inline
#else
#define ALWAYS_INLINE_RELEASE ALWAYS_INLINE
#endif
#endif

#ifndef UNREFERENCED_VARIABLE
#if defined(_MSC_VER)
#define UNREFERENCED_VARIABLE(P) (P)
#else
#define UNREFERENCED_VARIABLE(P) (void)(P)
#endif
#endif

/* Strip C++11/14/17 `constexpr` on VS2010 (keyword unknown before VS2015). */
#if defined(_MSC_VER) && _MSC_VER < 1900
  #ifndef GPU_DUCK_CONSTEXPR_DEFINED
    #define GPU_DUCK_CONSTEXPR_DEFINED
    #define constexpr
  #endif
#endif

/* std::clamp is C++17. Provide it in a neutral namespace so ported code
 * can call gpu_duck::clamp<T>(v, lo, hi) where std::clamp would be used. */
namespace gpu_duck
{
  template <typename T>
  inline const T& clamp(const T& v, const T& lo, const T& hi)
  {
    return (v < lo) ? lo : ((hi < v) ? hi : v);
  }
}

/* On VS2010, `std::clamp` does not exist. Route calls through our helper.
 * We intentionally define a macro rather than injecting into std:: so we
 * don't violate [namespace.std]. The ported .cpp files will call
 * `DUCK_CLAMP(...)` in place of `std::clamp(...)`. */
#if defined(_MSC_VER) && _MSC_VER < 1900
  #define DUCK_CLAMP(v, lo, hi) ::gpu_duck::clamp((v), (lo), (hi))
#else
  #define DUCK_CLAMP(v, lo, hi) std::clamp((v), (lo), (hi))
#endif

/* ------------------------------------------------------------------
 * Host endianness detection.
 *
 * Exposed from compat (rather than from gpu_duck_gpu_backend.h) so that
 * lower-level headers — notably gpu_duck_gpu_types.h, which contains
 * structs whose byte layout must match the PSX wire format — can use
 * it. gpu_backend.h includes gpu_types.h, not the other way around, so
 * the macro had to move down here.
 *
 * On Xbox 360 / XDK (MSC_VER==1600, _M_PPC, PPC big-endian), on any
 * explicit __BIG_ENDIAN__ or __PPC__ host, we set DUCK_BIG_ENDIAN=1.
 * Everywhere else (x86/x64 Windows dev host, Linux LE) it's 0.
 * ---------------------------------------------------------------- */
#if defined(_XBOX) || (defined(_MSC_VER) && defined(_M_PPC)) || defined(__BIG_ENDIAN__) || defined(__PPC__)
  #define DUCK_BIG_ENDIAN 1
#else
  #define DUCK_BIG_ENDIAN 0
#endif

/* Type-trait polyfills for ::_t / ::_v forms that arrived in C++14/17. */
#if defined(_MSC_VER) && _MSC_VER < 1900
  namespace gpu_duck
  {
    template <typename T> struct make_signed   { typedef typename std::make_signed<T>::type type; };
    template <typename T> struct make_unsigned { typedef typename std::make_unsigned<T>::type type; };
  }
  #define DUCK_MAKE_SIGNED_T(T)   typename ::gpu_duck::make_signed<T>::type
  #define DUCK_MAKE_UNSIGNED_T(T) typename ::gpu_duck::make_unsigned<T>::type
#else
  #define DUCK_MAKE_SIGNED_T(T)   std::make_signed_t<T>
  #define DUCK_MAKE_UNSIGNED_T(T) std::make_unsigned_t<T>
#endif
