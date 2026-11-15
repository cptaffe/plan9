; Comments

[
  (line_comment)
  (block_comment)
] @comment

; Strings

[
  (string_literal)
  (character_literal)
] @string

(escape_sequence) @string

; Numbers

[
  (decimal_integer_literal)
  (hex_integer_literal)
  (octal_integer_literal)
  (binary_integer_literal)
  (decimal_floating_point_literal)
  (hex_floating_point_literal)
] @number

; Literals treated as keywords

[
  (true)
  (false)
  (null_literal)
] @keyword

; Types

(type_identifier) @type

[
  (boolean_type)
  (integral_type)
  (floating_point_type)
  (void_type)
] @type

(class_declaration
  name: (identifier) @type)

(interface_declaration
  name: (identifier) @type)

(enum_declaration
  name: (identifier) @type)

(record_declaration
  name: (identifier) @type)

(constructor_declaration
  name: (identifier) @type)

((field_access
  object: (identifier) @type)
 (#match? @type "^[A-Z]"))

((scoped_identifier
  scope: (identifier) @type)
 (#match? @type "^[A-Z]"))

((method_invocation
  object: (identifier) @type)
 (#match? @type "^[A-Z]"))

; Methods

(method_declaration
  name: (identifier) @function.method)

(method_invocation
  name: (identifier) @function.method)

(super) @function.builtin

; Keywords

[
  "abstract"
  "assert"
  "break"
  "case"
  "catch"
  "class"
  "continue"
  "default"
  "do"
  "else"
  "enum"
  "exports"
  "extends"
  "final"
  "finally"
  "for"
  "if"
  "implements"
  "import"
  "instanceof"
  "interface"
  "module"
  "native"
  "new"
  "non-sealed"
  "open"
  "opens"
  "package"
  "permits"
  "private"
  "protected"
  "provides"
  "public"
  "record"
  "requires"
  "return"
  "sealed"
  "static"
  "strictfp"
  "switch"
  "synchronized"
  "throw"
  "throws"
  "to"
  "transient"
  "transitive"
  "try"
  "uses"
  "volatile"
  "when"
  "while"
  "with"
  "yield"
] @keyword
