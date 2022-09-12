// If Builders Built Buildings the Way Programmers Wrote Programs, 
// Then the First Woodpecker That Came Along Would Destroy Civilization

// A curious signed overflow undefined behavior example by Dmytry Lavrov.

// GCC with -O2 can segfault on this code! 
// But -fwrapv saves the day. 
// -ftrapv makes it fail safely at the point of overflow.
// -fsanitize=undefined reports error and prevents crashes
// Segfaults with GCC 11.2.0, and 12.0.1 , x86-64 linux target
// Doesn't segfault with GCC 8.4.0 or GCC 9.3.0

#include <iostream>
#include <stdexcept>
#include <string>
#include <array>
#include <cstdint>
#include <limits>

#define my_int int
#define my_uint unsigned int
// Also fails with 64 bits
//#define my_int int64_t;
//#define my_uint uint64_t;

constexpr my_int array_size=200;
std::array<int, array_size> a={};
// Works the same with:
//int a[array_size]={};

int internal_get(my_uint index){    
    if(index<array_size){        
        std::cout<<"Trying to read array at index="<<index<<std::endl;
        return a.at(index);
        // Also works the same:
        // return a[index];
    }else{
        return 0;
    }
}

int safe_get_from_array(my_int index){    
    if(index>=0 && index<array_size){
        return internal_get(index);
    }else{
        return 0;
    }
}
// This function puts a "curse" on b , making range checks on b misbehave
// The curse only works starting from GCC 12
inline void curse(my_int b){
    if(b<0)throw std::invalid_argument("argument is too low");
    my_int b100=b+100;
    std::cout<<"b+100 is "<<b100<<std::endl;
    if(b100>101)throw std::invalid_argument("argument is too high");
}

// This simplified curse does not work yet, but may work in a future version of GCC
inline void curse2(my_int b){
    my_int b100=b+(std::numeric_limits<my_int>::max()-3);
    std::cout<<"overflown number is "<<b100<<std::endl;
}

void print_array_at(my_int b){
    std::cout<<safe_get_from_array(b)<<std::endl;
}

void crash_with_cursed_integer(my_int b){
    std::cout<<"Cursing "<<b<<" with undefined behavior:"<<std::endl;
    curse(b);
    std::cout<<"Using cursed number for array access:"<<std::endl;
    // This doesn't segfault until GCC 12.0.1
    std::cout<<safe_get_from_array(b)<<std::endl;// This segfaults
}

// A possible future curse
void crash_with_cursed_integer2(my_int b){
    std::cout<<"Cursing2 "<<b<<" with undefined behavior:"<<std::endl;
    curse2(b);
    std::cout<<"Using cursed number for array access:"<<std::endl;
    // This doesn't segfault until GCC 12.0.1
    if(b>0)std::cout<<safe_get_from_array(b)<<std::endl;// This segfaults
}

void crash_with_cursed_sanitization(my_int b){    
    // Maliciously sanitize b
    std::cout<<"Insanitizing "<<b<<":"<<std::endl;
    if(b<0)return;
    my_int b_plus_100=b+100; // One might think that this is slightly flawed but harmless.
    std::cout<<"b+100 is "<<b_plus_100<<std::endl;    
    if(b_plus_100>101)return;
    std::cout<<"Using cursed number for array access:"<<std::endl;
    // Read from array at b
    std::cout<<safe_get_from_array(b)<<std::endl;// This segfaults
}

int main(int argc, char** argv) {
    if(argc!=2){
        std::cout << "Usage: OptimizationTest "<<std::numeric_limits<my_int>::max()<<std::endl;
        return 1;
    }    
    my_int b=0;
    try{
        b=std::stoll(argv[1]);
    }catch(std::invalid_argument &ex){
        std::cout << "Usage: OptimizationTest "<<std::numeric_limits<my_int>::max()<<std::endl;
        return 1;
    }
    for(int i=0;i<array_size;++i){
        a[i]=i+1;
    }
    try{
        // Succeeds 
        print_array_at(b);
        crash_with_cursed_integer2(b);
        // Crashes in gcc12+
        crash_with_cursed_integer(b);
        // Crashes in gcc9+
        crash_with_cursed_sanitization(b);
        std::cout<<"Didn't crash! Use GCC 12 or later with -O2 or higher"<<std::endl;
    }catch(...){
        std::cout<<"Acceptable outcome, caught exception."<<std::endl;
    }
    return 0;
}
/*
Output in GCC 12.0.1 with -O2  -Wall -Wextra -Werror :
./cursed_integer 2147483647
0
Insanitizing 2147483647:
b+100 is -2147483549
Using cursed number for array access:
Trying to read array at index=2147483647
Aborted (core dumped)
*/

