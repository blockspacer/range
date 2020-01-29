
// think-cell public library
//
// Copyright (C) 2016-2020 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once
#include "range_defines.h"
#include "const_forward.h"
#include "compare.h"
#include "implements_compare.h"
#include "storage_for.h"
#include "reference_or_value.h"
#include "type_traits.h"
#include "explicit_cast.h"
#include "tag_type.h"
#include "transform.h"
#include "equal.h"
#include "cont_assign.h"
#include "construction_restrictiveness.h"

#include <cstdint>
#include <boost/iterator/indirect_iterator.hpp>

namespace tc {
	DEFINE_TAG_TYPE(fill_tag)
	DEFINE_TAG_TYPE(func_tag)
	DEFINE_TAG_TYPE(range_tag)

	namespace no_adl {
		template< typename T >
		struct empty_array_storage {
			constexpr operator T*() { return nullptr; }
			constexpr operator T const*() const { return nullptr; }
		};

		template< typename T, std::size_t N >
		struct array_storage {
			using type = T[N];
		};

		template< typename T >
		struct array_storage<T, 0> {
			using type = empty_array_storage<T>;
		};
	}

	namespace array_adl {
		template< typename T, std::size_t N >
		struct array : tc::implements_compare_partial<array<T, N>> {
			static_assert(!std::is_const<T>::value);
			static_assert(!std::is_volatile<T>::value);
		private:
			typename no_adl::array_storage<T, N>::type m_a;

		public:
			using value_type = T;
			using reference = value_type &;
			using const_reference = value_type const&;
			using pointer = value_type *;
			using const_pointer = value_type const*;
			using iterator = pointer;
			using const_iterator = const_pointer;
			using reverse_iterator = std::reverse_iterator<iterator>;
			using const_reverse_iterator = std::reverse_iterator<const_iterator>;
			using size_type = std::size_t;
			using difference_type = std::ptrdiff_t;

			constexpr T* data() & noexcept {
				return m_a;
			}
			constexpr T const* data() const& noexcept {
				return m_a;
			}
			static constexpr std::size_t size() noexcept {
				return N;
			}

			// We cannot tell if *this is constructed using value-initialization syntax or default-initialization syntax. Therefore, we must value-initialize here.
			constexpr array() noexcept(std::is_nothrow_constructible<T>::value) : m_a() {}
			array(boost::container::default_init_t) noexcept {}

		private:
			template<typename Func, std::size_t ...IndexPack>
			constexpr array(func_tag_t, Func func, std::index_sequence<IndexPack...>) MAYTHROW
				: m_a{func(IndexPack)...}
			{}
		public:
			template<typename Func>
			constexpr array(func_tag_t, Func func) MAYTHROW
				: array(tc::func_tag, func, std::make_index_sequence<N>())
			{}

		private:
			template <std::size_t, typename... Args>
			static constexpr T explicit_cast_index(Args&&... args) noexcept {
				return tc::explicit_cast<T>( std::forward<Args>(args)... );
			}

			template<
				typename... Args,
				std::size_t ...IndexPack
#if defined(_MSC_FULL_VER) && 191326128<=_MSC_FULL_VER // Copy-elision is required to initialize the array from function return value
				, ENABLE_SFINAE,
				std::enable_if_t<
					std::is_move_constructible<SFINAE_TYPE(T)>::value
				>* = nullptr
#endif
			>
			constexpr explicit array(fill_tag_t, std::index_sequence<IndexPack...>, Args&&... args) MAYTHROW
				: m_a{
					explicit_cast_index<IndexPack>(tc::const_forward<Args>(args)...)...,
					tc::explicit_cast<T>(std::forward<Args>(args)...)
				}
			{
				STATICASSERTEQUAL(N, sizeof...(IndexPack)+1);
			}
		public:
			template<
				typename... Args
#if defined(_MSC_FULL_VER) && 191326128<=_MSC_FULL_VER
				, ENABLE_SFINAE,
				std::enable_if_t<
					std::is_move_constructible<SFINAE_TYPE(T)>::value
				>* = nullptr
#endif
			>
			constexpr explicit array(fill_tag_t, Args&&... args) MAYTHROW
				: array(fill_tag, std::make_index_sequence<N-1>(), std::forward<Args>(args)...)
			{}
#if defined(_MSC_FULL_VER) && 191326128<=_MSC_FULL_VER
		private:
			template <std::size_t, typename Arg>
			static constexpr Arg&& identity_index(Arg&& args) noexcept {
				return std::forward<Arg>(args);
			}

