#pragma once

#include <sstream>
#include <typeindex>

namespace tester
{
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

	template <class T>
	std::enable_if_t<is_streamable<T>::value, std::ostream&> print(std::ostream& out, const T& value) { return out << value; }
	template <class T>
	std::enable_if_t<!is_streamable<T>::value, std::ostream&> print(std::ostream& out, const T&) { return out << '{' << typeid(T).name() << '}'; }

	inline std::ostream& print(std::ostream& out, const      type_info&  type) { return out << type.name(); }
	inline std::ostream& print(std::ostream& out, const std::type_index& type) { return out << type.name(); }

	template <class A, class B>
	std::ostream& print(std::ostream& out, const std::pair<A, B>& pair) { return out << '(' << pair.first << ", " << pair.second << ')'; }


	template <class T>
	class Parameter
	{
		using Setter = void(*)(T);
		using Getter = T(*)();

		Setter _set;
		Getter _get;
	public:
		Parameter(Setter set, Getter get) : _set(set), _get(get) { }

		void operator= (T value) { _set(value); }
		void operator()(T value) { _set(value); }

		T operator()() const { return _get(); }
	};

	extern Parameter<double> presicion;

	static constexpr double default_float_presicion = 1e-6;
	static constexpr double default_double_presicion = 1e-12;

	extern Parameter<std::string> section;


	enum class Op { E, NE, L, LE, G, GE };

	template <Op OP>
	struct Applier { };

	template <> struct Applier<Op::E>  { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a == b; } };
	template <> struct Applier<Op::NE> { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a != b; } };
	template <> struct Applier<Op::L>  { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a <  b; } };
	template <> struct Applier<Op::LE> { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a <= b; } };
	template <> struct Applier<Op::GE> { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a >= b; } };
	template <> struct Applier<Op::G>  { template <typename A, typename B> static bool apply(A&& a, B&& b) { return a >  b; } };


	template <Op OP>
	struct Approximator { };

	template <> 
	struct Approximator<Op::E>
	{
		template <typename A, typename B>
		static bool apply(A&& a, B&& b)
		{
			const auto p = presicion();
			const auto da = double(a);
			const auto db = double(b);
			return (da/db-1) < p && (db/da-1) < p;
		}
	};
	template <>
	struct Approximator<Op::NE>
	{
		template <typename A, typename B>
		static bool apply(A&& a, B&& b)
		{
			return !Approximator<Op::E>::apply(std::forward<A>(a), std::forward<B>(b));
		}
	};

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
		static constexpr Op op = OP;

		A lhs;
		B rhs;

		template <class U, class V>
		Results(U&& a, V&& b) : lhs(std::forward<U>(a)), rhs(std::forward<V>(b)) { }

		explicit operator bool() const { return Applier<OP>::apply(lhs, rhs); }

		bool approximate() const { return Approximator<OP>::apply(lhs, rhs); }
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


	bool report_failure();

	template <class F>
	class Once
	{
		F    _func;
		bool _done;
	public:
		using result_t = std::result_of_t<F()>;

		Once(F&& f) : _func(f), _done(false) { }

		result_t operator()() { _done = true; return _func(); }

		void operator()(result_t& result) { if (!_done) result = (*this)(); }

		explicit operator bool() const { return !_done; }
	};

	template <class F>
	Once<F> once(F&& f) { return { std::forward<F>(f) }; }


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
	void check_noexcept(AssertionOf<Proc>&& test)
	{
		try
		{
			test();
		}
		catch (std::exception& e)
		{
			Assertion::increaseCount();
			if (report_failure()) 
				Subreport{} <<
				test << "failed:\n" <<
				"    threw " << (typeid(e).name() + 6) << " with message:\n" <<
				"      " << e.what() << "\n";
		}
		catch (...)
		{
			Assertion::increaseCount();
			if (report_failure())
				Subreport{} <<
				test << "failed:\n" <<
				"    threw unknown exception\n";
		}

	}

	template <class Proc>
	void check(AssertionOf<Proc>&& test)
	{
		check_noexcept(make_assertion(test.file, test.line, test.expr, [&test]
		{
			Assertion::increaseCount();
			auto result = test();
			if (result)
			{

			}
			else
			{
				if (report_failure())
					Subreport{} <<
					test << "failed: expands to\n" <<
					"    " << result << "\n";
			}
		}));
	}

	template <class Proc>
	void check_approx(AssertionOf<Proc>&& test)
	{
		check_noexcept(make_assertion(test.file, test.line, test.expr, [&test]
		{
			Assertion::increaseCount();
			auto result = test();
			if (result.approximate())
			{

			}
			else
			{
				if (report_failure())
					Subreport{} <<
					test << "failed: expands to\n" <<
					"    " << result << "\n";
			}
		}));
	}

	namespace details
	{
		template <template <Op> class Comparer, class Proc>
		void check_each(AssertionOf<Proc>&& test)
		{
			check_noexcept(make_assertion(test.file, test.line, test.expr, [&test]
			{
				Assertion::increaseCount();
				Subreport subreport;

				auto report_once = once(report_failure);
				bool do_report = false;
				auto result = test();
				auto ita = std::begin(result.lhs); auto enda = std::end(result.lhs);
				auto itb = std::begin(result.rhs); auto endb = std::end(result.rhs);

				for (size_t i = 0; ita != enda && itb != endb; ++ita, ++itb, ++i)
				{
					const auto ai = *ita;
					const auto bi = *itb;
					if (!Comparer<result.op>::apply(ai, bi))
					{
						report_once(do_report);
						if (do_report)
							subreport <<
							"at index " << i << ":\n"
							"    " << ai << " " << result.op << " " << bi << "\n";
					}
				}
				const bool different_size = (ita != enda || itb != endb);
				if (different_size)
					report_once(do_report);
				if (do_report)
				{
					auto pack_subreport = subreport.str();
					subreport.str("");
					if (different_size)
					{
						subreport <<
							test << "failed: size mismatch\n";
					}
					if (!pack_subreport.empty())
					{
						subreport <<
							test << "failed: element-by-element mismatch:\n" <<
							pack_subreport;
					}
				}
			}));
		}
	}

	template <class Proc>
	void check_each(AssertionOf<Proc>&& test)
	{
		details::check_each<Applier>(std::move(test));
	}
	template <class Proc>
	void check_each_approx(AssertionOf<Proc>&& test)
	{
		details::check_each<Approximator>(std::move(test));
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
		friend class Repeat;
		const bool _shall_enter;
	public:
		Subcase(std::string_view name);
		~Subcase();

		explicit operator bool() { return _shall_enter; }
	};

	class Repeat
	{
		struct sentinel { };

		class iterator
		{
			size_t _i;
		public:
			iterator(size_t i) : _i(i) { }

			iterator& operator++() { ++_i; return *this; }

			size_t operator*() const { return _i; }

			bool operator!=(const iterator&) const;
		};

		Subcase _case;
		const size_t _count;
	public:
		Repeat(size_t count);

		size_t size() const { return _count; }

		iterator begin() const { return { 0 }; }
		iterator   end() const { return { _count }; }
	};

	void runTests();
};
