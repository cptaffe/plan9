; Object keys stand out as keywords.
(pair
  key: (string) @keyword)

(string) @string

(number) @number

; JSON literals; map to keyword so true/false/null are highlighted.
[
  (null)
  (true)
  (false)
] @keyword

(escape_sequence) @operator

(comment) @comment

[
  "["
  "]"
  "{"
  "}"
] @punctuation.bracket

[
  ","
  ":"
] @punctuation.delimiter
