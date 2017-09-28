#pragma once

#include <sstream>
#include <typeindex>
#include <functional>

namespace tester
{
	using Procedure = std::function<void()>;
	using Report = std::ostringstream;

	extern Report report;

	// Subreport contents are automatically added to the main report on destruction
	class Subreport : public Report
	{
	public:
		~Subreport();
	};

	template <class T>
	struct is_streamable
	{
		template <class U>
		static auto test(U* u) -> decltype(std::declval<std::ostream&>() << *u);
		static auto test(...) -> std::false_type;

		static const bool value = !std::is_same_v<std::false_type, decltype(test((T*)nullptr))>;
	};

	template <class T, bool = is_streamable<T>::value>
	struct EnsurePrintable
	{
		const T& value;

		EnsurePrintable(const T& value) : value(value) { }
	};
	template <class T>
	EnsurePrintable<T> print(const T& value) { return { value }; }
	inline const char* print(const type_info& type) { return type.name(); }
	inline const char* print(const std::type_index& type) { return type.name(); }

	template <class T>
	std::ostream& operator<<(std::ostream& out, EnsurePrintable<T, true>&& printable)
	{
		return out << printable.value;
	}
	template <class T>
	std::ostream& operator<<(std::ostream& out, EnsurePrintable<T, false>&&)
	{
		return out << '{' << typeid(T).name() << '}';
	}
	template <class A, class B>
	std::ostream& operator<<(std::ostream& out, EnsurePrintable<std::pair<A, B>, false>&& pair)
	{
		return out << '(' << print(pair.value.first) << ',' << ' ' << print(pair.value.second) << ')';
	}

	template <class T>
	class Parameter
	{
		using Setter = void(*)(T);
		using Getter = T(*)();

		Setter _set;
		Getter _get;
	public:
		Parameter() = delete;
		Parameter(Parameter&&) = delete;
		Parameter(const Parameter&) = delete;
		Parameter(Setter set, Getter get) : _set(set), _get(get) { }

		Parameter& operator=(Parameter&&) = delete;
		Parameter& operator=(const Parameter&) = delete;
		template <class A> void operator= (A value) { _set(std::move(value)); }
		template <class A> void operator()(A value) { _set(std::move(value)); }

		T operator()() const { return _get(); }
	};

	extern Parameter<double> presicion;

	static constexpr double default_float_presicion = 1e-6;
	static constexpr double default_double_presicion = 1e-12;

	extern Parameter<std::string> section;


	enum class Op { EQ, NE, SL, LE, SG, GE };

	template <Op OP>
	struct Applier { template <typename A, typename B> static bool apply(const A& a, const B& b); };

