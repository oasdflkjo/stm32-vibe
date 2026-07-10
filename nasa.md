NASA has a list of 10 rules for software development
Read the full document (local copy) or the original. Read it first, before reading these comments.

Those rules were written from the point of view of people writing embedded software for extremely expensive spacecraft, where tolerating a lot of programming pain is a good tradeoff for not losing a mission. I do not know why someone in that situation does not use the SPARK subset of Ada, which subset was explicitly designed for verification, and is simply a better starting point for embedded programming than C.

I am criticising them from the point of view of people writing programming language processors (compilers, interpreters, editors) and application software.

We are supposed to teach critical thinking. This is an example.

How have Gerard J. Holzmann's and my different contexts affected our judgement?
Can you blindly follow his advice without considering your context?
Can you blindly follow my advice without considering your context?
Would these rules necessarily apply to a different/better programming language? What if function pointers were tamed? What if the language provided opaque abstract data types as Ada does?
1. Restrict all code to very simple control flow constructs — do not use goto statements, setjmp() or longjmp() constructs, and direct or indirect recursion.
Note that setjmp() and longjmp() are how C does exception handling, so this rule bans any use of exception handling.

It is true that banning recursion and jumps and loops without explicit bounds means that you know your program is going to terminate. It is also true that recursive functions can be proven to terminate about as often as loops can, with reasonably well-understood methods. What's more important here is that “sure to terminate” does not imply “sure to terminate in my lifetime”:

    int const N = 1000000000;
    for (x0 = 0; x0 != N; x0++)
    for (x1 = 0; x1 != N; x1++)
    for (x2 = 0; x2 != N; x2++)
    for (x3 = 0; x3 != N; x3++)
    for (x4 = 0; x4 != N; x4++)
    for (x5 = 0; x5 != N; x5++)
    for (x6 = 0; x6 != N; x6++)
    for (x7 = 0; x7 != N; x7++)
    for (x8 = 0; x8 != N; x8++)
    for (x9 = 0; x9 != N; x9++)
        -- do something --;
This does a bounded number of iterations. The bound is N10. In this case, that's 1090. If each iteration of the loop body takes 1 nsec, that's 1081 seconds, or about 7.9×1072 years. What is the practical difference between “will stop in 7,900,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000 years” and “will never stop”?

Worse still, taking a problem that is naturally expressed using recursion and contorting it into something that manipulates an explicit stack, while possible, turns clear maintainable code into buggy spaghetti. (I've done it, several times. There's an example on this web site. It is not a good idea.)

2. All loops must have a fixed upper-bound. It must be trivially possible for a checking tool to prove statically that a preset upper-bound on the number of iterations of a loop cannot be exceeded. If the loop-bound cannot be proven statically, the rule is considered violated.
This is an old idea. As the example above shows, it is not enough by itself to be of any practical use. You have to try to make the bounds reasonably tight, and you have to regard hitting an artificial bound as a run-time error.

By the way, note that putting depth bounds on recursive procedures makes them every bit as safe as loops with fixed bounds.

3. Do not use dynamic memory allocation after initialization.
This is also a very old idea. Some languages designed for embedded work don't even have dynamic memory allocation. The big thing, of course, is that embedded applications have a fixed amount of memory to work with, are never going to get any more, and should not crash because they couldn't handle another record.

Note that the rationale actually supports a much stronger rule: don't even simulate dynamic memory allocation. You can of course manage your own storage pool:

    typedef struct Foo_Record *foo;
    struct Foo_Record {
	foo next;
	...
    };
    #define MAX_FOOS ...
    static struct Foo_Record foo_zone[MAX_FOOS];
    foo foo_free_list = 0;

    void init_foo_free_list() {
	for (int i = MAX_FOOS - 1; i >= 0; i--) {
	    foo_zone[i].next = foo_free_list;
	    foo_free_list = &foo_zone[i];
	}
    }

    foo malloc_foo() {
	foo r = foo_free_list;
	if (r == 0) report_error();
	foo_free_list = r->next;
	return r;
    }

    void free_foo(foo x) {
	x->next = foo_free_list;
	foo_free_list = x;
    }
This technically satisfies the rule, but it violates the spirit of the rule. Simulating malloc() and free() this way is worse than using the real thing, because the memory in foo_zone is permanently tied up for Foo_Records, even if we don't need any of those at the moment but do desperately need the memory for something else.

What you really need to do is to use a memory allocator with known behaviour, and to prove that the amount of memory in use at any given time (data bytes + headers) is bounded by a known value.

Note also that SPlint can verify at compile time that the errors NASA speak of do not occur.

One of the reasons given for the ban is that the performance of malloc() and free() is unpredictable. Are these the only functions we use with unpredictable performance? Is there anything about malloc() and free() which makes them necessarily unpredictable? The existence of hard-real-time garbage collectors suggests not.

