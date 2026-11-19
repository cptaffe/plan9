BEGIN {
	print "#include <u.h>"
	print "#include <libc.h>"
	print "#include <md.h>"
	print "#include \"impl.h\""
	print ""
	ne = 0
	print "Mdentity _mdent[] = {"
}
/"&[A-Za-z0-9]+;"/ {
	name = $0
	sub(/^ *"&/, "", name)
	sub(/;".*/, "", name)
	cp = $0
	sub(/.*"codepoints": \[/, "", cp)
	sub(/\].*/, "", cp)
	n = split(cp, v, ", *")
	printf "\t\"%s\", 0x%X, 0x%X,\n", name, v[1], (n > 1 ? v[2] : 0)
	ne++
}
END {
	print "};"
	printf "int _mdnent = %d;\n", ne
}
