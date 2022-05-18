# balance
The balance programming language

Example of syntax:
```
def fib(Int n) -> Int {
    if (n <= 1) {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}

print(fib(35))
```
