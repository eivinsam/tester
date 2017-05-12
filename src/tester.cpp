#include "tester.h"

#include <vector>
#include <optional>
#include <chrono>

#include "../base/gsl.h"

namespace tester
{
	Report report;


	auto& cases()
	{
		struct CaseData
		{
			const char* name;
			Case::Procedure proc;
		};
		static std::vector<CaseData> data; 
		return data;
	}
	auto& subcase_stack()
	{
		struct AssertData
		{
			std::string first_fail;
			size_t fail_count = 0;
		};
		struct SubcaseData
		{
			std::string name;
			size_t child_count = 0;
			size_t child_index = 0;
			size_t assert_count = 0;
			std::vector<AssertData> fails;

			void reset() { child_count = 0; assert_count = 0; }
		};
		static std::vector<SubcaseData> data;
		return data;
	}
	auto& subcase_depth()
	{
		static size_t depth;
		return depth;
	}
	//auto& parent_subcase() { return subcase_stack()[subcase_depth() - 1]; }
	auto& subcase()
	{
		auto& stack = subcase_stack();
		auto depth = subcase_depth();
		Expects(depth < stack.size());
		return stack[depth];
	}

	struct SubcaseInfo
	{
		std::string id;
		size_t assert_count = 0;
		size_t fail_count   = 0;
	};

	SubcaseInfo subcase_report()
	{
		SubcaseInfo result;
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
	void increase_subcase_index()
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

	bool shall_enter()
	{
		auto& stack = subcase_stack();
		if (subcase_depth() + 1 == stack.size())
			stack.emplace_back();
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
	Repeat::Repeat(size_t count) : 
		_case("repeat(" + std::to_string(count) + ")"), 
		_count(_case._shall_enter ? count : 0)
	{
	}
	void Assertion::increaseCount()
	{
		subcase().assert_count += 1;
	}

	bool Repeat::iterator::operator!=(const iterator& other) const
	{
		if (_i != other._i)
		{
			subcase().reset();
			return true;
		}
		return false;
	}

	void runTests()
	{
		using namespace std::chrono;
		auto then = high_resolution_clock::now();
		size_t subcase_count = 0;
		size_t assert_count = 0;
		size_t fail_count = 0;
		for (auto& test : cases())
		{
			Expects(subcase_stack().empty());
			subcase_stack().emplace_back();
			subcase().name = test.name;
			int i = 0;
			while (!subcase_stack().empty())
			{
				subcase_count += 1;
				subcase().reset();
				test.proc();

				auto info = subcase_report();
				if (info.fail_count > 0)
					report << "subcase " << info.id << " done\n"
					<< info.assert_count << " assertions\n"
					<< info.fail_count << " failures\n\n";
				assert_count += info.assert_count;
				fail_count += info.fail_count;


				increase_subcase_index();
				++i;
			}
		}
		auto dt = duration<double>(high_resolution_clock::now() - then);
		report << "tests done in " << dt.count() << "s\n"
			<< cases().size() << " cases\n" 
			<< subcase_count << " subcases\n"
			<< assert_count << " asserts\n"
			<< fail_count << " failures\n";
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
		int indent = 0;
		for (auto& subcase : subcase_stack())
		{
			for (int i = 0; i < indent; ++i)
				out << ' ';
			out << subcase.name << "\n";
			indent += 2;
		}
		return out <<
			test.file << "(" << test.line << ")\n" <<
			"    " << test.expr << "\n";
	}


	std::ostream& operator<<(std::ostream& out, Op op)
	{
		switch (op)
		{
		case Op::E:  return out << "==";
		case Op::NE: return out << "!=";
		case Op::L:  return out << "<";
		case Op::LE: return out << "<=";
		case Op::GE: return out << ">=";
		case Op::G:  return out << ">";
		default: return out << "!!";
		}
	}

	Case Case::operator<<(Procedure proc) &&
	{
		cases().push_back({ _name, proc });
		return *this;
	}
}
