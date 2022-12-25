# TODOs:
- test duplicate class/interface/variable names
- test duplicate class/interface/variable names on import
- make sure you can't 'new' up interfaces, builtins etc?

## interfaces
- test reusing interface value value in function call that takes same interface value
- test type checking with wrong interface type
- test array of interfaces

## classes
- define None/null - how do we handle that?
- shorthand - test types not matching
- shorthand - test variable names not matching

## Lambdas
- check reassignment with correct type
- check reassignment with incorrect type

## Builtins
- Some are imported by default (e.g. print, open, String, Int), others should be explicitly imported (and user-classes can therefore use these names)

## Imports
- Should be covered by test-suite
- naming convention