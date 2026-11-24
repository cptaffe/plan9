(identifier) @variable

((identifier) @constant
 (#match? @constant "^[A-Z][A-Z\\d_]*$"))

"break" @keyword
"case" @keyword
"const" @keyword
"continue" @keyword
"default" @keyword
"do" @keyword
"else" @keyword
"enum" @keyword
"extern" @keyword
"for" @keyword
"if" @keyword
"inline" @keyword
"return" @keyword
"sizeof" @keyword
"static" @keyword
"struct" @keyword
"switch" @keyword
"typedef" @keyword
"union" @keyword
"volatile" @keyword
"while" @keyword

"#define" @keyword
"#elif" @keyword
"#else" @keyword
"#endif" @keyword
"#if" @keyword
"#ifdef" @keyword
"#ifndef" @keyword
"#include" @keyword
(preproc_directive) @keyword

"--" @operator
"-" @operator
"-=" @operator
"->" @operator
"=" @operator
"!=" @operator
"*" @operator
"&" @operator
"&&" @operator
"+" @operator
"++" @operator
"+=" @operator
"<" @operator
"==" @operator
">" @operator
"||" @operator

"." @delimiter
";" @delimiter

(string_literal) @string
(system_lib_string) @string

(null) @constant
(number_literal) @number
(char_literal) @number

(field_identifier) @property
(statement_identifier) @label
(type_identifier) @type
(primitive_type) @type
(sized_type_specifier) @type

(call_expression
  function: (identifier) @function)
(call_expression
  function: (field_expression
    field: (field_identifier) @function))
(function_declarator
  declarator: (identifier) @function)
(preproc_function_def
  name: (identifier) @function.special)

(comment) @comment

[
  "!"
  "%"
  "%="
  "&="
  "*="
  "/"
  "/="
  "<<"
  "<<="
  "<="
  ">="
  ">>"
  ">>="
  "^"
  "^="
  "|"
  "|="
  "~"
  "?"
  "..."
] @operator
(conditional_expression ":" @operator)

[
  "#elifdef"
  "#elifndef"
  "defined"
  "auto"
  "goto"
  "register"
  "restrict"
  "_Atomic"
  "_Noreturn"
  "noreturn"
  "_Alignas"
  "alignas"
  "_Alignof"
  "alignof"
  "offsetof"
  "_Generic"
  "asm"
  "__asm__"
  "__attribute__"
  "thread_local"
  "constexpr"
] @keyword

(preproc_def name: (identifier) @constant.macro)
(preproc_ifdef name: (identifier) @constant.macro)
(preproc_defined (identifier) @constant.macro)
(preproc_params (identifier) @variable.parameter)
(parameter_declaration declarator: (identifier) @variable.parameter)
(parameter_declaration declarator: (pointer_declarator declarator: (identifier) @variable.parameter))
