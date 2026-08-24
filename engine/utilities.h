#ifndef CGE_UTILITIES_H
#define CGE_UTILITIES_H

#include <string_view>

// Lightweight compile-time replacement mechanisms for RTTI

namespace cge
{
	/**
	* Standard implementation of the FNV-1a hash algorithm.
	* 
	* @param str The input string to hash.
	* @return The computed hash value as a size_t.
	*/
	inline constexpr size_t fnv1aHash(std::string_view str)
	{
		// Seed for FNV-1a hash algorithm. The exact value is not critical, but it should be a large prime number to reduce the chance of collisions. This number is derived from the FNV-1a specification.
		constexpr size_t fnvOffsetBasis = 14695981039346656037ull;
		// Prime for FNV-1a hash algorithm, derived from the FNV-1a specification.
		constexpr size_t fnvPrime = 1099511628211ull;
		size_t hash = fnvOffsetBasis;
		for (char c : str)
		{
			hash ^= static_cast<size_t>(c);
			hash *= fnvPrime;
		}
		return hash;
	}
	/**
	* Resolves the type name of a given template type T to a string_view at compile time.
	* 
	* @tparam T The type for which to resolve the name.
	* @return A string_view representing the name of the type T.
	* @note This function uses compiler-specific macros to extract the type name. It supports Clang, GCC, and MSVC. If an unsupported compiler is used, a compilation error will be generated.
	*/
	template<typename T>
	inline constexpr std::string_view getTypeName()
	{
		// Use compiler-specific macros to get the type name
#if defined(__clang__) || defined(__GNUC__)
		std::string_view name = __PRETTY_FUNCTION__;
		size_t start = name.find("T = ") + 4;
		size_t end = name.find(']', start);
#elif defined(_MSC_VER)
		std::string_view name = __FUNCSIG__;
		size_t start = name.find("getTypeName<") + 12;
		size_t end = name.find('>', start);
#else
		#error "Unsupported compiler"
#endif
		return name.substr(start, end - start);
	}

	/**
	* Generates a unique type ID for the given template type T by hashing its name using the FNV-1a hash algorithm.
	* 
	* @tparam T The type for which to generate the unique ID.
	* @return A size_t representing the unique type ID.
	*/
	template <typename T>
	inline constexpr size_t getTypeId()	{ return fnv1aHash(getTypeName<T>()); }
}
#endif // CGE_UTILITIES_H