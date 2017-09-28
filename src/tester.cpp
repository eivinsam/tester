#include "tester.h"

#include <vector>
#include <optional>
#include <chrono>

#include "../base/gsl.h"

namespace tester
{
	Report report;

	static double _presicion = default_double_presicion;

	struct CaseData
	{
		const char* name;
		Case::Procedure proc;
	};
	static auto& cases()
	{
		static std::vector<CaseData> data; 
		return data;
	}
	static auto& subcase_stack()
	{
		struct AssertData
		{
			std::string first_fail;
			size_t fail_count = 0;
		};
		struct SubcaseData
		{
			std::string name;
			std::string section;
			size_t child_count = 0;
			size_t child_index = 0;
			size_t assert_count = 0;
			double presicion = 0;
			std::vector<AssertData> fails;

			void reset() { child_count = 0; assert_count = 0; }
		};
		static std::vector<SubcaseData> data;
		return data;
	}
	static auto& subcase_depth()
	{
		static size_t depth;
		return depth;
	}
	//auto& parent_subcase() { return subcase_stack()[subcase_depth() - 1]; }
	static auto& subcase()
	{
		auto& stack = subcase_stack();
		auto depth = subcase_depth();
		Expects(depth < stack.size());
		return stack[depth];
	}

	decltype(presicion) presicion(
		[](double value)
	{ 
		if (subcase_stack().empty())
			_presicion = value;
		else
			subcase().presicion = value; 
	}, 
		[] 
	{ 
		return subcase_stack().empty() ?
			_presicion :
			subcase().presicion; 
	});

	decltype(section) section(
		[](std::string value)
	{
		auto& stack = subcase_stack();
		const auto depth = subcase_depth();
		if (depth < stack.size())
		{
			stack[depth].section = std::move(value);
			stack[depth].presicion = depth > 0 ? stack[depth - 1].presicion : _presicion;
		}
	},
		[]
	{
		return subcase_stack().empty() ? "" : subcase().section;
	});


	struct SubcaseInfo
	{
		std::string id;
		std::string exception;
		size_t assert_count = 0;
		size_t fail_count   = 0;
	};

	static SubcaseInfo runCase(const CaseData& test)
	{
		SubcaseInfo result;
		try { test.proc(); }
		catch (std::exception& e)
		{
			result.exception = typeid(e).name() + std::string(":\n(") + e.what() + ")";
		}
		catch (...)
		{
			result.exception = "unknown exception";
		}

		auto& stack = subcase_stack();

		for (size_t i = 0; i < stack.size(); ++i)
		{
			auto& level = stack[i];
			result.id += "/" + level.name;
			result.assert_count += level.assert_count;
			for (auto& fail : level.fails) if (fail.fail_count > 0)
			{
				result.fail_count += 1;

				report << fail.first_fail;
				if (fail.fail_count > 1)
				{
					report <<
						"  (first failure, failed " << fail.fail_count << " times)\n";
				}
				report << "\n";
			}
			level.assert_count = 0;
			level.fails.clear();
		}
		return result;
	}
	static void increase_subcase_index()
	{
		auto& stack = subcase_stack();
		while (!stack.empty())
		{
			auto& back = stack.back();
			back.child_index += 1;
			if (back.child_index < back.child_count)
				return;
		
			stack.pop_back();
		}
		return;
	}

	static bool shall_enter()
	{
		auto& stack = subcase_stack();
		if (subcase_depth() + 1 == stack.size())
		{
			const auto p = stack.back().presicion;
			stack.emplace_back();
			stack.back().presicion = p;
		}
		auto& parent = subcase();
		return parent.child_index == parent.child_count;
	}

	Subcase::Subcase(std::string_view name) 
		: _shall_enter(shall_enter())
	{
		if (_shall_enter)
		{
			subcase_depth() += 1;
			subcase().name = name;
			subcase().reset();
		}
	}
	Subcase::~Subcase()
	{
		if (_shall_enter)
			subcase_depth() -= 1;
		subcase().child_count += 1;
	}
	void Assertion::increaseCount()
	{
		subcase().assert_count += 1;
	}

	TestResults runTests()
	{
		using namespace std::chrono;
		auto then = high_resolution_clock::now();
		TestResults result;
		for (auto& test : cases())
		{
			Expects(subcase_stack().empty());
			subcase_stack().emplace_back();
			subcase().name = test.name;
			subcase().presicion = _presicion;
			int i = 0;
			while (!subcase_stack().empty())
			{
				result.subcase_count += 1;
				subcase().reset();

				auto info = runCase(test);

				if (info.exception.empty())
				{
					if (info.fail_count > 0)
						report << "subcase " << info.id << " done\n";
				}
				else
				{
					report << "subcase " << info.id << " threw " << info.exception << "\nafter ";
					result.uncaught_exceptions += 1;
				}
				if (info.fail_count > 0 || !info.exception.empty())
					report << info.fail_count << " failures / " << info.assert_count << " assertions\n\n";
				result.assert_count += info.assert_count;
				result.fail_count += info.fail_count;


				increase_subcase_index();
				++i;
			}
		}
		auto dt = duration<double>(high_resolution_clock::now() - then);
		report << "tests done in " << dt.count() << "s\n"
			<< cases().size() << " cases\n" 
			<< result.subcase_count << " subcases\n"
			<< result.assert_count << " asserts\n"
			<< result.fail_count << " failures\n"
			<< result.uncaught_exceptions << " uncaught exceptions\n";
		return result;
	}

	Subreport::~Subreport()
	{
		seekp(0, std::ios::end);
		auto size = size_t(tellp());
		if (size != 0)
			subcase().fails[subcase().assert_count - 1].first_fail = str();
	}

	bool report_failure()
	{
		auto& subc = subcase();
		if (subc.fails.size() < subc.assert_count)
			subc.fails.resize(subc.assert_count);
		auto& fail = subc.fails[subc.assert_count - 1];
		fail.fail_count += 1;
		return fail.fail_count == 1;
	}


	std::ostream& operator<<(std::ostream& out, const Assertion& test)
	{
		for (auto& subcase : subcase_stack())
		{
			out << '/' << subcase.name;
			if (!subcase.section.empty())
				out << ':' << subcase.section;
		}
		return out << '\n' <<
			test.file << '(' << test.line << ')' << '\n' <<
			"    " << test.expr << '\n';
	}


	std::ostream& operator<<(std::ostream& out, Op op)
	{
		switch (op)
		{
		case Op::EQ:  return out << "==";
		case Op::NE: return out << "!=";
		case Op::SL:  return out << "<";
		case Op::LE: return out << "<=";
		case Op::GE: return out << ">=";
		case Op::SG:  return out << ">";
		default: return out << "!!";
		}
	}

	Case Case::operator<<(Procedure proc) &&
	{
		cases().push_back({ _name, proc });
		return *this;
	}
	void Subcase::operator<<(const std::function<void()>& procedure) const
	{
		if (_shall_enter)
			procedure();
	}

	void Repeat::operator<<(const std::function<void()>& procedure) const
	{
		Subcase("repeat(" + std::to_string(_count) + ")") << [&]
		{
			for (size_t i = 0; i < _count; ++i)
			{
				subcase().reset();
				section = std::to_string(i);
				procedure();
			}
		};
	}
}
