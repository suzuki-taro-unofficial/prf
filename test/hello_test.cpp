#include<iostream>
#include "hello.h"

bool hello_test(){
    std::cout << "Start hello test" << std::endl;
    return hello("john") == "Hello, john!";
}

int main(){
    if(not hello_test()){
        return 1;
    }
    return 0;
}
