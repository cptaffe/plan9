; Strong emphasis (**text** or __text__) — whole span including delimiters.
(strong_emphasis) @markup.bold

; Emphasis (*text* or _text_) — whole span including delimiters.
(emphasis) @markup.italic

; Inline code spans — the (code_span) parent capture wins at byte 0 before
; the child (code_span_delimiter) fires, so the whole span (backticks included)
; is one @markup.raw.inline run.  That is the desired behaviour: the span reads
; as a single visual unit, not delimiter-content-delimiter.
(code_span) @markup.raw.inline

; Links — only destination and title within a complete link construct.
; (link_text) alone is omitted: bare [word] is too common in technical prose.
(link_destination) @markup.link.url
(link_title)       @markup.link.url

; Reference labels ([text][label]) and image alt text.
(link_label)        @markup.link.label
(image_description) @markup.link.label

; Autolinks <uri> and <email@host> — tightly constrained, low false-positive rate.
(uri_autolink)   @markup.link.url
(email_autolink) @markup.link.url

; GFM strikethrough (~~text~~).
(strikethrough) @markup.strikethrough

; Backslash escape sequences (\[ \* etc.) — @string.escape falls back to string
; via dot-strip unless overridden; token_names.txt maps it to operator.
(backslash_escape) @string.escape
(hard_line_break)  @string.escape
