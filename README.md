# MiniCOBOL Compiler

A minimal COBOL compiler written in C++ that compiles COBOL source code to assembly language.

## Overview

MiniCOBOL is a lightweight compiler implementation that processes COBOL programs through multiple compilation stages:
- **Lexical Analysis** - Tokenizes COBOL source code
- **Parsing** - Builds an Abstract Syntax Tree (AST)
- **Semantic Analysis** - Validates program semantics
- **Code Generation** - Produces assembly output

## Project Structure

```
├── src/                    # Source code directory
│   ├── main.cpp           # Main entry point
│   ├── lexer.cpp/h        # Lexical analyzer
│   ├── parser.cpp/h       # Parser implementation
│   ├── semantic_analyzer.cpp/h  # Semantic analysis
│   ├── code_generator.cpp/h     # Assembly code generator
│   ├── ast.h              # Abstract Syntax Tree definitions
│   ├── token.h            # Token definitions
│   ├── pic_descriptor.cpp/h     # PIC descriptor handling
│   └── CMakeLists.txt     # Build configuration
├── test_cases/            # Test files
│   ├── valid/             # Valid COBOL programs
│   └── error/             # Error test cases
└── README.md              # This file
```

## Requirements

- C++17 compatible compiler (GCC, Clang, or MSVC)
- CMake 3.10 or higher

## Building

### Using CMake

```bash
mkdir build
cd build
cmake ../src
make
```

This will create the `minicobol` executable in the build directory.

### Direct Compilation

Alternatively, compile directly with g++:

```bash
g++ -std=c++17 -Wall -Wextra -O3 -o minicobol src/*.cpp
```

## Usage

```bash
./minicobol <cobol-file> [output-file]
```

- `<cobol-file>`: Path to the COBOL source file (`.cob`)
- `[output-file]`: Optional output assembly file (default: `output.asm`)

### Examples

```bash
# Compile a COBOL file with default output
./minicobol test_cases/valid/arith_compute_expr.cob

# Specify custom output file
./minicobol test_cases/valid/arith_compute_expr.cob output.asm
```

## Features

- COBOL lexical analysis
- Recursive descent parsing
- Semantic validation
- PIC clause support for data definitions
- Arithmetic expression evaluation
- Assembly code generation

## Testing

Test cases are provided in the `test_cases/` directory:

- **Valid programs** (`test_cases/valid/`): COBOL programs that should compile successfully
  - Arithmetic expressions
  - Decimal precision tests
  - Mixed operations
  - Signed arithmetic

- **Error cases** (`test_cases/error/`): Programs designed to trigger compilation errors
  - Division by zero
  - Overflow conditions
  - Size mismatches
  - Syntax errors

## License

This project is provided as-is for educational purposes.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.
