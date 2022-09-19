#pragma once

#include <type_traits>
#include <cstdint>
#include <array>

namespace concat_array_internal
{
	template<typename T>
	struct sequence_size;

	template<size_t N>
	struct sequence_size<std::index_sequence<N>>
	{
		static constexpr size_t value = N + 1;
	};

	template<size_t Front, size_t ...Is>
	struct sequence_size<std::index_sequence<Front, Is...>>
	{
		static constexpr size_t value = sequence_size<std::index_sequence<Is...>>::value;
	};

	template<size_t ...Is>
	static constexpr size_t sequence_size_v = sequence_size<std::index_sequence<Is...>>::value;

	template<typename T>
	struct array_size;

	template<size_t N>
	struct array_size<uint8_t[N]>
	{
		static constexpr size_t value = N;
	};

	template<size_t N>
	struct  array_size<std::array<uint8_t, N>>
	{
		static constexpr size_t value = N;
	};

	template<typename T>
	static constexpr size_t array_size_v = array_size<T>::value;

	template<typename T1, typename T2, size_t ...I1, size_t ...I2>
	inline constexpr auto concat_arrays_impl(const T1& a, const T2& b, std::index_sequence<I1...>, std::index_sequence<I2...>)
	{
		return std::array<uint8_t, sequence_size_v<I1...> +sequence_size_v<I2...>>{a[I1]..., b[I2]...};
	}
}

template<typename T1, typename T2>
inline constexpr auto concat_arrays(const T1& a, const T2& b)
{
	using namespace concat_array_internal;
	return concat_arrays_impl(a, b, std::make_index_sequence<array_size_v<T1>>{}, std::make_index_sequence<array_size_v<T2>>{});
}

template<typename T1, typename T2, typename ...Ts>
inline constexpr auto concat_arrays(const T1& a, const T2& b, const Ts&... rem)
{
	return concat_arrays(a, concat_arrays(b, rem...));
}