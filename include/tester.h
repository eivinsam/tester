#pragma once

#include <sstream>

namespace tester
{
	extern std::ostringstream report;

	enum class Op { E, NE, L, LE, G, GE };

	template <class A, class B>
	bool apply(Op op, const A& a, const B& b) 
	{
		switch (op)
		{
		case Op::E:  return a == b;
		case Op::NE: return a != b;
		case Op::L:  return a <  b;
		case Op::LE: return a <= b;
		case Op::GE: return a >= b;
		case Op::G:  return a >  b;
		default: return false;
		}
	}

	std::ostream& operator<<(std::ostream& out, Op op);


	template <typename...>
	struct Result;

	template <>
	struct Result<> { };


	template <typename T>
	struct Result<T>
	{
		T value;

		explicit operator bool() const { return bool(value); }
	};
	template <typename T>
	std::ostream& operator<<(std::ostream& out, const Result<T>& result)
	{
		return out << result.value;
	}

	template <typename A, typename B>
	struct Result<A, B>
	{
		A lhs;
		B rhs;
		Op op;

		explicit operator bool() const { return apply(op, lhs, rhs); }
	};
	template <typename A, typename B>
	std::ostream& operator<<(std::ostream& out, const Result<A, B>& result)
	{
		return out << result.lhs << ' ' << result.op << ' ' << result.rhs;
	}

	template <typename T>
	Result<T> operator<<(Result<>&&, T&& value) { return { std::forward<T>(value) }; }

	template <class A, class B> Result<A, B> operator==(Result<A>&& lhsr, B&& rhs) { return { std::move(lhsr.value), std::forward<B>(rhs), Op::E }; }
	template <class A, class B> Result<A, B> operator!=(Result<A>&& lhsr, B&& rhs) { return { std::move(lhsr.value), std::forward<B>(rhs), Op::NE }; }
	template <class A, class B> Result<A, B> operator< (Result<A>&& lhsr, B&& rhs) { return { std::move(lhsr.value), std::forward<B>(rhs), Op::L }; }
	template <class A, class B> Result<A, B> operator<=(Result<A>&& lhsr, B&& rhs) { return { std::move(lhsr.value), std::forward<B>(rhs), Op::LE }; }
	template <class A, class B> Result<A, B> operator>=(Result<A>&& lhsr, B&& rhs) { return { std::move(lhsr.value), std::forward<B>(rhs), Op::GE }; }
	template <class A, class B> Result<A, B> operator> (Result<A>&& lhsr, B&& rhs) { return { std::move(lhsr.value), std::forward<B>(rhs), Op::G }; }


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

#define TESTER_ASSERTION(expr) ::tester::make_assertion(__FILE__, __LINE__, #expr, [&] { return ::tester::Result<>{} << expr; })
#define TESTER_CHECK(expr) ::tester::check(TESTER_ASSERTION(expr))
#define TESTER_CHECK_EACH(expr) ::tester::check_each(TESTER_ASSERTION(expr))
#define TESTER_TEST_CASE(name) static const auto Case = ::tester::Case(name) << []
#define TESTER_SUBCASE(name) if (auto TESTER_PASTE(_subcase_, __COUNTER__) = ::tester::Subcase(name)) 

#ifndef TESTER_NO_ALIAS
#define TEST_CASE(name) TESTER_TEST_CASE(name)
#define SUBCASE(name) TESTER_SUBCASE(name)
#define CHECK(expr) TESTER_CHECK(expr)
#define CHECK_EACH(expr) TESTER_CHECK_EACH(expr)
#endif