	template <> struct Applier<Op::EQ> { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a == b); } };
	template <> struct Applier<Op::NE> { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a != b); } };
	template <> struct Applier<Op::SL> { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a <  b); } };
	template <> struct Applier<Op::LE> { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a <= b); } };
	template <> struct Applier<Op::GE> { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a >= b); } };
	template <> struct Applier<Op::SG> { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a >  b); } };

	template <class T>
	struct Magnitude { double operator()(const T& x) const { return double(abs(x)); } };
	template <class T>
	double magnitude(const T& x) { return Magnitude<T>{}(x); }

	template <class A, class B>
	struct Difference { double operator()(const A& a, const B& b) const { return magnitude(a - b); } };
	template <class A, class B>
	double difference(const A& a, const B& b) { return Difference<A, B>{}(a, b); }

	template <Op OP>
	struct Approximator { };

	template <> 
	struct Approximator<Op::EQ>
	{
		template <typename A, typename B>
		static bool apply(const A& a, const B& b)
		{
			const auto p = presicion();
			const auto ma = magnitude(a);
			const auto mb = magnitude(b);
			const auto mm = std::sqrt(ma*mb); // geometric average
			// If either argument is equal to zero, fall back to absolute presicion
			if (mm == 0) return ma < p && mb < p;
			// Use relative presicion!
			return (difference(a, b) / mm) < p;
		}
	};
	template <>
	struct Approximator<Op::NE>
	{
		template <typename A, typename B>
		static bool apply(const A& a, const B& b)
		{
			return !Approximator<Op::EQ>::apply(a, b);
		}
	};

	std::ostream& operator<<(std::ostream& out, Op op);

	struct Split { };
	static constexpr Split split = {};


	namespace result
	{
		static constexpr Op EQ = Op::EQ;
		static constexpr Op NE = Op::NE;
		static constexpr Op SL = Op::SL;
		static constexpr Op LE = Op::LE;
		static constexpr Op GE = Op::GE;
		static constexpr Op SG = Op::SG;

		template <class T, Op OP>
		struct TypeOp { };

		template <class Last, class... Rest>
		struct Type
		{
			static_assert(sizeof...(Rest) == 0);

			Last last;

			Type(Last last) : last(last) { }

			explicit operator bool() const { return bool(last); }
			constexpr bool approximate() const { return true; }

			void print(std::ostream& out) const
			{
				out << tester::print(last);
			}
		};

		template <class Last, class Next, Op LO, class... Rest>
		struct Type<Last, TypeOp<Next, LO>, Rest...>
		{
			using RestType = Type<Next, Rest...>;
			RestType rest;
			Last last;

			Type(const RestType& rest, Last last) : rest(rest), last(last) { }

			explicit operator bool() const
			{ 
				const bool next_and_last = { Applier<LO>::apply(rest.last, last) };
				if constexpr (sizeof...(Rest) > 0)
					return bool(rest) && next_and_last;
				else
					return next_and_last;
			}
			bool approximate() const { return rest.approximate() && Approximator<LO>::apply(rest.last, last); }

			void print(std::ostream& out) const
			{
				rest.print(out);
				out << LO << tester::print(last);
			}

		};

		template <class Last, class... Rest>
		std::ostream& operator<<(std::ostream& out, const Type<Last, Rest...>& result)
		{
			result.print(out);
			return out;
		}

		template <class L, class N, class... R> Type<L, TypeOp<N, EQ>, R...> operator==(const Type<N, R...>& rest, L&& last) { return { rest, std::forward<L>(last) }; }
		template <class L, class N, class... R> Type<L, TypeOp<N, NE>, R...> operator!=(const Type<N, R...>& rest, L&& last) { return { rest, std::forward<L>(last) }; }
		template <class L, class N, class... R> Type<L, TypeOp<N, SL>, R...> operator< (const Type<N, R...>& rest, L&& last) { return { rest, std::forward<L>(last) }; }
		template <class L, class N, class... R> Type<L, TypeOp<N, LE>, R...> operator<=(const Type<N, R...>& rest, L&& last) { return { rest, std::forward<L>(last) }; }
		template <class L, class N, class... R> Type<L, TypeOp<N, GE>, R...> operator>=(const Type<N, R...>& rest, L&& last) { return { rest, std::forward<L>(last) }; }
		template <class L, class N, class... R> Type<L, TypeOp<N, SG>, R...> operator> (const Type<N, R...>& rest, L&& last) { return { rest, std::forward<L>(last) }; }
	}

	template <typename T>
	result::Type<T> operator<<(const Split&, T&& value) { return { std::forward<T>(value) }; }


	bool report_failure();



	class Assertion
	{
	public:
		const char* const file;
		unsigned    const line;
		const char* const expr;

		static void increaseCount();
	};

	std::ostream& operator<<(std::ostream& out, const Assertion& test);

	template <class Proc>
	void check_noexcept(const Assertion& info, const Proc& test)
	{
		Assertion::increaseCount();
		try
		{
			test();
		}
		catch (std::exception& e)
		{
			if (report_failure()) 
				Subreport{} <<
				info << "failed:\n" <<
				"    threw " << (typeid(e).name() + 6) << " with message:\n" <<
				"      " << e.what() << "\n";
		}
		catch (...)
		{
			if (report_failure())
				Subreport{} <<
				info << "failed:\n" <<
				"    threw unknown exception\n";
		}

	}

	template <class Result>
	void check(const Assertion& info, const Result& result)
	{
		Assertion::increaseCount();
		if (!result)
		{
			if (report_failure())
				Subreport{} <<
				info << "failed: expands to\n" <<
				"    " << result << "\n";
		}
	}

	template <class First, class Last, Op OP>
	void check_approx(const Assertion& info, const result::Type<Last, result::TypeOp<First, OP>>& result)
	{
		Assertion::increaseCount();
		if (!result.approximate())
		{
			if (report_failure())
			{
				Subreport{}
					<< info << "failed: expands to\n"
					<< "    " << result
					<< "  (difference: " << difference(result.rest.last, result.last) << ")\n";

			}
		}
	}

	namespace details
	{
		template <class T>
		using std_begin_type = decltype(std::begin(std::declval<T>()));
		template <class T>
		using std_end_type = decltype(std::end(std::declval<T>()));

		template <class T>
		struct is_iterable
		{
			template <class U, class = std_begin_type<U>>
			static std::true_type test(U&&);
			static std::false_type test(...);

			static constexpr bool value = decltype(test(std::declval<T>()))::value;
		};

		static_assert(is_iterable<std::string>::value);
		static_assert(!is_iterable<int>::value);

		template <class T, bool>
		struct anything_end
		{
			std_end_type<T> value;

			anything_end(T& c) : value(std::end(c)) { }
		};
		template <class T>
		struct anything_end<T, false>
		{
			anything_end(T&) { }
		};

		template <class T, bool>
		struct anything_iterator
		{
			std_begin_type<T> value;

			anything_iterator(T& c) : value(std::begin(c)) { }

			anything_iterator& operator++() { ++value; return *this; }

			auto& operator*() { return *value; }

			bool operator!=(const anything_end<T, true>& end) const { return value != end.value; }
		};
		template <class T>
		struct anything_iterator<T, false>
		{
			T& value;

			anything_iterator(T& v) : value(v) { }

			anything_iterator& operator++() { return *this; }

			T& operator*() { return value; }

			bool operator!=(const anything_end<T, false>&) const { return true; }
		};

		template <class T>
		anything_iterator<T, is_iterable<T>::value> begin(T& x) { return { x }; }
		template <class T>
		anything_end<T, is_iterable<T>::value> end(T& x) { return { x }; }

		template <template <Op> class Comparer, class First, class Last, Op OP>
		void check_each(const Assertion& info, const result::Type<Last, result::TypeOp<First, OP>>& result)
		{
			using result_type = std::decay_t<decltype(result)>;
			Assertion::increaseCount();
			Subreport subreport;

			bool no_report = true;
			bool print_report = false;
			auto report_once = [&] { if (no_report) { print_report = report_failure(); no_report = false; } };
			static constexpr bool a_iterable = details::is_iterable<decltype(result.rest.last)>::value;
			static constexpr bool b_iterable = details::is_iterable<decltype(result.last)>::value;
			static_assert(a_iterable || b_iterable, "neither side is iterable");
			auto ita  = ::tester::details::begin(result.rest.last); 
			auto enda = ::tester::details::end  (result.rest.last);
			auto itb  = ::tester::details::begin(result.last); 
			auto endb = ::tester::details::end  (result.last);

			for (size_t i = 0; ita != enda && itb != endb; ++ita, ++itb, ++i)
			{
				if (!Comparer<OP>::apply(*ita, *itb))
				{
					report_once();
					if (print_report)
					{
						subreport <<
							"at index " << i << ":\n"
							"    " << print(*ita) << ' ' << OP << ' ' << print(*itb) << '\n';
					}
				}
			}
			const bool different_size = (a_iterable && b_iterable && (ita != enda || itb != endb));
			if (different_size)
				report_once();
			if (print_report)
			{
				auto pack_subreport = subreport.str();
				subreport.str("");
				if (different_size)
				{
					subreport <<
						info << "failed: size mismatch\n";
				}
				if (!pack_subreport.empty())
				{
					subreport <<
						info << "failed: element-by-element mismatch:\n" <<
						pack_subreport;
				}
			}
		}
	}

	template <class First, class Last, Op OP>
	void check_each(const Assertion& info, const result::Type<Last, result::TypeOp<First, OP>>& result)
	{
		details::check_each<Applier>(info, result);
	}
	template <class First, class Last, Op OP>
	void check_each_approx(const Assertion& info, const result::Type<Last, result::TypeOp<First, OP>>& test)
	{
		details::check_each<Approximator>(info, result);
	}

	class Case
	{
		const char* _name;
	public:

		Case(const char* name) : _name(name){ }

		Case operator<<(Procedure proc) && ;
	};

	class Subcase
	{
		const bool _shall_enter;
	public:
		Subcase(std::string_view name);
		~Subcase();

		void operator<<(const Procedure& procedure) const;
	};

	class Repeat
	{
		size_t _count;
	public:
		Repeat(size_t count) : _count(count) { }

		void operator<<(const Procedure& procedure) const;
	};


	struct TestResults
	{
		size_t subcase_count = 0;
		size_t assert_count = 0;
		size_t fail_count = 0;
		size_t exception_count = 0;
	};

	TestResults runTests();
};
