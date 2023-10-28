;; (print null "\n")
;; ($define n2 null)
;; (print n2 "\n")
;; ($define r 10)
;; ($define r_a ($let (a 11
;;                   x 10
;;                   r 100)
;;                   (* 1.19 (* r r))))
;; ($define r_b (* 1.19 (* r r)))
;; (print "a=" r_a " b=" r_b "\n")

($define ($suite . args) env
    (begin
        (print "SUITE" env "\n")
        (print (eval 'secret env) "\n")
        ($define ($wat1) e (+ 100 a))
        ($define ($wat2) e '(+ 100 a))
        ($define a 13)
        ($define (define_a v) (begin ($define a v) (getenv)))
        (print "In current env " (eval ($wat1) env) "\n")
        (print "In modfied env1 "(eval '(+ 100 a) (define_a 100)) "\n")
        (print "In modfied env2 "(eval ($wat2) (define_a 100)) "\n")
        ))

($define ($case . args) env
    (print "CASE"))
