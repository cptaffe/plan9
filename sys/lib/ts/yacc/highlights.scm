(comment) @comment

; %token, %type, %start, %left, %right, %nonassoc, etc.
(declaration_name) @keyword

; Section delimiters and prologue delimiters
"%%" @keyword
"%{" @keyword
"%}" @keyword

; LHS of a grammar rule
(grammar_rule_declaration) @function

; Literals
(string_literal) @string
(char_literal) @string
(number_literal) @number

; <typename> type tags
(type_tag) @type

; Punctuation
":" @operator
"|" @operator
";" @operator
