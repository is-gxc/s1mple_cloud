#include <iostream>
#include "http.hpp"
#include "server.hpp"
using namespace std;

int main()
{
    Server srv;
    srv.Start(10001); 
    return 0;
}
