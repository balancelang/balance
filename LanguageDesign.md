- Language
    - No semi-colons to terminate - but optional to allow multiple statements in one line
    - Use curly brackets for control-flow (functions, if-else, lambda etc.)
    - Allow top-level definitions, but not run code?
        e.g. allow:
            const var A = 123
        but not:
            var a = myFunc();
    - favor x.y().z().q() over q(z(y(x)))
    - use "from X import Y", rather than "import Y from X", since intellisense can't guess Y before knowing X.
    - variable def: 'var a = 2' or 'Int a = 2'. Favor this over 'var a: Int = 2' since this requires both 'var', 'a', and 'Int'.
- Features
    + If statements
    + Function definitions
    + while loops
    + for loops
        - break
        - continue
        - funky: Should for-loops be expressions?
            - var f = for (...) {

            }
            - then you can reference f and e.g. break out of it?
    - Pattern matching
        - strict checking all types with enums, unions, etc.
    + comments
    - "compiled" docstrings, e.g. warnings when wrong
    + lambda expressions with body
    - destructuring
        - var a, b, c  = getObj() , where obj has properties a,b,c
    - attributes [Route(xxx)] - both above class, function, property, in front of parameter?
    - Java-like "throws" keyword - checked exceptions
    + Classes
    + interfaces
    - abstract methods?
    - builtin serialization (JSON, XML, YAML)
    + all strings can be multiline?
    - all strings be f-strings?  var a = "Test: {y}"
    - extension methods on types
    + forward references - e.g. extend a class defined later in the same file
    - Not compile classes/functions not imported?
    + Cyclic dependencies
    - Garbage collection
    - Sockets
    + File handles
    + Range syntax
        + builtin class, with overloads: var r = new Range(...)
        + builtin function, with overloads: range(...)
        + alternative syntax '0 to 10' (equivalent of range(0, 10))
    - overloading
        + constructor overloading
        - method overloading
        + function overloading
    - Only explicit imports, e.g. no transitive imports
    - default parameter values, e.g. func(Int a = 5)
    + constructors
    - tuples / anonymous class syntax?
        - E.g. x: <Int, Int>
    + support dictionary initializer expression, e.g.
        class A {
            a: Int
            b: Int
        }
        var a: A = { a: 123, b: 456 }
    - async / await
        - await single syntax:
            var b = await ...
        - await multiple syntax:
            await {
                var q = ...
                var b = ...
            }
            ^-- awaits all tasks and continues when all are done, leaving q and b with the results
            the {} block can only contain awaitable expressions
            Nested awaits?
    - built-in support for running OS commands
        os {
            lsOut, lsErr, lsRc = ls -l
            echo lsOut > test.txt
        }
    - operator overloading?
    - Generics
        - class X<T<Q>> {} ?
    - assignment as side-effect?
        if (var a = a < 5) { ...            (sets var a = true/false inside the if-block)
        print(var a = x.getString())        equivalent to var a = x.getString(); print(a)
    - constrained values?
        - E.g. Regex strings, only ints between 0..10

- What if main didn't take "string[] args", but something smarter, more CLI appropriate?
- Wont-do
    - traits
    - indentation based
- How do we make documentation available, e.g. that "print" function exists
- Language design document or repository
- Language website
- Language documentation
- Builtin types
    - Int (+), Float, Double (+), Bool (+), Any, None (+), Array (+), List, Dict, Optional
- CLI
    - package.json determines version
    - updater built in, e.g. "balance --update" to upgrade to latest version
    - create project "balance new"
        - eventually wizard options for use-cases
- Formatter

- Testing

- Code coverage

- Profiling

- Debugger
- vscode extension
    - run with profiling, show how much a function adds to runtime/memory
    - when inspecting builtin functions, add a helper-pane that documents usage
        - also with examples of for-loops, if statements etc.

- Package manager
    - All packages compiled in cloud-service, which lazily builds requested architectures?
    - Code-coverage requirements?