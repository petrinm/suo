#include <iostream>

#include <cppunit/TestFixture.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestCaller.h>
#include <cppunit/ui/text/TestRunner.h>

#include "suo.hpp"
#include "generators.hpp"


using namespace std;
using namespace suo;


SymbolGenerator test_counter()
{
	for (int i = 0; i < 10; i++)
		co_yield i;

	SymbolVector v;
	for (int i = 10; i < 20; i++)
		v.push_back(i);
	co_yield v;
}


class GeneratorTest: public CppUnit::TestFixture
{
public:


	void test_counter_sourcing_1() {
		SymbolVector symbols;
		symbols.reserve(30); // Oversized destination buffer

		SymbolGenerator symbol_gen = test_counter();
		CPPUNIT_ASSERT(symbol_gen.running() == true);
		symbol_gen.sourceSymbols(symbols);
		CPPUNIT_ASSERT(symbol_gen.running() == false);
		CPPUNIT_ASSERT(symbols.size() == 20);
		for (int i = 0; i < 20; i++)
			CPPUNIT_ASSERT(symbols[i] == i);

		symbols.clear();
		symbol_gen.sourceSymbols(symbols);
		CPPUNIT_ASSERT(symbols.size() == 0);
	}



	void test_counter_sourcing_2() {
		SymbolVector symbols;
		symbols.reserve(20); // Just enough

		SymbolGenerator symbol_gen = test_counter();
		CPPUNIT_ASSERT(symbol_gen.running() == true);
		symbol_gen.sourceSymbols(symbols);
		//CPPUNIT_ASSERT(symbol_gen.running() == false);
		CPPUNIT_ASSERT(symbols.size() == 20);
		for (int i = 0; i < 20; i++)
			CPPUNIT_ASSERT(symbols[i] == i);

		symbols.clear();
		symbol_gen.sourceSymbols(symbols);
		CPPUNIT_ASSERT(symbols.size() == 0);
		symbol_gen.sourceSymbols(symbols);
		CPPUNIT_ASSERT(symbols.size() == 0);
	}


	void test_counter_sourcing_3() {
		SymbolVector symbols;
		symbols.reserve(10); // Smaller than will be generated

		SymbolGenerator symbol_gen = test_counter();
		CPPUNIT_ASSERT(symbol_gen.running() == true);
		symbol_gen.sourceSymbols(symbols);
		CPPUNIT_ASSERT(symbol_gen.running() == true);
		CPPUNIT_ASSERT(symbols.size() == 10);
		for (int i = 0; i < 10; i++)
			CPPUNIT_ASSERT(symbols[i] == i);

		symbols.clear();
		symbol_gen.sourceSymbols(symbols);
		//CPPUNIT_ASSERT(symbol_gen.running() == false);
		CPPUNIT_ASSERT(symbols.size() == 10);
		for (int i = 0; i < 10; i++)
			CPPUNIT_ASSERT(symbols[i] == i + 10);

		
		symbols.clear();
		symbol_gen.sourceSymbols(symbols);
		CPPUNIT_ASSERT(symbols.size() == 0);
	}


	void test_counter_iterating() {
		SymbolGenerator symbol_gen = test_counter();
		CPPUNIT_ASSERT(symbol_gen.running() == true);
		int i = 0;
		for (Symbol s : symbol_gen) 
			CPPUNIT_ASSERT(s == i++);
		CPPUNIT_ASSERT(symbol_gen.running() == false);
	}


	static CppUnit::Test* suite()
	{
		CppUnit::TestSuite* suite = new CppUnit::TestSuite("GeneratorTest");
		suite->addTest(new CppUnit::TestCaller<GeneratorTest>("Counter Sourcing 1", &GeneratorTest::test_counter_sourcing_1));
		suite->addTest(new CppUnit::TestCaller<GeneratorTest>("Counter Sourcing 2", &GeneratorTest::test_counter_sourcing_2));
		suite->addTest(new CppUnit::TestCaller<GeneratorTest>("Counter Sourcing 3", &GeneratorTest::test_counter_sourcing_3));
		suite->addTest(new CppUnit::TestCaller<GeneratorTest>("Counter Iterating", &GeneratorTest::test_counter_iterating));
		return suite;
	}

};


#ifndef COMBINED_TEST
int main(int argc, char** argv)
{
	CppUnit::TextUi::TestRunner runner;
	runner.addTest(GeneratorTest::suite());
	runner.run();
	return 0;
}
#endif