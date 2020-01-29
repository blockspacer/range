
// think-cell public library
//
// Copyright (C) 2016-2020 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include "range_defines.h"
#include "range_fwd.h"
#include "accumulator.h"
#include "enum.h"
#include "utility.h"
#include "noncopyable.h"
#include "derivable.h"
#include "invoke.h"

#include <boost/mpl/has_xxx.hpp>
#include <boost/preprocessor.hpp>

#include <type_traits>
#include <functional>

namespace tc {

	DEFINE_ENUM(break_or_continue, BOOST_PP_EMPTY(), (break_)(continue_))

	[[nodiscard]] inline constexpr tc::break_or_continue continue_if(tc::bool_context bCondition) noexcept {
		if( bCondition ) {
			return tc::continue_;
		} else {
			return tc::break_;
		}
	}

	#define RETURN_IF_BREAK(...) \
	{ \
		auto breakorcontinue__ = (__VA_ARGS__); \
		/* Inline constexpr bools as soon as "constexpr if" works properly in MSVC */ \
		constexpr bool bAlwaysBreaks = std::is_same<decltype(breakorcontinue__), INTEGRAL_CONSTANT(tc::break_)>::value; \
		constexpr bool bNeverBreaks = std::is_same<decltype(breakorcontinue__), INTEGRAL_CONSTANT(tc::continue_)>::value; \
		if constexpr (bAlwaysBreaks) { \
			return INTEGRAL_CONSTANT(tc::break_)(); \
		} \
		else if constexpr (!bNeverBreaks) { \
			if( tc::break_ == breakorcontinue__ ) { \
				return tc::break_; \
			} \
		} \
	}

	//////////////////////////////////////////////////////////////////////////

	//// continue_if_not_break ///////////////////////////////////////////////////////////////////////////
	// Func returns break_or_continue

	template <typename Func, typename ...Args,
		typename R = decltype(tc::invoke(std::declval<Func>(), std::declval<Args>()...)),
		std::enable_if_t<
			tc::type::find_unique<tc::type::list<tc::break_or_continue, INTEGRAL_CONSTANT(tc::break_), INTEGRAL_CONSTANT(tc::continue_)>, tc::decay_t<R>>::found
		>* = nullptr
	>
	constexpr R continue_if_not_break(Func&& func, Args&& ... args) noexcept(noexcept(
		tc::invoke(std::forward<Func>(func), std::forward<Args>(args)...)
	)) {
		static_assert(tc::is_decayed<R>::value);
		return tc::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
	}

	// Func does not return break_or_continue
	template <typename Func, typename ...Args,
		typename R = decltype(tc::invoke(std::declval<Func>(), std::declval<Args>()...)),
		std::enable_if_t<
			!tc::type::find_unique<tc::type::list<tc::break_or_continue, INTEGRAL_CONSTANT(tc::break_), INTEGRAL_CONSTANT(tc::continue_)>, tc::decay_t<R>>::found
		>* = nullptr
	>
	constexpr INTEGRAL_CONSTANT(tc::continue_) continue_if_not_break(Func&& func, Args&& ... args) noexcept(noexcept(
		tc::invoke(std::forward<Func>(func), std::forward<Args>(args)...)
	)) {
		tc::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
		return {};
	}


	///////////////////////////////////////////////////
	// std::function should support tc::continue_;
	//	struct Fn {
	//		int operator()(int) const {
	//			return 3;
	//		}
	//	} fn;
	//	std::function< void(int) > erased=fn; // erased(x) runs fn(x), discards the return value and returns void
	//	tc::function{_view}< tc::break_or_continue(int) > erased2=fn; // erased2(x) runs fn(x), discards the return value and returns tc::continue_
	
	namespace no_adl {

		///////////////////////////////////////////////////
		// tc::function

		// tc::function is noncopyable, but implemented on top of std::function, which requires the functor to be copyable.
		template<typename Func>
		struct movable_functor_adaptor_base : tc::derivable_t<tc::decay_t<Func>> {
		private:
			using base_t = tc::derivable_t<tc::decay_t<Func>>;
		public:
			movable_functor_adaptor_base(Func&& func) noexcept : base_t(std::forward<Func>(func)) {}
			movable_functor_adaptor_base(movable_functor_adaptor_base&&) = default; // not noexcept to "inherit" exception-specifier from base class
			movable_functor_adaptor_base(movable_functor_adaptor_base const& mfa) noexcept
				: base_t(tc_move_always(tc::as_mutable(tc::base_cast<base_t>(mfa))))
			{
				// On the Mac, the std::function move ctor may actually copy our movable_functor_adaptor.
				// Since tc::function is noncopyable, we always move here.
			}
		};

		template<typename Ret, typename Func>
		struct movable_functor_adaptor final : movable_functor_adaptor_base<Func> {
			using movable_functor_adaptor_base<Func>::movable_functor_adaptor_base;

			template<typename... Args>
			Ret operator()(Args&& ... args) & MAYTHROW {
				return tc::base_cast<tc::decay_t<Func>>(*this)(std::forward<Args>(args)...);
			}

			template<typename... Args>
			Ret operator()(Args&& ... args) const& MAYTHROW {
				return tc::base_cast<tc::decay_t<Func>>(*this)(std::forward<Args>(args)...);
			}
		};

		template<typename Func>
		struct movable_functor_adaptor<tc::break_or_continue, Func> final : movable_functor_adaptor_base<Func> {
			using movable_functor_adaptor_base<Func>::movable_functor_adaptor_base;

			template<typename... Args>
			tc::break_or_continue operator()(Args&& ... args) & MAYTHROW {
				return tc::continue_if_not_break(tc::base_cast<tc::decay_t<Func>>(*this), std::forward<Args>(args)...);
			}

			template<typename... Args>
			tc::break_or_continue operator()(Args&& ... args) const& MAYTHROW {
				return tc::continue_if_not_break(tc::base_cast<tc::decay_t<Func>>(*this), std::forward<Args>(args)...);
			}
		};

		template< bool bNoExcept, typename Ret, typename ... Args >
		struct function_base: tc::noncopyable {
			function_base() noexcept {} // creates an empty function
			function_base(std::nullptr_t) noexcept {} // creates an empty function

			function_base(function_base&& func) noexcept : m_func(tc_move(func).m_func) {}
			function_base& operator=(function_base&& func) noexcept {
				m_func=tc_move(func).m_func;
				return *this;
			}

			template< typename Func >
			function_base(Func&& func, std::enable_if_t<!tc::is_base_of_decayed< function_base, Func >::value, unused_arg> =unused_arg()) MAYTHROW
				: m_func( tc::no_adl::movable_functor_adaptor<Ret, Func>( std::forward<Func>(func) ) )
			{
				static_assert(!tc::is_base_of_decayed< std::function< Ret(Args...) >, Func >::value);
				// Checking the noexcept value of the function call is commented out because
				// 1. std::ref(func)'s operator() is not noexcept
				// 2. tc::unordered_set's move ctor is not noexcept
				// 3. boost::bind overloaded operators are not noexcept
				//static_assert(!bNoExcept || noexcept(std::declval<tc::decay_t<Func>&>()(std::declval<Args>()...)));
			}

			explicit operator bool() const& noexcept { return tc::bool_cast(m_func); }

			Ret operator()(Args ... args) const& noexcept(bNoExcept) {
				return m_func(tc_move_if_owned(args)...);
			}

		private:
			std::function< Ret(Args...) > m_func;
		};

		template< typename Signature >
		struct function;
	
		template< typename Ret, typename ...Args >
		struct function< Ret(Args...) > : tc::no_adl::function_base</*bNoExcept*/false, Ret, Args...>
		{
			using base_ = tc::no_adl::function_base</*bNoExcept*/false, Ret, Args...>;
			using base_::base_;

			using base_::operator bool;
#ifndef _MSC_VER
			using base_::operator();
#else
			// Workaround C1001: An internal error has occurred in the compiler
			Ret operator()(Args... args) const& noexcept(false) {
				return base_::operator()(tc_move_if_owned(args)...);
			}
#endif
		};

		template< typename Ret, typename ...Args >
		struct function< Ret(Args...) noexcept > : tc::no_adl::function_base</*bNoExcept*/true, Ret, Args...>
		{
			using base_ = tc::no_adl::function_base</*bNoExcept*/true, Ret, Args...>;
			using base_::base_;

			using base_::operator bool;
#ifndef _MSC_VER
			using base_::operator();
#else
			// Workaround C1001: An internal error has occurred in the compiler
			Ret operator()(Args... args) const& noexcept {
				return base_::operator()(tc_move_if_owned(args)...);
			}
#endif
		};

		///////////////////////////////////////////////////
		// tc::function_ref

		namespace function_detail {
			union void_function_pointer {
				// Any pointer to function can be converted to a pointer to a different function type. Calling the
				// function through a pointer to a different function type is undefined, but converting such pointer
				// back to pointer to the original function type yields the pointer to the original function.
				// https://en.cppreference.com/w/cpp/language/reinterpret_cast

				template <typename Func, std::enable_if_t<std::is_class<Func>::value>* = nullptr>
				explicit void_function_pointer(Func const& func) noexcept
					: m_pvFunc(std::addressof(func))
				{}

				template <typename Func, std::enable_if_t<std::is_function<Func>::value>* = nullptr>
				explicit void_function_pointer(Func const& func) noexcept
					: m_fpvFunc(reinterpret_cast<void(*)()>(std::addressof(func)))
				{}

				template <typename Func> // may be const and/or volatile
				Func& get_func() const& noexcept {
					static_assert(!std::is_reference<Func>::value); 
					if constexpr (std::is_function<Func>::value) {
						return *reinterpret_cast<Func*>(m_fpvFunc);
					} else {
						static_assert(std::is_class<Func>::value);
						return *static_cast<Func*>(tc::make_mutable_ptr(m_pvFunc));
					}
				}

				void const* m_pvFunc;
				void (*m_fpvFunc)();
			};
		} // namespace function_detail

#ifndef _MSC_VER
		template <bool bNoExcept, typename Ret, typename ...Args>
		using type_erased_function_ptr = Ret(*)(tc::no_adl::function_detail::void_function_pointer, Args...) noexcept(bNoExcept);
#else
		// Workaround C1001: An internal error has occurred in the compiler
		template <bool bNoExcept, typename Ret, typename ...Args>
		struct get_type_erased_function_ptr;

		template <typename Ret, typename ...Args>
		struct get_type_erased_function_ptr</*bNoExcept*/true, Ret, Args...> {
			using type=Ret(*)(tc::no_adl::function_detail::void_function_pointer, Args...) noexcept;
		};

		template <typename Ret, typename ...Args>
		struct get_type_erased_function_ptr</*bNoExcept*/false, Ret, Args...> {
			using type=Ret(*)(tc::no_adl::function_detail::void_function_pointer, Args...) noexcept(false);
		};
		
		template <bool bNoExcept, typename Ret, typename ...Args>
		using type_erased_function_ptr = typename tc::no_adl::get_type_erased_function_ptr<bNoExcept, Ret, Args...>::type;
#endif

		template <bool bNoExcept, typename Func, typename Ret, typename ...Args>
		struct make_type_erased_function_ptr final {
			tc::no_adl::type_erased_function_ptr<bNoExcept, Ret, Args...> operator()() const& noexcept {
				return [](tc::no_adl::function_detail::void_function_pointer voidfunctionpointer, Args... args) noexcept(bNoExcept) -> Ret { // implicit cast of stateless lambda to function pointer
					return voidfunctionpointer.get_func<Func>()(std::forward<Args>(args)...); // MAYTHROW unless bNoExcept
				};
			}
		};

		template <bool bNoExcept, typename Func, typename ...Args>
		struct make_type_erased_function_ptr<bNoExcept, Func, void, Args...> final {
			tc::no_adl::type_erased_function_ptr<bNoExcept, void, Args...> operator()() const& noexcept {
				return [](tc::no_adl::function_detail::void_function_pointer voidfunctionpointer, Args... args) noexcept(bNoExcept) { // implicit cast of stateless lambda to function pointer
					voidfunctionpointer.get_func<Func>()(std::forward<Args>(args)...); // MAYTHROW unless bNoExcept
				};
			}
		};

		template <bool bNoExcept, typename Func, typename ...Args>
		struct make_type_erased_function_ptr<bNoExcept, Func, tc::break_or_continue, Args...> final {
			tc::no_adl::type_erased_function_ptr<bNoExcept, tc::break_or_continue, Args...> operator()() const& noexcept {
				return [](tc::no_adl::function_detail::void_function_pointer voidfunctionpointer, Args... args) noexcept(bNoExcept) -> tc::break_or_continue { // implicit cast of stateless lambda to function pointer
					return tc::continue_if_not_break(voidfunctionpointer.get_func<Func>(), std::forward<Args>(args)...); // MAYTHROW unless bNoExcept
				};
			}
		};

		template <bool bNoExcept, typename Ret, typename ...Args>
		struct function_ref_base {
			function_ref_base(function_ref_base const&) noexcept = default;

			Ret operator()(Args... args) const& noexcept(bNoExcept) {
				return m_pfuncTypeErased(m_voidfunctionpointer, std::forward<Args>(args)...); // MAYTHROW unless bNoExcept
			}
				
			template <typename Func, std::enable_if_t<
				!tc::is_base_of_decayed< function_ref_base, Func >::value
				&& std::is_invocable<std::remove_reference_t<Func>&, Args...>::value
				&& (
					std::is_convertible<decltype(std::declval<std::remove_reference_t<Func>&>()(std::declval<Args>()...)), Ret>::value
					|| std::is_same<Ret, tc::break_or_continue>::value
					|| std::is_same<Ret, void>::value
				)
			>* =nullptr>
			function_ref_base(Func&& func) noexcept
				: m_pfuncTypeErased(tc::no_adl::make_type_erased_function_ptr<bNoExcept, std::remove_reference_t<Func>, Ret, Args...>{}())
				, m_voidfunctionpointer(std::forward<Func>(func))
			{
				static_assert(!std::is_member_function_pointer<Func>::value, "Raw member functions are not supported (how would you call them?).  Pass in a lambda or use std::mem_fn/TC_MEM_FN instead.");
				static_assert(!std::is_pointer<Func>::value, "Pass in functions rather than function pointers.");
				// Checking the noexcept value of the function call is commented out because MAYTHROW is widely used in generic code such as for_each, range_adaptor...
				// static_assert(!bNoExcept || noexcept(std::declval<tc::decay_t<Func>&>()(std::declval<Args>()...)));
			}

		private:
			tc::no_adl::type_erased_function_ptr<bNoExcept, Ret, Args...> m_pfuncTypeErased;
			tc::no_adl::function_detail::void_function_pointer m_voidfunctionpointer;
		};

		template <typename Sig>
		struct function_ref;

		template <typename Ret, typename ...Args>
		struct function_ref<Ret(Args...)> : private tc::no_adl::function_ref_base</*bNoExcept*/false, Ret, Args...> {
			using base_ = tc::no_adl::function_ref_base</*bNoExcept*/false, Ret, Args...>;
			using base_::base_;
#ifndef _MSC_VER
			using base_::operator();
#else
			// Workaround C1001: An internal error has occurred in the compiler
			Ret operator()(Args... args) const& noexcept(false) {
				return base_::operator()(std::forward<Args>(args)...); // MAYTHROW
			}
#endif
		};

		template <typename Ret, typename ...Args>
		struct function_ref<Ret(Args...) noexcept> : private tc::no_adl::function_ref_base</*bNoExcept*/true, Ret, Args...> {
			using base_ = tc::no_adl::function_ref_base</*bNoExcept*/true, Ret, Args...>;
			using base_::base_;
#ifndef _MSC_VER
			using base_::operator();
#else
			// Workaround C1001: An internal error has occurred in the compiler
			Ret operator()(Args... args) const& noexcept {
				return base_::operator()(std::forward<Args>(args)...);
			}
#endif
		};
	} // no_adl

	using no_adl::function;
	using no_adl::function_ref;

	inline bool cyclic_improve_impl(int n, int& nSkipRule) noexcept {
		return false;
	}

	template<typename F0, typename... F>
	bool cyclic_improve_impl(int n, int& nSkipRule, F0& f0, F&... f) {
		if (n != nSkipRule && f0()) {
			nSkipRule = n;
			return true;
		} else {
			return cyclic_improve_impl(n+1, nSkipRule, f...);
		}
	}

	template< typename... F >
	bool cyclic_improve(F... f) noexcept {
		int nSkipRule=-1;
		while(cyclic_improve_impl(0, nSkipRule, f...)) {}
		return -1 != nSkipRule;
	}
}
