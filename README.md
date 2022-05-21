# Balance
[![Build](https://github.com/balancelang/balance/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/balancelang/balance/actions/workflows/build.yml)
## The Balance programming language

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

## Roadmap for v1.0.0
| Status | Milestone | ETA |
| :---: | :--- | :---: |
| 🚀 | **[Define syntax](#define-syntax)**  | 2022-08-01 |
| 🚀 | **[Implement syntax](#implement-syntax)**  | TBD |
| 🚀 | **[Define standard library](#define-standard-library)** | TBD |
| 🚀 | **[Implement standard library](#implement-standard-library)** | TBD |
| 🚀 | **[Define CLI commands](#define-cli-commands)** | TBD |
| 🚀 | **[Implement CLI commands](#implement-cli-commands)** | TBD |
| 🚀 | **[Define package framework](#define-package-framework)** | TBD |
| 🚀 | **[Installer](#installer)** | TBD |
| 🚀 | **[Miscellaneous](#miscellaneous)** | TBD |


### Define syntax
* Create location for storing syntax and versions, e.g. another repository
* Describe process of updating syntax, e.g. through user-raised issues
* Generate specification in some format (document, website, etc.)

### Implement syntax
* Implement syntax as described in specification

### Define standard library
* Describe all modules, functions, types
* Describe how/where standard library API documentation is hosted
* Create API documentation and automatic generation
* Describe how examples/tutorials are associated

### Implement standard library
* Implement standard library as described by API
* Testing
* Examples/tutorials

### Define CLI commands
> There will only be 1 self-updatable executable, which can target multiple Balance versions - downloaded as needed
* Select tools for managing CLI commands, arguments, help messages
* Create documentation

### Implement CLI commands
* Implement CLI commands as described by documentation
* Testing
* Examples/tutorials

### Define package framework
* Describe how packages are published
* Describe how packages are installed/included
* Describe handling portability
* Describe how/where packages are hosted
* Tutorial on how to publish a package
* Investigate if hosting can be provided for free from sponsorships

### Implement package framework
* Implement package framework as described

### Installer
* Create linux installer
* Create Windows installer
* Create MacOS installer
* Describe others

### Miscellaneous
#### Formatting
* Implement formatting
#### Implement vscode extension
* IDE extension: vscode
* Language server?
* Debugging capabilities
#### Test framework
* Implement test framework
####
