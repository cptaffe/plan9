function hex(s,  i, n, c) {
	n = 0
	for(i = 1; i <= length(s); i++){
		c = index("0123456789ABCDEF", toupper(substr(s, i, 1))) - 1
		n = n*16 + c
	}
	return n
}

BEGIN {
	FS = ";"
	np = 0
	ncf = 0
	print "#include <u.h>"
	print "#include <libc.h>"
	print "#include <md.h>"
	print "#include \"impl.h\""
	print ""
}
FILENAME ~ /UnicodeData/ {
	c = hex($1)
	if(c < 128)
		next
	p = $3 ~ /^[PS]/
	if($2 ~ /, First>$/){
		first = c
		next
	}
	if($2 ~ /, Last>$/)
		lo = first
	else
		lo = c
	if(p){
		if(np > 0 && plo[np-1] <= lo && phi[np-1] + 1 >= lo)
			phi[np-1] = c
		else{
			plo[np] = lo
			phi[np] = c
			np++
		}
	}
	next
}
FILENAME ~ /CaseFolding/ {
	if($0 ~ /^#/ || NF < 3)
		next
	s = $2
	gsub(/ /, "", s)
	if(s != "C" && s != "F")
		next
	t = $3
	sub(/^ */, "", t)
	sub(/ *$/, "", t)
	nt = split(t, tv, " ")
	line = sprintf("\t0x%s,", $1)
	for(i = 1; i <= 3; i++)
		line = line sprintf(" 0x%s,", i <= nt ? tv[i] : "0")
	cf[ncf++] = line
	next
}
END {
	print "Rune _mdpunct[][2] = {"
	for(i = 0; i < np; i++)
		printf "\t0x%X, 0x%X,\n", plo[i], phi[i]
	print "};"
	printf "int _mdnpunct = %d;\n\n", np
	print "Rune _mdfold[][4] = {"
	for(i = 0; i < ncf; i++)
		print cf[i]
	print "};"
	printf "int _mdnfold = %d;\n", ncf
}
