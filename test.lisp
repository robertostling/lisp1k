(define true    (= 0 0))
(define false   (= 0 1))

(define not     (lambda (x) (if x false true)))
(define and     (lambda (x y) (if x (if y true false) false)))
(define or      (lambda (x y) (if x true (if y true false))))

(define /=      (lambda (x y) (not (= x y))))
(define >       (lambda (x y) (< y x)))
(define >=      (lambda (x y) (not (< x y))))
(define <=      (lambda (x y) (not (< y x))))

(define -       (lambda (x y) (+ x (neg y))))

(define range
  (lambda (x y)
    (if (= x y)
      (cons x ())
      (cons x (range (+ x 1) y)))))

(define map
  (lambda (f xs)
    (if (= xs ())
      ()
      (cons (f (head xs)) (map f (tail xs))))))

(define !
  (lambda (x)
    (if (= x 1)
      1
      (* x (! (- x 1))))))

(print "Please have some factorials.")
(print (map ! (range 1 13)))

(print "LISP saps that 13 + 5! is:")
(print (+ 13 (! 5)))

"FORTH says that 13 + 5! is:" print
13 (! 5) + print

