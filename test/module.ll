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

;; ($define ($module name . args) env body)


;; ($module foo
;;  ($define (area x x) (* x y)))

;; ($define (func a) (begin
;;     ($if (lt a 0) error .)
;;     (print 1)
;;     (print 2)
;; )