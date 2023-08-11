#include <exception>
#include <cstdlib>
#include <iostream>


void may_throw()
{
    if ((rand() % 2) == 0)
        throw std::exception();
}


void catcher()
{
    static bool threw = false;
    try
    {
        may_throw();
    }
    catch(const std::exception& e)
    {
        threw = true;
        throw;
    }
    threw = false;
}

int main()
{
    try
    {
        catcher();
    }
    catch(const std::exception& e)
    {
        std::cerr << "threw" << std::endl;
    }
}