			template<
				typename Arg,
				typename... Args,
				std::size_t ...IndexPack,
				ENABLE_SFINAE,
				std::enable_if_t<
					!std::is_move_constructible<SFINAE_TYPE(T)>::value
				>* = nullptr
			>
			constexpr explicit array(fill_tag_t, std::index_sequence<IndexPack...>, Arg&& arg, Args&&... args) MAYTHROW
				: m_a{
					T(identity_index<IndexPack>(tc::const_forward<Arg>(arg)), const_forward<Args>(args)...)...,
					T(std::forward<Arg>(arg), std::forward<Args>(args)...)
				}
			{
				STATICASSERTEQUAL(N, sizeof...(IndexPack)+1);
			}
		public:
			template<
				typename... Params,
				std::enable_if_t<
					0 < sizeof...(Params)
					&& !std::is_move_constructible<T>::value
				>* = nullptr
			>
			constexpr explicit array(fill_tag_t, Params&&... params) MAYTHROW
				: array(fill_tag, std::make_index_sequence<N-1>(), std::forward<Params>(params)...)
			{}
#endif

			template <typename... Args,
				std::enable_if_t<
					tc::econstructionIMPLICIT==tc::elementwise_construction_restrictiveness<T, Args...>::value
				>* = nullptr
			>
			constexpr array(tc::aggregate_tag_t, Args&& ... args) noexcept(std::conjunction<std::is_nothrow_constructible<T, Args&&>...>::value)
				: m_a{static_cast<T>(std::forward<Args>(args))...}
			{
				STATICASSERTEQUAL(sizeof...(Args), N, "array initializer list does not match number of elements");
			}

			template <typename... Args,
				std::enable_if_t<
					tc::econstructionEXPLICIT==tc::elementwise_construction_restrictiveness<T, Args...>::value
				>* = nullptr
			>
			constexpr explicit array(tc::aggregate_tag_t, Args&& ... args) MAYTHROW
				: m_a{tc::explicit_cast<T>(std::forward<Args>(args))...}
			{
				STATICASSERTEQUAL(sizeof...(Args), N, "array initializer list does not match number of elements");
			}

		private:
			template<typename Iterator, std::size_t ...IndexPack>
			constexpr array(range_tag_t, Iterator it, Iterator itEnd, std::index_sequence<IndexPack...>) MAYTHROW
				: m_a{static_cast<T>((_ASSERTE(itEnd!=it), *it)), static_cast<T>((static_cast<void>(IndexPack), ++it, _ASSERTE(itEnd!=it), *it))...} // static_cast because int to double considered narrowing, forbidden in list initialization
			{
				STATICASSERTEQUAL(N, sizeof...(IndexPack)+1);
				_ASSERTE(itEnd==++it);
			}
		public:
			template< typename Rng,
				std::enable_if_t< 0!=N
					&& tc::econstructionIMPLICIT==tc::construction_restrictiveness<T, decltype(*tc::as_lvalue(tc::begin(std::declval<Rng&>())))>::value
				>* = nullptr
			>
			constexpr explicit array(Rng&& rng) MAYTHROW
				: array(range_tag, tc::begin(rng), tc::end(rng), std::make_index_sequence<N-1>())
			{}

			template< typename Rng,
				std::enable_if_t< 0 == N
					&& tc::econstructionIMPLICIT == tc::construction_restrictiveness<T, decltype(*tc::as_lvalue(tc::begin(std::declval<Rng&>())))>::value
				>* = nullptr
			>
			constexpr explicit array(Rng&& rng) MAYTHROW
				: m_a{}
			{
				_ASSERTE(tc::empty(rng));
			}

			template< typename Rng,
				std::enable_if_t<
					tc::econstructionEXPLICIT==tc::construction_restrictiveness<T, decltype(*tc::as_lvalue(tc::begin(std::declval<Rng&>())))>::value
				>* = nullptr 
			>
			explicit array(Rng&& rng) MAYTHROW
				: array(tc::transform( std::forward<Rng>(rng), tc::fn_explicit_cast<T>()))
			{}
			
