function num(x) {
	if(x < 1e-12 && x > -1e-12)
		x = 0
	return sprintf("%.12g", x)
}
function at(i, x, y) {
	px[i] = x; py[i] = y
	if(n++ == 0 || x < lx) lx = x
	if(n == 1 || x > hx) hx = x
	if(n == 1 || y < ly) ly = y
	if(n == 1 || y > hy) hy = y
}
function turn(x, y) {
	tx = x*c - y*s; ty = x*s + y*c
}
BEGIN {
	r = a*atan2(0, -1)/180; c = cos(r); s = sin(r)
}
{ line[NR] = $0 }
/^[A-Za-z][A-Za-z0-9]*: \(/ {
	v = $0
	sub(/^[^(]*\(/, "", v)
	sub(/\).*$/, "", v)
	split(v, xy, ",")
	kind[NR] = "p"
	at(NR, xy[1]+0, xy[2]+0)
}
$1 == "#" && $2 == "sk" && $3 == "text" {
	kind[NR] = "t"
	at(NR, $5+0, $6+0)
}
$1 == "#" && $2 == "sk" && ($3 == "scalar" || $3 == "W" || $3 == "Y") { kind[NR] = "k" }
/^[A-Za-z][A-Za-z0-9]*: arc cw / { kind[NR] = "a" }
/^[A-Za-z][A-Za-z0-9]*: arc from / { kind[NR] = "a" }
END {
	cx = (lx+hx)/2; cy = (ly+hy)/2
	for(i = 1; i <= NR; i++){
		l = line[i]
		if(!(i in kind)){
			print l
			continue
		}
		if(kind[i] == "a"){
			if(op == "scale" && k < 0){
				if(l ~ / arc cw /)
					sub(/ arc cw /, " arc ", l)
				else
					sub(/ arc /, " arc cw ", l)
			}
			print l
			continue
		}
		if(kind[i] == "k"){
			if(op == "scale"){
				nf = split(l, f, " ")
				f[nf] = num(f[nf]*(k < 0 ? -k : k))
				l = f[1]
				for(j = 2; j <= nf; j++)
					l = l " " f[j]
			}
			print l
			continue
		}
		if(op == "rot"){
			turn(px[i]-cx, py[i]-cy)
			x = cx + tx; y = cy + ty
		}else{
			x = cx + (px[i]-cx)*k; y = cy + (py[i]-cy)*k
		}
		if(kind[i] == "p"){
			sub(/\(.*$/, "", l)
			print l "(" num(x) "," num(y) ")"
			continue
		}
		split(l, f, " ")
		dx = f[7]+0; dy = f[8]+0
		if(op == "rot"){
			turn(dx, dy)
			dx = tx; dy = ty
		}else{
			dx *= k; dy *= k
		}
		rest = l
		sub(/^#[ \t]+sk[ \t]+text[ \t]+[^ \t]+[ \t]+[^ \t]+[ \t]+[^ \t]+[ \t]+[^ \t]+[ \t]+[^ \t]+/, "", rest)
		print "# sk text " f[4] " " num(x) " " num(y) " " num(dx) " " num(dy) rest
	}
}
