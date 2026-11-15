; ATX headings — capture the WHOLE heading node (marker + text) per level so
; the heading text is part of the styled span, not just the ## sigil.
(atx_heading (atx_h1_marker)) @markup.heading.1
(atx_heading (atx_h2_marker)) @markup.heading.2
(atx_heading (atx_h3_marker)) @markup.heading.3
(atx_heading (atx_h4_marker)) @markup.heading.4
(atx_heading (atx_h5_marker)) @markup.heading.5
(atx_heading (atx_h6_marker)) @markup.heading.6

; Setext headings — underline node determines level.
(setext_heading (setext_h1_underline)) @markup.heading.1
(setext_heading (setext_h2_underline)) @markup.heading.2

; Untagged fenced code block — no info_string between the opening delimiter
; and the content.  Style the whole block, delimiters included, as one raw
; run.  Must precede the delimiter pattern below (first capture wins).
; A (block_continuation) usually sits between the two.
(fenced_code_block
  (fenced_code_block_delimiter)
  .
  (block_continuation)
  .
  (code_fence_content)) @markup.raw.block
(fenced_code_block
  (fenced_code_block_delimiter)
  .
  (code_fence_content)) @markup.raw.block

; Tagged fenced code block.  The content is left uncaptured so that
; markdown.injections.scm can highlight it with the tagged language.
(fenced_code_block_delimiter) @markup.raw.delimiter
(info_string) @attribute

; Indented code block — no language tag; style as raw block.
(indented_code_block) @markup.raw.block

; Block-quote marker (>).
(block_quote_marker) @markup.quote

; Thematic break (---, ***, ___).
(thematic_break) @punctuation.special

; List markers (ordered and unordered).
[
  (list_marker_minus)
  (list_marker_plus)
  (list_marker_star)
  (list_marker_dot)
  (list_marker_parenthesis)
] @markup.list

; Task-list checkboxes — fall back to @markup.list via dot-strip if no
; separate palette entry is defined.
(task_list_marker_checked)   @markup.list.checked
(task_list_marker_unchecked) @markup.list.unchecked

; Link reference definitions.
(link_label)       @markup.link.label
(link_destination) @markup.link.url
(link_title)       @markup.link.url

; Raw HTML block.
(html_block) @markup.html

; Pipe tables: header cells read as headings; pipes and the delimiter row
; are structure.
(pipe_table_header (pipe_table_cell) @markup.heading)
(pipe_table_header "|" @markup.table)
(pipe_table_row "|" @markup.table)
(pipe_table_delimiter_row) @markup.table

; Backslash escapes outside inline content (e.g. in link destinations).
(backslash_escape) @string.escape