			template <typename T2,
				std::enable_if_t<tc::is_safely_assignable<T&, T2 const&>::value>* =nullptr
			>
			array& operator=(array<T2, N> const& rhs) & MAYTHROW {
				for (std::size_t i = 0; i<N; ++i) {
					*(data() + i)=VERIFYINITIALIZED(rhs[i]);
				}
				return *this;
			}

			template <typename T2,
				std::enable_if_t<tc::is_safely_assignable<T&, T2&&>::value>* =nullptr
			>
			array& operator=(array<T2, N>&& rhs) & MAYTHROW {
				for (std::size_t i = 0; i<N; ++i) {
					// Use tc_move(rhs)[i] instead of tc_move(rhs[i]) here so we do not
					// need to specialize for the case that rhs is an array of reference.
					// Note that it is safe to call rhs::operator[]()&& on different indices.
					*(data() + i)=VERIFYINITIALIZED(tc_move(rhs)[i]);
				}
				return *this;
			}

			// iterators
			constexpr const_iterator begin() const& noexcept {
				return data();
			}
			constexpr const_iterator end() const& noexcept {
				return data() + N;
			}
			constexpr iterator begin() & noexcept {
				return data();
			}
			constexpr iterator end() & noexcept {
				return data() + N;
			}

			// access
			[[nodiscard]] constexpr T const& operator[](std::size_t i) const& noexcept {
				// Analysis of release mode assembly showed a significant overhead here:
				// Without _ASSERT, this function usually consist of only one or a few "lea" instructions that can easily be inlined.
				// With (non-debug) _ASSERT, the comparison, jump and call to ReportAssert dominate the run-time and prevent the function to get inlined.
				_ASSERTDEBUG(i<N);
				return *(data() + i);
			}
			[[nodiscard]] constexpr T& operator[](std::size_t i) & noexcept {
				_ASSERTDEBUG(i<N);
				return *(data() + i);
			}
			[[nodiscard]] constexpr T&& operator[](std::size_t i) && noexcept {
				_ASSERTDEBUG(i<N);
				return std::forward<T>(*(data() + i)); // forward instead of tc_move does the right thing if T is a reference
			}
			[[nodiscard]] constexpr T const&& operator[](std::size_t i) const&& noexcept {
				return static_cast<T const&&>((*this)[i]);
			}

			void fill(T const& t) & noexcept {
				std::fill_n(data(), N, t);
			}

			[[nodiscard]] friend tc::order compare(array const& lhs, array const& rhs) noexcept {
#ifdef _DEBUG
				for (std::size_t i=0; i<N; ++i) _ASSERTINITIALIZED(rhs[i]);
				for (std::size_t i=0; i<N; ++i) _ASSERTINITIALIZED(lhs[i]);
#endif
				return tc::lexicographical_compare_3way(lhs, rhs);
			}

			[[nodiscard]] friend bool operator==(array const& lhs, array const& rhs) noexcept {
#ifdef _DEBUG
				for (std::size_t i=0; i<N; ++i) _ASSERTINITIALIZED(rhs[i]);
				for (std::size_t i=0; i<N; ++i) _ASSERTINITIALIZED(lhs[i]);
#endif
				return tc::equal(lhs, rhs);
			}
		};

		template< typename T, std::size_t N >
		struct array<T&, N> : tc::implements_compare_partial<array<T&, N>> {
			static_assert( !std::is_reference<T>::value );
		private:
			typename no_adl::array_storage<T*, N>::type m_a;

		public:
			using value_type = tc::decay_t<T>;
			using reference = T&;
			using const_reference = T&; // reference semantics == no deep constness
			using pointer = T *;
			using iterator = boost::indirect_iterator<T* const*, value_type, /*CategoryOrTraversal*/boost::use_default, reference>;
			using const_iterator = iterator; // reference semantics == no deep constness
			using reverse_iterator = std::reverse_iterator<iterator>;
			using const_reverse_iterator = std::reverse_iterator<const_iterator>;
			using size_type = std::size_t;
			using difference_type = std::ptrdiff_t;

			static constexpr std::size_t size() noexcept {
				return N;
			}

