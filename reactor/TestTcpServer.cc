#include <iostream>
#include "TcpServer.h"



void test(){
    TcpServer server("172.23.181.53",8888,4,10);
    server.start();
}
int main(){
    test();
    return 0;   

}