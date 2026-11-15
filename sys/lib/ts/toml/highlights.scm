; Pair keys stand out as keywords.
(pair
  (bare_key) @keyword)

(pair
  (quoted_key) @keyword)

(pair
  (dotted_key
    (bare_key) @keyword))

; Table and array-of-table header keys only (not the whole block).
(table
  (bare_key) @type)

(table
  (dotted_key
    (bare_key) @type))

(table_array_element
  (bare_key) @type)

(table_array_element
  (dotted_key
    (bare_key) @type))

(boolean) @keyword

(comment) @comment

(string) @string

[
  (integer)
  (float)
] @number

[
  (offset_date_time)
  (local_date_time)
  (local_date)
  (local_time)
] @string

"=" @operator

[
  "."
  ","
] @punctuation.delimiter

[
  "["
  "]"
  "[["
  "]]"
  "{"
  "}"
] @punctuation.bracket
