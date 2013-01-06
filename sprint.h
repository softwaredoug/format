#ifndef SPRINT_20120105_H
#define SPRINT_20120105_H

#include <cstddef>
#include <stdint.h>
#include <limits>

namespace format { namespace sprint {

// Sprints will
// - have constructors that hold a copy of whats going to be appended
//   to the buffer. For complex types, a pointer or reference should be
//   passed in.
// - they will implement an size_t appendTo(char*, size_t) that will
//   basically do what sprintf would do
//   - return the number of bytes written 
//	 - return -1 on an error
//   - an expected error: the passed in size_t is insufficient or negative
//     that means the end-user did not give Ape a big enough buffer. Do
//     nothing and return -1
//
template <class T>
class AppendTransaction
{
public: 
	AppendTransaction() {}

	virtual ~AppendTransaction() {}

	enum {
		TRANSACTION_FAILED = 0xFFFFFFFF
	};
	virtual size_t AppendTo(T* dest, size_t destSize) const = 0;
};
   
// Configure pad 
template <unsigned int _minWidth, char _padChar>
class Pad {
public:
	static inline std::size_t charWidth(std::size_t charsNeeded)
	{
		return (charsNeeded > _minWidth) ? charsNeeded : _minWidth;
	}
	static inline void pad(char* currPos, std::size_t width, std::size_t charsNeeded)
	{
		// pad width - charsNeeded
		std::size_t remainingChars = width - charsNeeded;
		if (remainingChars > 0)
		{
			while (remainingChars > 0)
			{
				*currPos-- = _padChar;
				remainingChars--;
			}
		}
	}
};

// no padding, default
template <>
class Pad<0, '\0'> {
public:
	static inline std::size_t charWidth(std::size_t widthNeeded) {return widthNeeded;}
	static inline void pad(char* , std::size_t , std::size_t )
	{

	}
};

typedef Pad<0, '\0'> NoPad;
		
// Configure the hex case
enum HexCase {
	lowercase = false,
	uppercase = true,
};

template <bool>
class Case {};

template <>
class Case<uppercase> {
public:
	static char lookup[16];
};

typedef Case<uppercase> UpperHex;

template <>
class Case<lowercase> {
public:
	static char lookup[16];

};

typedef Case<lowercase> LowerHex;

	
// 2^4 = hex, 2^3 = oct, etc
enum PowerConsts {
	bin = 1,
	oct = 3,
	hex = 4,
};

template <int _p>
class Power
{
public:
	enum 
	{
		pow = _p,
		mask = (1 << _p) - 1,
	};
};

// force compiler errors for incorrectly sized
// unsigend arguments
template <class unsignedT>
struct UnsignedProxy;

template<>
struct UnsignedProxy<uint32_t>
{

	uint32_t value;
	UnsignedProxy(uint32_t val) : value(val) {};
	UnsignedProxy(int32_t val) : value(val) {};
};

template<>
struct UnsignedProxy<uint64_t>
{
	uint64_t value;
	UnsignedProxy(uint64_t val) : value(val) {};
	UnsignedProxy(int64_t val) : value(val) {};

};

// TODO:
//  (2) define for arbitrary integer size
//  (5) minimize potential code bloat
//  (6) test
template <typename PowerT, typename CaseT = LowerHex, typename PadT = NoPad, typename unsignedT=uint32_t>
class SpBin : public AppendTransaction<char>
{
private:
	mutable unsignedT m_val;
public:
	SpBin(UnsignedProxy<unsignedT> val) : m_val(val.value)
	{
	}

	static inline std::size_t charLen(unsignedT val)
	{
		std::size_t rVal = 0;
		while (val)
		{
			++rVal;
			val = val >> PowerT::pow;
		}
		return rVal;
	}

	// Append to dest, returning num chars written
	size_t AppendTo(char* dest, size_t destLen) const
	{
		std::size_t charsNeeded = charLen(m_val);
		std::size_t width = PadT::charWidth(charsNeeded);
		if (width > destLen)
		{
			return 0;
		}
		char* cursor = dest + (width);
		*cursor-- = '\0';
		*cursor = '0';
		while (m_val)
		{
			*cursor-- = CaseT::lookup[m_val & PowerT::mask];
			m_val = m_val >> PowerT::pow;
		}
		PadT::pad(cursor, width, charsNeeded);
		return width;
	}
};


// default hex formatters

// Lowercase Hex
template <typename PadT = NoPad, typename unsignedT=uint32_t> 
class asHexL : public SpBin< Power<4>, LowerHex, PadT, unsignedT> 
	{ 
	public:
		asHexL(UnsignedProxy<unsignedT> val) : SpBin<Power<4>, LowerHex, PadT, unsignedT>(val) {}
	};

// Uppercase Hex
template <typename PadT = NoPad, typename unsignedT=uint32_t> 
class asHexU : public SpBin< Power<4>, UpperHex, PadT, unsignedT> 
	{ 
	public:
		asHexU(UnsignedProxy<unsignedT> val) : SpBin<Power<4>, UpperHex, PadT>(val) {}
	};

// Octal Formatting
template <typename PadT = NoPad, typename unsignedT=uint32_t> 
class asOct : public SpBin< Power<3>, LowerHex, PadT, unsignedT> 
	{ 
	public:
		asOct(UnsignedProxy<unsignedT> val) : SpBin<Power<3>, LowerHex, PadT>(val) {}
	};


// Binary Formatting
template <typename PadT = NoPad, typename unsignedT=uint32_t> 
class asBin : public SpBin< Power<1>, LowerHex, PadT, unsignedT> 
	{ 
	public:
		asBin(UnsignedProxy<unsignedT> val) : SpBin<Power<1>, LowerHex, PadT>(val) {}
	};
}}
#endif
