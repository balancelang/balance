# TODOs:
- test duplicate class name
- test duplicate parameter name in same function signature
- test duplicate class/interface/variable names
- test duplicate class/interface/variable names on import
- make sure you can't 'new' up interfaces, builtins etc?

## interfaces
- test duplicate interface name
- test missing function implementation
- test wrong return type
- test wrong parameters
- test class doesnt implement interface
- test interface as property type
- test interface as function parameter
- test reusing interface value value in function call that takes same interface value
- test type checking with wrong interface type
- test returning interface?

## classes
- test class as property
    - get
    - set
- test nested class property
    - e.g. a.b.c.d
- define None/null - how do we handle that?

## Builtins
- Some are imported by default (e.g. print, open, String, Int), others should be explicitly imported (and user-classes can therefore use these names)

## Imports
- Should be covered by test-suite
- naming convention