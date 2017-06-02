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

	template <> struct Applier<Op::E>  { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a == b); } };
	template <> struct Applier<Op::NE> { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a != b); } };
	template <> struct Applier<Op::L>  { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a <  b); } };
	template <> struct Applier<Op::LE> { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a <= b); } };
	template <> struct Applier<Op::GE> { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a >= b); } };
	template <> struct Applier<Op::G>  { template <typename A, typename B> static bool apply(const A& a, const B& b) { return bool(a >  b); } };


	template <Op OP>
	struct Approximator { };

	template <> 
	struct Approximator<Op::E>
	{
		template <typename A, typename B>
		static bool apply(const A& a, const B& b)
		{
			const auto p = presicion();
			const auto da = double(a);
			const auto db = double(b);
			// If either argument is equal to zero, fall back to absolute presicion
			if (da == 0) return abs(db) < p;
			if (db == 0) return abs(da) < p;
			// Use relative presicion!
			return (da/db-1) < p && (db/da-1) < p;
		}
	};
	template <>
	struct Approximator<Op::NE>
	{
		template <typename A, typename B>
		static bool apply(const A& a, const B& b)
		{
			return !Approximator<Op::E>::apply(a, b);
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
		return out << print(result.value);
	}

	template <typename A, Op OP, typename B>
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
	template <typename A, Op OP, typename B>
	std::ostream& operator<<(std::ostream& out, const Results<A, OP, B>& result)
	{
		return out << 
			print(result.lhs) << ' ' << OP << ' ' << 
			print(result.rhs);
	}

	template <typename A, Op OP0, typename B, Op OP1, typename C>
	struct Resultss
	{
		static constexpr Op op0 = OP0;
		static constexpr Op op1 = OP1;

		A left;
		B center;
		C right;

		template <class U, class V, class W>
		Resultss(U&& a, V&& b, W&& c) : left(std::forward<U>(a)), center(std::forward<V>(b)), right(std::forward<W>(c)) { }

		explicit operator bool() const { return Applier<OP0>::apply(left, center) && Applier<OP1>::apply(center, right); }

		bool approximate() const { return Approximator<OP0>::apply(left, center) && Approximator<OP1>::apply(center, right); }
	};
	template <typename A, Op OP0, typename B, Op OP1, typename C>
	std::ostream& operator<<(std::ostream& out, const Resultss<A, OP0, B, OP1, C>& result)
	{
		return out << 
			print(result.left)   << ' ' << OP0 << ' ' << 
			print(result.center) << ' ' << OP1 << ' ' << 
			print(result.right);
	}

	template <typename T>
	Result<T> operator<<(Split&&, T&& value) { return { std::forward<T>(value) }; }

#pragma push_macro("DEFINE_OP")
#define DEFINE_OP(op, code) template <class A, class B>\
	auto operator##op(Result<A>&& lhs, B&& rhs)\
	{ return Results<A, code, B>{ std::forward<A>(lhs.value), std::forward<B>(rhs) }; }

	DEFINE_OP(== , Op::E);
	DEFINE_OP(!= , Op::NE);
	DEFINE_OP(<  , Op::L);
	DEFINE_OP(<= , Op::LE);
	DEFINE_OP(>= , Op::GE);
	DEFINE_OP(>  , Op::G);

#undef DEFINE_OP
#define DEFINE_OP(op, code) template <class A, Op OP0, class B, class C>\
	auto operator##op(Results<A, OP0, B>&& lhs, C&& rhs)\
	{ return Resultss<A, OP0, B, code, C>{ std::forward<A>(lhs.lhs), std::forward<B>(lhs.rhs), std::forward<C>(rhs) }; }

	DEFINE_OP(== , Op::E);
	DEFINE_OP(!= , Op::NE);
	DEFINE_OP(<  , Op::L);
	DEFINE_OP(<= , Op::LE);
	DEFINE_OP(>= , Op::GE);
	DEFINE_OP(>  , Op::G);

#pragma pop_macro("DEFINE_OP")

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

		template <class A>
		void operator()(const A& asserter) const { _proc(asserter); }
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
			test([](const auto&) {});
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
		check_noexcept(make_assertion(test.file, test.line, test.expr, [&test](const auto& asserter)
		{
			test([&test, &asserter](const auto& result)
			{
				asserter(result);
				Assertion::increaseCount();
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
			});
		}));
	}

	template <class Proc>
	void check_approx(AssertionOf<Proc>&& test)
	{
		check_noexcept(make_assertion(test.file, test.line, test.expr, [&test](const auto& asserter)
		{
			test([&test, &asserter](const auto& result)
			{
				asserter(result);
				Assertion::increaseCount();
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
			});
		}));
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
			template <class = std_begin_type<T>>
			static std::true_type test(T&&);
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

		template <template <Op> class Comparer, class Proc>
		void check_each(AssertionOf<Proc>&& test)
		{
			check_noexcept(make_assertion(test.file, test.line, test.expr, [&test](const auto& asserter)
			{
				test([&test,&asserter](const auto& result)
				{
					asserter(result);
					Assertion::increaseCount();
					Subreport subreport;

					auto report_once = once(report_failure);
					bool do_report = false;
					static constexpr bool a_iterable = details::is_iterable<decltype(result.lhs)>::value;
					static constexpr bool b_iterable = details::is_iterable<decltype(result.rhs)>::value;
					static_assert(a_iterable || b_iterable, "neither side is iterable");
					auto ita = ::tester::details::begin(result.lhs); auto enda = ::tester::details::end(result.lhs);
					auto itb = ::tester::details::begin(result.rhs); auto endb = ::tester::details::end(result.rhs);

					for (size_t i = 0; ita != enda && itb != endb; ++ita, ++itb, ++i)
					{
						if (!Comparer<result.op>::apply(*ita, *itb))
						{
							report_once(do_report);
							if (do_report)
							{
								subreport <<
									"at index " << i << ":\n"
									"    " << print(*ita) << ' ' << result.op << ' ' << print(*itb) << '\n';
							}
						}
					}
					const bool different_size = (a_iterable && b_iterable && (ita != enda || itb != endb));
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
				});
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
