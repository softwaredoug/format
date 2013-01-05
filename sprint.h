#ifndef SPRINT_20120105_H
#define SPRINT_20120105_H

#include <cstddef>
#include <stdint.h>
#include <limits>

namespace sprint {

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
#undef max
template <class T>
class AppendTransaction
{
public: 
	AppendTransaction() {}

	virtual ~AppendTransaction() {}

	enum {
		TRANSACTION_FAILED = 0xFFFFFFFF
	};
	virtual size_t AppendTo(T* dest, size_t destSize) = 0;
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
enum class HexCase : bool {
	lower = false,
	upper = true,
};

template <HexCase>
class Case {};

template <>
class Case<HexCase::upper> {
public:
	static char lookup[16];
};

typedef Case<HexCase::upper> UpperHex;

template <>
class Case<HexCase::lower> {
public:
	static char lookup[16];

};

typedef Case<HexCase::lower> LowerHex;

	
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

static_assert( Power<4>::mask == 0xf, "Power template not correctly calculating mask for hex");
static_assert( Power<3>::mask == 0x7, "Power template not correctly calculating mask for oct");
static_assert( Power<1>::mask == 0x1, "Power template not correctly calculating mask for bin");

// TODO:
//  (2) define for arbitrary integer size
//  (5) minimize potential code bloat
//  (6) test
template <typename PowerT, typename CaseT = LowerHex, typename PadT = NoPad>
class SpBin : public AppendTransaction<char>
{
private:
	uint32_t m_val;
public:
	SpBin(uint32_t val) : m_val(val)
	{
	}

	static inline std::size_t charLen(uint32_t val)
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
	size_t AppendTo(char* dest, size_t destLen)
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
template <typename PadT = NoPad> 
class asHexL : public SpBin< Power<4>, LowerHex, PadT> 
	{ 
	public:
		asHexL(uint32_t val) : SpBin<Power<4>, LowerHex, PadT>(val) {}
	};

// Uppercase Hex
template <typename PadT = NoPad> 
class asHexU : public SpBin< Power<4>, UpperHex, PadT> 
	{ 
	public:
		asHexU(uint32_t val) : SpBin<Power<4>, UpperHex, PadT>(val) {}
	};

// Octal Formatting
template <typename PadT = NoPad> 
class asOct : public SpBin< Power<3>, LowerHex, PadT> 
	{ 
	public:
		asOct(uint32_t val) : SpBin<Power<3>, LowerHex, PadT>(val) {}
	};


// Binary Formatting
template <typename PadT = NoPad> 
class asBin : public SpBin< Power<1>, LowerHex, PadT> 
	{ 
	public:
		asBin(uint32_t val) : SpBin<Power<1>, LowerHex, PadT>(val) {}
	};
}
#endif