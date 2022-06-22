#### File A

from fileB import someFunc

class MyClass {
    Int a
}

var cls = new MyClass()

print("Hello from fileA")
someFunc(cls)



#### File B

from filaA import MyClass

def someFunc(MyClass element) -> None {
    print("Hello from fileB.bl")
}