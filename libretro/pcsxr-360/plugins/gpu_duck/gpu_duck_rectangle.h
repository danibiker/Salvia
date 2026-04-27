/*
 * gpu_duck_rectangle.h
 *
 * VS2010-compatible rewrite of SwanStation's common/rectangle.h.
 *
 * Differences from upstream:
 *   - constexpr is stripped (compat shim); the methods stay inline.
 *   - std::clamp calls are rewritten as DUCK_CLAMP (clamp polyfill).
 *   - FromExtents kept as static non-constexpr.
 *
 * `std::tie` used in relational operators is available in VS2010 <tuple>.
 * `std::min`/`std::max` used for Include are in VS2010 <algorithm>.
 */
#pragma once

#include "gpu_duck_compat.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <type_traits>
#include <cstring>

namespace Common {

template<typename T>
struct Rectangle
{
  /* Upstream had these as static constexpr members initialised from
   * std::numeric_limits<T>::max/min(). In VS2010, numeric_limits::max
   * isn't constexpr, so an in-class initialiser is ill-formed. Expose
   * them as static inline accessors instead; callers that read these
   * constants go through InvalidMinCoord() / InvalidMaxCoord(). */
  static T InvalidMinCoord() { return (std::numeric_limits<T>::max)(); }
  static T InvalidMaxCoord() { return (std::numeric_limits<T>::min)(); }

  Rectangle()
    : left((std::numeric_limits<T>::max)()),
      top((std::numeric_limits<T>::max)()),
      right((std::numeric_limits<T>::min)()),
      bottom((std::numeric_limits<T>::min)())
  {
  }

  Rectangle(T left_, T top_, T right_, T bottom_) : left(left_), top(top_), right(right_), bottom(bottom_) {}

  Rectangle(const Rectangle& copy) : left(copy.left), top(copy.top), right(copy.right), bottom(copy.bottom) {}

  void Set(T left_, T top_, T right_, T bottom_)
  {
    left = left_;
    top = top_;
    right = right_;
    bottom = bottom_;
  }

  static Rectangle FromExtents(T x, T y, T width, T height) { return Rectangle(x, y, x + width, y + height); }

  T GetWidth() const { return right - left; }
  T GetHeight() const { return bottom - top; }

  bool Valid() const { return left <= right && top <= bottom; }

  Rectangle& operator=(const Rectangle& rhs)
  {
    left = rhs.left;
    top = rhs.top;
    right = rhs.right;
    bottom = rhs.bottom;
    return *this;
  }

#define RELATIONAL_OPERATOR(op)                                                                                        \
  bool operator op(const Rectangle& rhs) const                                                                         \
  {                                                                                                                    \
    return std::tie(left, top, right, bottom) op std::tie(rhs.left, rhs.top, rhs.right, rhs.bottom);                   \
  }

  RELATIONAL_OPERATOR(==)
  RELATIONAL_OPERATOR(!=)
  RELATIONAL_OPERATOR(<)
  RELATIONAL_OPERATOR(<=)
  RELATIONAL_OPERATOR(>)
  RELATIONAL_OPERATOR(>=)

#undef RELATIONAL_OPERATOR

#define ARITHMETIC_OPERATOR(op)                                                                                        \
  Rectangle& operator op##=(const T amount)                                                                            \
  {                                                                                                                    \
    left op## = amount;                                                                                                \
    top op## = amount;                                                                                                 \
    right op## = amount;                                                                                               \
    bottom op## = amount;                                                                                              \
    return *this;                                                                                                      \
  }                                                                                                                    \
  Rectangle operator op(const T amount) const                                                                          \
  {                                                                                                                    \
    return Rectangle(left op amount, top op amount, right op amount, bottom op amount);                                \
  }

  ARITHMETIC_OPERATOR(+)
  ARITHMETIC_OPERATOR(-)
  ARITHMETIC_OPERATOR(*)
  ARITHMETIC_OPERATOR(/)
  ARITHMETIC_OPERATOR(%)
  ARITHMETIC_OPERATOR(>>)
  ARITHMETIC_OPERATOR(<<)
  ARITHMETIC_OPERATOR(|)
  ARITHMETIC_OPERATOR(&)
  ARITHMETIC_OPERATOR(^)

#undef ARITHMETIC_OPERATOR

  bool Intersects(const Rectangle& rhs) const
  {
    return !(left >= rhs.right || rhs.left >= right || top >= rhs.bottom || rhs.top >= bottom);
  }

  void Include(T x, T y)
  {
    left = (std::min)(left, x);
    right = (std::max)(right, x + static_cast<T>(1));
    top = (std::min)(top, y);
    bottom = (std::max)(bottom, y + static_cast<T>(1));
  }

  void Include(const Rectangle& rhs)
  {
    left = (std::min)(left, rhs.left);
    right = (std::max)(right, rhs.right);
    top = (std::min)(top, rhs.top);
    bottom = (std::max)(bottom, rhs.bottom);
  }

  void Include(T other_left, T other_right, T other_top, T other_bottom)
  {
    left = (std::min)(left, other_left);
    right = (std::max)(right, other_right);
    top = (std::min)(top, other_top);
    bottom = (std::max)(bottom, other_bottom);
  }

  void Clamp(T x1, T y1, T x2, T y2)
  {
    left = DUCK_CLAMP(left, x1, x2);
    right = DUCK_CLAMP(right, x1, x2);
    top = DUCK_CLAMP(top, y1, y2);
    bottom = DUCK_CLAMP(bottom, y1, y2);
  }

  Rectangle Clamped(T x1, T y1, T x2, T y2) const
  {
    return Rectangle(DUCK_CLAMP(left, x1, x2), DUCK_CLAMP(top, y1, y2),
                     DUCK_CLAMP(right, x1, x2), DUCK_CLAMP(bottom, y1, y2));
  }

  T left;
  T top;
  T right;
  T bottom;
};

} // namespace Common
