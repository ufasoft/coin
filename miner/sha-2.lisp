#|#########       Copyright (c) 1997-2013 Ufasoft   http://ufasoft.com   mailto:support@ufasoft.com, Sergey Pavlov mailto:dev@ufasoft.com      #################################################################################################
#                                                                                                                                                                                                                                              #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3, or (at your option) any later version.             #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.     #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                        #
#############################################################################################################################################################################################################################################=|#

;;; SHA-2 hash implementation

(require :hash-common "hash-common")

(defpackage :sha-2
  (:export #:k #:hinit)
  (:use :cl :hash-common))

(in-package :sha-2)


(defun primep (number)
  (when (> number 1)
    (loop for fac from 2 to (isqrt number) never (zerop (mod number fac)))))

(defun next-prime (number)
  (loop for n from number when (primep n) return n))

(defun root-n (a n)
  (exp (/ (log a) n)))

(defconstant k (let ((v (make-array 64)))
                 (do ((i 0 (1+ i))
                      (p (next-prime 2) (next-prime (1+ p))))
                     ((>= i 64) v)
                   (setf (svref v i) (floor (scale-float (nth-value 1 (floor (root-n (float p 1.0d0) 3))) 32))))))

(defconstant hinit (let ((v (make-array 8)))
                     (do ((i 0 (1+ i))
                          (p (next-prime 2) (next-prime (1+ p))))
                         ((>= i 8) v)
                       (setf (svref v i) (floor (scale-float (nth-value 1 (floor (root-n (float p 1.0d0) 2))) 32))))))

