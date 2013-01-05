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

#ifndef FORMAT_H_
#define FORMAT_H_

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>

namespace format {

namespace internal {

// A simple array for POD types with the first SIZE elements stored in
// the object itself. It supports a subset of std::vector's operations.
template <typename T, std::size_t SIZE>
class Array {
 private:
  std::size_t size_;
  std::size_t capacity_;
  T *ptr_;
  T data_[SIZE];

  void Grow(std::size_t size);

  // Do not implement!
  Array(const Array &);
  void operator=(const Array &);

 public:
  Array() : size_(0), capacity_(SIZE), ptr_(data_) {}
  ~Array() {
    if (ptr_ != data_) delete [] ptr_;
  }

  // Returns the size of this array.
  std::size_t size() const { return size_; }

  // Returns the capacity of this array.
  std::size_t capacity() const { return capacity_; }

  // Resizes the array. If T is a POD type new elements are not initialized.
  void resize(std::size_t new_size) {
    if (new_size > capacity_)
      Grow(new_size);
    size_ = new_size;
  }

  void reserve(std::size_t capacity) {
    if (capacity > capacity_)
      Grow(capacity);
  }

  void clear() { size_ = 0; }

  void push_back(const T &value) {
    if (size_ == capacity_)
      Grow(size_ + 1);
    ptr_[size_++] = value;
  }

  // Appends data to the end of the array.
  void append(const T *begin, const T *end);

  T &operator[](std::size_t index) { return ptr_[index]; }
  const T &operator[](std::size_t index) const { return ptr_[index]; }
};

template <typename T, std::size_t SIZE>
void Array<T, SIZE>::Grow(std::size_t size) {
  capacity_ = std::max(size, capacity_ + capacity_ / 2);
  T *p = new T[capacity_];
  std::copy(ptr_, ptr_ + size_, p);
  if (ptr_ != data_)
    delete [] ptr_;
  ptr_ = p;
}

template <typename T, std::size_t SIZE>
void Array<T, SIZE>::append(const T *begin, const T *end) {
  std::ptrdiff_t num_elements = end - begin;
  if (size_ + num_elements > capacity_)
    Grow(num_elements);
  std::copy(begin, end, ptr_ + size_);
  size_ += num_elements;
}

class ArgInserter;
}

/**
  \rst
  A string reference. It can be constructed from a C string, ``std::string``
  or as a result of a formatting operation. It is most useful as a parameter
  type to allow passing different types of strings in a function, for example::

    TempFormatter<> Format(StringRef format);

    Format("{}") << 42;
    Format(std::string("{}")) << 42;
    Format(Format("{{}}")) << 42;
  \endrst
*/
class StringRef {
 private:
  const char *data_;
  mutable std::size_t size_;

 public:
  StringRef(const char *s, std::size_t size = 0) : data_(s), size_(size) {}
  StringRef(const std::string &s) : data_(s.c_str()), size_(s.size()) {}

  operator std::string() const { return std::string(data_, size()); }

  const char *c_str() const { return data_; }

  std::size_t size() const {
    if (size_ == 0) size_ = std::strlen(data_);
    return size_;
  }
};

class FormatError : public std::runtime_error {
 public:
  explicit FormatError(const std::string &message)
  : std::runtime_error(message) {}
};

enum Alignment {
  ALIGN_DEFAULT, ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTER, ALIGN_NUMERIC
};

struct FormatSpec {
  Alignment align;
  unsigned flags;
  unsigned width;
  char type;
  char fill;

  FormatSpec(unsigned width = 0, char type = 0, char fill = ' ')
  : align(ALIGN_DEFAULT), flags(0), width(width), type(type), fill(fill) {}
};

class BasicFormatter {
 protected:
  enum { INLINE_BUFFER_SIZE = 500 };
  mutable internal::Array<char, INLINE_BUFFER_SIZE> buffer_;  // Output buffer.

  // Grows the buffer by n characters and returns a pointer to the newly
  // allocated area.
  char *GrowBuffer(std::size_t n) {
    std::size_t size = buffer_.size();
    buffer_.resize(size + n);
    return &buffer_[size];
  }

  char *PrepareFilledBuffer(unsigned size, const FormatSpec &spec, char sign);