The rationale for this rule says that

Note that the only way to dynamically claim memory in the absence of memory allocation from the heap is to use stack memory. In the absence of recursion (Rule 1), an upper bound on the use of stack memory can derived statically, thus making it possible to prove that an application will always live within its pre-allocated memory means.
Unfortunately, the sunny optimism shown here is unjustified. Given the ISO C standard (any version, C89, C99, or C11) it is impossible to determine an upper bound on the use of stack memory. There is not even any standard way to determine how much memory a compiler will use for the stack frame of a given function. (There could have been. There just isn't.) There isn't even any requirement that two invocations of the same function with the same arguments will use the same amount of memory. Such a bound can only be calculated for a specific version of a specific compiler with specific options. Here's a trivial example:

void f() {
    char a[100000];
}
How much memory will that take on the stack? Compiled for debugging, it might take a full stack frame (however big that is) plus traceback information plus a million bytes for a[]. Compiled with optimisation, the compiler might notice that a[] isn't used, and might even compile calls to f() inline so that they generate no code and take no space. That's an extreme example, but not really unfair. If you want bounds you can rely on, you had better check what your compiler does, and recheck every time anything about the compiler changes.

4. No function should be longer than what can be printed on a single sheet of paper in a standard reference format with one line per statement and one line per declaration. Typically, this means no more than about 60 lines of code per function.
Since programmers these days typically read their code on-screen, not on paper, it's not clear why the size of a sheet of paper is relevant any longer.

The rule is arguably stated about the wrong thing. The thing that needs to be bounded is not the size of a function, but the size of a chunk that a programmer needs to read and comprehend.

There are also question marks about how to interpret this if you are using a sensible language (like Algol 60, Simula 67, Algol 68, Pascal, Modula2, Ada, Lisp, functional languages like ML, O'CAML, F#, Clean, Haskell, or Fortran) that allows nested procedures. Suppose you have a folding editor that presents a procedure to you like this:

function Text_To_Floating(S: string, E: integer): Double;
   � variables �
   � procedure Mul(Carry: integer) �
   � function Evaluate: Double �

   Base, Sign, Max, Min, Point, Power := 10, 0, 0, 1, 0, 0;
   for N := 1 to S.length do begin
       C := S[N];
       if C = '.' then begin
          Point := -1
       end else
       if C = '_' then begin
          Base := Round(Evaluate);
          Max, Min, Power := 0, 1, 0
       end else
       if Char ≠ ' ' then begin
          Q := ord(C) - ord('0');
          if Q > 9 then Q := ord(C) - ord('A') + 10
          Power := Point + Point
          Mul(Q)
       end
    end;
    Power := Power + Exp;
    Value := Evaluate;
    if Sign < 0 then Value := -Value;
end;
which would be much bigger if the declarations were expanded out instead of being hidden behind �folds�. Which size do we count? The folded size or the unfolded size?

I was using a folding editor called Apprentice on the Classic Mac back in the 1980s. It was written by Peter McInerny and was lightning fast.

5. The assertion density of the code should average to a minimum of two assertions per function.
Assertions are wonderful documentation and the very best debugging tool I know of. I have never seen any real code that had too many assertions.

The example here is one of the ugliest pieces of code I've seen in a while.

if (!c_assert(p >= 0) == true) {
    return ERROR;
}
It should, of course, just be

if (!c_assert(p >= 0)) {
    return ERROR;
}
Better still, it should be something like

check(ERROR, p >= 0);
with

#ifdef NDEBUG
#define check(e, c) (void)0
#else
#define check(e, c) if (!(c)) return bugout(c), (e)
#ifdef NDEBUG_LOG
#define bugout(c) (void)0
#else
#define bugout(c) \
    fprintf(stderr, "%s:%d: assertion '%s' failed.\n", \
    __FILE__, __LINE__, #s)
#endif
#endif
Ahem. The more interesting part is the required density. I just checked an open source project from a large telecoms company, and 23 out of 704 files (not functions) contained at least one assertion. I just checked my own Smalltalk system and one SLOC out of every 43 was an assertion, but the average Smalltalk “function” is only a few lines. If the biggest function allowed is 60 lines, then let's suppose the average function is about 36 lines, so this rule requires 1 assertion per 18 lines.

Assertions are good, but what they are especially good for is expressing the requirements on data that come from outside the function. I suggest then that

Every argument whose validity is not guaranteed by its typed should have an assertion to check it.
Every datum that is obtained from an external source (file, data base, message) whose validity is not guaranteed by its type should have an assertion to check it.
The NASA 10 rules are written for embedded systems, where reading stuff from sensors is fairly common.

6. Data objects must be declared at the smallest possible level of scope.
This is excellent advice, but why limit it to data objects? Oh yeah, the rules were written for crippled languages where you couldn't declare functions in the right place.

People using Ada, Pascal (Delphi), JavaScript, or functional languages should also declare types and functions as locally as possible.

7. The return value of non-void functions must be checked by each calling function, and the validity of parameters must be checked inside each function.
This again is mainly about C, or any other language that indicates failure by returning special values. “Standard libraries famously violate this rule”? No, the C library does.

You have to be reasonable about this: it simply isn't practical to check every aspect of validity for every argument. Take the C function

void *bsearch(
    void const *key  /* what we are looking for */,
    void const *base /* points to an array of things like that */,
    size_t      n    /* how many elements base has */,
    size_t      size /* the common size of key and base's elements */
    int (*      cmp)(void const *, void const *)
);
This does a binary search in an array. We must have key≠0, base≠0, size≠0, cmp≠0, cmp(key,key)=0, and for all 1<i<n,

cmp((char*)base+size*(i-1), (char*)base+size*i) <= 0
Checking the validity in full would mean checking that [key..key+size) is a range of readable addresses, [base..base+size*n) is a range of readable addresses, and doing n calls to cmp. But the whole point of binary search is to do O(log(n)) calls to cmp.

The fundamental rules here are

Don't let run-time errors go un-noticed, and
any check is safer than no check.
8. The use of the preprocessor must be limited to the inclusion of header files and simple macro definitions. Token pasting, variable argument lists (ellipses), and recursive macro calls are not allowed.
Recursive macro calls don't really work in C, so no quarrel there. Variable argument lists were introduced into macros in C99 so that you could write code like

#define err_printf(level, ...) \
    if (debug_level >= level) fprintf(stderr, __VA_ARGS__)
...
    err_printf(HIGH, "About to frob %d\n", control_index);
This is a good thing; conditional tracing like this is a powerful debugging aid. It should be encouraged, not banned.

The rule goes on to ban macros that expand into things that are not complete syntactic units. This would, for example, prohibit simulating try-catch blocks with macros. (Fair enough, an earlier rule banned exception handling anyway.) Consider this code fragment, from an actual program.

    row_flag = border;     
    if (row_flag) printf("\\hline");
    for_each_element_child(e0, i, j, e1)
        printf(row_flag ? "\\\\\n" : "\n");
        row_flag = true;  
        col_flag = false;
        for_each_element_child(e1, k, l, e2)
            if (col_flag) printf(" & ");
            col_flag = true;
            walk_paragraph("", e2, "");
        end_each_element_child
    end_each_element_child
    if (border) printf("\\\\\\hline");
    printf("\n\\end{tabular}\n");
It's part of a program converting slides written in something like HTML into another notation for formatting. The for_each_element_child … end_each_element_child loops walk over a tree. Using these macros means that the programmer has no need to know and no reason to care how the tree is represented and how the loop actually works. You can easily see that for_each_element_child must have at least one unmatched { and end_each_element_child must have at least one unmatched }. That's the kind of macro that's banned by requiring complete syntactic units. Yet the readability and maintainability of the code is dramatically improved by these macros.

One thing the rule covers, but does not at the beginning stress, is “no conditional macro processing”. That is, no #if. The argument against it is, I'm afraid, questionable. If there are 10 conditions, there are 210 combinations to test, whether they are expressed as compile-time conditionals or run-time conditionals.

In particular, the rule against conditional macro processing would prevent you defining your own assertion macros. It is not obvious that that's a good idea.

9. The use of pointers should be restricted. Specifically, no more than one level of dereferencing is allowed. Pointer dereference operations may not be hidden in macro definitions or inside typedef declarations. Function pointers are not permitted.
Let's look at the last point first.

double integral(double (*f)(double), double lower, double upper, int n) {
    // Compute the integral of f from lower to upper 
    // using Simpson's rule with n+1 points.
    double const h = (upper - lower) / n;
    double       s;
    double       t;
    int          i;
    
    s = 0.0;
    for (i = 0; i < n; i++) s += f((lower + h/2.0) + h*i);
    t = 0.0;
    for (i = 1; i < n; i++) t += f(lower + h*i);
    return (f(lower) + f(upper) + s*4.0 + t*2.0) * (h/6.0);
}
This kind of code has been important in numerical calculations since the very earliest days. Pascal could do it. Algol 60 could do it. In the 1950s, Fortran could do it. And NASA would ban it, because in C, f is a function pointer.

Now it's important to write functions like this once and only once. For example, the code has at least one error. The comment says n+1 points, but the function is actually evaluated at 2n+1 points. If we need to bound the number of calls to f in order to meet a deadline, having that number off by a factor of two will not help.

It's nice to have just one place to fix. Perhaps I should not have copied that code from a well-known source (:-). Certainly I should not have more than one copy!

What can we do if we're not allowed to use function pointers? Suppose there are four functions foo, bar, ugh, and zoo that we need to integrate. Now we can write

enum Fun {FOO, BAR, UGH, ZOO};

double call(enum Fun which, double what) {
    switch (which) {
        case FOO: return foo(what);
        case BAR: return bar(what);
        case UGH: return ugh(what);
        case ZOO: return zoo(what);
    }
}

double integral(enum Fun which, double lower, double upper, int n) {
    // Compute the integral of a function from lower to upper 
    // using Simpson's rule with n+1 points.
    double const h = (upper - lower) / n;
    double       s;
    double       t;
    int          i;
    
    s = 0.0;
    for (i = 0; i < n; i++) s += call(which, (lower + h/2.0) + h*i);
    t = 0.0;
    for (i = 1; i < n; i++) t += call(which, lower + h*i);
    return (call(which, lower) + call(which, upper) + s*4.0 + t*2.0) * (h/6.0);
}
Has obeying NASA's rule made the code more reliable? No, it has made the code harder to understand, less maintainable, and prone to a mistake that it wasn't before. Here's a call illustrating the mistake:

x = integral(4, 0.0, 1.0, 10);
I have checked this with two C compilers and a static checker at their highest settings, and they are completely silent about this.

So there are legitimate uses for function pointers, and simulating them makes programs worse, not better.

Now this wasn't an issue in Fortran, Algol 60, or Pascal. Those languages had procedure parameters but not procedure pointers. You could pass a subprogram name as a parameter, and such a parameter could be passed on, but you could not store them in variables. You could have a subset of C which allowed function pointer parameters, but made all function pointer variables read-only. That would give you a statically checkable subset of C that allowed integral().

The other use of function pointers is simulating object-orientation. Imagine for example

struct Channel {
    void (*send)(struct Channel *, Message const *);
    bool (*recv)(struct Channel *, Message *);
    ...
};
inline void send(struct Channel *c, Message const *m) {
    c->send(c, m);
}
inline bool recv(struct Channel *c, Message *m) {
    return c->recv(c, m);
}
This lets us use a common interface for sending and receiving messages on different kinds of channels. This approach has been used extensively in operating systems (at least as far back as the Burroughs MCP in the 1960s) to decouple the code that uses a device from the actual device driver. I would expect any program that controls more than one hardware device to do something like this. It's one of our key tools for controlling complexity.

Again, we can simulate this, but it makes adding a new kind of channel harder than it should be, and the code is worse when we do it, not better.

The rule against more than one level of dereferencing is also an assault on good programming. One of the key ideas that was developed in the 1960s is the idea of abstract data types; the idea that it should be possible for one module to define a data type and operations on it and another module to use instances of that data type and its operations without having to know anything about what the data type is.

One of the things I detest about Java is that it spits in the face of the people who worked out that idea. Yes, Java (now) has generic type parameters, and that's good, but you cannot use a specific type without knowing what that type is.

Suppose I have a module that offers operations

stash(item) → token
recall(token) → item
delete(token) → void
And suppose that I have two interfaces in mind. One of them uses integers as tokens.

// stasher.h, version 1.
typedef int token;
extern token stash(item);
extern item  recall(token);
extern void  delete(token);
Another uses pointers as tokens.

// stasher.h, version 2.
typedef struct Hidden *token;
extern  token stash(item);
extern  item  recall(token);
extern  void  delete(token);
Now let's have a client.

void snoo(token *ans, item x, item y) {
    if (better(x, y)) {
	*ans = stash(x);
    } else {
	*ans = stash(y);
    }
}
By the NASA rule, the function snoo() would not be accepted or rejected on its own merits. With stasher.h, version 1, it would be accepted. With stasher.h, version 2, it would be rejected.

One reason to prefer version 2 to version 1 is that version 2 gets more use out of type checking. There are ever so many ways to get an int in C. Ask yourself if it ever makes sense to do

token t1 = stash(x);
token t2 = stash(y);
delete(t1*t2);
I really do not like the idea of banning abstract data types.

10. All code must be compiled, from the first day of development, with all compiler warnings enabled at the compiler’s most pedantic setting. All code must compile with these setting without any warnings. All code must be checked daily with at least one, but preferably more than one, state-of-the-art static source code analyzer and should pass the analyses with zero warnings.
This one is good advice. Rule 9 is really about making your code worse in order to get more benefit from limited static checkers. (Since C has no standard way to construct new functions at run time, the set of functions that a particular function pointer could point to can be determined by a fixed-point data flow analysis, at least for most programs.) So is rule 1.