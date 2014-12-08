#|#########       Copyright (c) 1997-2013 Ufasoft   http://ufasoft.com   mailto:support@ufasoft.com, Sergey Pavlov mailto:dev@ufasoft.com      #################################################################################################
#                                                                                                                                                                                                                                              #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3, or (at your option) any later version.             #
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.     #
# You should have received a copy of the GNU General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                                                        #
#############################################################################################################################################################################################################################################=|#

;;; Common part of hash implementations


(defpackage :hash-common
  (:export #:hash-common))

(in-package :hash-common)


(defun hash-common (stm hb res)
  (let ((blk (make-array 64))
        (len 0)
        last)
      (loop
        (fill blk 0)
        (if last (return))
        (dotimes (i 64)
          (setf (svref blk i) (or (read-byte stm nil)
                                  (progn (setq last t) #x80)))
          (if last (return))
          (incf len))
        (if (and last (< (logand len 63) 56)) (return))
        (funcall hb blk))
	(setq len (ash len 3))
	(dotimes (i 8)
      (setf (svref blk (+ 56 i))
            (ldb (byte 8 (ash i 3)) len)))
    (funcall hb blk))
  (funcall res))