		private:
			template<typename Func, std::size_t ...IndexPack>
			constexpr array(func_tag_t, Func func, std::index_sequence<IndexPack...>) MAYTHROW 
				: m_a{std::addressof(func(IndexPack))...} 
			{
				static_assert(tc::is_safely_constructible<T&, decltype(func(0))>::value);
			}
		public:
			template<typename Func>
			array(func_tag_t, Func func) MAYTHROW 
				: array(tc::func_tag, func, std::make_index_sequence<N>())
			{}

			// make sure forwarding ctor has at least two parameters, so no ambiguity with copy/move ctors
			template <typename... Args,
				std::enable_if_t<
					tc::econstructionIMPLICIT==tc::elementwise_construction_restrictiveness<T&, Args&...>::value
				>* =nullptr
			>
			constexpr array(tc::aggregate_tag_t, Args& ... args) MAYTHROW
				: m_a{std::addressof(args)...}
			{
				STATICASSERTEQUAL(sizeof...(Args), N, "array initializer list does not match number of elements");
			}

		private:
			DEFINE_NESTED_TAG_TYPE(range_tag)

			template<typename Iterator, std::size_t ...IndexPack>
			constexpr array(range_tag_t, Iterator it, Iterator itEnd, std::index_sequence<IndexPack...>) MAYTHROW
				: m_a{(_ASSERTE(itEnd!=it), std::addressof(*it)), (static_cast<void>(IndexPack), ++it, _ASSERTE(itEnd!=it), std::addressof(*it))...}
			{
				static_assert(tc::is_safely_constructible<T&, decltype(*it)>::value);
				STATICASSERTEQUAL(N, sizeof...(IndexPack)+1);
				_ASSERTE(itEnd==++it);
			}
		public:
			template< typename Rng,
				std::enable_if_t< 0!=N
					&& tc::econstructionIMPLICIT==tc::construction_restrictiveness<T&, decltype(tc_front(std::declval<Rng&>()))>::value
				>* = nullptr
			>
			constexpr explicit array(Rng&& rng) MAYTHROW
				: array(range_tag, tc::begin(rng), tc::end(rng), std::make_index_sequence<N-1>())
			{}

			array const& operator=(array const& rhs) const& noexcept(std::is_nothrow_copy_assignable<T>::value) {
				tc::cont_assign(*this,rhs);
				return *this;
			}

			template <typename T2,
				std::enable_if_t<tc::is_safely_assignable<T&, T2 const&>::value>* =nullptr
			>
			array const& operator=(array<T2, N> const& rhs) const& MAYTHROW {
				tc::cont_assign(*this,rhs);
				return *this;
			}

			template <typename T2,
				std::enable_if_t<tc::is_safely_assignable<T&, T2&&>::value>* =nullptr
			>
			array const& operator=(array<T2, N>&& rhs) const& MAYTHROW {
				for (std::size_t i = 0; i<N; ++i) {
					// Use tc_move(rhs)[i] instead of tc_move(rhs[i]) here so we do not
					// need to specialize for the case that rhs is an array of reference.
					// Note that it is safe to call rhs::operator[]()&& on different indices.
					m_a[i]=tc_move_always(rhs)[i];
				}
				return *this;
			}

			// iterators
			// reference semantics == no deep constness
			iterator begin() const& noexcept {
				return std::addressof(m_a[0]);
			}
			iterator end() const& noexcept {
				return std::addressof(m_a[0]) + N;
			}

			// access (no rvalue-qualified overloads, must not move data out of a reference)
			// reference semantics == no deep constness
			[[nodiscard]] T& operator[](std::size_t i) const& noexcept {
				_ASSERTDEBUG(i<N);
				return *m_a[i];
			}

			void fill(T const& t) const& noexcept {
				std::fill_n(begin(), N, t);
			}

			[[nodiscard]] friend tc::order compare(array const& lhs, array const& rhs) noexcept {
				return tc::lexicographical_compare_3way(lhs, rhs);
			}

			[[nodiscard]] friend bool operator==(array const& lhs, array const& rhs) noexcept {
				return tc::equal(lhs, rhs);
			}
		};

		static_assert(has_mem_fn_size<array<int, 10>>::value);
		static_assert(has_mem_fn_size<array<int&, 10>>::value);

