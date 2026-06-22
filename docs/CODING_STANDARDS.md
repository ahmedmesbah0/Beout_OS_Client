# Beout_OS Security Appliance - Coding Standards

## 1. C++ Standards
- Use modern C++20 features.
- Avoid raw pointers. Prefer `std::unique_ptr` and `std::shared_ptr` (RAII).
- Use `const` variables and `const` methods wherever possible (const correctness).
- Avoid global mutable state.
- Adhere to SOLID principles.
- Use strong typing instead of primitive types where it adds clarity.
- Utilize Dependency Injection for testability.

## 2. Naming Conventions
- Namespaces: `snake_case` (e.g., `beout_os::config_engine`)
- Classes/Structs: `PascalCase` (e.g., `ConfigParser`)
- Functions/Methods: `snake_case` (e.g., `parse_file()`)
- Variables: `snake_case` (e.g., `config_path`)
- Private members: `snake_case_` (with trailing underscore)
- Constants/Enums: `UPPER_SNAKE_CASE` (e.g., `MAX_RETRIES`)

## 3. Formatting
- Indentation: 4 spaces (no tabs).
- Max line length: 100 characters.
- Braces: Attach to the control statement (e.g., `if (...) {`)
- Use `clang-format` to automatically enforce formatting.

## 4. Documentation
- Write Doxygen-style comments for public interfaces (classes, methods).
- Add high-level descriptions at the top of each header file.
- Explain "why", not "what" in inline comments.

## 5. Error Handling
- Use exceptions for exceptional situations (e.g., allocation failure, system errors).
- For expected control flow errors (e.g., invalid user input, network timeout), use `std::expected` or `Result` types.
- Always check return values and handle errors explicitly.
