; Comments

(comment) @comment
(block_comment) @comment

; Strings

[
  (string)
  (character_literal)
  (interpolated_string_expression)
] @string

; Numbers

(integer_literal) @number
(floating_point_literal) @number

; Literals treated as keywords

(boolean_literal) @keyword
(null_literal) @keyword

; Types

(type_identifier) @type

(class_definition
  name: (identifier) @type)

(object_definition
  name: (identifier) @type)

(trait_definition
  name: (identifier) @type)

(enum_definition
  name: (identifier) @type)

(full_enum_case
  name: (identifier) @type)

(simple_enum_case
  name: (identifier) @type)

(type_definition
  name: (type_identifier) @type)

; Capital-letter identifiers are types by convention in Scala

((identifier) @type
 (#match? @type "^[A-Z]"))

; Functions and methods

(function_definition
  name: (identifier) @function)

(function_declaration
  name: (identifier) @function.method)

(call_expression
  function: (identifier) @function.call)

(call_expression
  function: (field_expression
    field: (identifier) @function.method))

(generic_function
  function: (identifier) @function.call)

; Operators

(infix_expression
  operator: (identifier) @operator)

(infix_expression
  operator: (operator_identifier) @operator)

(infix_type
  operator: (operator_identifier) @operator)

(operator_identifier) @operator

; Keywords — all keyword-like captures collapsed to @keyword

[
  "abstract"
  "case"
  "catch"
  "class"
  "def"
  "derives"
  "do"
  "else"
  "end"
  "enum"
  "export"
  "extends"
  "extension"
  "final"
  "finally"
  "for"
  "given"
  "if"
  "implicit"
  "import"
  "lazy"
  "match"
  "new"
  "object"
  "opaque"
  "open"
  "override"
  "package"
  "private"
  "protected"
  "return"
  "sealed"
  "then"
  "throw"
  "trait"
  "transparent"
  "try"
  "type"
  "using"
  "val"
  "var"
  "while"
  "with"
  "yield"
  "=>"
  "<-"
  "@"
] @keyword
