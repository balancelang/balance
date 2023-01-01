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
- test generic property
- test not providing generic types for class which requires a generic type, e.g. Array<Int> used as Array
- test arrays/generic types as class properties
- test shorthand: non-string as key

## Inheritance
- throw error if inherited property already exists in ancestor
- test overriding function (same parameters/return type)

## Lambdas
- check reassignment with correct type
- check reassignment with incorrect type

## Builtins
- Some are imported by default (e.g. print, open, String, Int), others should be explicitly imported (and user-classes can therefore use these names)

## Imports
- Should be covered by test-suite
- naming convention

## Types
- isinstance(x, Foo) is then translated to x.typeId == Foo.typeId
- handle clash between user choosing e.g. 'typeId' as class property name, when it also exists in Any
- Handle optionally providing type for class properties, lambdas, etc.

## Simple types
- Whenever e.g. an Int is referenced as Any (or other non-simple types), box it as a struct version
    - this way, when a function e.g. accepts x: Any, we can always query x for its typeId (int32 won't have a getType() function)
    - Int can manually define a getType which statically returns Type == Int always.