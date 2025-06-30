#include <iostream>
#include "TcpServer.h"



void test(){
    std::cout << "Server starting..." << std::endl;
    TcpServer server("192.168.111.128",8888, 4, 10);//192.168.111.128 172.23.181.53
    server.start();
}
int main(){
    test();
    return 0;   

}