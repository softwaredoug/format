/*
 String formatting library for C++

 Copyright (c) 2012, Victor Zverovich
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Disable useless MSVC warnings.
#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#undef _SCL_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#include "format.h"

#include <math.h>
#include <stdint.h>

#include <cassert>
#include <cctype>
#include <climits>
#include <cstring>
#include <algorithm>

using std::size_t;
using fmt::BasicFormatter;
using fmt::Formatter;
using fmt::FormatSpec;
using fmt::StringRef;

namespace sprint {
char Case<HexCase::upper>::lookup[16] =  { '0', '1', '2', '3', '4', '5', '6', '7', '8',
									'9', 'A', 'B', 'C', 'D', 'E', 'F'};

char Case<HexCase::lower>::lookup[16] =  { '0', '1', '2', '3', '4', '5', '6', '7', '8',
									'9', 'a', 'b', 'c', 'd', 'e', 'f'};
}

#if _MSC_VER
# undef snprintf
# define snprintf _snprintf
# define isinf(x) (!_finite(x))
#endif

namespace {

// Flags.
enum { SIGN_FLAG = 1, PLUS_FLAG = 2, HASH_FLAG = 4 };

void ReportUnknownType(char code, const char *type) {
  if (std::isprint(static_cast<unsigned char>(code))) {
    throw fmt::FormatError(
        str(fmt::Format("unknown format code '{0}' for {1}") << code << type));
  }
  throw fmt::FormatError(
      str(fmt::Format("unknown format code '\\x{0:02x}' for {1}")
        << static_cast<unsigned>(code) << type));
}

// Information about an integer type.
template <typename T>
struct IntTraits {
  typedef T UnsignedType;
  static bool IsNegative(T) { return false; }
};

template <>
struct IntTraits<int> {
  typedef unsigned UnsignedType;
  static bool IsNegative(int value) { return value < 0; }
};

template <>
struct IntTraits<long> {
  typedef unsigned long UnsignedType;
  static bool IsNegative(long value) { return value < 0; }
};

template <typename T>
struct IsLongDouble { enum {VALUE = 0}; };

template <>
struct IsLongDouble<long double> { enum {VALUE = 1}; };

inline unsigned CountDigits(uint64_t n) {
  unsigned count = 1;
  for (;;) {
    // Integer division is slow so do it for a group of four digits instead
    // of for every digit. The idea comes from the talk by Alexandrescu
    // "Three Optimization Tips for C++". See speed-test for a comparison.
    if (n < 10) return count;
    if (n < 100) return count + 1;
    if (n < 1000) return count + 2;
    if (n < 10000) return count + 3;
    n /= 10000u;
    count += 4;
  }
}

const char DIGITS[] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

void FormatDecimal(char *buffer, uint64_t value, unsigned num_digits) {
  --num_digits;
  while (value >= 100) {
    // Integer division is slow so do it for a group of two digits instead
    // of for every digit. The idea comes from the talk by Alexandrescu
    // "Three Optimization Tips for C++". See speed-test for a comparison.
    unsigned index = (value % 100) * 2;
    value /= 100;
    buffer[num_digits] = DIGITS[index + 1];
    buffer[num_digits - 1] = DIGITS[index];
    num_digits -= 2;
  }
  if (value < 10) {
    *buffer = static_cast<char>('0' + value);
    return;
  }
  unsigned index = static_cast<unsigned>(value * 2);
  buffer[1] = DIGITS[index + 1];
  buffer[0] = DIGITS[index];
}

// Fills the padding around the content and returns the pointer to the
// content area.
char *FillPadding(char *buffer,
    unsigned total_size, std::size_t content_size, char fill) {
  std::size_t padding = total_size - content_size;
  std::size_t left_padding = padding / 2;
  std::fill_n(buffer, left_padding, fill);
  buffer += left_padding;
  char *content = buffer;
  std::fill_n(buffer + content_size, padding - left_padding, fill);
  return content;
}

#ifdef _MSC_VER
int signbit(double value) {
  if (value < 0) return 1;
  if (value == value) return 0;
  int dec = 0, sign = 0;
  _ecvt(value, 0, &dec, &sign);
  return sign;
}
#endif
}

char *BasicFormatter::PrepareFilledBuffer(
    unsigned size, const FormatSpec &spec, char sign) {
  if (spec.width <= size) {
    char *p = GrowBuffer(size);
    *p = sign;
    return p + size - 1;
  }
  char *p = GrowBuffer(spec.width);
  char *end = p + spec.width;
  if (spec.align == ALIGN_LEFT) {
    *p = sign;
    p += size;
    std::fill(p, end, spec.fill);
  } else if (spec.align == ALIGN_CENTER) {
    p = FillPadding(p, spec.width, size, spec.fill);
    *p = sign;
    p += size;
  } else {
    if (spec.align == ALIGN_NUMERIC) {
      if (sign) {
        *p++ = sign;
        --size;
      }
    } else {
      *(end - size) = sign;
    }
    std::fill(p, end - size, spec.fill);
    p = end;
  }
  return p - 1;
}

template <typename T>
void BasicFormatter::FormatInt(T value, const FormatSpec &spec) {
  unsigned size = 0;
  char sign = 0;
  typedef typename IntTraits<T>::UnsignedType UnsignedType;
  UnsignedType abs_value = value;
  if (IntTraits<T>::IsNegative(value)) {
    sign = '-';
    ++size;
    abs_value = 0 - abs_value;
  } else if ((spec.flags & SIGN_FLAG) != 0) {
    sign = (spec.flags & PLUS_FLAG) != 0 ? '+' : ' ';
    ++size;
  }
  switch (spec.type) {
  case 0: case 'd': {
    unsigned num_digits = CountDigits(abs_value);
    char *p = PrepareFilledBuffer(size + num_digits, spec, sign)
        - num_digits + 1;
    FormatDecimal(p, abs_value, num_digits);
    break;
  }
  case 'x': case 'X': {
    UnsignedType n = abs_value;
    bool print_prefix = (spec.flags & HASH_FLAG) != 0;
    if (print_prefix) size += 2;
    do {
      ++size;
    } while ((n >>= 4) != 0);
    char *p = PrepareFilledBuffer(size, spec, sign);
    n = abs_value;
    const char *digits = spec.type == 'x' ?
        "0123456789abcdef" : "0123456789ABCDEF";
    do {
      *p-- = digits[n & 0xf];
    } while ((n >>= 4) != 0);
    if (print_prefix) {
      *p-- = spec.type;
      *p = '0';
    }
    break;
  }
  case 'o': {
    UnsignedType n = abs_value;
    bool print_prefix = (spec.flags & HASH_FLAG) != 0;
    if (print_prefix) ++size;
    do {
      ++size;
    } while ((n >>= 3) != 0);
    char *p = PrepareFilledBuffer(size, spec, sign);
    n = abs_value;
    do {
      *p-- = '0' + (n & 7);
    } while ((n >>= 3) != 0);
    if (print_prefix)
      *p = '0';
    break;
  }
  default:
    ReportUnknownType(spec.type, "integer");
    break;
  }
}

template <typename T>
void BasicFormatter::FormatDouble(
    T value, const FormatSpec &spec, int precision) {
  // Check type.
  char type = spec.type;
  bool upper = false;
  switch (type) {
  case 0:
    type = 'g';
    break;
  case 'e': case 'f': case 'g':
    break;
  case 'F':
#ifdef _MSC_VER
    // MSVC's printf doesn't support 'F'.
    type = 'f';
#endif
    // Fall through.
  case 'E': case 'G':
    upper = true;
    break;
  default:
    ReportUnknownType(type, "double");
    break;
  }

  char sign = 0;
  // Use signbit instead of value < 0 because the latter is always
  // false for NaN.
  if (signbit(value)) {
    sign = '-';
    value = -value;
  } else if ((spec.flags & SIGN_FLAG) != 0) {
    sign = (spec.flags & PLUS_FLAG) != 0 ? '+' : ' ';
  }

  if (value != value) {
    // Format NaN ourselves because sprintf's output is not consistent
    // across platforms.
    std::size_t size = 4;
    const char *nan = upper ? " NAN" : " nan";
    if (!sign) {
      --size;
      ++nan;
    }
    char *out = FormatString(nan, size, spec);
    if (sign)
      *out = sign;
    return;
  }

  if (isinf(value)) {
    // Format infinity ourselves because sprintf's output is not consistent
    // across platforms.
    std::size_t size = 4;
    const char *inf = upper ? " INF" : " inf";
    if (!sign) {
      --size;
      ++inf;
    }
    char *out = FormatString(inf, size, spec);
    if (sign)
      *out = sign;
    return;
  }

  size_t offset = buffer_.size();
  unsigned width = spec.width;
  if (sign) {
    buffer_.reserve(buffer_.size() + std::max(width, 1u));
    if (width > 0)
      --width;
    ++offset;
  }

  // Build format string.
  enum { MAX_FORMAT_SIZE = 10}; // longest format: %#-*.*Lg
  char format[MAX_FORMAT_SIZE];
  char *format_ptr = format;
  *format_ptr++ = '%';
  unsigned width_for_sprintf = width;
  if ((spec.flags & HASH_FLAG) != 0)
    *format_ptr++ = '#';
  if (spec.align == ALIGN_CENTER) {
    width_for_sprintf = 0;
  } else {
    if (spec.align == ALIGN_LEFT)
      *format_ptr++ = '-';
    if (width != 0)
      *format_ptr++ = '*';
  }
  if (precision >= 0) {
    *format_ptr++ = '.';
    *format_ptr++ = '*';
  }
  if (IsLongDouble<T>::VALUE)
    *format_ptr++ = 'L';
  *format_ptr++ = type;
  *format_ptr = '\0';

  // Format using snprintf.
  for (;;) {
    size_t size = buffer_.capacity() - offset;
    int n = 0;
    char *start = &buffer_[offset];
    if (width_for_sprintf == 0) {
      n = precision < 0 ?
          snprintf(start, size, format, value) :
          snprintf(start, size, format, precision, value);
    } else {
      n = precision < 0 ?
          snprintf(start, size, format, width_for_sprintf, value) :
          snprintf(start, size, format, width_for_sprintf, precision, value);
    }
    if (n >= 0 && offset + n < buffer_.capacity()) {
      if (sign) {
        if ((spec.align != ALIGN_RIGHT && spec.align != ALIGN_DEFAULT) ||
            *start != ' ') {
          *(start - 1) = sign;
          sign = 0;
        } else {
          *(start - 1) = spec.fill;
        }
        ++n;
      }
      if (spec.align == ALIGN_CENTER && spec.width > static_cast<unsigned>(n)) {
        char *p = GrowBuffer(spec.width);
        std::copy(p, p + n, p + (spec.width - n) / 2);
        FillPadding(p, spec.width, n, spec.fill);
        return;
      }
      if (spec.fill != ' ' || sign) {
        while (*start == ' ')
          *start++ = spec.fill;
        if (sign)
          *(start - 1) = sign;
      }
      GrowBuffer(n);
      return;
    }
    buffer_.reserve(n >= 0 ? offset + n + 1 : 2 * buffer_.capacity());
  }
}

char *BasicFormatter::FormatString(
    const char *s, std::size_t size, const FormatSpec &spec) {
  char *out = 0;
  if (spec.width > size) {
    out = GrowBuffer(spec.width);
    if (spec.align == ALIGN_RIGHT) {
      std::fill_n(out, spec.width - size, spec.fill);
      out += spec.width - size;
    } else if (spec.align == ALIGN_CENTER) {
      out = FillPadding(out, spec.width, size, spec.fill);
    } else {
      std::fill_n(out + size, spec.width - size, spec.fill);
    }
  } else {
    out = GrowBuffer(size);
  }
  std::copy(s, s + size, out);
  return out;
}

void BasicFormatter::operator<<(int value) {
  unsigned abs_value = value;
  unsigned num_digits = 0;
  char *out = 0;
  if (value >= 0) {
    num_digits = CountDigits(abs_value);
    out = GrowBuffer(num_digits);
  } else {
    abs_value = 0 - abs_value;
    num_digits = CountDigits(abs_value);
    out = GrowBuffer(num_digits + 1);
    *out++ = '-';
  }
  FormatDecimal(out, abs_value, num_digits);
}

void BasicFormatter::operator<<(sprint::AppendTransaction<char>& spr) {
	// Acting optimistically on the buffer
	buffer_.appendTransact(spr);
}

// Throws Exception(message) if format contains '}', otherwise throws
// FormatError reporting unmatched '{'. The idea is that unmatched '{'
// should override other errors.
void Formatter::ReportError(const char *s, StringRef message) const {
  for (int num_open_braces = num_open_braces_; *s; ++s) {
    if (*s == '{') {
      ++num_open_braces;
    } else if (*s == '}') {
      if (--num_open_braces == 0)
        throw fmt::FormatError(message);
    }
  }
  throw fmt::FormatError("unmatched '{' in format");
}

// Parses an unsigned integer advancing s to the end of the parsed input.
// This function assumes that the first character of s is a digit.
unsigned Formatter::ParseUInt(const char *&s) const {
  assert('0' <= *s && *s <= '9');
  unsigned value = 0;
  do {
    unsigned new_value = value * 10 + (*s++ - '0');
    if (new_value < value)  // Check if value wrapped around.
      ReportError(s, "number is too big in format");
    value = new_value;
  } while ('0' <= *s && *s <= '9');
  return value;
}

inline const Formatter::Arg &Formatter::ParseArgIndex(const char *&s) {
  unsigned arg_index = 0;
  if (*s < '0' || *s > '9') {
    if (*s != '}' && *s != ':')
      ReportError(s, "invalid argument index in format string");
    if (next_arg_index_ < 0) {
      ReportError(s,
          "cannot switch from manual to automatic argument indexing");
    }
    arg_index = next_arg_index_++;
  } else {
    if (next_arg_index_ > 0) {
      ReportError(s,
          "cannot switch from automatic to manual argument indexing");
    }
    next_arg_index_ = -1;
    arg_index = ParseUInt(s);
    if (arg_index >= args_.size())
      ReportError(s, "argument index is out of range in format");
  }
  return *args_[arg_index];
}

void Formatter::CheckSign(const char *&s, const Arg &arg) {
  if (arg.type > LAST_NUMERIC_TYPE) {
    ReportError(s,
        Format("format specifier '{0}' requires numeric argument") << *s);
  }
  if (arg.type == UINT || arg.type == ULONG) {
    ReportError(s,
        Format("format specifier '{0}' requires signed argument") << *s);
  }
  ++s;
}

void Formatter::DoFormat() {
  const char *start = format_;
  format_ = 0;
  next_arg_index_ = 0;
  const char *s = start;
  while (*s) {
    char c = *s++;
    if (c != '{' && c != '}') continue;
    if (*s == c) {
      buffer_.append(start, s);
      start = ++s;
      continue;
    }
    if (c == '}')
      throw FormatError("unmatched '}' in format");
    num_open_braces_= 1;
    buffer_.append(start, s - 1);

    const Arg &arg = ParseArgIndex(s);

    FormatSpec spec;
    int precision = -1;
    if (*s == ':') {
      ++s;

      // Parse fill and alignment.
      if (char c = *s) {
        const char *p = s + 1;
        spec.align = ALIGN_DEFAULT;
        do {
          switch (*p) {
          case '<':
            spec.align = ALIGN_LEFT;
            break;
          case '>':
            spec.align = ALIGN_RIGHT;
            break;
          case '=':
            spec.align = ALIGN_NUMERIC;
            break;
          case '^':
            spec.align = ALIGN_CENTER;
            break;
          }
          if (spec.align != ALIGN_DEFAULT) {
            if (p != s) {
              if (c == '}') break;
              if (c == '{')
                ReportError(s, "invalid fill character '{'");
              s += 2;
              spec.fill = c;
            } else ++s;
            if (spec.align == ALIGN_NUMERIC && arg.type > LAST_NUMERIC_TYPE)
              ReportError(s, "format specifier '=' requires numeric argument");
            break;
          }
        } while (--p >= s);
      }

      // Parse sign.
      switch (*s) {
      case '+':
        CheckSign(s, arg);
        spec.flags |= SIGN_FLAG | PLUS_FLAG;
        break;
      case '-':
        CheckSign(s, arg);
        break;
      case ' ':
        CheckSign(s, arg);
        spec.flags |= SIGN_FLAG;
        break;
      }

      if (*s == '#') {
        if (arg.type > LAST_NUMERIC_TYPE)
          ReportError(s, "format specifier '#' requires numeric argument");
        spec.flags |= HASH_FLAG;
        ++s;
      }

      // Parse width and zero flag.
      if ('0' <= *s && *s <= '9') {
        if (*s == '0') {
          if (arg.type > LAST_NUMERIC_TYPE)
            ReportError(s, "format specifier '0' requires numeric argument");
          spec.align = ALIGN_NUMERIC;
          spec.fill = '0';
        }
        // Zero may be parsed again as a part of the width, but it is simpler
        // and more efficient than checking if the next char is a digit.
        unsigned value = ParseUInt(s);
        if (value > INT_MAX)
          ReportError(s, "number is too big in format");
        spec.width = value;
      }

      // Parse precision.
      if (*s == '.') {
        ++s;
        precision = 0;
        if ('0' <= *s && *s <= '9') {
          unsigned value = ParseUInt(s);
          if (value > INT_MAX)
            ReportError(s, "number is too big in format");
          precision = value;
        } else if (*s == '{') {
          ++s;
          ++num_open_braces_;
          const Arg &precision_arg = ParseArgIndex(s);
          unsigned long value = 0;
          switch (precision_arg.type) {
          case INT:
            if (precision_arg.int_value < 0)
              ReportError(s, "negative precision in format");
            value = precision_arg.int_value;
            break;
          case UINT:
            value = precision_arg.uint_value;
            break;
          case LONG:
            if (precision_arg.long_value < 0)
              ReportError(s, "negative precision in format");
            value = precision_arg.long_value;
            break;
          case ULONG:
            value = precision_arg.ulong_value;
            break;
          default:
            ReportError(s, "precision is not integer");
          }
          if (value > INT_MAX)
            ReportError(s, "number is too big in format");
          precision = value;
          if (*s++ != '}')
            throw FormatError("unmatched '{' in format");
          --num_open_braces_;
        } else {
          ReportError(s, "missing precision in format");
        }
        if (arg.type != DOUBLE && arg.type != LONG_DOUBLE) {
          ReportError(s,
              "precision specifier requires floating-point argument");
        }
      }

      // Parse type.
      if (*s != '}' && *s)
        spec.type = *s++;
    }

    if (*s++ != '}')
      throw FormatError("unmatched '{' in format");
    start = s;

    // Format argument.
    switch (arg.type) {
    case INT:
      FormatInt(arg.int_value, spec);
      break;
    case UINT:
      FormatInt(arg.uint_value, spec);
      break;
    case LONG:
      FormatInt(arg.long_value, spec);
      break;
    case ULONG:
      FormatInt(arg.ulong_value, spec);
      break;
    case DOUBLE:
      FormatDouble(arg.double_value, spec, precision);
      break;
    case LONG_DOUBLE:
      FormatDouble(arg.long_double_value, spec, precision);
      break;
    case CHAR: {
      if (spec.type && spec.type != 'c')
        ReportUnknownType(spec.type, "char");
      char *out = 0;
      if (spec.width > 1) {
        out = GrowBuffer(spec.width);
        if (spec.align == ALIGN_RIGHT) {
          std::fill_n(out, spec.width - 1, spec.fill);
          out += spec.width - 1;
        } else if (spec.align == ALIGN_CENTER) {
          out = FillPadding(out, spec.width, 1, spec.fill);
        } else {
          std::fill_n(out + 1, spec.width - 1, spec.fill);
        }
      } else {
        out = GrowBuffer(1);
      }
      *out = arg.int_value;
      break;
    }
    case STRING: {
      if (spec.type && spec.type != 's')
        ReportUnknownType(spec.type, "string");
      const char *str = arg.string.value;
      size_t size = arg.string.size;
      if (size == 0) {
        if (!str)
          throw FormatError("string pointer is null");
        if (*str)
          size = std::strlen(str);
      }
      FormatString(str, size, spec);
      break;
    }
    case POINTER:
      if (spec.type && spec.type != 'p')
        ReportUnknownType(spec.type, "pointer");
      spec.flags = HASH_FLAG;
      spec.type = 'x';
      FormatInt(reinterpret_cast<uintptr_t>(arg.pointer_value), spec);
      break;
    case CUSTOM:
      if (spec.type)
        ReportUnknownType(spec.type, "object");
      (this->*arg.custom.format)(arg.custom.value, spec);
      break;
    default:
      assert(false);
      break;
    }
  }
  buffer_.append(start, s);
}
