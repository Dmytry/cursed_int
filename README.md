# Cursed integers

An example of how undefined behavior signed overflow can 'infect' a number and cause removal of common types of range checks (e.g. on array access), with significant security implications. 

It is quite common to sanitize inputs to a function. However, any arithmetics in sanitization is extremely hazardous, because an overflow during input sanitization may easily result in removal of subsequent array bounds checks. 

It is pretty well known that signed overflow's undefined behavior can result in rather bizzare changes to what the code does
(such as making a finite loop infinite by changing the iteration step to eliminate a multiply within the loop), but I believe this particular specific case is most relevant to realistic security exploits.

Use GCC 12 with -O2 or -O3 flag to build the samples for maximum craziness. Run them with 2147483647 as the command line argument.

Why does this happen? Why won't numbers simply wrap around?

This happens because C/C++ standard defines signed overflow as equivalently harmful to *a buffer overrun* (meanwhile, for the unsigned, they just state that the arithmetics is done modulo 2^N so there is no overflow).

Of course, the standard doesn't specify that the signed overflow *has* to be as harmful as a buffer overrun; but it does waggle its eyebrows suggestively and gesture furtively (to paraphrase from xkcd):

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

When both the compiler *and the code being compiled* are harmoniously "ignoring the situation completely with unpredictable results", certain optimizations can be made that can't possibly be made otherwise.
## If Builders Built Buildings the Way Programmers Wrote Programs, Then the First Woodpecker That Came Along Would Destroy Civilization

The basic principle at play here is as following:

* You drew extremely detailed plans for a skyscraper with a central load bearing column.
* In the corner of one room, a ceiling tile was accidentally left unsupported.
* A correct design can not result in tiles falling down.
* Therefore the builder can conclude that gravity is absent.
* Therefore the central load bearing column could be omitted.
* Oftentimes, nobody inspects the building and catches the problem, so it just falls down. 

This kind of thing is why people say things like "software engineering is not real engineering".

A huge practical problem with the above "cursed integers" example is that it makes much easier for dangerous code to slip in unnoticed. A seemingly harmless sanitization routine, which looks like "defense in depth", or is meant to better report invalid argument errors, may cause subsequent (or, in theory, preceding) secure code to become non secure.

Note also that a future version of GCC may allow a similar curse in a pure noexcept function, without any red flags like unchecked array access. Maybe already does.
## What's an integer overflow, anyway?

It's when you compute something (e.g. an addition) and find out that the result wouldn't fit into the number's range. Note how we just defined the overflow as something which you will know during or after performing the calculation.

The hardware (x86, x64, ARM) follows that same pattern; the ALU has overflow and carry outputs. When you perform a 64-bit addition, hardware can give you 65th bit of the result. Even if it's [made of relays](https://www.youtube.com/watch?v=k1hJoalcK68). 

For unsigned numbers, you can write something like ```x=a*b; if(b!=0 && x/b != a)``` , and the compiler is able to simplify this to a multiply instruction followed by a jump-if-not-overflow instruction. The standard defining the arithmetics as modulo 2^N (and thus not overflowing) does not stand in the way of using the overflow flag which is available on hardware.

For signed numbers, to the contrary, with the signed overflow leading to *undefined behavior* by the standard, the standard effectively prohibits after-the-fact detection of overflow. The code has to adopt an inverse approach - it has to prevent signed overflow ahead of time, being careful that the calculations which predict the overflow do not themselves overflow. (Prevention of overflow in a multiply is almost comically cumbersome)

Murphy's law being in effect, the optimizer that is smart enough to remove security checks because of some UB, is nowhere near smart enough to replace overflow prevention with less costly overflow detection.

This makes signed overflow rather unique comparing to other undefined behaviors in the standard. Other undefined behaviors do not significantly interfere with mitigation of the corresponding logic bugs. But signed overflow being undefined does: the typical pattern for preventing overflow-related logic bugs is to verify the result, which requires well defined behavior.

Until relatively recently in the life of C and C++ languages, the status quo was to simply forgo such mitigations and hope for the best. Shove software out of the door and not worry about security or reliability.

