(define r 10)
(define r_a (let (a 11
                  x 10
                  r 100)
                  (* pi (* r r))))
(define r_b (* pi (* r r)))
(print "a=" r_a " b=" r_b "\n")