		static_assert(!std::is_convertible<array<int,10>, array<int, 9>>::value);
		static_assert(!std::is_convertible<array<int,9>, array<int, 10>>::value);
	} // namespace array_adl
	using array_adl::array;

	namespace no_adl {
		template<typename T, std::size_t N>
		struct constexpr_size_base<tc::array<T,N>, void> : std::integral_constant<std::size_t, N> {};

		struct deduce_tag;

		template <typename T, typename...>
		struct delayed_deduce final {
			using type = T;
		};

		template <typename... Ts>
		struct delayed_deduce<deduce_tag, Ts...> final {
			using type = tc::common_type_t<Ts...>;
		};
	}

	template<std::size_t N, typename Rng>
	[[nodiscard]] constexpr auto make_array(Rng&& rng) noexcept {
		return tc::array<tc::decay_t<decltype(tc_front(rng))>, N>(std::forward<Rng>(rng));
	}

	template <typename Rng>
	[[nodiscard]] constexpr auto make_array(Rng&& rng) noexcept {
		return make_array<tc::constexpr_size<Rng>::value>(std::forward<Rng>(rng));
	}

	template <typename T = no_adl::deduce_tag, typename... Ts, std::enable_if_t<!std::is_reference<T>::value>* = nullptr>
	[[nodiscard]] constexpr auto make_array(tc::aggregate_tag_t, Ts&&... ts) noexcept {
		static_assert(!std::is_reference<typename no_adl::delayed_deduce<T, Ts...>::type>::value);
		return tc::array<typename no_adl::delayed_deduce<T, Ts...>::type, sizeof...(Ts)>(tc::aggregate_tag, std::forward<Ts>(ts)...);
	}

	// If T is a reference, force argument type T for all given arguments. That way, conversions
	// take place in the calling expression, and cases such as
	//
	//		tc::find_unique<tc::return_bool>(tc::make_array<Foo const&>(convertible_to_Foo), foo);
	//
	// will work as expected. With the usual variadic template + std::forward pattern, conversions
	// would take place inside the array constructor, resulting in a dangling reference.

	// Unfortunately, there seems to be no way to make this work in C++ without using macros
#define MAKE_ARRAY_LVALUE_REF(z, n, d) \
	template <typename T,std::enable_if_t<std::is_lvalue_reference<T>::value>* = nullptr> \
	[[nodiscard]] auto make_array(tc::aggregate_tag_t, BOOST_PP_ENUM_PARAMS(n, T t)) noexcept { \
		return tc::array<T, n>(tc::aggregate_tag, BOOST_PP_ENUM_PARAMS(n, t)); \
	}

	BOOST_PP_REPEAT_FROM_TO(1, 20, MAKE_ARRAY_LVALUE_REF, _)
#undef MAKE_ARRAY_LVALUE_REF

	template< typename T, std::enable_if_t<!std::is_reference<T>::value>* =nullptr >
	[[nodiscard]] constexpr auto single(T&& t) noexcept {
		// not tc::decay_t, we want to keep reference-like proxy objects as proxy objects
		// just like the reference overload tc::single preserves lvalue references.
		return tc::make_array<std::remove_cv_t<T> >(tc::aggregate_tag,std::forward<T>(t));
	}

	template <typename T, std::size_t N>
	struct decay<tc::array<T, N>> {
		using type = tc::array<tc::decay_t<T>, N>;
	};

	template <typename... Ts>
	[[nodiscard]] constexpr std::size_t count_args(Ts&&...) { return sizeof...(Ts); }

	template <typename... Ts>
	tc::common_type_t<Ts&&...> declval_common_type(Ts&&...) noexcept;
}

#define MAKE_TYPED_CONSTEXPR_ARRAY(type, ...) \
	[]() noexcept -> tc::array<type, tc::count_args(__VA_ARGS__)> const& { \
		static constexpr tc::array<type, tc::count_args(__VA_ARGS__)> c_at(tc::aggregate_tag, __VA_ARGS__); \
		return c_at; \
	}()

#define MAKE_CONSTEXPR_ARRAY(...) \
	MAKE_TYPED_CONSTEXPR_ARRAY(decltype(tc::declval_common_type(__VA_ARGS__)), __VA_ARGS__)