  // Formats an integer.
  template <typename T>
  void FormatInt(T value, const FormatSpec &spec);

  // Formats a floating point number (double or long double).
  template <typename T>
  void FormatDouble(T value, const FormatSpec &spec, int precision);

  char *FormatString(const char *s, std::size_t size, const FormatSpec &spec);

 public:
  /**
    \rst
    Returns the number of characters written to the output buffer.
    \endrst
   */
  std::size_t size() const { return buffer_.size(); }

  /**
    \rst
    Returns a pointer to the output buffer content. No terminating null
    character is appended.
    \endrst
   */
  const char *data() const { return &buffer_[0]; }

  /**
    \rst
    Returns a pointer to the output buffer content with terminating null
    character appended.
    \endrst
   */
  const char *c_str() const {
    std::size_t size = buffer_.size();
    buffer_.reserve(size + 1);
    buffer_[size] = '\0';
    return &buffer_[0];
  }

  /**
    \rst
    Returns the content of the output buffer as an ``std::string``.
    \endrst
   */
  std::string str() const { return std::string(&buffer_[0], buffer_.size()); }

  void operator<<(int value);

  void operator<<(char value) {
    *GrowBuffer(1) = value;
  }

  void operator<<(const char *value) {
    std::size_t size = std::strlen(value);
    std::strncpy(GrowBuffer(size), value, size);
  }

  BasicFormatter &Write(int value, const FormatSpec &spec) {
    FormatInt(value, spec);
    return *this;
  }

  void Clear() {
	  buffer_.clear();
  }
};

/**
  \rst
  The :cpp:class:`format::Formatter` class provides string formatting
  functionality similar to Python's `str.format
  <http://docs.python.org/3/library/stdtypes.html#str.format>`__.
  The output is stored in a memory buffer that grows dynamically.

  **Example**::

     Formatter out;
     out("Current point:\n");
     out("(-{:+f}, {:+f})") << 3.14 << -3.14;

  This will populate the buffer of the ``out`` object with the following
  output:

  .. code-block:: none

     Current point:
     (-3.140000, +3.140000)

  The buffer can be accessed using :meth:`data` or :meth:`c_str`.
  \endrst
 */
class Formatter : public BasicFormatter {
 private:
  enum Type {
    // Numeric types should go first.
    INT, UINT, LONG, ULONG, DOUBLE, LONG_DOUBLE,
    LAST_NUMERIC_TYPE = LONG_DOUBLE,
    CHAR, STRING, WSTRING, POINTER, CUSTOM
  };

  typedef void (Formatter::*FormatFunc)(
      const void *arg, const FormatSpec &spec);

  // A format argument.
  class Arg {
   private:
    // This method is private to disallow formatting of arbitrary pointers.
    // If you want to output a pointer cast it to const void*. Do not implement!
    template <typename T>
    Arg(const T *value);

    // This method is private to disallow formatting of arbitrary pointers.
    // If you want to output a pointer cast it to void*. Do not implement!
    template <typename T>
    Arg(T *value);

    // This method is private to disallow formatting of wide characters.
    // If you want to output a wide character cast it to integer type.
    // Do not implement!
    Arg(wchar_t value);

   public:
    Type type;
    union {
      int int_value;
      unsigned uint_value;
      double double_value;
      long long_value;
      unsigned long ulong_value;
      long double long_double_value;
      const void *pointer_value;
      struct {
        const char *value;
        std::size_t size;
      } string;
      struct {
        const void *value;
        FormatFunc format;
      } custom;
    };
    mutable Formatter *formatter;

    Arg(int value) : type(INT), int_value(value), formatter(0) {}
    Arg(unsigned value) : type(UINT), uint_value(value), formatter(0) {}
    Arg(long value) : type(LONG), long_value(value), formatter(0) {}
    Arg(unsigned long value) : type(ULONG), ulong_value(value), formatter(0) {}
    Arg(double value) : type(DOUBLE), double_value(value), formatter(0) {}
    Arg(long double value)
    : type(LONG_DOUBLE), long_double_value(value), formatter(0) {}
    Arg(char value) : type(CHAR), int_value(value), formatter(0) {}

    Arg(const char *value) : type(STRING), formatter(0) {
      string.value = value;
      string.size = 0;
    }

