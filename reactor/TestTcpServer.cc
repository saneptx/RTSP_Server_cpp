#include <iostream>
#include "TcpServer.h"



void test(){
    TcpServer server("192.168.111.128",8888,4,10);
    server.start();
}
int main(){
    test();
    return 0;   

}