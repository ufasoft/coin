#|#########       Copyright (c) 1997-2013 Ufasoft   http://ufasoft.com   mailto:support@ufasoft.com, Sergey Pavlov mailto:dev@ufasoft.com      #################################################################################################
#                                                                                                                                                                                                                                              #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3, or (at your option) any later version.             #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.     #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                        #
#############################################################################################################################################################################################################################################=|#

(require 'sha2 "sha-2")


(defvar *evergreen* nil)
(defvar *vector* nil)
(defvar *bfi-int* nil)
(defvar result)
(defvar *nonce-add*)


(defun gen-register-pool (prefix n vector &optional (i 0))
  (when (< i n)
    (if vector
      (list* (intern (format nil "~A~A" prefix i))
           (gen-register-pool prefix n vector (1+ i)))
      (list* (intern (format nil "~A~A.x" prefix i))
             (intern (format nil "~A~A.y" prefix i))
             (intern (format nil "~A~A.z" prefix i))
             (intern (format nil "~A~A.w" prefix i))
             (gen-register-pool prefix n vector (1+ i))))))

(defvar *register-pool*)

(defun alloc-register ()
  (unless *register-pool*
    (error "No free register in the *register-pool*"))
  (pop *register-pool*))

(defun free-register (reg)
;  (format *trace-output* "~A~%" reg)
  (when (member reg *register-pool*)
    (error (format nil "Regiser ~A is already free" reg)))
  (push reg *register-pool*))



(defvar *i*)
(defvar *last-i*)



(defmacro assign (var val)
  `(progn (if (and ,var (symbolp ,var))
            (free-register ,var))
          (setq ,var ,val)))

(defmacro ren (var val)
	`(rotatef ,var ,val))


(defvar *literals*)
(defvar *litcount*)

(defconstant *xyzw* #("x" "y" "z" "w"))

(defun val (v)
  (cond ((numberp v) (let ((lit (gethash v *literals*)))
                       (unless lit
                         (setf (gethash v *literals*) (setq lit (incf *litcount*))))
                       (format nil "l~A.~A" (truncate lit 4) (svref *xyzw* (rem lit 4)))))
        (t v)))



(defun alu-op (op &rest args)
  (let* ((dst (car args))
         (dv (symbol-value dst)))
    (unless (eq dst 'result)
      (if (and dv (symbolp dv))
        (free-register dv))
      (setf (symbol-value dst) (setq dv (alloc-register))))
    (format t "	~A	~A, ~{~A~^, ~}~%" op dv (mapcar #'val (cdr args)))))


(defmacro iadd (d s1 s2)
  `(alu-op "iadd" ',d ,s1 ,s2))

(defmacro ior (d s1 s2)
  `(alu-op "ior" ',d ,s1 ,s2))

(defmacro inot (d s)
  `(alu-op "inot" ',d ,s))

(defmacro ixor (d s1 s2)
  `(alu-op "ixor" ',d ,s1 ,s2))

(defmacro iand (d s1 s2)
  `(alu-op "iand" ',d ,s1 ,s2))


(defmacro ishl (d s1 s2)
  `(alu-op "ishl" ',d ,s1 ,s2))

(defmacro ushr (d s1 s2)
  `(alu-op "ushr" ',d ,s1 ,s2))

#|
(if *evergreen*
  (defmacro ror (d s1 s2)
    `(alu-op "bitalign" ',d ,s1 ,s1 ,s2))
  (defmacro ror (d s1 s2)
    `(let (m0 m1)
       (declare (special m0 m1))
       (ushr m0 ,s1 ,s2)
       (ishl m1 ,s1 ,(- 32 s2))
       (ior ,d m0 m1)
       (free-register m1)
       (free-register m0))))
|#

(defun ror-op (d s1 s2)
  (if *evergreen*
    (alu-op "bitalign" d s1 s1 s2)
    (let (m0 m1)
       (declare (special m0 m1))
       (alu-op "ushr" 'm0 s1 s2)
       (alu-op "ishl" 'm1 s1 (- 32 s2))
       (alu-op "ior" d m0 m1)
       (free-register m1)
       (free-register m0))))

(defmacro bytealign (d s0 s1 s2)
  `(alu-op "bytealign" ',d ,s0 ,s1 ,s2))

(defmacro ror (d s1 s2)
  `(ror-op ',d ,s1 ,s2))

(defmacro def-vars (&rest vars)
  `(progn
     ,@(mapcar #'(lambda (var)
                  `(defvar ,var nil)) 
               vars)))


(defvar input #("cb1[3].w" "cb1[0].x" "cb1[0].y" "cb1[0].z"
                "cb1[0].w" "cb1[1].x" "cb1[1].y" "cb1[1].z" 
                "cb1[1].w" "cb1[2].x" "cb1[2].y" "cb1[2].z" 
                "cb1[2].w" "cb1[3].x" "cb1[3].y" "cb1[3].z"))


(defvar midstate3 #("cb3[0].x" "cb3[0].y" "cb3[0].z" "cb3[0].w"
                    "cb3[1].x" "cb3[1].y" "cb3[1].z" "cb3[1].w"))

(defvar midstate #("cb3[2].x" "cb3[2].y" "cb3[2].z" "cb3[2].w"
                   "cb3[3].x" "cb3[3].y" "cb3[3].z" "cb3[3].w"))


(def-vars a b c d e f g h
          w0 w1 w2 w3 w4 w5 w6 w7 w8 w9 w10 w11 w12 w13 w14 w15
          t0 t1 t2 t3 t4 t5)



(defun sha-256-iterate ()
  (when (>= *i* 16)
    (ror t0 w1 7)
    (ror t1 w1 18)
    (ixor t0 t0 t1)
    (ushr t1 w1 3)
    (ixor t0 t0 t1)
	(iadd w0 w0 t0)

    (ror t1 w14 17)
    (ror t0 w14 19)
    (ixor t1 t1 t0)
    (ushr t0 w14 10)
    (ixor t1 t1 t0)

    (iadd w0 w0 t1)
    (iadd w0 w0 w9))

  (when (= *i* *last-i*)
    (assign w1 nil)
    (assign w9 nil)
    (assign w14 nil))

  (rotatef w0 w1 w2 w3 w4 w5 w6 w7 w8 w9 w10 w11 w12 w13 w14 w15)

  (iadd h h w15)
  (ror t0 e 6)
  (ror t1 e 11)
  (ixor t0 t0 t1)
  (ror t1 e 25)
  (ixor t0 t0 t1)
  (iadd h h t0)

  (if *bfi-int*
    (bytealign t0 e f g)
    (progn
      (ixor t0 f g)		
      (iand t0 t0 e)
      (ixor t0 t0 g)))    ; ch(e, f, g)


  (iadd h h t0)
  (iadd h h (svref sha-2:k *i*))
  (iadd d d h)  ; e = 

  (ixor t0 a b)
  (if *bfi-int*
    (bytealign t0 t0 c a)		; Ma(a, b, c)
    (progn
      (iand t1 t0 t5)
      (rotatef t0 t5)
      (ixor t0 t1 b)))

  (iadd h h t0)
  (ror t1 a 2)
  (ror t0 a 13)
  (ixor t1 t1 t0)
  (ror t0 a 22)
  (ixor t1 t1 t0)
  (iadd h h t1)  ; a =

  (rotatef h g f e d c b a))




(defun main ()

  (dotimes (i 16)
    (eval `(assign ,(find-symbol (format nil "W~A" i)) ,(svref input (mod (+ i 3) 16)))))

  (iadd w0 w0 *nonce-add*)

  (assign a (svref midstate3 0))
  (assign b (svref midstate3 1))
  (assign c (svref midstate3 2))
  (assign d (svref midstate3 3))
  (assign e (svref midstate3 4))
  (assign f (svref midstate3 5))
  (assign g (svref midstate3 6))
  (assign h (svref midstate3 7))

  (if (not *bfi-int*)
  	(ixor t5 b c))
  (loop for *i* from 3 to (setq *last-i* 63) do  ; was to 63
    (sha-256-iterate))

  (iadd	w0 a (svref midstate 0))
  (iadd	w1 b (svref midstate 1))
  (iadd	w2 c (svref midstate 2))
  (iadd	w3 d (svref midstate 3))
  (iadd	w4 e (svref midstate 4))
  (iadd	w5 f (svref midstate 5))
  (iadd	w6 g (svref midstate 6))
  (iadd	w7 h (svref midstate 7))
  (assign w8 #x80000000)
  (assign w9 0)
  (assign w10 0)
  (assign w11 0)
  (assign w12 0)
  (assign w13 0)
  (assign w14 0)
  (assign w15 #x100)

  (assign a (svref sha-2:hinit 0))
  (assign b (svref sha-2:hinit 1))
  (assign c (svref sha-2:hinit 2))
  (assign d (svref sha-2:hinit 3))
  (assign e (svref sha-2:hinit 4))
  (assign f (svref sha-2:hinit 5))
  (assign g (svref sha-2:hinit 6))
  (assign h (svref sha-2:hinit 7))

  (if (not *bfi-int*)
  	(ixor t5 b c))
  (loop for *i* from 0 to (setq *last-i* 60) do  ; was to 60
    (sha-256-iterate))

  (iadd result e (svref sha-2:hinit 7)))



(defun gen-cal-il (&optional evergreen vector)
  (setq result (if vector "r0" "r0.y" ))
  (setq *nonce-add* (if vector "r0" "vAbsTidFlat.x"))
;!!!R  (setq *bfi-int* evergreen)
  (let ((*evergreen* evergreen)
        (*vector* vector)
        (*register-pool*  (gen-register-pool "R" (if vector 64 16) vector))
        (*i* 0)
        (*literals* (make-hash-table))
        (*litcount* 11)
        a b c d e f g h
        w0 w1 w2 w3 w4 w5 w6 w7 w8 w9 w10 w11 w12 w13 w14 w15
        t0 t1 t2 t3 t4 t5)

    (with-output-to-string (*standard-output*)
      (let ((il-code (with-output-to-string (*standard-output*)
                       (main))))
        (princ "il_cs_2_0")
        (when *evergreen* (princ ";;; Works on Evergreen and later GPUs
;;; Calc hash using BITALIGN instuction
"))
        (princ "
;;;
;;; Generated by Ufasoft VLIW compiler
;;;
;;; (c) Ufasoft 2011, This code placed in the PUBLIC DOMAIN


dcl_num_thread_per_group 128

dcl_literal l0, 3, 64, 0, 61
dcl_literal l1, 0, 1, 2, 3
dcl_literal l2, 5, 7, 0, 0

dcl_cb cb1[4]				; input
dcl_cb cb3[4]				; midstate after 3 + midstate

")

      (let (lits)
        (maphash #'(lambda (k v) (push (list v k) lits))
           *literals*)
        (let ((litar (make-array 
                       (ash (ash (+ 4 (apply #'max (mapcar #'car lits))) -2) 2))))
          (dolist (lit lits)
            (setf (svref litar (car lit)) (cadr lit)))

          (dotimes (i (/ (length litar) 4))
            (let ((x (svref litar (* i 4)))
                  (y (svref litar (+ (* i 4) 1)))
                  (z (svref litar (+ (* i 4) 2)))
                  (w (svref litar (+ (* i 4) 3))))
              (when (or x y z w)
                (format t "dcl_literal l~A, 0x~x, 0x~x, 0x~x, 0x~x~%" i (or x 0) (or y 0) (or z 0) (or w 0)))))))


    (when *vector*
      (princ "
	    ishl	r0, vAbsTidFlat.x, l1.z
        iadd	r0, r0, l1					; nonce
          "))


      (princ il-code)

      (if *vector*
        (princ "

	ieq		r0, r0, l1.x
	ior		r1.xy, r0.xy, r0.zw
	ior		r1.x, r1.x, r1.y
	if_logicalnz	r1.x	
		ushr	r2.y, vAbsTidFlat.x, l2.x
		mov		g[r2.y], r0
	endif
endmain

         ")

      (princ "
	if_logicalz	r0.y
		ushr	r2.xy__, vAbsTidFlat.x, l2.xy
		iand	r2.x, r2.x, l1.w		; (tid>>3) & 3

		switch r2.x
			case 0
				mov		g[r2.y].x, l1.y
			break
			case 1
				mov		g[r2.y].y, l1.y
			break
			case 2
				mov		g[r2.y].z, l1.y
			break
			case 3
				mov		g[r2.y].w, l1.y
			break
		endswitch
		;mov		g[1].x, vAbsTidFlat.x
		;mov		g[2], r2
	endif
endmain

         "))

      (princ "end")))))


(when *args*
  (princ (gen-cal-il (member "evergreen" *args* :test #'equal)
                     (member "vector" *args* :test #'equal))))
