// GCC with -O2 can segfault on this code! 
// But -fwrapv saves the day. 
// -ftrapv makes it fail safely at the point of overflow.
// -fsanitize=undefined reports error and prevents crashes
// Segfaults with GCC 9.30, 11.2.0, and 12.0.1 , x86-64 linux target
// Doesn't segfault with GCC 8.4.0
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>

const int array_size=200;

int a[array_size]={};

std::array<int, 200> a2;

// Using unsigned comparison won't save you if an enclosing function has sanitized an overflown int. Neither will using std::array::at
int internal_get(unsigned int index){
    // index<array_size gets unsafely optimized out
    if(index<array_size){
        // and so does the same check in std::array::at
        std::cout<<"Trying to read array at index="<<index<<std::endl;
        return a2.at(index);
    }else{
        return 0;
    }
}

int safe_get_from_array(int index){
    // Index>=0 gets optimized out here
    if(index>=0 && index<array_size){
        return internal_get(index);
    }else{
        return 0;
    }
}

void some_function_without_sanitization(int b){
    int c=b+1;
    std::cout<<"Going to get from array at "<<c<<std::endl;
    std::cout<<safe_get_from_array(c)<<std::endl;// Won't segfault
}

void some_function_with_sanitization(int b){
    // Sanitize b. Can't make it worse, right? Harmless to last-minute add a bunch of sanitization to some large codebase during SDL pre-release process, right?
    if(b<0)return;
    int c=b+1;
    std::cout<<"Going to get from array at "<<c<<std::endl;
    std::cout<<safe_get_from_array(c)<<std::endl;// Will segfault
}

void inadvertent_range_check(unsigned int b){    
    if(b>=(1UL<<31))throw std::invalid_argument("Negative numbers are not allowed");
    unsigned int v=b;
    std::cout<<"inadvertent_range_check: "<<v<<std::endl;
}

void inadvertent_range_check2(unsigned int b){    
    if(b>=(1UL<<31))throw std::invalid_argument("Unsigned number is too large");
    std::cout<<"inadvertent_range_check: "<<b<<std::endl;
}

void unobviously_sanitized(int b){
    // int mis-sanitization can also happen in an unobvious way, by calling a function that throws
    inadvertent_range_check2(b);
    int c=b+1;    
    std::cout<<"Going to get from array at "<<c<<std::endl;
    std::cout<<safe_get_from_array(c)<<std::endl;// Will segfault
}

// This curse works starting in GCC 12
inline void curse(int b){
    if(b<0){
        std::cout<<"magic1"<<std::endl;
        throw std::invalid_argument("Whatever");
    }    
    int b100=b+100;
    std::cout<<"b+100 is "<<b100<<std::endl;
    if(b100>110){
        std::cout<<"magic2"<<std::endl;
        throw std::invalid_argument("Whatever");
    }
}

inline int curse1(int b){
    if(b<0){
        std::cout<<"magic1"<<std::endl;
        throw std::invalid_argument("Whatever");
    }
    return b+12345;
}
inline void curse2(int b){
    int b100=b+100;
    std::cout<<"b+100 is "<<b100<<std::endl;
    if(b100>110){
        std::cout<<"magic2"<<std::endl;
        throw std::invalid_argument("Whatever");
    }
}

inline void part_curse(int b100){
    if(b100>110)throw std::invalid_argument("Whatever");
}

inline bool bad(int b){
    if(b<0)return true;
  
    int b100=b+100;
    std::cout<<"b+100 is "<<b100<<std::endl;
    if(b100>110)return true;
    
    return false;
}

void cursed_int_experiment(int b){
    std::cout<<"Cursing "<<b<<std::endl;
        
    // Adding this "sanitization" curses b
    if(b<0)throw std::invalid_argument("bad argument");// also work with return
    int b_plus_100=b+100;
    // Logging a message prevents it from eliminating addition.
    std::cout<<"b+100 is "<<b_plus_100<<std::endl;
    // if you change 101 to 100, this statement throws 
    if(b_plus_100>101)throw std::invalid_argument("b_plus_100>101");
    
    
    // Now, the undefined behavior of b+100 has "cursed" b , 
    // even though b itself is not product of undefined behavior
    // and we have not done anything dangerous with the result of b+100

    // The curse is that until the end of the try{} block, the compiler thinks b is either 0 or 1
    // but the actual value of b can be well outside that range

    // Curses put in separate functions don't work for some reason, but may start working in the future    
    //curse(b);
    //curse2(curse1(b));  

    std::cout<<safe_get_from_array(b)<<std::endl;// This segfaults
}

void cursed_int_experiment_modular(int b){
    std::cout<<"Cursing "<<b<<std::endl;
    curse(b);
    // This doesn't segfault until GCC 12.0.1
    std::cout<<safe_get_from_array(b)<<std::endl;
}

// same as cursed_int_experiment but without using exceptions
void cursed_int_experiment_returns(int b){
    std::cout<<"Cursing "<<b<<std::endl;
    if(b<0)return;
    int b_plus_100=b+100;    
    std::cout<<"b+100 is "<<b_plus_100<<std::endl;    
    if(b_plus_100>101)return;

    std::cout<<safe_get_from_array(b)<<std::endl;// This segfaults
}

int main(int argc, char** argv) {
    if(argc!=2){
        std::cout << "Usage: "<<argv[0]<<" 2147483647"<<std::endl;
        return 1;
    }    
    int b=0;
    try{
        b=std::stoll(argv[1]);//std::stoi(argv[1]);
    }catch(std::invalid_argument &ex){
        std::cout << "Usage: "<<argv[0]<<" 2147483647"<<std::endl;
        return 1;
    }
    for(int i=0;i<array_size;++i){
        a[i]=i;
    }
    try{
        cursed_int_experiment_modular(b);
        cursed_int_experiment_returns(b);
        cursed_int_experiment(b);        
        std::cout<<"Without sanitization:"<<std::endl;
        some_function_without_sanitization(b);
        std::cout<<"With sanitization:"<<std::endl;
        // Either of those crashes
        //some_function_with_sanitization(b);
        unobviously_sanitized(b);
        std::cout<<"Didn't crash!"<<std::endl;
    }catch(...){
        std::cout<<"Caught exception."<<std::endl;
    }
    return 0;
}
/*
gcc main.cpp -O2 -o OptimizationTest -lstdc++
./OptimizationTest 2147483647
Output:
Cursing 2147483647
b+100 is -2147483549
Trying to read array at index=2147483647
Aborted (core dumped)
*/

/* 
Surprising conclusion: improper input sanitization can result in 
removal of correctly implemented range checks inside inlined functions, 
including range checks inside std::array::at 
and cause surprising new security exploits.

Additionally, since clang does not do those optimizations, fuzzing with AFL may not catch those issues

What is particularly harmful about this compiler behavior? Isn't it just like any other undefined behavior issue?

If a malicious party submits something like this to an open source project
int some_function(int a)
{
- if(i>=0 && i<size)x=a[i];
+ x=a[i];
that patch is easy to reject or flag for review as potentially malicious.

However, something like

int some_function(int a)
{
+ if(...)return ERROR_INPUT_TOO_LARGE;
+ if(...)return ERROR_INPUT_TOO_SMALL;

is much more likely to be incorporated into the code base, if the consequences of erroneously reporting ERROR_INPUT_*; seem harmless enough.

With UB based optimizations, the flaws in the above code can result in removal of subsequent range checks.

Undefined behavior:
"If Builders Built Buildings the Way Programmers Wrote Programs, Then the First Woodpecker 
That Came Along Would Destroy Civilization"
*/