Fragments of this mindset unfortunately permeate performance discussions to this day:

* It is presumed that the code being optimized does not contain conditionals to reliably guard against logic bugs or undefined behavior. 
* There is no concern that a facet of the standard may force such conditionals to be unnecessarily slow or complex to write (the default expectation is simply that nobody's going to bother writing any).
* It is presumed that a library function typically delegates the task of guarding against UB to the user of that function, and thus the triggering of UB is a source of unique information about input values' range. 

All of those assumptions are, however, increasingly false for production software - the code you use in your daily life and whose outputs you wait on, the code which wastes non negligible number of kilowatt-hours. In short, the code that is actually worth optimizing. 

The patterns assumed in optimization discussions haven't been "good practice" for a long time.

Below, I hammer out the specific points with some examples.
## People say that undefined signed overflow allows compilers to optimize better.

This is rather hard to take seriously, with lack of any real world data and optimization examples like ```a+c<a``` which are also great examples of code you shouldn't write because signed overflow is undefined behavior. What's the point in better optimizing the code that you shouldn't be writing?

What's about the code that you should write instead?

Here is how you're supposed to multiply two numbers in pure C / C++ in a portable way:

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
Condensed for brevity. Note that the alternative of casting to a 2x larger type prior to multiply is not possible if you're already using the largest type. The other alternative, __builtin_mul_overflow, is not standard.

What does a compiler do with this almost comical contraption? Maybe it gets optimized out?

[Full source on godbolt - see the disassembly](https://godbolt.org/#z:OYLghAFBqd5QCxAYwPYBMCmBRdBLAF1QCcAaPECAMzwBtMA7AQwFtMQByARg9KtQYEAysib0QXACx8BBAKoBnTAAUAHpwAMvAFYTStJg1DIApACYAQuYukl9ZATwDKjdAGFUtAK4sGIABykrgAyeAyYAHI%2BAEaYxCCSGqQADqgKhE4MHt6%2BASlpGQKh4VEssfGJtpj2jgJCBEzEBNk%2BfoF2mA6Z9Y0ExZExcQlJCg1NLbntY31hA2VDiQCUtqhexMjsHAD0WwDUACpMANaMu1TEqCy7CAQEyQogOwDueEd4AHRKH8gsXu%2BY6C8WzQDCo3kYGy2%2BAUyQMAE9gVsAJIRfYAZjMAFo3O9rNgGAo1phrAQEEwCNZUMk4uTMgpKQxrOlgOF0NYwgRMMA4vTLOhUNYGKgKZZiJhCbQRRYwpSAG5xMGoJ4mDQAQR2u0UAN2TAUOoYu0wqlYsMwu1QVF2aBYyTocTOTDoYWAuyIVoE8qa7tGtDh5s9iqeu2SYvlgkyuw5qF2KvVe2QCEwTGS/oVtCVuywnK6Al2EG00b22iFi1xapjavMaLCyG8WBjaLco3wqHeCBMaOwsarNbrZo7TYI6FoeGibY7XcrZmrDFrXnrA9GxGd4873envfn/cbI5YhAUq8nqvXaKwNHCuxYcIA%2BhzdumjPeBC6OSez7NLzevHevAS8CztQfF0gMjQRuzVK9b0EM5f2QCBILvdJryYUhPygghdiQ6JFhjAB2Kxy0jS0ICQpgG2wXYNBwkx8N2XYtgAKkwvBkMjPVUnSRx5V2BitljOi6LwYisPIyjqNo%2BimNI/V0GY69oh1MVgwKLizV4/iBME4SWLIic82CYIAHkIgAcWvABZVUAA16Lk7DxII1VNM0xjdgACUMYczTiC5iB4vjCOc4Nl0EKgIHMMxDIDdMgxMABWNxGTMMxFg7RygrosUCDWA0NDSjTNJogARAq8KKw1aCUPCLEkuSyI4jJ5VQkShQYBrVP80qtLzESB30ozTIslFbNIxYHK6gTXI8hgvMNYhfM6wLnJDDkwoiqK0wzeLEoi1K0XSjKspyyj8qWujitK4rauk9q8Cauzdla27uPUwiruqKqaJq1zpLwPUnpUu61ICpyBKEnqWIUvSqOq66dLYx6BGezBmshhHkcW0HNPBkj4b6iADOMszzOGvYsLG6qJroqbPPoOaFterHlpCgg1uSjbiEDGMEqSlLTqZzSjuIXL%2BaCi6lveyr%2B1on74c8h7GjNAHOKBzGgpxiHWLAMAO3KmHzAANkNzWocbfqiYs6yRp0sbxrOyamOm2afJINWMpW0LwvZ6Ktp53bRcOzBsuFk79om8Wmau1z8Vk8HpPlkTFcRtrAZekHztw8ro5moi6oR5XGuBjShYNaSpMh/mI9jO8WEdBgIDvRpgGQVCE0aBimKb2U7aZoSICb5Btd1tFFhL3YuADuiEOg1RdaXIhaFofviGAWV4osCe4qK1CGC8RfkgIMhx6oye0LvOE58Phel679ezHi7fHr32gD6PrgT7Dpbp4wgAvXWqFghAVQpA4R7RqgJUqHtWZezMH/NEJUzBxUXmyZKqEf5gNKmPPKn9jyZ1jBwZYtBOBxV4H4DgWhSCoE4G4aw1hMKrHWP2acPBSAEE0AQ5YRwQBogAJzvFwpILgGJ/AGzRBoMwXBcJxTRPoTgkhSHsMoZwXgDwkhsPIQQ0gcBYBIGtLaegZAKAQD0XaeIwAuCoJoJKHklBoiKOiGERocJOAsOtGwQQhkGC%2BkUVgWuRhxAaNIPgMUXR5QPECUaToXhOQuN4ByaoiiRzRGIE4jwWBFGHzwCwWJywwRMGAAoAAangTATxDLUjISw/gggRBiHYFIGQggtRqEUboLg%2BhDDGFoZYfQo4HiQGWFSWoBJOCYkMmiFR1ROjDJcDNCYfgzBJBCLMUo5QQASPyCrAQ8z1lJFukUFZgx4gbI6DmBgPRxieFaCAHhVQajdGmP0VZQwTnTB2bc0YvQnlHPWVwZYCgGEbD0IfTAmweCEOIQowJVCOCqBEZiA2khdjN2QOPMw7wzB5hoZYawqFcCEFdlWdpuwPA2lMTGZhixeDqK0GNUgXCzAG3eDwxBaI0T%2BA0GiRFkgeFcANrIjg8jSBkIoTClRIA1HsOWNoxAIBInIGiSQcglAm4KGUIYaoQgEBKkqbwExBhhnqvCLQLVOrFH6qGOY1BFriCGWiaap4IreDytVCvcJmIuC7ExCy6caJcKLK5f4OKkhcJSI5f4Z1qhOj1HwGQ3g1ThCiHEA0hNzT1CBLaR0owKBuk2CSf0iAgyD50lGeMlRgL6m2CHGEI1mrtWOtiawsUYLeBPBSckHJAqSHCsUTC7AUaFVED8nCg2CKkUorRRirFua8X4CHRStEfzqVSs4dwtE7w2Wbq3dugVQqnVKI4OKyVGi6VEI4GYKForlGsJXaQT06RnCSCAA%3D%3D)

Behold a bunch of branches and division operations. The compiler, unsurprisingly, fails to optimise out the divisions.

Division is much more expensive than multiply, but that's not even the biggest problem here. If the input is a stream of numbers of random signs, the branches are unpredictable and extremely expensive - even if no overflows actually occur. 

And here is how you check for overflow when the behavior is well defined:
```
unsigned int x, y; 
...
unsigned int z=x*y;
if(x!=0 && z/x != y)overflow();
```
[godbolt link](https://godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:1,endLineNumber:19,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:19,startColumn:1,startLineNumber:1),source:'%23include+%3Cstdio.h%3E%0A%23include+%3Cstdlib.h%3E%0A%23include+%3Cstring.h%3E%0A%0A%23define+my_int+int%0A%23define+my_uint+unsigned+long+long%0A%0Aint+main(int+argc,+char**+argv)+%7B%0A++++if(argc!!%3D3)return+1%3B%0A++++my_uint+x%3Dstrtoll(argv%5B1%5D,+nullptr,+10)%3B%0A++++my_uint+y%3Dstrtoll(argv%5B2%5D,+nullptr,+10)%3B%0A++++my_uint+z%3Dx*y%3B%0A++++if(x!!%3D0+%26%26+z/x+!!%3D+y)%7B%0A++++++++//+cleaner+assembly+than+cout%0A++++++++printf(%22Overflow+%5Cn%22)%3B%0A++++%7D%0A++++return+0%3B%0A%7D%0A'),l:'5',n:'0',o:'C%2B%2B+source+%231',t:'0')),k:48.61056751467711,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:compiler,i:(compiler:g122,filters:(b:'0',binary:'1',commentOnly:'0',demangle:'0',directives:'0',execute:'1',intel:'0',libraryCode:'0',trim:'1'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-O3',selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1,tree:'1'),l:'5',n:'0',o:'x86-64+gcc+12.2+(C%2B%2B,+Editor+%231,+Compiler+%231)',t:'0')),k:51.38943248532289,l:'4',n:'0',o:'',s:0,t:'0')),l:'2',n:'0',o:'',t:'0')),version:4)

You check for it after the fact (which you can do because it's not UB).

A thing of beauty. The compiler optimizes out the division and uses the "jo" instruction (jump if overflow). For a non overflow triggering stream of numbers, all branches are predictable.

Ok, sanitizing for multiplication is hard. Sanitizing for addition is easy, maybe the compiler can optimize that one down to an overflow flag check?

[Nope, it couldn't](https://godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:1,endLineNumber:33,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:33,startColumn:1,startLineNumber:1),source:'%23include+%3Cstdio.h%3E%0A%23include+%3Cstdlib.h%3E%0A%23include+%3Cstring.h%3E%0A%23include+%3Climits.h%3E%0A%0A//+Allow+easy+experimentation+on+different+types%0A%23define+my_int+long+long+int%0A%23define+my_MAX+LLONG_MAX%0A%23define+my_MIN+LLONG_MIN%0A%0Astatic+my_int+safe_sum(my_int+si_a,+my_int+si_b)+%7B%0A++signed+int+sum%3B%0A++if+(((si_b+%3E+0)+%26%26+(si_a+%3E+(my_MAX+-+si_b)))+%7C%7C%0A++++++((si_b+%3C+0)+%26%26+(si_a+%3C+(my_MIN+-+si_b))))+%7B%0A++++/*+Handle+error+*/%0A++++printf(%22Overflow%5Cn%22)%3B%0A++++return+0%3B%0A++%7D+else+%7B%0A++++return+si_a+%2B+si_b%3B%0A++%7D%0A++/*+...+*/%0A%7D%0A%0Aint+main(int+argc,+char**+argv)+%7B%0A++++if(argc!!%3D3)return+1%3B%0A++++my_int+x%3Dstrtoll(argv%5B1%5D,+nullptr,+10)%3B%0A++++my_int+y%3Dstrtoll(argv%5B2%5D,+nullptr,+10)%3B%0A++++my_int+z%3B%0A++++z%3Dsafe_sum(x,y)%3B++++%0A++++printf(%22sum%3D%25lld%22,+z)%3B%0A++++return+0%3B%0A%7D%0A'),l:'5',n:'0',o:'C%2B%2B+source+%231',t:'0')),k:39.74132863021753,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:compiler,i:(compiler:g122,filters:(b:'0',binary:'1',commentOnly:'0',demangle:'0',directives:'0',execute:'1',intel:'0',libraryCode:'0',trim:'1'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-O3',selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1,tree:'1'),l:'5',n:'0',o:'x86-64+gcc+12.2+(C%2B%2B,+Editor+%231,+Compiler+%231)',t:'0')),k:26.92533803644916,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:executor,i:(argsPanelShown:'0',compilationPanelShown:'0',compiler:g122,compilerOutShown:'0',execArgs:'-1+-9223372036854775808',execStdin:'',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-O3',source:1,stdinPanelShown:'1',tree:'1',wrap:'1'),l:'5',n:'0',o:'Executor+x86-64+gcc+12.2+(C%2B%2B,+Editor+%231)',t:'0')),k:33.33333333333333,l:'4',n:'0',o:'',s:0,t:'0')),l:'2',n:'0',o:'',t:'0')),version:4)

That's it, folks. For every arithmetic operation you have to do a bunch of branching and an inverse operation.

And you absolutely should not be expecting to get better performance out of using signed numbers instead of unsigned.

If you switch from unsigned to signed, you will have to replace all your after-the-fact overflow checks with ahead-of-time overflow checks, that can be dramatically slower and which the compiler will not optimize well.

I have a slight suspicion that the standard (or people talking about how it enables optimizations in general) do not expect you or anyone to actually *do* anything like this, given just how cumbersome it is. Instead perhaps you document that your function has undefined behavior if the parameters cause an integer overflow. Of course, that's unacceptable for production code that accepts untrusted inputs.
## What kind of code benefits from UB overflow derived optimization?

Not your Linux kernel or your web browser, or their components. Not only are they careful about avoiding overflows, they also turn those optimizations off using -fwrapv or -fno-strict-overflow.

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

Note that in production quality code, all of those examples would require some kind of value sanitization to avoid the risk of UB at runtime, *regardless of whether the result of the calculation is security relevant or not*. Even if the result is not security critical, even if any result is acceptable, even if the optimization may decrease the risk of actual overflow, the code still has to avoid undefined behavior because the optimizer can and will propagate undefined behavior.

Without value sanitization, they're all "bad code" - especially according to people who think UB optimizations are a good idea.

The corresponding post-UB-pocalyptic examples would look something like
```
if( c > 0 && x <= std::numeric_limits<decltype(x)>::max()/c && x >= std::numeric_limits<decltype(x)>::min()/c ){
    if(x*c>0){....}
}
```
This admittedly looks quite contrived because it is easier for the programmer to simplify out the multiply than to sanitize for it, but I'll roll with it.

Note that ```(x * c) cmp 0  ->   x cmp 0``` above no longer depend on undefined behavior in the standard at all, since the range of x is limited and the compiler should know that ``` x*c ``` does not overflow, and so it can be replaced with x. That optimization can now be performed even for unsigned numbers.

[Here, check for yourself](https://godbolt.org/#g:!((g:!((g:!((h:codeEditor,i:(filename:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,selection:(endColumn:1,endLineNumber:28,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:28,startColumn:1,startLineNumber:1),source:'%23include+%3Ciostream%3E%0A%23include+%3Cstdexcept%3E%0A%23include+%3Climits%3E%0A%0A%23define+my_int+int%0A%23define+my_uint+unsigned+int%0A%0Aint+main(int+argc,+char**+argv)+%7B%0A++++my_uint+x%3D0%3B%0A++++my_uint+c%3D123456%3B%0A++++try%7B%0A++++++++if(argc!!%3D2)throw+std::invalid_argument(%22Wrong+number+of+arguments%22)%3B%0A++++++++x%3Dstd::stoll(argv%5B1%5D)%3B%0A++++%7Dcatch(std::invalid_argument+%26ex)%7B%0A++++++++std::cout+%3C%3C+%22Usage:+test_opt+NUMBER%22%3C%3Cstd::endl%3B%0A++++++++return+1%3B%0A++++%7D%0A++++//+Sanitization+that+you+have+to+perform+if+the+overflow+is+undefined%0A++++//+Note+that+this+sanitization+also+allows+to+optimize+out+*c+without+relying+on+undefined+behavior%0A++++if(c%3E0+%26%26+x+%3C%3D+std::numeric_limits%3Cmy_int%3E::max()/c+%26%26+x+%3E%3D+std::numeric_limits%3Cmy_int%3E::min()/c)%0A++++%7B%0A++++++++if(x*c%3E0)%7B//+Multiplication+by+c+is+optimized+out%0A++++++++++++std::cout%3C%3C%22a*b%3E0%22%3C%3Cstd::endl%3B%0A++++++++%7D%0A++++%7D%0A++++return+0%3B%0A%7D%0A'),l:'5',n:'0',o:'C%2B%2B+source+%231',t:'0')),k:48.61056751467711,l:'4',n:'0',o:'',s:0,t:'0'),(g:!((h:compiler,i:(compiler:g122,filters:(b:'0',binary:'1',commentOnly:'0',demangle:'0',directives:'0',execute:'1',intel:'0',libraryCode:'0',trim:'1'),flagsViewOpen:'1',fontScale:14,fontUsePx:'0',j:1,lang:c%2B%2B,libs:!(),options:'-O3+-fwrapv',selection:(endColumn:1,endLineNumber:1,positionColumn:1,positionLineNumber:1,selectionStartColumn:1,selectionStartLineNumber:1,startColumn:1,startLineNumber:1),source:1,tree:'1'),l:'5',n:'0',o:'x86-64+gcc+12.2+(C%2B%2B,+Editor+%231,+Compiler+%231)',t:'0')),k:51.38943248532289,l:'4',n:'0',o:'',s:0,t:'0')),l:'2',n:'0',o:'',t:'0')),version:4)

For the additions and subtractions, likewise, a range checks enables optimizations:
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

Same applies to the other examples: the compiler can apply optimizations that assume no wrap around, if it can predict no wrap around. That is what is done for unsigned numbers.

Curiously enough, -fwrapv on signed is not quite adequate for signed integers, but that's not very surprising considering it is a hacky compiler option and doesn't carry the same weight as overflow being defined in the standard.
## But loops! 

> The canonical example of why undefined signed overflow helps loop optimizations is that loops like
>
> ```for (int i = 0; i <= m; i++)```
>
> are guaranteed to terminate for undefined overflow...

They aren't guaranteed anything, m==MAX_INT is undefined behavior (i++ will overflow). The code may loop forever (e.g. on -O0), or it may never enter the loop, or it can do literally anything else, it can even misbehave *prior* to entering the loop.

Here's a loop that's guaranteed to terminate: ```if (m < INT_MAX) for (int i = 0; i <= m; i++)``` , not entering the loop if it would loop forever. With unsigned m, you could write ```for (unsigned int i = 0; i < m+1; i++)``` to the same effect.

It's kind of bad practice to write loops that could loop forever in a corner case.

> This helps architectures that have specific loop instructions, as they do in general not handle infinite loops.

1: You can always use what ever instruction you want, like this:
```
mov ecx, m 
cmp ecx, 0
je l2               ; <- Necessary because we converted for loop into a while loop so we can use the "loop" instruction
l1:
... 
...
...
loop l1             ; <- We really want to use that instruction for looping
mov ecx, m          ; Extra instructions
cmp ecx, 0x7fffffff ; For making it
je l1               ; Loop forever
l2:
```

The cost is negligible, since the inner loop remains the same.

While you're at it, you could output a warning message about a potentially infinite loop.

2: UB or no UB, CISC style instructions like [loop had been slow since the late 1980s.](https://stackoverflow.com/questions/35742570/why-is-the-loop-instruction-slow-couldnt-intel-have-implemented-it-efficiently) Modern instruction sets don't even have those. I had to write that "loop" example by hand.
## Something else about loops? Unrolling? Vectorization?

[I tried to make an example](https://godbolt.org/z/EzdaEq3c1) without much success.

Not even vectorization and loop unrolling (see pmuludq) seem to be impacted by either -fwrapv -fno-strict-overflow or by use of unsigned iterand. That also holds for wider vectorization (when setting a recent -march=... flag).

I tried a few things, I could get it to generate slightly different code that is placed after the loop, but couldn't get it to change the body of the loop.

I think this is likely because of a combination of factors:

* The compiler is always free to fix up corner cases after the loop (and does this already for unrolling and vectorization).
* Unsigned iterands are very common in C++ , which encourages UB-independent approaches to loop optimization.
* There's enough other UB in most loops:
  * Out of bounds array access is UB.
  * Out of bounds pointer arithmetics is also UB.
* Typical "for" statement restricts the range of the iterand anyway.

To find a loop that's significantly improved by undefined behavior signed overflow, you need to somehow slip past all these bullet points.

I think the fatal one is the first one. There's no reason for a corner case at the end of the loop to impact the loop itself.
## But some platforms got saturating math.

And without undefined behavior you can write something nice and portable like
```
y=x+150;
if(y<x)y=UINT_MAX;
```
and a good compiler could [convert that](https://stackoverflow.com/questions/121240/how-to-do-unsigned-saturating-addition-in-c) to a single saturating add opcode, on those platforms.


Alas, not for signed numbers, because of undefined behavior. (As established earlier, pre-checks for overflow do not get optimized out.)

I get it, the idea here is to start arguing that in principle there could exist a hypothetical computer that only has saturating operations or where saturating operations are faster. That seems rather silly; how would you go about implementing longer integers on that computer? Or C's unsigned numbers? A saturating adder is nothing more than a normal adder with an extra circuit for saturation, so you'd just implement a way to disable that circuit.
## How common are overflow related optimization opportunities in the real world?

According to [this source](https://research.checkpoint.com/2020/optout-compiler-undefined-behavior-optimizations/) , they're extremely rare - they instrumented GCC to print a message any time it removed code based on undefined overflow, and tried it on a number of open source projects. All they found was a few post-checks for overflow that GCC optimized out, in libtiff, causing a security issue. Which had to be rewritten as pre-checks.

To this day, GCC still can't optimize pre-checks down to the jo or jno (or similar) , so all they got out of it in the end was updated code with a slower form of overflow test as required by the standard, and no optimization opportunities.

This strongly suggests that undefined signed overflow in the standard, on the whole, probably resulted in slower production code.

To summarize, it seems that those overflow-related optimizations only occur when both the compiler *and the code being compiled* are "ignoring the situation completely with unpredictable results", which is increasingly rare in production code due to security implications of "unpredictable results".
## Why is signed overflow undefined behavior in the first place, anyway? 

The original rationale for not specifying integers was to allow compilers to use native 
integers on a variety of platforms where the overflow could give an implementation-defined result
or cause a hardware trap (an exception of sorts).

Unsigned numbers escaped this curse because the implementations were consistent. 

Floating point numbers narrowly escaped this curse thanks to IEEE defined floating point formats.
Instead of doing this kind of malarkey, compiler implementers put all their unsafe behaviors into -funsafe-math-optimizations and -ffast-math (the latter can be particularly awful because the init code for a library that sets it, impacts calculations in code built without -ffast-math).

Integers, however, were varied enough between platforms, and yet the variant platforms were rare or outdated enough,
that no standardization effort took place until recently (see Language Independent Arithmetic).
## One advantage of "undefined behavior"

Compiler flags like -ftrapv (where the overflow is treated as an error) presumably wouldn't exist if signed overflow was defined as a wraparound. 

Those flags could be useful for security, although you still get denial-of-service exploits, so it's a bit of a mixed bag, plus it has a huge performance impact, making its use in production uncommon. Note that denial-of-service failures, too, can be very serious in control systems; not everything is a web browser's tab where maybe you can tolerate if a tab crashes.

And of course, without -ftrapv , undefined behavior makes signed overflows far more dangerous than they would normally be, as the optimizer can and will remove other checks which would prevent exploitability.
## What should be done?

The standard committees needs to remove "undefined behavior" from signed overflows.

Note that this does not necessarily mean the resulting value has to be defined in the standard, or traps prohibited. The standard could allow the implementation to have an option to not return (i.e. trap) or return an implementation-defined value that follows normal value semantics.

Leaving it under-specified would be less than ideal, but nowhere near as bad as the status quo. 

As for compiler optimization impact, without solid benchmarking data on production quality code those concerns should be summarily dismissed. When someone claims an improvement, the onus is on them to demonstrate said improvement.

An alternative is to define signed overflow behavior as part of the ABI for the platform, alongside definition of floating point numbers as conformant to IEEE standard. With the same justification as for floating point number standardization.

A more general note is that in the modern world, software often controls important infrastructure. Unfortunately, much of that software is written in C and C++. Those languages are thus probably the least suitable place to have fun pretending you're Monkey's Paw and someone asked you to optimize their code.