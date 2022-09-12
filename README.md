# Cursed integers

An example of how undefined behavior signed overflow can 'infect' a number and cause removal of common types of range checks (e.g. on array access), with significant security implications. 

It is pretty well known that signed overflow's undefined behavior can result in rather bizzare changes to what the code does
(such as making a finite loop infinite by pre-multiplying the iterand), but I believe this particular specific case
is most relevant to realistic security exploits.

Use GCC 12 with -O2 or -O3 flag to build the samples for maximum craziness. Run them with 2147483647 as the command line argument.

Why does this happen? Why won't numbers simply wrap around? Or maybe the hardware trigger a trap, like for division by zero?

This happens because C/C++ standard defines signed overflow as equivalently harmful to *a buffer overrun* (meanwhile, unsigned overflow is a wrap-around and completely fine).

Of course, the standard doesn't specify that the signed overflow *has* to be as harmful as a buffer overrun; but it does waggle its eyebrows suggestively and gesture furtively while mouthing 'give them hell' (to paraphrase from xkcd):

> C17 standard
>
> undefined behavior
>   behavior, upon use of a nonportable or erroneous program construct or of erroneous data, for which this document imposes no requirements
>
>    2 Note 1 to entry: Possible undefined behavior ranges from **ignoring the situation completely with unpredictable results**,
>    to behaving during translation or program execution in a documented manner characteristic of the environment (with or
>    without the issuance of a diagnostic message), to terminating a translation or execution (with the issuance of a diagnostic
>    message).
>
>    3 Note 2 to entry: J.2 gives an overview over properties of C programs that lead to undefined behavior.
>
>    4 EXAMPLE An example of undefined behavior is the **behavior on integer overflow**.

[source](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2310.pdf)

(emphasis mine). 
## If Builders Built Buildings the Way Programmers Wrote Programs, Then the First Woodpecker That Came Along Would Destroy Civilization

The basic principle at play here is as following:

* You drew extremely detailed plans for a skyscraper with a central load bearing column.
* In the corner of one room, a ceiling tile was accidentally left unsupported.
* A correct design can not result in tiles falling down.
* Therefore the builder can conclude that gravity is absent.
* Therefore the central load bearing column could be omitted.
* Oftentimes, nobody inspects the building and catches the problem, so it just falls down. 

This kind of thing is why people say things like "software engineering is not real engineering".

A huge practical problem with the above "cursed integers" example is that it makes much easier for dangerous code to slip in unnoticed.

Note also that a future version of GCC may allow a similar curse in a pure noexcept function, without any red flags like unchecked array access. Hell, for all I know GCC allows that already and I just hadn't found a way yet. I still have some things I haven't tried.
## What's an integer overflow, anyway?

Well, it's when you compute something and find out that the result wouldn't fit into the number's range.

Note how we defined the overflow as something which you will know during or after performing the calculation. The hardware (x86, x64, arm, you name it) follows that same pattern; an overflow flag that is set if an arithmetic operation had an overflow, and a 2-complement result. The unsigned code typically also follows that pattern: does the calculation, if wraparound is undesirable, checks the result.

Curiously enough, on unsigned numbers where the behavior is well defined by the standard (silent wrap around), the compiler is smart enough to replace common after-the-fact detection patterns (even as complex as ```x=a*b; if(b!=0 && x/b != a)```) with the CPU's overflow flag. The wraparound check is reduced to a single conditional jump with no additional calculations (the division is eliminated).

And yet, with the signed overflow being *undefined* by the standard, the standard effectively prohibits after-the-fact detection of the signed overflow in built-in arithmetics. Not even if the signed numbers were vetted to be non-negative. The code has to adopt an inverse approach - it has to prevent the overflow ahead of time, being careful that the calculations which predict the overflow do not themselves overflow.

