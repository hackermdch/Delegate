#include <iostream>
#include "delegate.h"
#include <string>

using namespace std;

MemPage* MemPage::CurrentPage = nullptr;

string fun1()
{
	return "来自全局函数1";
}

void fun2()
{
	cout << "来自全局函数2" << endl;
}

int fun3(int a, int b)
{
	return a + b;
}

class C1
{
public:
	string str;
	int integer = 0;

	static string fun1()
	{
		return "来自类函数1";
	}

	string fun2()
	{
		return str;
	}

	static void fun3()
	{
		cout << "来自类函数2" << endl;
	}

	void fun4()
	{
		cout << str << endl;
	}

	static int fun5(int a)
	{
		return a + 10;
	}

	int fun6(int a)
	{
		return a + integer;
	}
};

void Demo1()
{
	cout << "-----------演示1:无参有返回值-------------" << endl;
	C1 c1;
	c1.str = "来自成员函数1";
	Delegate d1(fun1);
	Delegate d2(C1::fun1);
	Delegate d3(function([]()->string {return "来自Lambda 1"; }));
	Delegate d4(c1, &C1::fun2);
	cout << d1() << endl;
	cout << d2() << endl;
	cout << d3() << endl;
	cout << d4() << endl;
}

void Demo2()
{
	cout << "-----------演示2:无参无返回值-------------" << endl;
	C1 c1;
	c1.str = "来自成员函数2";
	Delegate d1(fun2);
	Delegate d2(C1::fun3);
	Delegate d3(function([]()->void {cout << "来自Lambda 2" << endl; }));
	Delegate d4(c1, &C1::fun4);
	d1();
	d2();
	d3();
	d4();
}

void Demo3()
{
	cout << "-----------演示3:有参有返回值-------------" << endl;
	C1 c1;
	c1.integer = 1024;
	int a = 100;
	Delegate d1(fun3);
	Delegate d2(C1::fun5);
	Delegate d3(function([a](int b)->int { return a + b; }));
	Delegate d4(c1, &C1::fun6);
	cout << d1(1, 2) << endl;
	cout << d2(3) << endl;
	cout << d3(1) << endl;
	cout << d4(20) << endl;
}

int (*Func())() {
	return nullptr;
}

void Demo4()
{
#ifdef _WIN64
	cout << "-----------演示4:转换为函数指针-----------" << endl;
	C1 c1;
	c1.integer = 1024;
	int a = 100;
	Delegate d1(fun3);
	Delegate d2(C1::fun5);
	Delegate d3(function([a](int b)->int { return a + b; }));
	Delegate d4(c1, &C1::fun6);

	auto p1 = d1.GetFuncPtr();
	auto p2 = d2.GetFuncPtr();
	auto p3 = d3.GetFuncPtr();
	auto p4 = d4.GetFuncPtr();
	cout << p1(2, 5) << endl;
	cout << p2(10) << endl;
	cout << p3(33) << endl;
	cout << p4(67) << endl;
#endif
}

int main()
{
	Demo1();
	cout << endl;
	Demo2();
	cout << endl;
	Demo3();
	cout << endl;
	Demo4();
	cout << endl;
	return 0;
}
