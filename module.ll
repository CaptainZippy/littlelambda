(module foo
    (define (area x y) (* x y))
    (define (perimeter x y) (* 2 (+ x y))))

(print "Area=" (foo.area 10 20) "\n")