By the standard, if you want to do a signed addition, you need to do a subtraction first to determine the range where addition is safe, then perform the addition. If you want to do a signed multiplication, you need to do a division first, which is a slower operation (and worse yet you have to branch four ways for operand sign cases). 

Murphy's law being in effect, the optimizer that was smart enough to wreck havoc upon the cursed_integer example, is nowhere near smart enough to replace overflow prevention with less costly overflow detection. I have examples below.

This makes signed overflow rather unique comparing to other UBs in the standard. Other UBs do not interfere with mitigation of the underlying logic bugs, but signed overflow being UB does.
## But someone said that undefined signed overflow allows compilers to optimize better!

This is rather hard to take seriously, with lack of any real world data and optimization examples like ```a+c<a``` which are also great examples of code you shouldn't write because signed overflow is undefined behavior. What's the point in better optimizing the code that you shouldn't be writing?

What's about the code that you should write instead?

Here is how you're supposed to multiply two numbers in pure C / C++ without making assumptions about available types:

[cite](https://wiki.sei.cmu.edu/confluence/display/c/INT32-C.+Ensure+that+operations+on+signed+integers+do+not+result+in+overflow) 

```
my_int func(my_int si_a, my_int si_b) {
  if (si_a > 0) {
    if (si_b > 0) {
      if (si_a > (LLONG_MAX / si_b))overflow();
    } else {
      if (si_b < (LLONG_MIN / si_a))overflow();
    }
  } else {
    if (si_b > 0) {
      if (si_a < (LLONG_MIN / si_b))overflow();
    } else {
      if ( (si_a != 0) && (si_b < (LLONG_MAX / si_a)))overflow();
    }
  }
  result = si_a * si_b;
```
(Condensed for brevity. Note that the alternative of casting to a larger type prior to multiply is not possible if you're already using the largest type)

[Full source on godbolt - see the disassembly](https://godbolt.org/#z:OYLghAFBqd5QCxAYwPYBMCmBRdBLAF1QCcAaPECAMzwBtMA7AQwFtMQByARg9KtQYEAysib0QXACx8BBAKoBnTAAUAHpwAMvAFYTStJg1DIApACYAQuYukl9ZATwDKjdAGFUtAK4sGIABykrgAyeAyYAHI%2BAEaYxCCSGqQADqgKhE4MHt6%2BASlpGQKh4VEssfGJtpj2jgJCBEzEBNk%2BfoF2mA6Z9Y0ExZExcQlJCg1NLbntY31hA2VDiQCUtqhexMjsHAD0WwDUACpMANaMu1TEqCy7CAQEyQogOwDueEd4AHRKH8gsXu%2BY6C8WzQDCo3kYGy2%2BAUyQMAE9gVsAJIRfYAZjMAFo3O9rNgGAo1phrAQEEwCNZUMk4uTMgpKQxrOlgOF0NYwgRMMA4vTLOhUNYGKgKZZiJhCbQRRYwpSAG5xMGoJ4mDQAQR2u0UAN2TAUOoYu0wqlYsMwu1QVF2aBYyTocTOTDoYWAuyIVoE8qa7tGtDh5s9iqeu2SYvlgkyuw5qF2KvVe2QCEwTGS/oVtCVuywnK6Al2EG00b22iFi1xapjavMaLCyG8WBjaLco3wqHeCBMaOwsarNbrZo7TYI6FoeGibY7XcrZmrDFrXnrA9GxGd4873envfn/cbI5YhAUq8nqvXaKwNHCuxYcIA%2BhzdumjPeBC6OSez7NLzevHevAS8CztQfF0gMjQRuzVK9b0EM5f2QCBILvdJryYUhPygghdiQ6JFhjAB2Kxy0jS0ICQpgG2wXYNBwkx8N2XYtgAKkwvBkMjPVUnSRx5V2BitljOi6LwYisPIyjqNo%2BimNI/V0GY69oh1MVgwKLizV4/iBME4SWLIic82CYIAHkIgAcWvABZVUAA16Lk7DxII1VNM0xjdgACUMYczTiC5iB4vjCOc4Nl0EKgIHMMxDIDdMgxMABWNxGTMMxFg7RygrosUCDWA0NDSjTNJogARAq8KKw1aCUPCLEkuSyI4jJ5VQkShQYBrVP80qtLzESB30ozTIslFbNIxYHK6gTXI8hgvMNYhfM6wLnJDDkwoiqK0wzeLEoi1K0XSjKspyyj8qWujitK4rauk9q8Cauzdla27uPUwiruqKqaJq1zpLwPUnpUu61ICpyBKEnqWIUvSqOq66dLYx6BGezBmshhHkcW0HNPBkj4b6iADOMszzOGvYsLG6qJroqbPPoOaFterHlpCgg1uSjbiEDGMEqSlLTqZzSjuIXL%2BaCi6lveyr%2B1on74c8h7GjNAHOKBzGgpxiHWLAMAO3KmHzAANkNzWocbfqiYs6yRp0sbxrOyamOm2afJINWMpW0LwvZ6Ktp53bRcOzBsuFk79om8Wmau1z8Vk8HpPlkTFcRtrAZekHztw8ro5moi6oR5XGuBjShYNaSpMh/mI9jO8WEdBgIDvRpgGQVCE0aBimKb2U7aZoSICb5Btd1tFFhL3YuADuiEOg1RdaXIhaFofviGAWV4osCe4qK1CGC8RfkgIMhx6oye0LvOE58Phel679ezHi7fHr32gD6PrgT7Dpbp4wgAvXWqFghAVQpA4R7RqgJUqHtWZezMH/NEJUzBxUXmyZKqEf5gNKmPPKn9jyZ1jBwZYtBOBxV4H4DgWhSCoE4G4aw1hMKrHWP2acPBSAEE0AQ5YRwQBogAJzvFwpILgGJ/AGzRBoMwXBcJxTRPoTgkhSHsMoZwXgDwkhsPIQQ0gcBYBIGtLaegZAKAQD0XaeIwAuCoJoJKHklBoiKOiGERocJOAsOtGwQQhkGC%2BkUVgWuRhxAaNIPgMUXR5QPECUaToXhOQuN4ByaoiiRzRGIE4jwWBFGHzwCwWJywwRMGAAoAAangTATxDLUjISw/gggRBiHYFIGQggtRqEUboLg%2BhDDGFoZYfQo4HiQGWFSWoBJOCYkMmiFR1ROjDJcDNCYfgzBJBCLMUo5QQASPyCrAQ8z1lJFukUFZgx4gbI6DmBgPRxieFaCAHhVQajdGmP0VZQwTnTB2bc0YvQnlHPWVwZYCgGEbD0IfTAmweCEOIQowJVCOCqBEZiA2khdjN2QOPMw7wzB5hoZYawqFcCEFdlWdpuwPA2lMTGZhixeDqK0GNUgXCzAG3eDwxBaI0T%2BA0GiRFkgeFcANrIjg8jSBkIoTClRIA1HsOWNoxAIBInIGiSQcglAm4KGUIYaoQgEBKkqbwExBhhnqvCLQLVOrFH6qGOY1BFriCGWiaap4IreDytVCvcJmIuC7ExCy6caJcKLK5f4OKkhcJSI5f4Z1qhOj1HwGQ3g1ThCiHEA0hNzT1CBLaR0owKBuk2CSf0iAgyD50lGeMlRgL6m2CHGEI1mrtWOtiawsUYLeBPBSckHJAqSHCsUTC7AUaFVED8nCg2CKkUorRRirFua8X4CHRStEfzqVSs4dwtE7w2Wbq3dugVQqnVKI4OKyVGi6VEI4GYKForlGsJXaQT06RnCSCAA%3D%3D)

Note a bunch of branches and division operations. The compiler, unsurprisingly, fails to optimise out the divisions, because they occur before the multiply. Division is much more expensive than multiply, but that's not even the biggest problem here. If the input is a stream of numbers of random signs, the branches are unpredictable and extremely expensive - even if no overflows actually occur. 

And here is how you check for wrap-around for unsigned numbers:
```
my_uint z=x*y;
if(x!=0 && z/x != y)overflow();
```
[godbolt link](https://godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:1,endLineNumber:19,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:19,startColumn:1,startLineNumber:1),source:'%23include+%3Cstdio.h%3E%0A%23include+%3Cstdlib.h%3E%0A%23include+%3Cstring.h%3E%0A%0A%23define+my_int+int%0A%23define+my_uint+unsigned+long+long%0A%0Aint+main(int+argc,+char**+argv)+%7B%0A++++if(argc!!%3D3)return+1%3B%0A++++my_uint+x%3Dstrtoll(argv%5B1%5D,+nullptr,+10)%3B%0A++++my_uint+y%3Dstrtoll(argv%5B2%5D,+nullptr,+10)%3B%0A++++my_uint+z%3Dx*y%3B%0A++++if(x!!%3D0+%26%26+z/x+!!%3D+y)%7B%0A++++++++//+cleaner+assembly+than+cout%0A++++++++printf(%22Overflow+%5Cn%22)%3B%0A++++%7D%0A++++return+0%3B%0A%7D%0A'),l:'5',n:'0',o:'C%2B%2B+source+%231',t:'0')),k:48.61056751467711,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:compiler,i:(compiler:g122,filters:(b:'0',binary:'1',commentOnly:'0',demangle:'0',directives:'0',execute:'1',intel:'0',libraryCode:'0',trim:'1'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-O3',selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1,tree:'1'),l:'5',n:'0',o:'x86-64+gcc+12.2+(C%2B%2B,+Editor+%231,+Compiler+%231)',t:'0')),k:51.38943248532289,l:'4',n:'0',o:'',s:0,t:'0')),l:'2',n:'0',o:'',t:'0')),version:4)

You check for it after the fact (which you can do because it's not UB).

A thing of beauty. The compiler optimizes out the division and uses the "jo" instruction (jump if overflow). For a non overflow triggering stream of numbers, all branches are predictable.

Ok, maybe for additions, sanitizing prior to addition could be optimized down to an overflow flag check?

[Nope, it couldn't](https://godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:1,endLineNumber:33,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:33,startColumn:1,startLineNumber:1),source:'%23include+%3Cstdio.h%3E%0A%23include+%3Cstdlib.h%3E%0A%23include+%3Cstring.h%3E%0A%23include+%3Climits.h%3E%0A%0A//+Allow+easy+experimentation+on+different+types%0A%23define+my_int+long+long+int%0A%23define+my_MAX+LLONG_MAX%0A%23define+my_MIN+LLONG_MIN%0A%0Astatic+my_int+safe_sum(my_int+si_a,+my_int+si_b)+%7B%0A++signed+int+sum%3B%0A++if+(((si_b+%3E+0)+%26%26+(si_a+%3E+(my_MAX+-+si_b)))+%7C%7C%0A++++++((si_b+%3C+0)+%26%26+(si_a+%3C+(my_MIN+-+si_b))))+%7B%0A++++/*+Handle+error+*/%0A++++printf(%22Overflow%5Cn%22)%3B%0A++++return+0%3B%0A++%7D+else+%7B%0A++++return+si_a+%2B+si_b%3B%0A++%7D%0A++/*+...+*/%0A%7D%0A%0Aint+main(int+argc,+char**+argv)+%7B%0A++++if(argc!!%3D3)return+1%3B%0A++++my_int+x%3Dstrtoll(argv%5B1%5D,+nullptr,+10)%3B%0A++++my_int+y%3Dstrtoll(argv%5B2%5D,+nullptr,+10)%3B%0A++++my_int+z%3B%0A++++z%3Dsafe_sum(x,y)%3B++++%0A++++printf(%22sum%3D%25lld%22,+z)%3B%0A++++return+0%3B%0A%7D%0A'),l:'5',n:'0',o:'C%2B%2B+source+%231',t:'0')),k:39.74132863021753,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:compiler,i:(compiler:g122,filters:(b:'0',binary:'1',commentOnly:'0',demangle:'0',directives:'0',execute:'1',intel:'0',libraryCode:'0',trim:'1'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-O3',selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1,tree:'1'),l:'5',n:'0',o:'x86-64+gcc+12.2+(C%2B%2B,+Editor+%231,+Compiler+%231)',t:'0')),k:26.92533803644916,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:executor,i:(argsPanelShown:'0',compilationPanelShown:'0',compiler:g122,compilerOutShown:'0',execArgs:'-1+-9223372036854775808',execStdin:'',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-O3',source:1,stdinPanelShown:'1',tree:'1',wrap:'1'),l:'5',n:'0',o:'Executor+x86-64+gcc+12.2+(C%2B%2B,+Editor+%231)',t:'0')),k:33.33333333333333,l:'4',n:'0',o:'',s:0,t:'0')),l:'2',n:'0',o:'',t:'0')),version:4)

And you absolutely should not be expecting to get better performance out of using signed numbers instead of unsigned.

If you switch from unsigned to signed, you will have to replace all your after-the-fact overflow checks with ahead-of-time overflow checks, that can be dramatically slower and which the compiler will not optimize well.

## What kind of code benefits from UB overflow derived optimization?

Not your Linux kernel or your web browser, or their components. They turned those optimizations off using -fwrapv or -fno-strict-overflow . (They're also adding other changes that make UB irrelevant. -fwrapv - type flags are a kludge and not reliable enough).

And not the code that uses unsigned numbers for sizes and indices, either (which includes much of C++ code where use of templates creates many optimization opportunities).

And not the code that's using non-portable compiler built-ins like '__builtin_add_overflow' to avoid UB. Those produce a well defined 2-complement result (and an overflow flag to go with it), no UB anywhere. Said built-ins may also undermine other optimizations.

And not the code which casts to a larger type and checks the upper half (the compiler easily proves that overflow can't occur, so there's no need for UB).

And not even code which checks ranges before doing arithmetics (ditto plus in many cases it is costly to determine ranges).

Basically, overflow being UB helps compilers optimize code that doesn't care about UB and upon encountering an unexpected input, just ends up where ever UB takes it. The code that you don't want to see in any of the software that you use everyday - web browsers, OS components, decoders, etc.

*That* code gets optimized, but also gets broken by the optimizations.

## Addressing common examples of undefined behavior "enabling your compiler to optimize better".

Cite: https://kristerw.blogspot.com/2016/02/how-undefined-signed-overflow-enables.html

```
(x * c) cmp 0   ->   x cmp 0 
(x * c1) / c2   ->   x * (c1 / c2)
(-x) / (-y)     ->   x / y
x + c < x       ->   false
```
et cetera.

Note that in production quality code, all of those examples would require some kind of value sanitization to avoid the risk of UB at runtime, *regardless of whether the result of the calculation is security relevant or not*. Even if the result is not security critical, even if any result is acceptable, even if the optimization may decrease the risk of actual overflow, the code still has to avoid undefined behavior because the optimizer can and will propagate undefined behavior upwards the calculation chain.

Without value sanitization, they're all "bad code" - especially according to people who think UB optimizations are a good idea.

The corresponding post-UB-pocalyptic examples would look something like
```
if( c > 0 && x <= std::numeric_limits<decltype(x)>::max()/c && x >= std::numeric_limits<decltype(x)>::min()/c ){
    if(x*c>0){....}
}
```
This admittedly looks quite contrived because a programmer would find it easier to avoid multiplication by c than to perform input sanitization for the multiplication but I'll roll with it.

Note that ```(x * c) cmp 0   ->   x cmp 0``` above no longer depend on undefined behavior in the standard at all, since the range of x is limited and the compiler should know that ``` x*c ``` does not overflow, and so it can be replaced with x.

[Here, check for yourself](https://godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:1,endLineNumber:28,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:28,startColumn:1,startLineNumber:1),source:'%23include+%3Ciostream%3E%0A%23include+%3Cstdexcept%3E%0A%23include+%3Climits%3E%0A%0A%23define+my_int+int%0A%23define+my_uint+unsigned+int%0A%0Aint+main(int+argc,+char**+argv)+%7B%0A++++my_uint+x%3D0%3B%0A++++my_uint+c%3D123456%3B%0A++++try%7B%0A++++++++if(argc!!%3D2)throw+std::invalid_argument(%22Wrong+number+of+arguments%22)%3B%0A++++++++x%3Dstd::stoll(argv%5B1%5D)%3B%0A++++%7Dcatch(std::invalid_argument+%26ex)%7B%0A++++++++std::cout+%3C%3C+%22Usage:+test_opt+NUMBER%22%3C%3Cstd::endl%3B%0A++++++++return+1%3B%0A++++%7D%0A++++//+Sanitization+that+you+have+to+perform+if+the+overflow+is+undefined%0A++++//+Note+that+this+sanitization+also+allows+to+optimize+out+*c+without+relying+on+undefined+behavior%0A++++if(c%3E0+%26%26+x+%3C%3D+std::numeric_limits%3Cmy_int%3E::max()/c+%26%26+x+%3E%3D+std::numeric_limits%3Cmy_int%3E::min()/c)%0A++++%7B%0A++++++++if(x*c%3E0)%7B//+Multiplication+by+c+is+optimized+out%0A++++++++++++std::cout%3C%3C%22a*b%3E0%22%3C%3Cstd::endl%3B%0A++++++++%7D%0A++++%7D%0A++++return+0%3B%0A%7D%0A'),l:'5',n:'0',o:'C%2B%2B+source+%231',t:'0')),k:48.61056751467711,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:compiler,i:(compiler:g122,filters:(b:'0',binary:'1',commentOnly:'0',demangle:'0',directives:'0',execute:'1',intel:'0',libraryCode:'0',trim:'1'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-O3+-fwrapv',selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1,tree:'1'),l:'5',n:'0',o:'x86-64+gcc+12.2+(C%2B%2B,+Editor+%231,+Compiler+%231)',t:'0')),k:51.38943248532289,l:'4',n:'0',o:'',s:0,t:'0')),l:'2',n:'0',o:'',t:'0')),version:4)

The example uses an unsigned type because unsigned types have well defined wrap-around behavior on overflow.

Same for the addition:
```
#include <iostream>
#include <stdexcept>
#include <limits>

#define my_int int
#define my_uint unsigned int
int main(int argc, char** argv) {
    my_uint x=0;
    my_uint c=123456;
    try{
        if(argc!=2)throw std::invalid_argument("Wrong number of arguments");
        x=std::stoll(argv[1]);
    }catch(std::invalid_argument &ex){
        std::cout << "Usage: test_opt NUMBER"<<std::endl;
        return 1;
    }
    // This can not be optimized out because unsigned wrap around is well defined
    if(x+c<x){
        std::cout<<"x+c overflows"<<std::endl;
    }
    // Same but with sanitization that you would need to avoid UB if you used signed numbers.
    if(x <= std::numeric_limits<my_int>::max()-c) // <- Sanitization
    {
        // This entire if statement gets optimized out
        if(x+c<x){
            std::cout<<"This string will never be printed and will be optimized out"<<std::endl;
        }
    }
    return 0;
}
```
[godbolt link](https://godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:2,endLineNumber:30,positionColumn:2,positionLineNumber:30,selectionStartColumn:2,selectionStartLineNumber:30,startColumn:2,startLineNumber:30),source:'%23include+%3Ciostream%3E%0A%23include+%3Cstdexcept%3E%0A%23include+%3Climits%3E%0A%0A%23define+my_int+int%0A%23define+my_uint+unsigned+int%0Aint+main(int+argc,+char**+argv)+%7B%0A++++my_uint+x%3D0%3B%0A++++my_uint+c%3D123456%3B%0A++++try%7B%0A++++++++if(argc!!%3D2)throw+std::invalid_argument(%22Wrong+number+of+arguments%22)%3B%0A++++++++x%3Dstd::stoll(argv%5B1%5D)%3B%0A++++%7Dcatch(std::invalid_argument+%26ex)%7B%0A++++++++std::cout+%3C%3C+%22Usage:+test_opt+NUMBER%22%3C%3Cstd::endl%3B%0A++++++++return+1%3B%0A++++%7D%0A++++//+This+can+not+be+optimized+out+because+unsigned+wrap+around+is+well+defined%0A++++if(x%2Bc%3Cx)%7B%0A++++++++std::cout%3C%3C%22x%2Bc+overflows%22%3C%3Cstd::endl%3B%0A++++%7D%0A++++//+Same+but+with+sanitization+that+you+would+need+to+avoid+UB+if+you+used+signed+numbers.%0A++++if(x+%3C%3D+std::numeric_limits%3Cmy_int%3E::max()-c)+//+%3C-+Sanitization%0A++++%7B%0A++++++++//+This+entire+if+statement+gets+optimized+out%0A++++++++if(x%2Bc%3Cx)%7B%0A++++++++++++std::cout%3C%3C%22This+string+will+never+be+printed+and+will+be+optimized+out%22%3C%3Cstd::endl%3B%0A++++++++%7D%0A++++%7D%0A++++return+0%3B%0A%7D'),l:'5',n:'0',o:'C%2B%2B+source+%231',t:'0')),k:48.61056751467711,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:compiler,i:(compiler:g122,filters:(b:'0',binary:'1',commentOnly:'0',demangle:'0',directives:'0',execute:'1',intel:'0',libraryCode:'0',trim:'1'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-O3+-fwrapv',selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1,tree:'1'),l:'5',n:'0',o:'x86-64+gcc+12.2+(C%2B%2B,+Editor+%231,+Compiler+%231)',t:'0')),k:51.38943248532289,l:'4',n:'0',o:'',s:0,t:'0')),l:'2',n:'0',o:'',t:'0')),version:4)

Same applies to the other examples. 

You can go through them, and find out that if you sanitize to avoid overflows, the compiler tends to correctly deduce that the overflows won't happen, without relying on any magical assumption stemming from UB.

Curiously enough, -fwrapv on signed is not quite adequate for signed integers, but that's not very surprising considering it is a hacky compiler option and doesn't carry the same weight as overflow being defined in the standard.

## But loops! Something about loops? Unrolling? Vectorization?

Loops, what loops? No benchmarking results, not even a lone contrived example where unrolling or vectorization fails if the iterand is unsigned. (Note that such example could be of great practical interest)

[I tried to make one](https://godbolt.org/z/EzdaEq3c1) without much success. 

Not even vectorization and some very serious loop unrolling (see pmuludq) seem to be impacted by either -fwrapv -fno-strict-overflow or by use of unsigned iterand. I tried a few things, I could get it to generate slightly different code that is placed after the loop, but not different loop body.

I think this is likely because of a combination of factors:

* Undefined overflow is not in fact a blanket license for the compiler to ignore everything having to do integer overflow. (The compiler is not allowed to create new overflows)
* There's enough other UB to go around:
  * Out of bounds array access is UB.
  * Out of bounds pointer arithmetics is also UB.
* Typical "for" statement restricts the range of the iterand anyway.
* When the compiler unrolls a loop, after the loop body there's a piece of code for dealing with the remainder of the loop. The compiler is free to restrict the range for the unrolled portion for optimizations, even without any UB. 
* Unsigned iterands are very common in C++, which discourages avoidable reliance on UB overflow.
* High quality code is very meticulous about avoiding buffer overruns, adding range checks immediately preceding the loop.

Bottom line is, there probably isn't much place for making use of signed UB.
## How common are overflow related optimization opportunities in the real world?

According to [this source](https://research.checkpoint.com/2020/optout-compiler-undefined-behavior-optimizations/) , they're extremely rare - they instrumented GCC to print a message any time it removed code based on undefined overflow, and tried it on a number of open source projects. All they found was a few post-checks for overflow that GCC optimized out, in libtiff, causing a security issue. Which had to be rewritten as pre-checks. 

To this day, GCC still can't optimize pre-checks down to the jo/jno , so all they got out of it in the end was updated code with a slower form of overflow test, and no optimization opportunities.

This strongly suggests that undefined signed overflow in the standard, on the whole, resulted in slower production code.
## Why is signed overflow undefined behavior in the first place, anyway? 

The original rationale for not specifying integers was to allow compilers to use native 
integers on a variety of platforms where the overflow could give an implementation-defined result
or cause a hardware trap (an exception of sorts).

Unsigned overflow escaped this curse because the implementations were consistent in how overflow works,
resulting in standardization of de-facto universal behaviors.

Floating point numbers narrowly escaped this curse thanks to IEEE defined floating point formats.
Instead of doing this kind of malarkey, compiler implementers put all their unsafe behaviors into -funsafe-math-optimizations and -ffast-math (the latter can be particularly awful because the init code for a library that sets it, impacts calculations in code built without -ffast-math).

Integers, however, were varied enough between platforms, and yet the variant platforms were rare enough,
that no standardization effort took place until recently (see Language Independent Arithmetic).

## One advantage of "undefined behavior"

Compiler flags like -ftrapv (where the overflow is treated as an error) presumably wouldn't exist if signed overflow was defined as a wraparound. 

Those flags could be useful for security, although you still get denial-of-service exploits, so it's a bit of a mixed bag, plus it has a huge performance impact, making its use in production uncommon. (Note that denial-of-service failures, too, can be very serious in control systems; not everything is a web browser's tab where maybe you can tolerate if a tab crashes).

And of course, without -ftrapv , undefined behavior makes signed overflows far more dangerous than they would normally be. (For a logic bug, due to common use of range checks on arrays, it is nonetheless likely that exploitability of the logic bug would be mitigated by a range check; but as cursed_integer example demonstrates, undefined behavior easily leads to removal of range checks).
## What should be done?

The standard committees needs to remove "undefined behavior" from signed overflows.

Note that this does not necessarily mean the resulting value has to be defined in the standard, or traps prohibited. The standard could allow the implementation to either not return (i.e. trap) or return an implementation-defined value that follows normal value semantics.

Leaving it under-specified would be less than ideal, but nowhere near as bad as the status quo. 

As for optimization impact, without solid benchmarking data on production quality code those concerns should be summarily dismissed. When someone claims an improvement, the onus is on them to demonstrate said improvement. Most of people pushing that point seem entirely unaware that production code puts a lot of effort into not allowing such "optimization opportunities", both eliminating them from the code base *and* disabling them with a compiler flag just to be extra certain that no such optimizations take place.

An alternative is to define signed overflow behavior as part of the ABI for the platform, alongside definition of floating point numbers as conformant to IEEE standard. With the same justification as for floating point number standardization.

A more general note is that in the modern world, software often controls important infrastructure. Unfortunately, much of that software is written in C and C++. Those languages are thus probably the least suitable place to have fun pretending you're Monkey's Paw and someone asked you to optimize their code.