    Arg(char *value) : type(STRING), formatter(0) {
      string.value = value;
      string.size = 0;
    }

    Arg(const void *value)
    : type(POINTER), pointer_value(value), formatter(0) {}

    Arg(void *value) : type(POINTER), pointer_value(value), formatter(0) {}

    Arg(const std::string &value) : type(STRING), formatter(0) {
      string.value = value.c_str();
      string.size = value.size();
    }

    template <typename T>
    Arg(const T &value) : type(CUSTOM), formatter(0) {
      custom.value = &value;
      custom.format = &Formatter::FormatCustomArg<T>;
    }

    ~Arg() {
      // Format is called here to make sure that a referred object is
      // still alive, for example:
      //
      //   Print("{0}") << std::string("test");
      //
      // Here an Arg object refers to a temporary std::string which is
      // destroyed at the end of the statement. Since the string object is
      // constructed before the Arg object, it will be destroyed after,
      // so it will be alive in the Arg's destructor where Format is called.
      // Note that the string object will not necessarily be alive when
      // the destructor of ArgInserter is called.
      formatter->CompleteFormatting();
    }
  };

  enum { NUM_INLINE_ARGS = 10 };
  internal::Array<const Arg*, NUM_INLINE_ARGS> args_;  // Format arguments.

  const char *format_;  // Format string.
  int num_open_braces_;
  int next_arg_index_;

  friend class internal::ArgInserter;
  friend class ArgFormatter;

  void Add(const Arg &arg) {
    args_.push_back(&arg);
  }

  void ReportError(const char *s, StringRef message) const;

  // Formats an argument of a custom type, such as a user-defined class.
  template <typename T>
  void FormatCustomArg(const void *arg, const FormatSpec &spec);

  unsigned ParseUInt(const char *&s) const;

  // Parses argument index and returns an argument with this index.
  const Arg &ParseArgIndex(const char *&s);

  void CheckSign(const char *&s, const Arg &arg);

  void DoFormat();

  void CompleteFormatting() {
    if (!format_) return;
    DoFormat();
  }

 public:
  /**
    \rst
    Constructs a formatter with an empty output buffer.
    \endrst
   */
  Formatter() : format_(0) {}

  /**
    \rst
    Formats a string appending the output to the internal buffer.
    Arguments are accepted through the returned ``ArgInserter`` object
    using inserter operator ``<<``.
    \endrst
  */
  internal::ArgInserter operator()(StringRef format);
};

namespace internal {

// This is a transient object that normally exists only as a temporary
// returned by one of the formatting functions. It stores a reference
// to a formatter and provides operator<< that feeds arguments to the
// formatter.
class ArgInserter {
 private:
  mutable Formatter *formatter_;

  friend class format::Formatter;
  friend class format::StringRef;

  // Do not implement.
  void operator=(const ArgInserter& other);

 protected:
  explicit ArgInserter(Formatter *f = 0) : formatter_(f) {}

  void Init(Formatter &f, const char *format) {
    const ArgInserter &other = f(format);
    formatter_ = other.formatter_;
    other.formatter_ = 0;
  }

  ArgInserter(const ArgInserter& other)
  : formatter_(other.formatter_) {
    other.formatter_ = 0;
  }

  const Formatter *Format() const {
    Formatter *f = formatter_;
    if (f) {
      formatter_ = 0;
      f->CompleteFormatting();
    }
    return f;
  }

  Formatter *formatter() const { return formatter_; }
  const char *format() const { return formatter_->format_; }

  void ResetFormatter() const { formatter_ = 0; }

  struct Proxy {
    Formatter *formatter;
    explicit Proxy(Formatter *f) : formatter(f) {}

    Formatter *Format() {
      formatter->CompleteFormatting();
      return formatter;
    }
  };

 public:
  ~ArgInserter() {
    if (formatter_)
      formatter_->CompleteFormatting();
  }

  // Feeds an argument to a formatter.
  ArgInserter &operator<<(const Formatter::Arg &arg) {
    arg.formatter = formatter_;
    formatter_->Add(arg);
    return *this;
  }

  operator Proxy() {
    Formatter *f = formatter_;
    formatter_ = 0;
    return Proxy(f);
  }

