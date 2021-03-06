; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-unknown-unknown | FileCheck %s

define i32 @add_undef_rhs(i32 %x) {
; CHECK-LABEL: add_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = add i32 %x, undef
  ret i32 %r
}

define <4 x i32> @add_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: add_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = add <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @add_undef_lhs(i32 %x) {
; CHECK-LABEL: add_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = add i32 undef, %x
  ret i32 %r
}

define <4 x i32> @add_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: add_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = add <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @sub_undef_rhs(i32 %x) {
; CHECK-LABEL: sub_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = sub i32 %x, undef
  ret i32 %r
}

define <4 x i32> @sub_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: sub_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = sub <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @sub_undef_lhs(i32 %x) {
; CHECK-LABEL: sub_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = sub i32 undef, %x
  ret i32 %r
}

define <4 x i32> @sub_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: sub_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = sub <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @mul_undef_rhs(i32 %x) {
; CHECK-LABEL: mul_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = mul i32 %x, undef
  ret i32 %r
}

define <4 x i32> @mul_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: mul_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = mul <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @mul_undef_lhs(i32 %x) {
; CHECK-LABEL: mul_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = mul i32 undef, %x
  ret i32 %r
}

define <4 x i32> @mul_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: mul_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = mul <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @sdiv_undef_rhs(i32 %x) {
; CHECK-LABEL: sdiv_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = sdiv i32 %x, undef
  ret i32 %r
}

define <4 x i32> @sdiv_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: sdiv_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = sdiv <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @sdiv_undef_lhs(i32 %x) {
; CHECK-LABEL: sdiv_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = sdiv i32 undef, %x
  ret i32 %r
}

define <4 x i32> @sdiv_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: sdiv_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = sdiv <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @udiv_undef_rhs(i32 %x) {
; CHECK-LABEL: udiv_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = udiv i32 %x, undef
  ret i32 %r
}

define <4 x i32> @udiv_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: udiv_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = udiv <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @udiv_undef_lhs(i32 %x) {
; CHECK-LABEL: udiv_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = udiv i32 undef, %x
  ret i32 %r
}

define <4 x i32> @udiv_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: udiv_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = udiv <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @srem_undef_rhs(i32 %x) {
; CHECK-LABEL: srem_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = srem i32 %x, undef
  ret i32 %r
}

define <4 x i32> @srem_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: srem_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = srem <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @srem_undef_lhs(i32 %x) {
; CHECK-LABEL: srem_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = srem i32 undef, %x
  ret i32 %r
}

define <4 x i32> @srem_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: srem_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = srem <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @urem_undef_rhs(i32 %x) {
; CHECK-LABEL: urem_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = urem i32 %x, undef
  ret i32 %r
}

define <4 x i32> @urem_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: urem_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = urem <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @urem_undef_lhs(i32 %x) {
; CHECK-LABEL: urem_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = urem i32 undef, %x
  ret i32 %r
}

define <4 x i32> @urem_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: urem_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = urem <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @ashr_undef_rhs(i32 %x) {
; CHECK-LABEL: ashr_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movl %edi, %eax
; CHECK-NEXT:    retq
  %r = ashr i32 %x, undef
  ret i32 %r
}

define <4 x i32> @ashr_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: ashr_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = ashr <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @ashr_undef_lhs(i32 %x) {
; CHECK-LABEL: ashr_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = ashr i32 undef, %x
  ret i32 %r
}

define <4 x i32> @ashr_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: ashr_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = ashr <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @lshr_undef_rhs(i32 %x) {
; CHECK-LABEL: lshr_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = lshr i32 %x, undef
  ret i32 %r
}

define <4 x i32> @lshr_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: lshr_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = lshr <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @lshr_undef_lhs(i32 %x) {
; CHECK-LABEL: lshr_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = lshr i32 undef, %x
  ret i32 %r
}

define <4 x i32> @lshr_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: lshr_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = lshr <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @shl_undef_rhs(i32 %x) {
; CHECK-LABEL: shl_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = shl i32 %x, undef
  ret i32 %r
}

define <4 x i32> @shl_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: shl_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = shl <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @shl_undef_lhs(i32 %x) {
; CHECK-LABEL: shl_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = shl i32 undef, %x
  ret i32 %r
}

define <4 x i32> @shl_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: shl_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = shl <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @and_undef_rhs(i32 %x) {
; CHECK-LABEL: and_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = and i32 %x, undef
  ret i32 %r
}

define <4 x i32> @and_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: and_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = and <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @and_undef_lhs(i32 %x) {
; CHECK-LABEL: and_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    xorl %eax, %eax
; CHECK-NEXT:    retq
  %r = and i32 undef, %x
  ret i32 %r
}

define <4 x i32> @and_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: and_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = and <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @or_undef_rhs(i32 %x) {
; CHECK-LABEL: or_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movl $-1, %eax
; CHECK-NEXT:    retq
  %r = or i32 %x, undef
  ret i32 %r
}

define <4 x i32> @or_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: or_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = or <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @or_undef_lhs(i32 %x) {
; CHECK-LABEL: or_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movl $-1, %eax
; CHECK-NEXT:    retq
  %r = or i32 undef, %x
  ret i32 %r
}

define <4 x i32> @or_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: or_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = or <4 x i32> undef, %x
  ret <4 x i32> %r
}

define i32 @xor_undef_rhs(i32 %x) {
; CHECK-LABEL: xor_undef_rhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = xor i32 %x, undef
  ret i32 %r
}

define <4 x i32> @xor_undef_rhs_vec(<4 x i32> %x) {
; CHECK-LABEL: xor_undef_rhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = xor <4 x i32> %x, undef
  ret <4 x i32> %r
}

define i32 @xor_undef_lhs(i32 %x) {
; CHECK-LABEL: xor_undef_lhs:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = xor i32 undef, %x
  ret i32 %r
}

define <4 x i32> @xor_undef_lhs_vec(<4 x i32> %x) {
; CHECK-LABEL: xor_undef_lhs_vec:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %r = xor <4 x i32> undef, %x
  ret <4 x i32> %r
}

