/*
 * gpu_duck_bitfield.h
 *
 * VS2010-compatible rewrite of SwanStation's common/bitfield.h.
 * The upstream header uses `if constexpr` (C++17) plus `std::is_same_v`
 * and `std::is_signed_v` (C++17) in GetValue(). VS2010 supports none of
 * those, so we split the three cases (bool / signed / unsigned) into
 * a tag-dispatched helper.
 *
 * The template surface matches upstream exactly:
 *   BitField<BackingDataType, DataType, BitIndex, BitCount>
 * so ported code that declares fields via this template needs no edits.
 */
#pragma once

#include "gpu_duck_compat.h"
#include <type_traits>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4800) /* forcing value to bool */
#endif

namespace gpu_duck_detail
{
  /* Three tag types representing the three GetValue paths. */
  struct bitfield_bool_tag {};
  struct bitfield_signed_tag {};
  struct bitfield_unsigned_tag {};

  /* Select the right tag for a given DataType.
   * Precedence: bool wins over signed wins over unsigned.
   * We can't use std::conditional chains cleanly on VS2010 without the
   * _t alias, so do it with a partial specialisation on a small helper. */
  template <typename DataType, bool IsBool, bool IsSigned>
  struct bitfield_tag_selector;

  template <typename DataType, bool IsSigned>
  struct bitfield_tag_selector<DataType, true, IsSigned>
  {
    typedef bitfield_bool_tag type;
  };

  template <typename DataType>
  struct bitfield_tag_selector<DataType, false, true>
  {
    typedef bitfield_signed_tag type;
  };

  template <typename DataType>
  struct bitfield_tag_selector<DataType, false, false>
  {
    typedef bitfield_unsigned_tag type;
  };

  template <typename DataType>
  struct bitfield_tag
  {
    typedef typename bitfield_tag_selector<
      DataType,
      std::is_same<DataType, bool>::value,
      std::is_signed<DataType>::value
    >::type type;
  };

  /* The three implementations of GetValue, one per tag.
   * Template parameters mirror the enclosing BitField so the helper is
   * fully generic over backing/data/shift. */
  template <typename BackingDataType, typename DataType, unsigned BitIndex, unsigned BitCount>
  ALWAYS_INLINE DataType bitfield_get(const BackingDataType& data, BackingDataType mask, bitfield_bool_tag)
  {
    return static_cast<DataType>(!!((data & mask) >> BitIndex));
  }

  template <typename BackingDataType, typename DataType, unsigned BitIndex, unsigned BitCount>
  ALWAYS_INLINE DataType bitfield_get(const BackingDataType& data, BackingDataType /*mask*/, bitfield_signed_tag)
  {
    /* Sign-extend: shift the target-type value left so the sign bit
     * lands at the top, then arithmetic-right-shift back. */
    const int shift = 8 * static_cast<int>(sizeof(DataType)) - static_cast<int>(BitCount);
    return static_cast<DataType>(
      (static_cast<DataType>(data >> BitIndex) << shift) >> shift);
  }

  template <typename BackingDataType, typename DataType, unsigned BitIndex, unsigned BitCount>
  ALWAYS_INLINE DataType bitfield_get(const BackingDataType& data, BackingDataType mask, bitfield_unsigned_tag)
  {
    return static_cast<DataType>((data & mask) >> BitIndex);
  }
}

template<typename BackingDataType, typename DataType, unsigned BitIndex, unsigned BitCount>
struct BitField
{
  ALWAYS_INLINE BackingDataType GetMask() const
  {
    return ((static_cast<BackingDataType>(~0)) >> (8 * sizeof(BackingDataType) - BitCount)) << BitIndex;
  }

  ALWAYS_INLINE operator DataType() const { return GetValue(); }

  ALWAYS_INLINE BitField& operator=(DataType value)
  {
    SetValue(value);
    return *this;
  }

  ALWAYS_INLINE DataType operator++()
  {
    DataType value = GetValue() + 1;
    SetValue(value);
    return GetValue();
  }

  ALWAYS_INLINE DataType operator++(int)
  {
    DataType value = GetValue();
    SetValue(value + 1);
    return value;
  }

  ALWAYS_INLINE DataType operator--()
  {
    DataType value = GetValue() - 1;
    SetValue(value);
    return GetValue();
  }

  ALWAYS_INLINE DataType operator--(int)
  {
    DataType value = GetValue();
    SetValue(value - 1);
    return value;
  }

  ALWAYS_INLINE BitField& operator+=(DataType rhs) { SetValue(GetValue() + rhs);  return *this; }
  ALWAYS_INLINE BitField& operator-=(DataType rhs) { SetValue(GetValue() - rhs);  return *this; }
  ALWAYS_INLINE BitField& operator*=(DataType rhs) { SetValue(GetValue() * rhs);  return *this; }
  ALWAYS_INLINE BitField& operator/=(DataType rhs) { SetValue(GetValue() / rhs);  return *this; }
  ALWAYS_INLINE BitField& operator&=(DataType rhs) { SetValue(GetValue() & rhs);  return *this; }
  ALWAYS_INLINE BitField& operator|=(DataType rhs) { SetValue(GetValue() | rhs);  return *this; }
  ALWAYS_INLINE BitField& operator^=(DataType rhs) { SetValue(GetValue() ^ rhs);  return *this; }
  ALWAYS_INLINE BitField& operator<<=(DataType rhs){ SetValue(GetValue() << rhs); return *this; }
  ALWAYS_INLINE BitField& operator>>=(DataType rhs){ SetValue(GetValue() >> rhs); return *this; }

  ALWAYS_INLINE DataType GetValue() const
  {
    typedef typename gpu_duck_detail::bitfield_tag<DataType>::type tag_t;
    return gpu_duck_detail::bitfield_get<BackingDataType, DataType, BitIndex, BitCount>(
      data, GetMask(), tag_t());
  }

  ALWAYS_INLINE void SetValue(DataType value)
  {
    data = static_cast<BackingDataType>(
      (data & ~GetMask()) |
      ((static_cast<BackingDataType>(value) << BitIndex) & GetMask()));
  }

  BackingDataType data;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif
