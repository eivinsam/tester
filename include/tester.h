#pragma once

#include <sstream>
#include <typeindex>

namespace tester
{
	extern std::ostringstream report;

	template <class T>
	struct is_streamable
	{
		template <class U>
		static auto test(U* u) -> decltype(std::declval<std::ostream&>() << *u);
		static auto test(...) -> std::false_type;

		static const bool value = !std::is_same_v<std::false_type, decltype(test((T*)nullptr))>;
	};

	template <class T>
	std::enable_if_t<is_streamable<T>::value, std::ostream&> print(std::ostream& out, const T& value) { return out << value; }
	template <class T>
	std::enable_if_t<!is_streamable<T>::value, std::ostream&> print(std::ostream& out, const T&) { return out << '{' << typeid(T).name() << '}'; }

	inline std::ostream& print(std::ostream& out, const      type_info&  type) { return out << type.name(); }
	inline std::ostream& print(std::ostream& out, const std::type_index& type) { return out << type.name(); }

	template <class A, class B>
	std::ostream& print(std::ostream& out, const std::pair<A, B>& pair) { return out << '(' << pair.first << ", " << pair.second << ')'; }


	enum class Op { E, NE, L, LE, G, GE };

	template <Op OP>
	struct Applier { };

	template <> struct Applier<Op::E>  { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a == b; } };
	template <> struct Applier<Op::NE> { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a != b; } };
	template <> struct Applier<Op::L>  { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a <  b; } };
	template <> struct Applier<Op::LE> { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a <= b; } };
	template <> struct Applier<Op::GE> { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a >= b; } };
	template <> struct Applier<Op::G>  { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a >  b; } };

	std::ostream& operator<<(std::ostream& out, Op op);

	struct Split { };

	template <typename T>
	struct Result
	{
		T value;

		explicit operator bool() const { return bool(value); }
	};

	template <typename T>
	std::ostream& operator<<(std::ostream& out, const Result<T>& result)
	{
		return print(out, result.value);
	}

	template <Op OP, typename A, typename B>
	struct Results
	{
		A lhs;
		B rhs;

		template <class U, class V>
		Results(U&& a, V&& b) : lhs(std::forward<U>(a)), rhs(std::forward<V>(b)) { }

		explicit operator bool() const { return Applier<OP>::apply(lhs, rhs); }
	};

	template <Op OP, typename A, typename B>
	std::ostream& operator<<(std::ostream& out, const Results<OP, A, B>& result)
	{
		return print(print(out, result.lhs) << ' ' << OP << ' ', result.rhs);
	}

	template <typename T>
	Result<T> operator<<(Split&&, T&& value) { return { std::forward<T>(value) }; }

	template <class A, class B> auto operator==(Result<A>&& lhsr, B&& rhs) { return Results<Op::E,  A, B>{ std::forward<A>(lhsr.value), std::forward<B>(rhs) }; }
	template <class A, class B> auto operator!=(Result<A>&& lhsr, B&& rhs) { return Results<Op::NE, A, B>{ std::forward<A>(lhsr.value), std::forward<B>(rhs) }; }
	template <class A, class B> auto operator< (Result<A>&& lhsr, B&& rhs) { return Results<Op::L,  A, B>{ std::forward<A>(lhsr.value), std::forward<B>(rhs) }; }
	template <class A, class B> auto operator<=(Result<A>&& lhsr, B&& rhs) { return Results<Op::LE, A, B>{ std::forward<A>(lhsr.value), std::forward<B>(rhs) }; }
	template <class A, class B> auto operator>=(Result<A>&& lhsr, B&& rhs) { return Results<Op::GE, A, B>{ std::forward<A>(lhsr.value), std::forward<B>(rhs) }; }
	template <class A, class B> auto operator> (Result<A>&& lhsr, B&& rhs) { return Results<Op::G,  A, B>{ std::forward<A>(lhsr.value), std::forward<B>(rhs) }; }


	class Assertion
	{
	public:
		const char* const file;
		unsigned    const line;
		const char* const expr;
	};
	std::ostream& operator<<(std::ostream& out, const Assertion& test);

	template <class Proc>
	class AssertionOf : public Assertion
	{
		Proc _proc;
	public:

		AssertionOf(const char* file, unsigned line, const char* expr, Proc&& procedure)
			: Assertion{ file, line, expr }, _proc(std::move(procedure)) { }

		auto operator()() const { return _proc(); }
	};

	template <class Proc>
	AssertionOf<Proc> make_assertion(const char* file, unsigned line, const char* expr, Proc&& proc) 
	{
		return { file, line, expr, std::forward<Proc>(proc) }; 
	}


	template <class Proc>
	void check(AssertionOf<Proc>&& test)
	{
		try 
		{
			auto result = test();
			if (result)
			{

			}
			else
			{
				report << 
					test << "failed: expands to\n" <<
					"    " << result << "\n\n";
			}
		}
		catch (std::exception& e)
		{
			report <<
				test << "failed:\n" <<
				"    threw " << (typeid(e).name()+6) << " with message:\n" <<
				"      " << e.what() << "\n\n";
		}
		catch (...)
		{
			report <<
				test << "failed:\n" <<
				"    threw unknown exception\n\n";
		}
	}
	template <class Proc>
	void check_each(AssertionOf<Proc>&& test)
	{
		try
		{
			std::ostringstream subreport;
			auto result = test();
			auto ita = std::begin(result.lhs); auto enda = std::end(result.lhs);
			auto itb = std::begin(result.rhs); auto endb = std::end(result.rhs);
			size_t i = 0;
			for (; ita != enda && itb != endb; ++ita, ++itb)
			{
				const auto ai = *ita;
				const auto bi = *itb;
				if (!apply(result.op, ai, bi))
				{
					subreport <<
						"at index " << i << ":\n"
						"    " << ai << " " << result.op << " " << bi << "\n";
				}
			}
			auto pack_subreport = subreport.str();
			if (!pack_subreport.empty())
			{
				report <<
					test << "failed: element-by-element mismatch:\n" <<
					pack_subreport << "\n";
			}
			else if (ita != enda || itb != endb)
			{
				report <<
					test << "failed: size mismatch\n\n";
			}
		}
		catch (std::exception& e)
		{
			report <<
				test << "failed:\n" <<
				"    threw " << (typeid(e).name() + 6) << " with message:\n" <<
				"      " << e.what() << "\n\n";
		}
		catch (...)
		{
			report <<
				test << "failed:\n" <<
				"    threw unknown exception\n\n";
		}
	}


	class Case
	{
		const char* _name;
	public:
		using Procedure = void(*)();

		Case(const char* name) : _name(name){ }

		Case operator<<(Procedure proc) && ;
	};

	class Subcase
	{
		const bool _shall_enter;
	public:
		Subcase(std::string_view name);
		~Subcase();

		explicit operator bool() { return _shall_enter; }
	};

	void runTests();
};

#define TESTER_PASTE_IMPL(a, b) a ## b
#define TESTER_PASTE(a, b) TESTER_PASTE_IMPL(a, b)

#define TESTER_ASSERTION(expr) ::tester::make_assertion(__FILE__, __LINE__, #expr, [&] { return ::tester::Split{} << expr; })
#define TESTER_CHECK(expr) ::tester::check(TESTER_ASSERTION(expr))
#define TESTER_CHECK_EACH(expr) ::tester::check_each(TESTER_ASSERTION(expr))
#define TESTER_TEST_CASE(name) static const auto TESTER_PASTE(_test_case_, __COUNTER__) = ::tester::Case(name) << []
#define TESTER_SUBCASE(name) if (auto TESTER_PASTE(_subcase_, __COUNTER__) = ::tester::Subcase(name)) 

#ifndef TESTER_NO_ALIAS
#define TEST_CASE(name) TESTER_TEST_CASE(name)
#define SUBCASE(name) TESTER_SUBCASE(name)
#define CHECK(expr) TESTER_CHECK(expr)
#define CHECK_EACH(expr) TESTER_CHECK_EACH(expr)
#endif
