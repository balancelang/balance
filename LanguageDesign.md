- Language
    - No semi-colons to terminate - but optional to allow multiple statements in one line
    - Use curly brackets for control-flow (functions, if-else, lambda etc.)
    - Allow top-level definitions, but not run code?
        e.g. allow:
            const var A = 123
        but not:
            var a = myFunc();
    - favor x.y().z().q() over q(z(y(x)))
    - variable def: 'var a = 2' or 'Int a = 2'. Favor this over 'var a: Int = 2' since this requires both 'var', 'a', and 'Int'.
- Features
    - If statements (+)
    - Function definitions (+)
    - while loops (+)
    - for loops
    - Pattern matching
    - checked exceptions
    - "compiled" docstrings, e.g. warnings when wrong
    - lambda expressions with body (+)
    - destructuring
    - attributes [Route(xxx)] - both above class, function, property, in front of parameter?
    - Java-like "throws" keyword
    - Classes (+)
    - interfaces
    - builtin serialization (JSON, XML, YAML)
    - all strings can be multiline? (+)
    - all strings be f-strings?  var a = "Test: {y}"
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
    - updater built in, e.g. "balance --update" to upgrade to latest version
    - set version, e.g. "balance --set-version 3.5"
    - create project
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

- Compilation stages
    - E.g. create a visitor that first gathers all type symbols (not evaluating them yet)
    - Then another visitor for creating all functions?
    - Then one visiting the rest, assuming all functions and type-symbols are known?