  operator StringRef() {
    const Formatter *f = Format();
    return StringRef(f->c_str(), f->size());
  }

  // Performs formatting and returns a std::string with the output.
  friend std::string str(Proxy p) {
    return p.Format()->str();
  }

  // Performs formatting and returns a C string with the output.
  friend const char *c_str(Proxy p) {
    return p.Format()->c_str();
  }
};

std::string str(ArgInserter::Proxy p);
const char *c_str(ArgInserter::Proxy p);
}

using format::internal::str;
using format::internal::c_str;

// ArgFormatter provides access to the format buffer within custom
// Format functions. It is not desirable to pass Formatter to these
// functions because Formatter::operator() is not reentrant and
// therefore can't be used for argument formatting.
class ArgFormatter {
 private:
  Formatter &formatter_;

 public:
  explicit ArgFormatter(Formatter &f) : formatter_(f) {}

  void Write(const std::string &s, const FormatSpec &spec) {
    formatter_.FormatString(s.data(), s.size(), spec);
  }
};

// The default formatting function.
template <typename T>
void Format(ArgFormatter &af, const FormatSpec &spec, const T &value) {
  std::ostringstream os;
  os << value;
  af.Write(os.str(), spec);
}

template <typename T>
void Formatter::FormatCustomArg(const void *arg, const FormatSpec &spec) {
  ArgFormatter af(*this);
  Format(af, spec, *static_cast<const T*>(arg));
}

inline internal::ArgInserter Formatter::operator()(StringRef format) {
  internal::ArgInserter formatter(this);
  format_ = format.c_str();
  args_.clear();
  return formatter;
}

// A formatting action that does nothing.
struct NoAction {
  void operator()(const Formatter &) const {}
};

/**
  \rst
  A formatter with an action performed when formatting is complete.
  Objects of this class normally exist only as temporaries returned
  by one of the formatting functions which explains the name.
  \endrst
 */
template <typename Action = NoAction>
class TempFormatter : public internal::ArgInserter {
 private:
  Formatter formatter_;
  Action action_;

  // Forbid copying other than from a temporary. Do not implement.
  TempFormatter(TempFormatter &);

  // Do not implement.
  TempFormatter& operator=(const TempFormatter &);

  struct Proxy {
    const char *format;
    Action action;

    Proxy(const char *fmt, Action a) : format(fmt), action(a) {}
  };

 public:
  // Creates an active formatter with a format string and an action.
  // Action should be an unary function object that takes a const
  // reference to Formatter as an argument. See Ignore and Write
  // for examples of action classes.
  explicit TempFormatter(StringRef format, Action a = Action())
  : action_(a) {
    Init(formatter_, format.c_str());
  }

  TempFormatter(const Proxy &p)
  : ArgInserter(0), action_(p.action) {
    Init(formatter_, p.format);
  }

  ~TempFormatter() {
    if (formatter())
      action_(*Format());
  }

  operator Proxy() {
    const char *fmt = format();
    ResetFormatter();
    return Proxy(fmt, action_);
  }
};

/**
  \rst
  Formats a string. Returns a temporary formatter object that accepts
  arguments via operator ``<<``. *format* is a format string that contains
  literal text and replacement fields surrounded by braces ``{}``.
  The formatter object replaces the fields with formatted arguments
  and stores the output in a memory buffer. The content of the buffer can
  be converted to ``std::string`` with :meth:`str` or accessed as a C string
  with :meth:`c_str`.

  **Example**::

    std::string message = str(Format("Elapsed time: {0:.2f} seconds") << 1.23);

  See also `Format String Syntax`_.
  \endrst
*/
inline TempFormatter<> Format(StringRef format) {
  return TempFormatter<>(format);
}

// A formatting action that writes formatted output to stdout.
struct Write {
  void operator()(const Formatter &f) const {
    std::fwrite(f.data(), 1, f.size(), stdout);
  }
};

// Formats a string and prints it to stdout.
// Example:
//   Print("Elapsed time: {0:.2f} seconds") << 1.23;
inline TempFormatter<Write> Print(StringRef format) {
  return TempFormatter<Write>(format);
}
}

namespace fmt = format;

#endif  // FORMAT_H_
