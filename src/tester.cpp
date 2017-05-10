#include "tester.h"

#include <vector>
#include <optional>

#include "gsl.h"

namespace tester
{
	std::ostringstream report;


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
		struct SubcaseData
		{
			std::string name;
			std::optional<std::string> first_name;
			size_t child_count = 0;
			size_t child_index = 0;
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
	}

	bool shall_enter(std::string_view name)
	{
		auto& stack = subcase_stack();
		if (subcase_depth() + 1 == stack.size())
			stack.emplace_back();
		auto& parent = subcase();
		if (parent.first_name)
		{
			if (*parent.first_name == name)
				parent.child_count = 0;
			return parent.child_index == parent.child_count;
		}
		else
		{
			parent.first_name = name;
			return true;
		}
	}

	Subcase::Subcase(std::string_view name) 
		: _shall_enter(shall_enter(name))
	{
		if (_shall_enter)
		{
			subcase_depth() += 1;
			subcase().name = name;
			subcase().child_count = 0;
		}
	}
	Subcase::~Subcase()
	{
		if (_shall_enter)
			subcase_depth() -= 1;
		subcase().child_count += 1;
	}

	void runTests()
	{
		for (auto& test : cases())
		{
			Expects(subcase_stack().empty());
			subcase_stack().emplace_back();
			subcase().name = test.name;
			int i = 0;
			while (!subcase_stack().empty())
			{
				report << "!! pass " << i << "\n";
				subcase().child_count = 0;
				test.proc();
				for (auto& e : subcase_stack())
					report << e.name << "(" << e.first_name.value_or("") << "):";
				report << "\n";
				increase_subcase_index();
				for (auto& e : subcase_stack())
					report << e.name << "(" << e.first_name.value_or("") << "):";
				report << "\n";
				++i;
			}
		}
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
