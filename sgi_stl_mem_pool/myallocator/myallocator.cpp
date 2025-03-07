// myallocator.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <iostream>
#include "myallocator.h"
#include <vector>
#include <string>

using namespace std;

class Test
{
public:
    Test(std::string ss):s(ss)
    {
    }
    ~Test()
    {
        //std::cout << "~Test()" << std::endl;
    }
    std::string s;
};

int main()
{
        for (int i = 0; i < 10000; i++)
        {
            vector<Test, myallocator<Test>> v;
            for (int j = 0; j < 100; ++j)
            {
                v.push_back(std::to_string(j));
            }
        }

        for (int i = 0; i < 10000; i++)
        {
            vector <Test, std::allocator<Test>> v;
            for (int j = 0; j < 100; ++j)
            {
                v.push_back(std::to_string(j));
            }
        }
    //myallocator<Test>::output_pool();

    std::cout << "Hello World!\n";
}