($module foo
    ($define (area x y) (* x y))
    ($define (perimeter x y) (* 2 (+ x y)))
    ($module bar
        ($define (thrice x) (* 3 x)))
)

(print "Area=" (foo.area 10 20) "\n")
(print "Thrice=" (foo.bar.thrice 11) "\n")

($import math)
(print "PI=" math.pi "\n")
