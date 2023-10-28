
($import test)

($define secret "supersecretkey")

(test.$suite "Basics"
    (test.$case
        (print " INCASE ")
        ($define w 10)
        ($define h 12)
        ($define d 3)
        (* w (* h d))
        )
)
