#ifndef CGE_UTILITIES_H
#define CGE_UTILITIES_H

#include <string_view>
#include <concepts>

// Lightweight compile-time replacement mechanisms for RTTI

namespace cge
{
	/**
	* Standard implementation of the FNV-1a hash algorithm.
	* 
	* @param str The input string to hash.
	* @return The computed hash value as a size_t.
	*/
	constexpr size_t fnv1aHash(std::string_view str)
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
	constexpr std::string_view getTypeName()
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
	constexpr size_t getTypeId() { return fnv1aHash(getTypeName<T>()); }

	/**
	* UDL for generating a unique type ID from a string literal at compile time.
	* @usage This user-defined literal can be used to generate a unique type ID from a string literal. For example, "MyType"_hash will return the hash value of the string "MyType".
	*/
	constexpr size_t operator""_hash(const char* str, size_t len)
	{
		return fnv1aHash(std::string_view(str, len));
	}

	// Hard requirement: Compound Requirements in requires-expressions are BANNED.
	// NEVER use (Grammar: { expression } [noexcept] [-> type-constraint] ;)
	// Use only simple requirements, which are either:
	// - an expression (which must be valid and well-formed)
	// - a type (which must be valid and well-formed)
	/**
	* Concept to check if a type T has a getTypeId() method that returns a size_t.
	*/
	template<typename T>
	concept HasTypeId = requires(const T& t)
	{
		t.getTypeId();
		requires std::convertible_to<decltype(t.getTypeId()), size_t>;
	};
}
#endif // CGE_UTILITIES_H