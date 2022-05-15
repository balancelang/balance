# balance
The balance programming language

```
# File: fib.bl
def fib(int n) -> int {
    if (n <= 1) {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}

print(fib(34))
# Outputs: 
```