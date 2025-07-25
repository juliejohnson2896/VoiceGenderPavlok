// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2007 Julien Pommier
// Copyright (C) 2014 Pedro Gonnet (pedro.gonnet@gmail.com)
// Copyright (C) 2009-2019 Gael Guennebaud <gael.guennebaud@inria.fr>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

/* The exp and log functions of this file initially come from
 * Julien Pommier's sse math library: http://gruntthepeon.free.fr/ssemath/
 */

#ifndef EIGEN_ARCH_GENERIC_PACKET_MATH_FUNCTIONS_H
#define EIGEN_ARCH_GENERIC_PACKET_MATH_FUNCTIONS_H

// IWYU pragma: private
#include "Eigen/src/Core/InternalHeaderCheck.h"

namespace Eigen {
namespace internal {

// Creates a Scalar integer type with same bit-width.
template <typename T>
struct make_integer;
template <>
struct make_integer<float> {
  typedef numext::int32_t type;
};
template <>
struct make_integer<double> {
  typedef numext::int64_t type;
};
template <>
struct make_integer<half> {
  typedef numext::int16_t type;
};
template <>
struct make_integer<bfloat16> {
  typedef numext::int16_t type;
};

/* polevl (modified for Eigen)
 *
 *      Evaluate polynomial
 *
 *
 *
 * SYNOPSIS:
 *
 * int N;
 * Scalar x, y, coef[N+1];
 *
 * y = polevl<decltype(x), N>( x, coef);
 *
 *
 *
 * DESCRIPTION:
 *
 * Evaluates polynomial of degree N:
 *
 *                     2          N
 * y  =  C  + C x + C x  +...+ C x
 *        0    1     2          N
 *
 * Coefficients are stored in reverse order:
 *
 * coef[0] = C  , ..., coef[N] = C  .
 *            N                   0
 *
 *  The function p1evl() assumes that coef[N] = 1.0 and is
 * omitted from the array.  Its calling arguments are
 * otherwise the same as polevl().
 *
 *
 * The Eigen implementation is templatized.  For best speed, store
 * coef as a const array (constexpr), e.g.
 *
 * const double coef[] = {1.0, 2.0, 3.0, ...};
 *
 */
template <typename Packet, int N>
struct ppolevl {
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run(const Packet& x,
                                                          const typename unpacket_traits<Packet>::type coeff[]) {
    EIGEN_STATIC_ASSERT((N > 0), YOU_MADE_A_PROGRAMMING_MISTAKE);
    return pmadd(ppolevl<Packet, N - 1>::run(x, coeff), x, pset1<Packet>(coeff[N]));
  }
};

template <typename Packet>
struct ppolevl<Packet, 0> {
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run(const Packet& x,
                                                          const typename unpacket_traits<Packet>::type coeff[]) {
    EIGEN_UNUSED_VARIABLE(x);
    return pset1<Packet>(coeff[0]);
  }
};

/* chbevl (modified for Eigen)
 *
 *     Evaluate Chebyshev series
 *
 *
 *
 * SYNOPSIS:
 *
 * int N;
 * Scalar x, y, coef[N], chebevl();
 *
 * y = chbevl( x, coef, N );
 *
 *
 *
 * DESCRIPTION:
 *
 * Evaluates the series
 *
 *        N-1
 *         - '
 *  y  =   >   coef[i] T (x/2)
 *         -            i
 *        i=0
 *
 * of Chebyshev polynomials Ti at argument x/2.
 *
 * Coefficients are stored in reverse order, i.e. the zero
 * order term is last in the array.  Note N is the number of
 * coefficients, not the order.
 *
 * If coefficients are for the interval a to b, x must
 * have been transformed to x -> 2(2x - b - a)/(b-a) before
 * entering the routine.  This maps x from (a, b) to (-1, 1),
 * over which the Chebyshev polynomials are defined.
 *
 * If the coefficients are for the inverted interval, in
 * which (a, b) is mapped to (1/b, 1/a), the transformation
 * required is x -> 2(2ab/x - b - a)/(b-a).  If b is infinity,
 * this becomes x -> 4a/x - 1.
 *
 *
 *
 * SPEED:
 *
 * Taking advantage of the recurrence properties of the
 * Chebyshev polynomials, the routine requires one more
 * addition per loop than evaluating a nested polynomial of
 * the same degree.
 *
 */

template <typename Packet, int N>
struct pchebevl {
  EIGEN_DEVICE_FUNC static EIGEN_STRONG_INLINE Packet run(Packet x,
                                                          const typename unpacket_traits<Packet>::type coef[]) {
    typedef typename unpacket_traits<Packet>::type Scalar;
    Packet b0 = pset1<Packet>(coef[0]);
    Packet b1 = pset1<Packet>(static_cast<Scalar>(0.f));
    Packet b2;

    for (int i = 1; i < N; i++) {
      b2 = b1;
      b1 = b0;
      b0 = psub(pmadd(x, b1, pset1<Packet>(coef[i])), b2);
    }

    return pmul(pset1<Packet>(static_cast<Scalar>(0.5f)), psub(b0, b2));
  }
};

template <typename Packet>
EIGEN_STRONG_INLINE EIGEN_DEVICE_FUNC Packet pfrexp_generic_get_biased_exponent(const Packet& a) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  typedef typename unpacket_traits<Packet>::integer_packet PacketI;
  static constexpr int mantissa_bits = numext::numeric_limits<Scalar>::digits - 1;
  return pcast<PacketI, Packet>(plogical_shift_right<mantissa_bits>(preinterpret<PacketI>(pabs(a))));
}

// Safely applies frexp, correctly handles denormals.
// Assumes IEEE floating point format.
template <typename Packet>
EIGEN_STRONG_INLINE EIGEN_DEVICE_FUNC Packet pfrexp_generic(const Packet& a, Packet& exponent) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  typedef typename make_unsigned<typename make_integer<Scalar>::type>::type ScalarUI;
  static constexpr int TotalBits = sizeof(Scalar) * CHAR_BIT, MantissaBits = numext::numeric_limits<Scalar>::digits - 1,
                       ExponentBits = TotalBits - MantissaBits - 1;

  constexpr ScalarUI scalar_sign_mantissa_mask =
      ~(((ScalarUI(1) << ExponentBits) - ScalarUI(1)) << MantissaBits);  // ~0x7f800000
  const Packet sign_mantissa_mask = pset1frombits<Packet>(static_cast<ScalarUI>(scalar_sign_mantissa_mask));
  const Packet half = pset1<Packet>(Scalar(0.5));
  const Packet zero = pzero(a);
  const Packet normal_min = pset1<Packet>((numext::numeric_limits<Scalar>::min)());  // Minimum normal value, 2^-126

  // To handle denormals, normalize by multiplying by 2^(int(MantissaBits)+1).
  const Packet is_denormal = pcmp_lt(pabs(a), normal_min);
  constexpr ScalarUI scalar_normalization_offset = ScalarUI(MantissaBits + 1);  // 24
  // The following cannot be constexpr because bfloat16(uint16_t) is not constexpr.
  const Scalar scalar_normalization_factor = Scalar(ScalarUI(1) << int(scalar_normalization_offset));  // 2^24
  const Packet normalization_factor = pset1<Packet>(scalar_normalization_factor);
  const Packet normalized_a = pselect(is_denormal, pmul(a, normalization_factor), a);

  // Determine exponent offset: -126 if normal, -126-24 if denormal
  const Scalar scalar_exponent_offset = -Scalar((ScalarUI(1) << (ExponentBits - 1)) - ScalarUI(2));  // -126
  Packet exponent_offset = pset1<Packet>(scalar_exponent_offset);
  const Packet normalization_offset = pset1<Packet>(-Scalar(scalar_normalization_offset));  // -24
  exponent_offset = pselect(is_denormal, padd(exponent_offset, normalization_offset), exponent_offset);

  // Determine exponent and mantissa from normalized_a.
  exponent = pfrexp_generic_get_biased_exponent(normalized_a);
  // Zero, Inf and NaN return 'a' unmodified, exponent is zero
  // (technically the exponent is unspecified for inf/NaN, but GCC/Clang set it to zero)
  const Scalar scalar_non_finite_exponent = Scalar((ScalarUI(1) << ExponentBits) - ScalarUI(1));  // 255
  const Packet non_finite_exponent = pset1<Packet>(scalar_non_finite_exponent);
  const Packet is_zero_or_not_finite = por(pcmp_eq(a, zero), pcmp_eq(exponent, non_finite_exponent));
  const Packet m = pselect(is_zero_or_not_finite, a, por(pand(normalized_a, sign_mantissa_mask), half));
  exponent = pselect(is_zero_or_not_finite, zero, padd(exponent, exponent_offset));
  return m;
}

// Safely applies ldexp, correctly handles overflows, underflows and denormals.
// Assumes IEEE floating point format.
template <typename Packet>
EIGEN_STRONG_INLINE EIGEN_DEVICE_FUNC Packet pldexp_generic(const Packet& a, const Packet& exponent) {
  // We want to return a * 2^exponent, allowing for all possible integer
  // exponents without overflowing or underflowing in intermediate
  // computations.
  //
  // Since 'a' and the output can be denormal, the maximum range of 'exponent'
  // to consider for a float is:
  //   -255-23 -> 255+23
  // Below -278 any finite float 'a' will become zero, and above +278 any
  // finite float will become inf, including when 'a' is the smallest possible
  // denormal.
  //
  // Unfortunately, 2^(278) cannot be represented using either one or two
  // finite normal floats, so we must split the scale factor into at least
  // three parts. It turns out to be faster to split 'exponent' into four
  // factors, since [exponent>>2] is much faster to compute that [exponent/3].
  //
  // Set e = min(max(exponent, -278), 278);
  //     b = floor(e/4);
  //   out = ((((a * 2^(b)) * 2^(b)) * 2^(b)) * 2^(e-3*b))
  //
  // This will avoid any intermediate overflows and correctly handle 0, inf,
  // NaN cases.
  typedef typename unpacket_traits<Packet>::integer_packet PacketI;
  typedef typename unpacket_traits<Packet>::type Scalar;
  typedef typename unpacket_traits<PacketI>::type ScalarI;
  static constexpr int TotalBits = sizeof(Scalar) * CHAR_BIT, MantissaBits = numext::numeric_limits<Scalar>::digits - 1,
                       ExponentBits = TotalBits - MantissaBits - 1;

  const Packet max_exponent = pset1<Packet>(Scalar((ScalarI(1) << ExponentBits) + ScalarI(MantissaBits - 1)));  // 278
  const PacketI bias = pset1<PacketI>((ScalarI(1) << (ExponentBits - 1)) - ScalarI(1));                         // 127
  const PacketI e = pcast<Packet, PacketI>(pmin(pmax(exponent, pnegate(max_exponent)), max_exponent));
  PacketI b = parithmetic_shift_right<2>(e);                                          // floor(e/4);
  Packet c = preinterpret<Packet>(plogical_shift_left<MantissaBits>(padd(b, bias)));  // 2^b
  Packet out = pmul(pmul(pmul(a, c), c), c);                                          // a * 2^(3b)
  b = pnmadd(pset1<PacketI>(3), b, e);                                                // e - 3b
  c = preinterpret<Packet>(plogical_shift_left<MantissaBits>(padd(b, bias)));         // 2^(e-3*b)
  out = pmul(out, c);
  return out;
}

// Explicitly multiplies
//    a * (2^e)
// clamping e to the range
// [NumTraits<Scalar>::min_exponent()-2, NumTraits<Scalar>::max_exponent()]
//
// This is approx 7x faster than pldexp_impl, but will prematurely over/underflow
// if 2^e doesn't fit into a normal floating-point Scalar.
//
// Assumes IEEE floating point format
template <typename Packet>
EIGEN_STRONG_INLINE EIGEN_DEVICE_FUNC Packet pldexp_fast(const Packet& a, const Packet& exponent) {
  typedef typename unpacket_traits<Packet>::integer_packet PacketI;
  typedef typename unpacket_traits<Packet>::type Scalar;
  typedef typename unpacket_traits<PacketI>::type ScalarI;
  static constexpr int TotalBits = sizeof(Scalar) * CHAR_BIT, MantissaBits = numext::numeric_limits<Scalar>::digits - 1,
                       ExponentBits = TotalBits - MantissaBits - 1;

  const Packet bias = pset1<Packet>(Scalar((ScalarI(1) << (ExponentBits - 1)) - ScalarI(1)));  // 127
  const Packet limit = pset1<Packet>(Scalar((ScalarI(1) << ExponentBits) - ScalarI(1)));       // 255
  // restrict biased exponent between 0 and 255 for float.
  const PacketI e = pcast<Packet, PacketI>(pmin(pmax(padd(exponent, bias), pzero(limit)), limit));  // exponent + 127
  // return a * (2^e)
  return pmul(a, preinterpret<Packet>(plogical_shift_left<MantissaBits>(e)));
}

// This function implements a single step of Halley's iteration for
// computing x = y^(1/3):
//   x_{k+1} = x_k - (x_k^3 - y) x_k / (2x_k^3 + y)
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet cbrt_halley_iteration_step(const Packet& x_k,
                                                                                      const Packet& y) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  Packet x_k_cb = pmul(x_k, pmul(x_k, x_k));
  Packet denom = pmadd(pset1<Packet>(Scalar(2)), x_k_cb, y);
  Packet num = psub(x_k_cb, y);
  Packet r = pdiv(num, denom);
  return pnmadd(x_k, r, x_k);
}

// Decompose the input such that x^(1/3) = y^(1/3) * 2^e_div3, and y is in the
// interval [0.125,1].
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet cbrt_decompose(const Packet& x, Packet& e_div3) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  // Extract the significant s in the range [0.5,1) and exponent e, such that
  // x = 2^e * s.
  Packet e, s;
  s = pfrexp(x, e);

  // Split the exponent into a part divisible by 3 and the remainder.
  // e = 3*e_div3 + e_mod3.
  constexpr Scalar kOneThird = Scalar(1) / 3;
  e_div3 = pceil(pmul(e, pset1<Packet>(kOneThird)));
  Packet e_mod3 = pnmadd(pset1<Packet>(Scalar(3)), e_div3, e);

  // Replace s by y = (s * 2^e_mod3).
  return pldexp_fast(s, e_mod3);
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet cbrt_special_cases_and_sign(const Packet& x,
                                                                                       const Packet& abs_root) {
  typedef typename unpacket_traits<Packet>::type Scalar;

  // Set sign.
  const Packet sign_mask = pset1<Packet>(Scalar(-0.0));
  const Packet x_sign = pand(sign_mask, x);
  Packet root = por(x_sign, abs_root);

  // Pass non-finite and zero values of x straight through.
  const Packet is_not_finite = por(pisinf(x), pisnan(x));
  const Packet is_zero = pcmp_eq(pzero(x), x);
  const Packet use_x = por(is_not_finite, is_zero);
  return pselect(use_x, x, root);
}

// Generic implementation of cbrt(x) for float.
//
// The algorithm computes the cubic root of the input by first
// decomposing it into a exponent and significant
//   x = s * 2^e.
//
// We can then write the cube root as
//
//   x^(1/3) = 2^(e/3) * s^(1/3)
//           = 2^((3*e_div3 + e_mod3)/3) * s^(1/3)
//           = 2^(e_div3) * 2^(e_mod3/3) * s^(1/3)
//           = 2^(e_div3) * (s * 2^e_mod3)^(1/3)
//
// where e_div3 = ceil(e/3) and e_mod3 = e - 3*e_div3.
//
// The cube root of the second term y = (s * 2^e_mod3)^(1/3) is coarsely
// approximated using a cubic polynomial and subsequently refined using a
// single step of Halley's iteration, and finally the two terms are combined
// using pldexp_fast.
//
// Note: Many alternatives exist for implementing cbrt. See, for example,
// the excellent discussion in Kahan's note:
//   https://csclub.uwaterloo.ca/~pbarfuss/qbrt.pdf
// This particular implementation was found to be very fast and accurate
// among several alternatives tried, but is probably not "optimal" on all
// platforms.
//
// This is accurate to 2 ULP.
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pcbrt_float(const Packet& x) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static_assert(std::is_same<Scalar, float>::value, "Scalar type must be float");

  // Decompose the input such that x^(1/3) = y^(1/3) * 2^e_div3, and y is in the
  // interval [0.125,1].
  Packet e_div3;
  const Packet y = cbrt_decompose(pabs(x), e_div3);

  // Compute initial approximation accurate to 5.22e-3.
  // The polynomial was computed using Rminimax.
  constexpr float alpha[] = {5.9220016002655029296875e-01f, -1.3859539031982421875e+00f, 1.4581282138824462890625e+00f,
                             3.408401906490325927734375e-01f};
  Packet r = ppolevl<Packet, 3>::run(y, alpha);

  // Take one step of Halley's iteration.
  r = cbrt_halley_iteration_step(r, y);

  // Finally multiply by 2^(e_div3)
  r = pldexp_fast(r, e_div3);

  return cbrt_special_cases_and_sign(x, r);
}

// Generic implementation of cbrt(x) for double.
//
// The algorithm is identical to the one for float except that a different initial
// approximation is used for y^(1/3) and two Halley iteration steps are peformed.
//
// This is accurate to 1 ULP.
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pcbrt_double(const Packet& x) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static_assert(std::is_same<Scalar, double>::value, "Scalar type must be double");

  // Decompose the input such that x^(1/3) = y^(1/3) * 2^e_div3, and y is in the
  // interval [0.125,1].
  Packet e_div3;
  const Packet y = cbrt_decompose(pabs(x), e_div3);

  // Compute initial approximation accurate to 0.016.
  // The polynomial was computed using Rminimax.
  constexpr double alpha[] = {-4.69470621553356115551736138513660989701747894287109375e-01,
                              1.072314636518546304699839311069808900356292724609375e+00,
                              3.81249427609571867048288140722434036433696746826171875e-01};
  Packet r = ppolevl<Packet, 2>::run(y, alpha);

  // Take two steps of Halley's iteration.
  r = cbrt_halley_iteration_step(r, y);
  r = cbrt_halley_iteration_step(r, y);

  // Finally multiply by 2^(e_div3).
  r = pldexp_fast(r, e_div3);
  return cbrt_special_cases_and_sign(x, r);
}

// Natural or base 2 logarithm.
// Computes log(x) as log(2^e * m) = C*e + log(m), where the constant C =log(2)
// and m is in the range [sqrt(1/2),sqrt(2)). In this range, the logarithm can
// be easily approximated by a polynomial centered on m=1 for stability.
// TODO(gonnet): Further reduce the interval allowing for lower-degree
//               polynomial interpolants -> ... -> profit!
template <typename Packet, bool base2>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet plog_impl_float(const Packet _x) {
  const Packet cst_1 = pset1<Packet>(1.0f);
  const Packet cst_minus_inf = pset1frombits<Packet>(static_cast<Eigen::numext::uint32_t>(0xff800000u));
  const Packet cst_pos_inf = pset1frombits<Packet>(static_cast<Eigen::numext::uint32_t>(0x7f800000u));

  const Packet cst_cephes_SQRTHF = pset1<Packet>(0.707106781186547524f);
  Packet e, x;
  // extract significant in the range [0.5,1) and exponent
  x = pfrexp(_x, e);

  // part2: Shift the inputs from the range [0.5,1) to [sqrt(1/2),sqrt(2))
  // and shift by -1. The values are then centered around 0, which improves
  // the stability of the polynomial evaluation.
  //   if( x < SQRTHF ) {
  //     e -= 1;
  //     x = x + x - 1.0;
  //   } else { x = x - 1.0; }
  Packet mask = pcmp_lt(x, cst_cephes_SQRTHF);
  Packet tmp = pand(x, mask);
  x = psub(x, cst_1);
  e = psub(e, pand(cst_1, mask));
  x = padd(x, tmp);

  // Polynomial coefficients for rational r(x) = p(x)/q(x)
  // approximating log(1+x) on [sqrt(0.5)-1;sqrt(2)-1].
  constexpr float alpha[] = {0.18256296349849254f, 1.0000000190281063f, 1.0000000190281136f};
  constexpr float beta[] = {0.049616247954120038f, 0.59923249590823520f, 1.4999999999999927f, 1.0f};

  Packet p = ppolevl<Packet, 2>::run(x, alpha);
  p = pmul(x, p);
  Packet q = ppolevl<Packet, 3>::run(x, beta);
  x = pdiv(p, q);

  // Add the logarithm of the exponent back to the result of the interpolation.
  if (base2) {
    const Packet cst_log2e = pset1<Packet>(static_cast<float>(EIGEN_LOG2E));
    x = pmadd(x, cst_log2e, e);
  } else {
    const Packet cst_ln2 = pset1<Packet>(static_cast<float>(EIGEN_LN2));
    x = pmadd(e, cst_ln2, x);
  }

  Packet invalid_mask = pcmp_lt_or_nan(_x, pzero(_x));
  Packet iszero_mask = pcmp_eq(_x, pzero(_x));
  Packet pos_inf_mask = pcmp_eq(_x, cst_pos_inf);
  // Filter out invalid inputs, i.e.:
  //  - negative arg will be NAN
  //  - 0 will be -INF
  //  - +INF will be +INF
  return pselect(iszero_mask, cst_minus_inf, por(pselect(pos_inf_mask, cst_pos_inf, x), invalid_mask));
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet plog_float(const Packet _x) {
  return plog_impl_float<Packet, /* base2 */ false>(_x);
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet plog2_float(const Packet _x) {
  return plog_impl_float<Packet, /* base2 */ true>(_x);
}

/* Returns the base e (2.718...) or base 2 logarithm of x.
 * The argument is separated into its exponent and fractional parts.
 * The logarithm of the fraction in the interval [sqrt(1/2), sqrt(2)],
 * is approximated by
 *
 *     log(1+x) = x - 0.5 x**2 + x**3 P(x)/Q(x).
 *
 * for more detail see: http://www.netlib.org/cephes/
 */
template <typename Packet, bool base2>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet plog_impl_double(const Packet _x) {
  Packet x = _x;

  const Packet cst_1 = pset1<Packet>(1.0);
  const Packet cst_neg_half = pset1<Packet>(-0.5);
  const Packet cst_minus_inf = pset1frombits<Packet>(static_cast<uint64_t>(0xfff0000000000000ull));
  const Packet cst_pos_inf = pset1frombits<Packet>(static_cast<uint64_t>(0x7ff0000000000000ull));

  // Polynomial Coefficients for log(1+x) = x - x**2/2 + x**3 P(x)/Q(x)
  //                             1/sqrt(2) <= x < sqrt(2)
  const Packet cst_cephes_SQRTHF = pset1<Packet>(0.70710678118654752440E0);
  const Packet cst_cephes_log_p0 = pset1<Packet>(1.01875663804580931796E-4);
  const Packet cst_cephes_log_p1 = pset1<Packet>(4.97494994976747001425E-1);
  const Packet cst_cephes_log_p2 = pset1<Packet>(4.70579119878881725854E0);
  const Packet cst_cephes_log_p3 = pset1<Packet>(1.44989225341610930846E1);
  const Packet cst_cephes_log_p4 = pset1<Packet>(1.79368678507819816313E1);
  const Packet cst_cephes_log_p5 = pset1<Packet>(7.70838733755885391666E0);

  const Packet cst_cephes_log_q0 = pset1<Packet>(1.0);
  const Packet cst_cephes_log_q1 = pset1<Packet>(1.12873587189167450590E1);
  const Packet cst_cephes_log_q2 = pset1<Packet>(4.52279145837532221105E1);
  const Packet cst_cephes_log_q3 = pset1<Packet>(8.29875266912776603211E1);
  const Packet cst_cephes_log_q4 = pset1<Packet>(7.11544750618563894466E1);
  const Packet cst_cephes_log_q5 = pset1<Packet>(2.31251620126765340583E1);

  Packet e;
  // extract significant in the range [0.5,1) and exponent
  x = pfrexp(x, e);

  // Shift the inputs from the range [0.5,1) to [sqrt(1/2),sqrt(2))
  // and shift by -1. The values are then centered around 0, which improves
  // the stability of the polynomial evaluation.
  //   if( x < SQRTHF ) {
  //     e -= 1;
  //     x = x + x - 1.0;
  //   } else { x = x - 1.0; }
  Packet mask = pcmp_lt(x, cst_cephes_SQRTHF);
  Packet tmp = pand(x, mask);
  x = psub(x, cst_1);
  e = psub(e, pand(cst_1, mask));
  x = padd(x, tmp);

  Packet x2 = pmul(x, x);
  Packet x3 = pmul(x2, x);

  // Evaluate the polynomial approximant , probably to improve instruction-level parallelism.
  // y = x - 0.5*x^2 + x^3 * polevl( x, P, 5 ) / p1evl( x, Q, 5 ) );
  Packet y, y1, y_;
  y = pmadd(cst_cephes_log_p0, x, cst_cephes_log_p1);
  y1 = pmadd(cst_cephes_log_p3, x, cst_cephes_log_p4);
  y = pmadd(y, x, cst_cephes_log_p2);
  y1 = pmadd(y1, x, cst_cephes_log_p5);
  y_ = pmadd(y, x3, y1);

  y = pmadd(cst_cephes_log_q0, x, cst_cephes_log_q1);
  y1 = pmadd(cst_cephes_log_q3, x, cst_cephes_log_q4);
  y = pmadd(y, x, cst_cephes_log_q2);
  y1 = pmadd(y1, x, cst_cephes_log_q5);
  y = pmadd(y, x3, y1);

  y_ = pmul(y_, x3);
  y = pdiv(y_, y);

  y = pmadd(cst_neg_half, x2, y);
  x = padd(x, y);

  // Add the logarithm of the exponent back to the result of the interpolation.
  if (base2) {
    const Packet cst_log2e = pset1<Packet>(static_cast<double>(EIGEN_LOG2E));
    x = pmadd(x, cst_log2e, e);
  } else {
    const Packet cst_ln2 = pset1<Packet>(static_cast<double>(EIGEN_LN2));
    x = pmadd(e, cst_ln2, x);
  }

  Packet invalid_mask = pcmp_lt_or_nan(_x, pzero(_x));
  Packet iszero_mask = pcmp_eq(_x, pzero(_x));
  Packet pos_inf_mask = pcmp_eq(_x, cst_pos_inf);
  // Filter out invalid inputs, i.e.:
  //  - negative arg will be NAN
  //  - 0 will be -INF
  //  - +INF will be +INF
  return pselect(iszero_mask, cst_minus_inf, por(pselect(pos_inf_mask, cst_pos_inf, x), invalid_mask));
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet plog_double(const Packet _x) {
  return plog_impl_double<Packet, /* base2 */ false>(_x);
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet plog2_double(const Packet _x) {
  return plog_impl_double<Packet, /* base2 */ true>(_x);
}

/** \internal \returns log(1 + x) computed using W. Kahan's formula.
    See: http://www.plunk.org/~hatch/rightway.php
 */
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet generic_log1p(const Packet& x) {
  typedef typename unpacket_traits<Packet>::type ScalarType;
  const Packet one = pset1<Packet>(ScalarType(1));
  Packet xp1 = padd(x, one);
  Packet small_mask = pcmp_eq(xp1, one);
  Packet log1 = plog(xp1);
  Packet inf_mask = pcmp_eq(xp1, log1);
  Packet log_large = pmul(x, pdiv(log1, psub(xp1, one)));
  return pselect(por(small_mask, inf_mask), x, log_large);
}

/** \internal \returns exp(x)-1 computed using W. Kahan's formula.
    See: http://www.plunk.org/~hatch/rightway.php
 */
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet generic_expm1(const Packet& x) {
  typedef typename unpacket_traits<Packet>::type ScalarType;
  const Packet one = pset1<Packet>(ScalarType(1));
  const Packet neg_one = pset1<Packet>(ScalarType(-1));
  Packet u = pexp(x);
  Packet one_mask = pcmp_eq(u, one);
  Packet u_minus_one = psub(u, one);
  Packet neg_one_mask = pcmp_eq(u_minus_one, neg_one);
  Packet logu = plog(u);
  // The following comparison is to catch the case where
  // exp(x) = +inf. It is written in this way to avoid having
  // to form the constant +inf, which depends on the packet
  // type.
  Packet pos_inf_mask = pcmp_eq(logu, u);
  Packet expm1 = pmul(u_minus_one, pdiv(x, logu));
  expm1 = pselect(pos_inf_mask, u, expm1);
  return pselect(one_mask, x, pselect(neg_one_mask, neg_one, expm1));
}

// Exponential function. Works by writing "x = m*log(2) + r" where
// "m = floor(x/log(2)+1/2)" and "r" is the remainder. The result is then
// "exp(x) = 2^m*exp(r)" where exp(r) is in the range [-1,1).
// exp(r) is computed using a 6th order minimax polynomial approximation.
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pexp_float(const Packet _x) {
  const Packet cst_zero = pset1<Packet>(0.0f);
  const Packet cst_one = pset1<Packet>(1.0f);
  const Packet cst_half = pset1<Packet>(0.5f);
  const Packet cst_exp_hi = pset1<Packet>(88.723f);
  const Packet cst_exp_lo = pset1<Packet>(-104.f);
  const Packet cst_pldexp_threshold = pset1<Packet>(87.0);

  const Packet cst_cephes_LOG2EF = pset1<Packet>(1.44269504088896341f);
  const Packet cst_p2 = pset1<Packet>(0.49999988079071044921875f);
  const Packet cst_p3 = pset1<Packet>(0.16666518151760101318359375f);
  const Packet cst_p4 = pset1<Packet>(4.166965186595916748046875e-2f);
  const Packet cst_p5 = pset1<Packet>(8.36894474923610687255859375e-3f);
  const Packet cst_p6 = pset1<Packet>(1.37449637986719608306884765625e-3f);

  // Clamp x.
  Packet zero_mask = pcmp_lt(_x, cst_exp_lo);
  Packet x = pmin(_x, cst_exp_hi);

  // Express exp(x) as exp(m*ln(2) + r), start by extracting
  // m = floor(x/ln(2) + 0.5).
  Packet m = pfloor(pmadd(x, cst_cephes_LOG2EF, cst_half));

  // Get r = x - m*ln(2). If no FMA instructions are available, m*ln(2) is
  // subtracted out in two parts, m*C1+m*C2 = m*ln(2), to avoid accumulating
  // truncation errors.
  const Packet cst_cephes_exp_C1 = pset1<Packet>(-0.693359375f);
  const Packet cst_cephes_exp_C2 = pset1<Packet>(2.12194440e-4f);
  Packet r = pmadd(m, cst_cephes_exp_C1, x);
  r = pmadd(m, cst_cephes_exp_C2, r);

  // Evaluate the 6th order polynomial approximation to exp(r)
  // with r in the interval [-ln(2)/2;ln(2)/2].
  const Packet r2 = pmul(r, r);
  Packet p_even = pmadd(r2, cst_p6, cst_p4);
  const Packet p_odd = pmadd(r2, cst_p5, cst_p3);
  p_even = pmadd(r2, p_even, cst_p2);
  const Packet p_low = padd(r, cst_one);
  Packet y = pmadd(r, p_odd, p_even);
  y = pmadd(r2, y, p_low);

  // Return 2^m * exp(r).
  const Packet fast_pldexp_unsafe = pcmp_lt(cst_pldexp_threshold, pabs(x));
  if (!predux_any(fast_pldexp_unsafe)) {
    // For |x| <= 87, we know the result is not zero or inf, and we can safely use
    // the fast version of pldexp.
    return pmax(pldexp_fast(y, m), _x);
  }
  return pselect(zero_mask, cst_zero, pmax(pldexp(y, m), _x));
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pexp_double(const Packet _x) {
  Packet x = _x;
  const Packet cst_zero = pset1<Packet>(0.0);
  const Packet cst_1 = pset1<Packet>(1.0);
  const Packet cst_2 = pset1<Packet>(2.0);
  const Packet cst_half = pset1<Packet>(0.5);

  const Packet cst_exp_hi = pset1<Packet>(709.784);
  const Packet cst_exp_lo = pset1<Packet>(-745.519);
  const Packet cst_pldexp_threshold = pset1<Packet>(708.0);
  const Packet cst_cephes_LOG2EF = pset1<Packet>(1.4426950408889634073599);
  const Packet cst_cephes_exp_p0 = pset1<Packet>(1.26177193074810590878e-4);
  const Packet cst_cephes_exp_p1 = pset1<Packet>(3.02994407707441961300e-2);
  const Packet cst_cephes_exp_p2 = pset1<Packet>(9.99999999999999999910e-1);
  const Packet cst_cephes_exp_q0 = pset1<Packet>(3.00198505138664455042e-6);
  const Packet cst_cephes_exp_q1 = pset1<Packet>(2.52448340349684104192e-3);
  const Packet cst_cephes_exp_q2 = pset1<Packet>(2.27265548208155028766e-1);
  const Packet cst_cephes_exp_q3 = pset1<Packet>(2.00000000000000000009e0);
  const Packet cst_cephes_exp_C1 = pset1<Packet>(0.693145751953125);
  const Packet cst_cephes_exp_C2 = pset1<Packet>(1.42860682030941723212e-6);

  Packet tmp, fx;

  // clamp x
  Packet zero_mask = pcmp_lt(_x, cst_exp_lo);
  x = pmin(x, cst_exp_hi);
  // Express exp(x) as exp(g + n*log(2)).
  fx = pmadd(cst_cephes_LOG2EF, x, cst_half);

  // Get the integer modulus of log(2), i.e. the "n" described above.
  fx = pfloor(fx);

  // Get the remainder modulo log(2), i.e. the "g" described above. Subtract
  // n*log(2) out in two steps, i.e. n*C1 + n*C2, C1+C2=log2 to get the last
  // digits right.
  tmp = pmul(fx, cst_cephes_exp_C1);
  Packet z = pmul(fx, cst_cephes_exp_C2);
  x = psub(x, tmp);
  x = psub(x, z);

  Packet x2 = pmul(x, x);

  // Evaluate the numerator polynomial of the rational interpolant.
  Packet px = cst_cephes_exp_p0;
  px = pmadd(px, x2, cst_cephes_exp_p1);
  px = pmadd(px, x2, cst_cephes_exp_p2);
  px = pmul(px, x);

  // Evaluate the denominator polynomial of the rational interpolant.
  Packet qx = cst_cephes_exp_q0;
  qx = pmadd(qx, x2, cst_cephes_exp_q1);
  qx = pmadd(qx, x2, cst_cephes_exp_q2);
  qx = pmadd(qx, x2, cst_cephes_exp_q3);

  // I don't really get this bit, copied from the SSE2 routines, so...
  // TODO(gonnet): Figure out what is going on here, perhaps find a better
  // rational interpolant?
  x = pdiv(px, psub(qx, px));
  x = pmadd(cst_2, x, cst_1);

  // Construct the result 2^n * exp(g) = e * x. The max is used to catch
  // non-finite values in the input.
  const Packet fast_pldexp_unsafe = pcmp_lt(cst_pldexp_threshold, pabs(_x));
  if (!predux_any(fast_pldexp_unsafe)) {
    // For |x| <= 708, we know the result is not zero or inf, and we can safely use
    // the fast version of pldexp.
    return pmax(pldexp_fast(x, fx), _x);
  }
  return pselect(zero_mask, cst_zero, pmax(pldexp(x, fx), _x));
}

// The following code is inspired by the following stack-overflow answer:
//   https://stackoverflow.com/questions/30463616/payne-hanek-algorithm-implementation-in-c/30465751#30465751
// It has been largely optimized:
//  - By-pass calls to frexp.
//  - Aligned loads of required 96 bits of 2/pi. This is accomplished by
//    (1) balancing the mantissa and exponent to the required bits of 2/pi are
//    aligned on 8-bits, and (2) replicating the storage of the bits of 2/pi.
//  - Avoid a branch in rounding and extraction of the remaining fractional part.
// Overall, I measured a speed up higher than x2 on x86-64.
inline float trig_reduce_huge(float xf, Eigen::numext::int32_t* quadrant) {
  using Eigen::numext::int32_t;
  using Eigen::numext::int64_t;
  using Eigen::numext::uint32_t;
  using Eigen::numext::uint64_t;

  const double pio2_62 = 3.4061215800865545e-19;     // pi/2 * 2^-62
  const uint64_t zero_dot_five = uint64_t(1) << 61;  // 0.5 in 2.62-bit fixed-point format

  // 192 bits of 2/pi for Payne-Hanek reduction
  // Bits are introduced by packet of 8 to enable aligned reads.
  static const uint32_t two_over_pi[] = {
      0x00000028, 0x000028be, 0x0028be60, 0x28be60db, 0xbe60db93, 0x60db9391, 0xdb939105, 0x9391054a, 0x91054a7f,
      0x054a7f09, 0x4a7f09d5, 0x7f09d5f4, 0x09d5f47d, 0xd5f47d4d, 0xf47d4d37, 0x7d4d3770, 0x4d377036, 0x377036d8,
      0x7036d8a5, 0x36d8a566, 0xd8a5664f, 0xa5664f10, 0x664f10e4, 0x4f10e410, 0x10e41000, 0xe4100000};

  uint32_t xi = numext::bit_cast<uint32_t>(xf);
  // Below, -118 = -126 + 8.
  //   -126 is to get the exponent,
  //   +8 is to enable alignment of 2/pi's bits on 8 bits.
  // This is possible because the fractional part of x as only 24 meaningful bits.
  uint32_t e = (xi >> 23) - 118;
  // Extract the mantissa and shift it to align it wrt the exponent
  xi = ((xi & 0x007fffffu) | 0x00800000u) << (e & 0x7);

  uint32_t i = e >> 3;
  uint32_t twoopi_1 = two_over_pi[i - 1];
  uint32_t twoopi_2 = two_over_pi[i + 3];
  uint32_t twoopi_3 = two_over_pi[i + 7];

  // Compute x * 2/pi in 2.62-bit fixed-point format.
  uint64_t p;
  p = uint64_t(xi) * twoopi_3;
  p = uint64_t(xi) * twoopi_2 + (p >> 32);
  p = (uint64_t(xi * twoopi_1) << 32) + p;

  // Round to nearest: add 0.5 and extract integral part.
  uint64_t q = (p + zero_dot_five) >> 62;
  *quadrant = int(q);
  // Now it remains to compute "r = x - q*pi/2" with high accuracy,
  // since we have p=x/(pi/2) with high accuracy, we can more efficiently compute r as:
  //   r = (p-q)*pi/2,
  // where the product can be be carried out with sufficient accuracy using double precision.
  p -= q << 62;
  return float(double(int64_t(p)) * pio2_62);
}

template <bool ComputeSine, typename Packet, bool ComputeBoth = false>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
#if EIGEN_COMP_GNUC_STRICT
    __attribute__((optimize("-fno-unsafe-math-optimizations")))
#endif
    Packet
    psincos_float(const Packet& _x) {
  typedef typename unpacket_traits<Packet>::integer_packet PacketI;

  const Packet cst_2oPI = pset1<Packet>(0.636619746685028076171875f);  // 2/PI
  const Packet cst_rounding_magic = pset1<Packet>(12582912);           // 2^23 for rounding
  const PacketI csti_1 = pset1<PacketI>(1);
  const Packet cst_sign_mask = pset1frombits<Packet>(static_cast<Eigen::numext::uint32_t>(0x80000000u));

  Packet x = pabs(_x);

  // Scale x by 2/Pi to find x's octant.
  Packet y = pmul(x, cst_2oPI);

  // Rounding trick to find nearest integer:
  Packet y_round = padd(y, cst_rounding_magic);
  EIGEN_OPTIMIZATION_BARRIER(y_round)
  PacketI y_int = preinterpret<PacketI>(y_round);  // last 23 digits represent integer (if abs(x)<2^24)
  y = psub(y_round, cst_rounding_magic);           // nearest integer to x * (2/pi)

// Subtract y * Pi/2 to reduce x to the interval -Pi/4 <= x <= +Pi/4
// using "Extended precision modular arithmetic"
#if defined(EIGEN_VECTORIZE_FMA)
  // This version requires true FMA for high accuracy.
  // It provides a max error of 1ULP up to (with absolute_error < 5.9605e-08):
  const float huge_th = ComputeSine ? 117435.992f : 71476.0625f;
  x = pmadd(y, pset1<Packet>(-1.57079601287841796875f), x);
  x = pmadd(y, pset1<Packet>(-3.1391647326017846353352069854736328125e-07f), x);
  x = pmadd(y, pset1<Packet>(-5.390302529957764765544681040410068817436695098876953125e-15f), x);
#else
  // Without true FMA, the previous set of coefficients maintain 1ULP accuracy
  // up to x<15.7 (for sin), but accuracy is immediately lost for x>15.7.
  // We thus use one more iteration to maintain 2ULPs up to reasonably large inputs.

  // The following set of coefficients maintain 1ULP up to 9.43 and 14.16 for sin and cos respectively.
  // and 2 ULP up to:
  const float huge_th = ComputeSine ? 25966.f : 18838.f;
  x = pmadd(y, pset1<Packet>(-1.5703125), x);  // = 0xbfc90000
  EIGEN_OPTIMIZATION_BARRIER(x)
  x = pmadd(y, pset1<Packet>(-0.000483989715576171875), x);  // = 0xb9fdc000
  EIGEN_OPTIMIZATION_BARRIER(x)
  x = pmadd(y, pset1<Packet>(1.62865035235881805419921875e-07), x);                      // = 0x342ee000
  x = pmadd(y, pset1<Packet>(5.5644315544167710640977020375430583953857421875e-11), x);  // = 0x2e74b9ee

// For the record, the following set of coefficients maintain 2ULP up
// to a slightly larger range:
// const float huge_th = ComputeSine ? 51981.f : 39086.125f;
// but it slightly fails to maintain 1ULP for two values of sin below pi.
// x = pmadd(y, pset1<Packet>(-3.140625/2.), x);
// x = pmadd(y, pset1<Packet>(-0.00048351287841796875), x);
// x = pmadd(y, pset1<Packet>(-3.13855707645416259765625e-07), x);
// x = pmadd(y, pset1<Packet>(-6.0771006282767103812147979624569416046142578125e-11), x);

// For the record, with only 3 iterations it is possible to maintain
// 1 ULP up to 3PI (maybe more) and 2ULP up to 255.
// The coefficients are: 0xbfc90f80, 0xb7354480, 0x2e74b9ee
#endif

  if (predux_any(pcmp_le(pset1<Packet>(huge_th), pabs(_x)))) {
    const int PacketSize = unpacket_traits<Packet>::size;
    EIGEN_ALIGN_TO_BOUNDARY(sizeof(Packet)) float vals[PacketSize];
    EIGEN_ALIGN_TO_BOUNDARY(sizeof(Packet)) float x_cpy[PacketSize];
    EIGEN_ALIGN_TO_BOUNDARY(sizeof(Packet)) Eigen::numext::int32_t y_int2[PacketSize];
    pstoreu(vals, pabs(_x));
    pstoreu(x_cpy, x);
    pstoreu(y_int2, y_int);
    for (int k = 0; k < PacketSize; ++k) {
      float val = vals[k];
      if (val >= huge_th && (numext::isfinite)(val)) x_cpy[k] = trig_reduce_huge(val, &y_int2[k]);
    }
    x = ploadu<Packet>(x_cpy);
    y_int = ploadu<PacketI>(y_int2);
  }

  // Compute the sign to apply to the polynomial.
  // sin: sign = second_bit(y_int) xor signbit(_x)
  // cos: sign = second_bit(y_int+1)
  Packet sign_bit = ComputeSine ? pxor(_x, preinterpret<Packet>(plogical_shift_left<30>(y_int)))
                                : preinterpret<Packet>(plogical_shift_left<30>(padd(y_int, csti_1)));
  sign_bit = pand(sign_bit, cst_sign_mask);  // clear all but left most bit

  // Get the polynomial selection mask from the second bit of y_int
  // We'll calculate both (sin and cos) polynomials and then select from the two.
  Packet poly_mask = preinterpret<Packet>(pcmp_eq(pand(y_int, csti_1), pzero(y_int)));

  Packet x2 = pmul(x, x);

  // Evaluate the cos(x) polynomial. (-Pi/4 <= x <= Pi/4)
  Packet y1 = pset1<Packet>(2.4372266125283204019069671630859375e-05f);
  y1 = pmadd(y1, x2, pset1<Packet>(-0.00138865201734006404876708984375f));
  y1 = pmadd(y1, x2, pset1<Packet>(0.041666619479656219482421875f));
  y1 = pmadd(y1, x2, pset1<Packet>(-0.5f));
  y1 = pmadd(y1, x2, pset1<Packet>(1.f));

  // Evaluate the sin(x) polynomial. (Pi/4 <= x <= Pi/4)
  // octave/matlab code to compute those coefficients:
  //    x = (0:0.0001:pi/4)';
  //    A = [x.^3 x.^5 x.^7];
  //    w = ((1.-(x/(pi/4)).^2).^5)*2000+1;         # weights trading relative accuracy
  //    c = (A'*diag(w)*A)\(A'*diag(w)*(sin(x)-x)); # weighted LS, linear coeff forced to 1
  //    printf('%.64f\n %.64f\n%.64f\n', c(3), c(2), c(1))
  //
  Packet y2 = pset1<Packet>(-0.0001959234114083702898469196984621021329076029360294342041015625f);
  y2 = pmadd(y2, x2, pset1<Packet>(0.0083326873655616851693794799871284340042620897293090820312500000f));
  y2 = pmadd(y2, x2, pset1<Packet>(-0.1666666203982298255503735617821803316473960876464843750000000000f));
  y2 = pmul(y2, x2);
  y2 = pmadd(y2, x, x);

  // Select the correct result from the two polynomials.
  if (ComputeBoth) {
    Packet peven = peven_mask(x);
    Packet ysin = pselect(poly_mask, y2, y1);
    Packet ycos = pselect(poly_mask, y1, y2);
    Packet sign_bit_sin = pxor(_x, preinterpret<Packet>(plogical_shift_left<30>(y_int)));
    Packet sign_bit_cos = preinterpret<Packet>(plogical_shift_left<30>(padd(y_int, csti_1)));
    sign_bit_sin = pand(sign_bit_sin, cst_sign_mask);  // clear all but left most bit
    sign_bit_cos = pand(sign_bit_cos, cst_sign_mask);  // clear all but left most bit
    y = pselect(peven, pxor(ysin, sign_bit_sin), pxor(ycos, sign_bit_cos));
  } else {
    y = ComputeSine ? pselect(poly_mask, y2, y1) : pselect(poly_mask, y1, y2);
    y = pxor(y, sign_bit);
  }
  // Update the sign and filter huge inputs
  return y;
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet psin_float(const Packet& x) {
  return psincos_float<true>(x);
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pcos_float(const Packet& x) {
  return psincos_float<false>(x);
}

// Trigonometric argument reduction for double for inputs smaller than 15.
// Reduces trigonometric arguments for double inputs where x < 15. Given an argument x and its corresponding quadrant
// count n, the function computes and returns the reduced argument t such that x = n * pi/2 + t.
template <typename Packet>
Packet trig_reduce_small_double(const Packet& x, const Packet& q) {
  // Pi/2 split into 2 values
  const Packet cst_pio2_a = pset1<Packet>(-1.570796325802803);
  const Packet cst_pio2_b = pset1<Packet>(-9.920935184482005e-10);

  Packet t;
  t = pmadd(cst_pio2_a, q, x);
  t = pmadd(cst_pio2_b, q, t);
  return t;
}

// Trigonometric argument reduction for double for inputs smaller than 1e14.
// Reduces trigonometric arguments for double inputs where x < 1e14. Given an argument x and its corresponding quadrant
// count n, the function computes and returns the reduced argument t such that x = n * pi/2 + t.
template <typename Packet>
Packet trig_reduce_medium_double(const Packet& x, const Packet& q_high, const Packet& q_low) {
  // Pi/2 split into 4 values
  const Packet cst_pio2_a = pset1<Packet>(-1.570796325802803);
  const Packet cst_pio2_b = pset1<Packet>(-9.920935184482005e-10);
  const Packet cst_pio2_c = pset1<Packet>(-6.123234014771656e-17);
  const Packet cst_pio2_d = pset1<Packet>(1.903488962019325e-25);

  Packet t;
  t = pmadd(cst_pio2_a, q_high, x);
  t = pmadd(cst_pio2_a, q_low, t);
  t = pmadd(cst_pio2_b, q_high, t);
  t = pmadd(cst_pio2_b, q_low, t);
  t = pmadd(cst_pio2_c, q_high, t);
  t = pmadd(cst_pio2_c, q_low, t);
  t = pmadd(cst_pio2_d, padd(q_low, q_high), t);
  return t;
}

template <bool ComputeSine, typename Packet, bool ComputeBoth = false>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS
#if EIGEN_COMP_GNUC_STRICT
    __attribute__((optimize("-fno-unsafe-math-optimizations")))
#endif
    Packet
    psincos_double(const Packet& x) {
  typedef typename unpacket_traits<Packet>::integer_packet PacketI;
  typedef typename unpacket_traits<PacketI>::type ScalarI;

  const Packet cst_sign_mask = pset1frombits<Packet>(static_cast<Eigen::numext::uint64_t>(0x8000000000000000u));

  // If the argument is smaller than this value, use a simpler argument reduction
  const double small_th = 15;
  // If the argument is bigger than this value, use the non-vectorized std version
  const double huge_th = 1e14;

  const Packet cst_2oPI = pset1<Packet>(0.63661977236758134307553505349006);  // 2/PI
  // Integer Packet constants
  const PacketI cst_one = pset1<PacketI>(ScalarI(1));
  // Constant for splitting
  const Packet cst_split = pset1<Packet>(1 << 24);

  Packet x_abs = pabs(x);

  // Scale x by 2/Pi
  PacketI q_int;
  Packet s;

  // TODO Implement huge angle argument reduction
  if (EIGEN_PREDICT_FALSE(predux_any(pcmp_le(pset1<Packet>(small_th), x_abs)))) {
    Packet q_high = pmul(pfloor(pmul(x_abs, pdiv(cst_2oPI, cst_split))), cst_split);
    Packet q_low_noround = psub(pmul(x_abs, cst_2oPI), q_high);
    q_int = pcast<Packet, PacketI>(padd(q_low_noround, pset1<Packet>(0.5)));
    Packet q_low = pcast<PacketI, Packet>(q_int);
    s = trig_reduce_medium_double(x_abs, q_high, q_low);
  } else {
    Packet qval_noround = pmul(x_abs, cst_2oPI);
    q_int = pcast<Packet, PacketI>(padd(qval_noround, pset1<Packet>(0.5)));
    Packet q = pcast<PacketI, Packet>(q_int);
    s = trig_reduce_small_double(x_abs, q);
  }

  // All the upcoming approximating polynomials have even exponents
  Packet ss = pmul(s, s);

  // Padé approximant of cos(x)
  // Assuring < 1 ULP error on the interval [-pi/4, pi/4]
  // cos(x) ~= (80737373*x^8 - 13853547000*x^6 + 727718024880*x^4 - 11275015752000*x^2 + 23594700729600)/(147173*x^8 +
  // 39328920*x^6 + 5772800880*x^4 + 522334612800*x^2 + 23594700729600)
  // MATLAB code to compute those coefficients:
  //    syms x;
  //    cosf = @(x) cos(x);
  //    pade_cosf = pade(cosf(x), x, 0, 'Order', 8)
  Packet sc1_num = pmadd(ss, pset1<Packet>(80737373), pset1<Packet>(-13853547000));
  Packet sc2_num = pmadd(sc1_num, ss, pset1<Packet>(727718024880));
  Packet sc3_num = pmadd(sc2_num, ss, pset1<Packet>(-11275015752000));
  Packet sc4_num = pmadd(sc3_num, ss, pset1<Packet>(23594700729600));
  Packet sc1_denum = pmadd(ss, pset1<Packet>(147173), pset1<Packet>(39328920));
  Packet sc2_denum = pmadd(sc1_denum, ss, pset1<Packet>(5772800880));
  Packet sc3_denum = pmadd(sc2_denum, ss, pset1<Packet>(522334612800));
  Packet sc4_denum = pmadd(sc3_denum, ss, pset1<Packet>(23594700729600));
  Packet scos = pdiv(sc4_num, sc4_denum);

  // Padé approximant of sin(x)
  // Assuring < 1 ULP error on the interval [-pi/4, pi/4]
  // sin(x) ~= (x*(4585922449*x^8 - 1066023933480*x^6 + 83284044283440*x^4 - 2303682236856000*x^2 +
  // 15605159573203200))/(45*(1029037*x^8 + 345207016*x^6 + 61570292784*x^4 + 6603948711360*x^2 + 346781323848960))
  // MATLAB code to compute those coefficients:
  //    syms x;
  //    sinf = @(x) sin(x);
  //    pade_sinf = pade(sinf(x), x, 0, 'Order', 8, 'OrderMode', 'relative')
  Packet ss1_num = pmadd(ss, pset1<Packet>(4585922449), pset1<Packet>(-1066023933480));
  Packet ss2_num = pmadd(ss1_num, ss, pset1<Packet>(83284044283440));
  Packet ss3_num = pmadd(ss2_num, ss, pset1<Packet>(-2303682236856000));
  Packet ss4_num = pmadd(ss3_num, ss, pset1<Packet>(15605159573203200));
  Packet ss1_denum = pmadd(ss, pset1<Packet>(1029037), pset1<Packet>(345207016));
  Packet ss2_denum = pmadd(ss1_denum, ss, pset1<Packet>(61570292784));
  Packet ss3_denum = pmadd(ss2_denum, ss, pset1<Packet>(6603948711360));
  Packet ss4_denum = pmadd(ss3_denum, ss, pset1<Packet>(346781323848960));
  Packet ssin = pdiv(pmul(s, ss4_num), pmul(pset1<Packet>(45), ss4_denum));

  Packet poly_mask = preinterpret<Packet>(pcmp_eq(pand(q_int, cst_one), pzero(q_int)));

  Packet sign_sin = pxor(x, preinterpret<Packet>(plogical_shift_left<62>(q_int)));
  Packet sign_cos = preinterpret<Packet>(plogical_shift_left<62>(padd(q_int, cst_one)));
  Packet sign_bit, sFinalRes;
  if (ComputeBoth) {
    Packet peven = peven_mask(x);
    sign_bit = pselect((s), sign_sin, sign_cos);
    sFinalRes = pselect(pxor(peven, poly_mask), ssin, scos);
  } else {
    sign_bit = ComputeSine ? sign_sin : sign_cos;
    sFinalRes = ComputeSine ? pselect(poly_mask, ssin, scos) : pselect(poly_mask, scos, ssin);
  }
  sign_bit = pand(sign_bit, cst_sign_mask);  // clear all but left most bit
  sFinalRes = pxor(sFinalRes, sign_bit);

  // If the inputs values are higher than that a value that the argument reduction can currently address, compute them
  // using std::sin and std::cos
  // TODO Remove it when huge angle argument reduction is implemented
  if (EIGEN_PREDICT_FALSE(predux_any(pcmp_le(pset1<Packet>(huge_th), x_abs)))) {
    const int PacketSize = unpacket_traits<Packet>::size;
    EIGEN_ALIGN_TO_BOUNDARY(sizeof(Packet)) double sincos_vals[PacketSize];
    EIGEN_ALIGN_TO_BOUNDARY(sizeof(Packet)) double x_cpy[PacketSize];
    pstoreu(x_cpy, x);
    pstoreu(sincos_vals, sFinalRes);
    for (int k = 0; k < PacketSize; ++k) {
      double val = x_cpy[k];
      if (std::abs(val) > huge_th && (numext::isfinite)(val)) {
        if (ComputeBoth)
          sincos_vals[k] = k % 2 == 0 ? std::sin(val) : std::cos(val);
        else
          sincos_vals[k] = ComputeSine ? std::sin(val) : std::cos(val);
      }
    }
    sFinalRes = ploadu<Packet>(sincos_vals);
  }
  return sFinalRes;
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet psin_double(const Packet& x) {
  return psincos_double<true>(x);
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pcos_double(const Packet& x) {
  return psincos_double<false>(x);
}

// Generic implementation of acos(x).
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pacos_float(const Packet& x_in) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static_assert(std::is_same<Scalar, float>::value, "Scalar type must be float");

  const Packet cst_one = pset1<Packet>(Scalar(1));
  const Packet cst_pi = pset1<Packet>(Scalar(EIGEN_PI));
  const Packet p6 = pset1<Packet>(Scalar(2.36423197202384471893310546875e-3));
  const Packet p5 = pset1<Packet>(Scalar(-1.1368644423782825469970703125e-2));
  const Packet p4 = pset1<Packet>(Scalar(2.717843465507030487060546875e-2));
  const Packet p3 = pset1<Packet>(Scalar(-4.8969544470310211181640625e-2));
  const Packet p2 = pset1<Packet>(Scalar(8.8804088532924652099609375e-2));
  const Packet p1 = pset1<Packet>(Scalar(-0.214591205120086669921875));
  const Packet p0 = pset1<Packet>(Scalar(1.57079637050628662109375));

  // For x in [0:1], we approximate acos(x)/sqrt(1-x), which is a smooth
  // function, by a 6'th order polynomial.
  // For x in [-1:0) we use that acos(-x) = pi - acos(x).
  const Packet neg_mask = psignbit(x_in);
  const Packet abs_x = pabs(x_in);

  // Evaluate the polynomial using Horner's rule:
  //   P(x) = p0 + x * (p1 +  x * (p2 + ... (p5 + x * p6)) ... ) .
  // We evaluate even and odd terms independently to increase
  // instruction level parallelism.
  Packet x2 = pmul(x_in, x_in);
  Packet p_even = pmadd(p6, x2, p4);
  Packet p_odd = pmadd(p5, x2, p3);
  p_even = pmadd(p_even, x2, p2);
  p_odd = pmadd(p_odd, x2, p1);
  p_even = pmadd(p_even, x2, p0);
  Packet p = pmadd(p_odd, abs_x, p_even);

  // The polynomial approximates acos(x)/sqrt(1-x), so
  // multiply by sqrt(1-x) to get acos(x).
  // Conveniently returns NaN for arguments outside [-1:1].
  Packet denom = psqrt(psub(cst_one, abs_x));
  Packet result = pmul(denom, p);
  // Undo mapping for negative arguments.
  return pselect(neg_mask, psub(cst_pi, result), result);
}

// Generic implementation of asin(x).
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pasin_float(const Packet& x_in) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static_assert(std::is_same<Scalar, float>::value, "Scalar type must be float");

  constexpr float kPiOverTwo = static_cast<float>(EIGEN_PI / 2);

  const Packet cst_half = pset1<Packet>(0.5f);
  const Packet cst_one = pset1<Packet>(1.0f);
  const Packet cst_two = pset1<Packet>(2.0f);
  const Packet cst_pi_over_two = pset1<Packet>(kPiOverTwo);

  const Packet abs_x = pabs(x_in);
  const Packet sign_mask = pandnot(x_in, abs_x);
  const Packet invalid_mask = pcmp_lt(cst_one, abs_x);

  // For arguments |x| > 0.5, we map x back to [0:0.5] using
  // the transformation x_large = sqrt(0.5*(1-x)), and use the
  // identity
  //   asin(x) = pi/2 - 2 * asin( sqrt( 0.5 * (1 - x)))

  const Packet x_large = psqrt(pnmadd(cst_half, abs_x, cst_half));
  const Packet large_mask = pcmp_lt(cst_half, abs_x);
  const Packet x = pselect(large_mask, x_large, abs_x);
  const Packet x2 = pmul(x, x);

  // For |x| < 0.5 approximate asin(x)/x by an 8th order polynomial with
  // even terms only.
  constexpr float alpha[] = {5.08838854730129241943359375e-2f, 3.95139865577220916748046875e-2f,
                             7.550220191478729248046875e-2f, 0.16664917767047882080078125f, 1.00000011920928955078125f};
  Packet p = ppolevl<Packet, 4>::run(x2, alpha);
  p = pmul(p, x);

  const Packet p_large = pnmadd(cst_two, p, cst_pi_over_two);
  p = pselect(large_mask, p_large, p);
  // Flip the sign for negative arguments.
  p = pxor(p, sign_mask);
  // Return NaN for arguments outside [-1:1].
  return por(invalid_mask, p);
}

template <typename Scalar>
struct patan_reduced {
  template <typename Packet>
  static EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet run(const Packet& x);
};

template <>
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet patan_reduced<double>::run(const Packet& x) {
  constexpr double alpha[] = {2.6667153866462208e-05, 3.0917513112462781e-03, 5.2574296781008604e-02,
                              3.0409318473444424e-01, 7.5365702534987022e-01, 8.2704055405494614e-01,
                              3.3004361289279920e-01};

  constexpr double beta[] = {
      2.7311202462436667e-04, 1.0899150928962708e-02, 1.1548932646420353e-01, 4.9716458728465573e-01, 1.0,
      9.3705509168587852e-01, 3.3004361289279920e-01};

  Packet x2 = pmul(x, x);
  Packet p = ppolevl<Packet, 6>::run(x2, alpha);
  Packet q = ppolevl<Packet, 6>::run(x2, beta);
  return pmul(x, pdiv(p, q));
}

// Computes elementwise atan(x) for x in [-1:1] with 2 ulp accuracy.
template <>
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet patan_reduced<float>::run(const Packet& x) {
  constexpr float alpha[] = {1.12026982009410858154296875e-01f, 7.296695709228515625e-01f, 8.109951019287109375e-01f};

  constexpr float beta[] = {1.00917108356952667236328125e-02f, 2.8318560123443603515625e-01f, 1.0f,
                            8.109951019287109375e-01f};

  Packet x2 = pmul(x, x);
  Packet p = ppolevl<Packet, 2>::run(x2, alpha);
  Packet q = ppolevl<Packet, 3>::run(x2, beta);
  return pmul(x, pdiv(p, q));
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet generic_atan(const Packet& x_in) {
  typedef typename unpacket_traits<Packet>::type Scalar;

  constexpr Scalar kPiOverTwo = static_cast<Scalar>(EIGEN_PI / 2);

  const Packet cst_signmask = pset1<Packet>(Scalar(-0.0));
  const Packet cst_one = pset1<Packet>(Scalar(1));
  const Packet cst_pi_over_two = pset1<Packet>(kPiOverTwo);

  //   "Large": For |x| > 1, use atan(1/x) = sign(x)*pi/2 - atan(x).
  //   "Small": For |x| <= 1, approximate atan(x) directly by a polynomial
  //            calculated using Rminimax.

  const Packet abs_x = pabs(x_in);
  const Packet x_signmask = pand(x_in, cst_signmask);
  const Packet large_mask = pcmp_lt(cst_one, abs_x);
  const Packet x = pselect(large_mask, preciprocal(abs_x), abs_x);
  const Packet p = patan_reduced<Scalar>::run(x);
  // Apply transformations according to the range reduction masks.
  Packet result = pselect(large_mask, psub(cst_pi_over_two, p), p);
  // Return correct sign
  return pxor(result, x_signmask);
}

/** \internal \returns the hyperbolic tan of \a a (coeff-wise)
    Doesn't do anything fancy, just a 9/8-degree rational interpolant which
    is accurate up to a couple of ulps in the (approximate) range [-8, 8],
    outside of which tanh(x) = +/-1 in single precision. The input is clamped
    to the range [-c, c]. The value c is chosen as the smallest value where
    the approximation evaluates to exactly 1.

    This implementation works on both scalars and packets.
*/
template <typename T>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS T ptanh_float(const T& a_x) {
  // Clamp the inputs to the range [-c, c] and set everything
  // outside that range to 1.0. The value c is chosen as the smallest
  // floating point argument such that the approximation is exactly 1.
  // This saves clamping the value at the end.
#ifdef EIGEN_VECTORIZE_FMA
  const T plus_clamp = pset1<T>(8.01773357391357422f);
  const T minus_clamp = pset1<T>(-8.01773357391357422f);
#else
  const T plus_clamp = pset1<T>(7.90738964080810547f);
  const T minus_clamp = pset1<T>(-7.90738964080810547f);
#endif
  const T x = pmax(pmin(a_x, plus_clamp), minus_clamp);

  // The following rational approximation was generated by rminimax
  // (https://gitlab.inria.fr/sfilip/rminimax) using the following
  // command:
  // $ ratapprox --function="tanh(x)" --dom='[-8.67,8.67]' --num="odd"
  //   --den="even" --type="[9,8]" --numF="[SG]" --denF="[SG]" --log
  //   --output=tanhf.sollya --dispCoeff="dec"

  // The monomial coefficients of the numerator polynomial (odd).
  constexpr float alpha[] = {1.394553628e-8f, 2.102733560e-5f, 3.520756727e-3f, 1.340216100e-1f};

  // The monomial coefficients of the denominator polynomial (even).
  constexpr float beta[] = {8.015776984e-7f, 3.326951409e-4f, 2.597254514e-2f, 4.673548340e-1f, 1.0f};

  // Since the polynomials are odd/even, we need x^2.
  const T x2 = pmul(x, x);
  const T x3 = pmul(x2, x);

  T p = ppolevl<T, 3>::run(x2, alpha);
  T q = ppolevl<T, 4>::run(x2, beta);
  // Take advantage of the fact that the constant term in p is 1 to compute
  // x*(x^2*p + 1) = x^3 * p + x.
  p = pmadd(x3, p, x);

  // Divide the numerator by the denominator.
  return pdiv(p, q);
}

/** \internal \returns the hyperbolic tan of \a a (coeff-wise)
    This uses a 19/18-degree rational interpolant which
    is accurate up to a couple of ulps in the (approximate) range [-18.7, 18.7],
    outside of which tanh(x) = +/-1 in single precision. The input is clamped
    to the range [-c, c]. The value c is chosen as the smallest value where
    the approximation evaluates to exactly 1.

    This implementation works on both scalars and packets.
*/
template <typename T>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS T ptanh_double(const T& a_x) {
  // Clamp the inputs to the range [-c, c] and set everything
  // outside that range to 1.0. The value c is chosen as the smallest
  // floating point argument such that the approximation is exactly 1.
  // This saves clamping the value at the end.
#ifdef EIGEN_VECTORIZE_FMA
  const T plus_clamp = pset1<T>(17.6610191624600077);
  const T minus_clamp = pset1<T>(-17.6610191624600077);
#else
  const T plus_clamp = pset1<T>(17.714196154005176);
  const T minus_clamp = pset1<T>(-17.714196154005176);
#endif
  const T x = pmax(pmin(a_x, plus_clamp), minus_clamp);

  // The following rational approximation was generated by rminimax
  // (https://gitlab.inria.fr/sfilip/rminimax) using the following
  // command:
  // $ ./ratapprox --function="tanh(x)" --dom='[-18.72,18.72]'
  //   --num="odd" --den="even" --type="[19,18]" --numF="[D]"
  //   --denF="[D]" --log --output=tanh.sollya --dispCoeff="dec"

  // The monomial coefficients of the numerator polynomial (odd).
  constexpr double alpha[] = {2.6158007860482230e-23, 7.6534862268749319e-19, 3.1309488231386680e-15,
                              4.2303918148209176e-12, 2.4618379131293676e-09, 6.8644367682497074e-07,
                              9.3839087674268880e-05, 5.9809711724441161e-03, 1.5184719640284322e-01};

  // The monomial coefficients of the denominator polynomial (even).
  constexpr double beta[] = {6.463747022670968018e-21, 5.782506856739003571e-17,
                             1.293019623712687916e-13, 1.123643448069621992e-10,
                             4.492975677839633985e-08, 8.785185266237658698e-06,
                             8.295161192716231542e-04, 3.437448108450402717e-02,
                             4.851805297361760360e-01, 1.0};

  // Since the polynomials are odd/even, we need x^2.
  const T x2 = pmul(x, x);
  const T x3 = pmul(x2, x);

  // Interleave the evaluation of the numerator polynomial p and
  // denominator polynomial q.
  T p = ppolevl<T, 8>::run(x2, alpha);
  T q = ppolevl<T, 9>::run(x2, beta);
  // Take advantage of the fact that the constant term in p is 1 to compute
  // x*(x^2*p + 1) = x^3 * p + x.
  p = pmadd(x3, p, x);

  // Divide the numerator by the denominator.
  return pdiv(p, q);
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet patanh_float(const Packet& x) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static_assert(std::is_same<Scalar, float>::value, "Scalar type must be float");

  // For |x| in [0:0.5] we use a polynomial approximation of the form
  // P(x) = x + x^3*(alpha[4] + x^2 * (alpha[3] + x^2 * (... x^2 * alpha[0]) ... )).
  constexpr float alpha[] = {0.1819281280040740966796875f, 8.2311116158962249755859375e-2f,
                             0.14672131836414337158203125f, 0.1997792422771453857421875f, 0.3333373963832855224609375f};
  const Packet x2 = pmul(x, x);
  const Packet x3 = pmul(x, x2);
  Packet p = ppolevl<Packet, 4>::run(x2, alpha);
  p = pmadd(x3, p, x);

  // For |x| in ]0.5:1.0] we use atanh = 0.5*ln((1+x)/(1-x));
  const Packet half = pset1<Packet>(0.5f);
  const Packet one = pset1<Packet>(1.0f);
  Packet r = pdiv(padd(one, x), psub(one, x));
  r = pmul(half, plog(r));

  const Packet x_gt_half = pcmp_le(half, pabs(x));
  const Packet x_eq_one = pcmp_eq(one, pabs(x));
  const Packet x_gt_one = pcmp_lt(one, pabs(x));
  const Packet sign_mask = pset1<Packet>(-0.0f);
  const Packet x_sign = pand(sign_mask, x);
  const Packet inf = pset1<Packet>(std::numeric_limits<float>::infinity());
  return por(x_gt_one, pselect(x_eq_one, por(x_sign, inf), pselect(x_gt_half, r, p)));
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet patanh_double(const Packet& x) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static_assert(std::is_same<Scalar, double>::value, "Scalar type must be double");
  // For x in [-0.5:0.5] we use a rational approximation of the form
  // R(x) = x + x^3*P(x^2)/Q(x^2), where P is or order 4 and Q is of order 5.
  constexpr double alpha[] = {3.3071338469301391e-03, -4.7129526768798737e-02, 1.8185306179826699e-01,
                              -2.5949536095445679e-01, 1.2306328729812676e-01};

  constexpr double beta[] = {-3.8679974580640881e-03, 7.6391885763341910e-02,  -4.2828141436397615e-01,
                             9.8733495886883648e-01,  -1.0000000000000000e+00, 3.6918986189438030e-01};

  const Packet x2 = pmul(x, x);
  const Packet x3 = pmul(x, x2);
  Packet p = ppolevl<Packet, 4>::run(x2, alpha);
  Packet q = ppolevl<Packet, 5>::run(x2, beta);
  Packet y_small = pmadd(x3, pdiv(p, q), x);

  // For |x| in ]0.5:1.0] we use atanh = 0.5*ln((1+x)/(1-x));
  const Packet half = pset1<Packet>(0.5);
  const Packet one = pset1<Packet>(1.0);
  Packet y_large = pdiv(padd(one, x), psub(one, x));
  y_large = pmul(half, plog(y_large));

  const Packet x_gt_half = pcmp_le(half, pabs(x));
  const Packet x_eq_one = pcmp_eq(one, pabs(x));
  const Packet x_gt_one = pcmp_lt(one, pabs(x));
  const Packet sign_mask = pset1<Packet>(-0.0);
  const Packet x_sign = pand(sign_mask, x);
  const Packet inf = pset1<Packet>(std::numeric_limits<double>::infinity());
  return por(x_gt_one, pselect(x_eq_one, por(x_sign, inf), pselect(x_gt_half, y_large, y_small)));
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pdiv_complex(const Packet& x, const Packet& y) {
  typedef typename unpacket_traits<Packet>::as_real RealPacket;
  // In the following we annotate the code for the case where the inputs
  // are a pair length-2 SIMD vectors representing a single pair of complex
  // numbers x = a + i*b, y = c + i*d.
  const RealPacket y_abs = pabs(y.v);                        // |c|, |d|
  const RealPacket y_abs_flip = pcplxflip(Packet(y_abs)).v;  // |d|, |c|
  const RealPacket y_max = pmax(y_abs, y_abs_flip);          // max(|c|, |d|), max(|c|, |d|)
  const RealPacket y_scaled = pdiv(y.v, y_max);              // c / max(|c|, |d|), d / max(|c|, |d|)
  // Compute scaled denominator.
  const RealPacket y_scaled_sq = pmul(y_scaled, y_scaled);  // c'**2, d'**2
  const RealPacket denom = padd(y_scaled_sq, pcplxflip(Packet(y_scaled_sq)).v);
  Packet result_scaled = pmul(x, pconj(Packet(y_scaled)));  // a * c' + b * d', -a * d + b * c
  // Divide elementwise by denom.
  result_scaled = Packet(pdiv(result_scaled.v, denom));
  // Rescale result
  return Packet(pdiv(result_scaled.v, y_max));
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet plog_complex(const Packet& x) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  typedef typename Scalar::value_type RealScalar;
  typedef typename unpacket_traits<Packet>::as_real RealPacket;

  RealPacket real_mask_rp = peven_mask(x.v);
  Packet real_mask(real_mask_rp);

  // Real part
  RealPacket x_flip = pcplxflip(x).v;  // b, a
  Packet x_norm = phypot_complex(x);   // sqrt(a^2 + b^2), sqrt(a^2 + b^2)
  RealPacket xlogr = plog(x_norm.v);   // log(sqrt(a^2 + b^2)), log(sqrt(a^2 + b^2))

  // Imag part
  RealPacket ximg = patan2(x.v, x_flip);  // atan2(a, b), atan2(b, a)

  const RealPacket cst_pos_inf = pset1<RealPacket>(NumTraits<RealScalar>::infinity());
  RealPacket x_abs = pabs(x.v);
  RealPacket is_x_pos_inf = pcmp_eq(x_abs, cst_pos_inf);
  RealPacket is_y_pos_inf = pcplxflip(Packet(is_x_pos_inf)).v;
  RealPacket is_any_inf = por(is_x_pos_inf, is_y_pos_inf);
  RealPacket xreal = pselect(is_any_inf, cst_pos_inf, xlogr);

  Packet xres = pselect(real_mask, Packet(xreal), Packet(ximg));  // log(sqrt(a^2 + b^2)), atan2(b, a)
  return xres;
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet pexp_complex(const Packet& a) {
  typedef typename unpacket_traits<Packet>::as_real RealPacket;
  typedef typename unpacket_traits<Packet>::type Scalar;
  typedef typename Scalar::value_type RealScalar;
  const RealPacket even_mask = peven_mask(a.v);
  const RealPacket odd_mask = pcplxflip(Packet(even_mask)).v;

  // Let a = x + iy.
  // exp(a) = exp(x) * cis(y), plus some special edge-case handling.

  // exp(x):
  RealPacket x = pand(a.v, even_mask);
  x = por(x, pcplxflip(Packet(x)).v);
  RealPacket expx = pexp(x);  // exp(x);

  // cis(y):
  RealPacket y = pand(odd_mask, a.v);
  y = por(y, pcplxflip(Packet(y)).v);
  RealPacket cisy = psincos_float<false, RealPacket, true>(y);
  cisy = pcplxflip(Packet(cisy)).v;  // cos(y) + i * sin(y)

  const RealPacket cst_pos_inf = pset1<RealPacket>(NumTraits<RealScalar>::infinity());
  const RealPacket cst_neg_inf = pset1<RealPacket>(-NumTraits<RealScalar>::infinity());

  // If x is -inf, we know that cossin(y) is bounded,
  //   so the result is (0, +/-0), where the sign of the imaginary part comes
  //   from the sign of cossin(y).
  RealPacket cisy_sign = por(pandnot(cisy, pabs(cisy)), pset1<RealPacket>(RealScalar(1)));
  cisy = pselect(pcmp_eq(x, cst_neg_inf), cisy_sign, cisy);

  // If x is inf, and cos(y) has unknown sign (y is inf or NaN), the result
  // is (+/-inf, NaN), where the signs are undetermined (take the sign of y).
  RealPacket y_sign = por(pandnot(y, pabs(y)), pset1<RealPacket>(RealScalar(1)));
  cisy = pselect(pand(pcmp_eq(x, cst_pos_inf), pisnan(cisy)), pand(y_sign, even_mask), cisy);
  Packet result = Packet(pmul(expx, cisy));

  // If y is +/- 0, the input is real, so take the real result for consistency.
  result = pselect(Packet(pcmp_eq(y, pzero(y))), Packet(por(pand(expx, even_mask), pand(y, odd_mask))), result);

  return result;
}

template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet psqrt_complex(const Packet& a) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  typedef typename Scalar::value_type RealScalar;
  typedef typename unpacket_traits<Packet>::as_real RealPacket;

  // Computes the principal sqrt of the complex numbers in the input.
  //
  // For example, for packets containing 2 complex numbers stored in interleaved format
  //    a = [a0, a1] = [x0, y0, x1, y1],
  // where x0 = real(a0), y0 = imag(a0) etc., this function returns
  //    b = [b0, b1] = [u0, v0, u1, v1],
  // such that b0^2 = a0, b1^2 = a1.
  //
  // To derive the formula for the complex square roots, let's consider the equation for
  // a single complex square root of the number x + i*y. We want to find real numbers
  // u and v such that
  //    (u + i*v)^2 = x + i*y  <=>
  //    u^2 - v^2 + i*2*u*v = x + i*v.
  // By equating the real and imaginary parts we get:
  //    u^2 - v^2 = x
  //    2*u*v = y.
  //
  // For x >= 0, this has the numerically stable solution
  //    u = sqrt(0.5 * (x + sqrt(x^2 + y^2)))
  //    v = 0.5 * (y / u)
  // and for x < 0,
  //    v = sign(y) * sqrt(0.5 * (-x + sqrt(x^2 + y^2)))
  //    u = 0.5 * (y / v)
  //
  //  To avoid unnecessary over- and underflow, we compute sqrt(x^2 + y^2) as
  //     l = max(|x|, |y|) * sqrt(1 + (min(|x|, |y|) / max(|x|, |y|))^2) ,

  // In the following, without lack of generality, we have annotated the code, assuming
  // that the input is a packet of 2 complex numbers.
  //
  // Step 1. Compute l = [l0, l0, l1, l1], where
  //    l0 = sqrt(x0^2 + y0^2),  l1 = sqrt(x1^2 + y1^2)
  // To avoid over- and underflow, we use the stable formula for each hypotenuse
  //    l0 = (min0 == 0 ? max0 : max0 * sqrt(1 + (min0/max0)**2)),
  // where max0 = max(|x0|, |y0|), min0 = min(|x0|, |y0|), and similarly for l1.

  RealPacket a_abs = pabs(a.v);                        // [|x0|, |y0|, |x1|, |y1|]
  RealPacket a_abs_flip = pcplxflip(Packet(a_abs)).v;  // [|y0|, |x0|, |y1|, |x1|]
  RealPacket a_max = pmax(a_abs, a_abs_flip);
  RealPacket a_min = pmin(a_abs, a_abs_flip);
  RealPacket a_min_zero_mask = pcmp_eq(a_min, pzero(a_min));
  RealPacket a_max_zero_mask = pcmp_eq(a_max, pzero(a_max));
  RealPacket r = pdiv(a_min, a_max);
  const RealPacket cst_one = pset1<RealPacket>(RealScalar(1));
  RealPacket l = pmul(a_max, psqrt(padd(cst_one, pmul(r, r))));  // [l0, l0, l1, l1]
  // Set l to a_max if a_min is zero.
  l = pselect(a_min_zero_mask, a_max, l);

  // Step 2. Compute [rho0, *, rho1, *], where
  // rho0 = sqrt(0.5 * (l0 + |x0|)), rho1 =  sqrt(0.5 * (l1 + |x1|))
  // We don't care about the imaginary parts computed here. They will be overwritten later.
  const RealPacket cst_half = pset1<RealPacket>(RealScalar(0.5));
  Packet rho;
  rho.v = psqrt(pmul(cst_half, padd(a_abs, l)));

  // Step 3. Compute [rho0, eta0, rho1, eta1], where
  // eta0 = (y0 / l0) / 2, and eta1 = (y1 / l1) / 2.
  // set eta = 0 of input is 0 + i0.
  RealPacket eta = pandnot(pmul(cst_half, pdiv(a.v, pcplxflip(rho).v)), a_max_zero_mask);
  RealPacket real_mask = peven_mask(a.v);
  Packet positive_real_result;
  // Compute result for inputs with positive real part.
  positive_real_result.v = pselect(real_mask, rho.v, eta);

  // Step 4. Compute solution for inputs with negative real part:
  //         [|eta0|, sign(y0)*rho0, |eta1|, sign(y1)*rho1]
  const RealPacket cst_imag_sign_mask = pset1<Packet>(Scalar(RealScalar(0.0), RealScalar(-0.0))).v;
  RealPacket imag_signs = pand(a.v, cst_imag_sign_mask);
  Packet negative_real_result;
  // Notice that rho is positive, so taking it's absolute value is a noop.
  negative_real_result.v = por(pabs(pcplxflip(positive_real_result).v), imag_signs);

  // Step 5. Select solution branch based on the sign of the real parts.
  Packet negative_real_mask;
  negative_real_mask.v = pcmp_lt(pand(real_mask, a.v), pzero(a.v));
  negative_real_mask.v = por(negative_real_mask.v, pcplxflip(negative_real_mask).v);
  Packet result = pselect(negative_real_mask, negative_real_result, positive_real_result);

  // Step 6. Handle special cases for infinities:
  // * If z is (x,+∞), the result is (+∞,+∞) even if x is NaN
  // * If z is (x,-∞), the result is (+∞,-∞) even if x is NaN
  // * If z is (-∞,y), the result is (0*|y|,+∞) for finite or NaN y
  // * If z is (+∞,y), the result is (+∞,0*|y|) for finite or NaN y
  const RealPacket cst_pos_inf = pset1<RealPacket>(NumTraits<RealScalar>::infinity());
  Packet is_inf;
  is_inf.v = pcmp_eq(a_abs, cst_pos_inf);
  Packet is_real_inf;
  is_real_inf.v = pand(is_inf.v, real_mask);
  is_real_inf = por(is_real_inf, pcplxflip(is_real_inf));
  // prepare packet of (+∞,0*|y|) or (0*|y|,+∞), depending on the sign of the infinite real part.
  Packet real_inf_result;
  real_inf_result.v = pmul(a_abs, pset1<Packet>(Scalar(RealScalar(1.0), RealScalar(0.0))).v);
  real_inf_result.v = pselect(negative_real_mask.v, pcplxflip(real_inf_result).v, real_inf_result.v);
  // prepare packet of (+∞,+∞) or (+∞,-∞), depending on the sign of the infinite imaginary part.
  Packet is_imag_inf;
  is_imag_inf.v = pandnot(is_inf.v, real_mask);
  is_imag_inf = por(is_imag_inf, pcplxflip(is_imag_inf));
  Packet imag_inf_result;
  imag_inf_result.v = por(pand(cst_pos_inf, real_mask), pandnot(a.v, real_mask));
  // unless otherwise specified, if either the real or imaginary component is nan, the entire result is nan
  Packet result_is_nan = pisnan(result);
  result = por(result_is_nan, result);

  return pselect(is_imag_inf, imag_inf_result, pselect(is_real_inf, real_inf_result, result));
}

// \internal \returns the norm of a complex number z = x + i*y, defined as sqrt(x^2 + y^2).
// Implemented using the hypot(a,b) algorithm from https://doi.org/10.48550/arXiv.1904.09481
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet phypot_complex(const Packet& a) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  typedef typename Scalar::value_type RealScalar;
  typedef typename unpacket_traits<Packet>::as_real RealPacket;

  const RealPacket cst_zero_rp = pset1<RealPacket>(static_cast<RealScalar>(0.0));
  const RealPacket cst_minus_one_rp = pset1<RealPacket>(static_cast<RealScalar>(-1.0));
  const RealPacket cst_two_rp = pset1<RealPacket>(static_cast<RealScalar>(2.0));
  const RealPacket evenmask = peven_mask(a.v);

  RealPacket a_abs = pabs(a.v);
  RealPacket a_flip = pcplxflip(Packet(a_abs)).v;       // |b|, |a|
  RealPacket a_all = pselect(evenmask, a_abs, a_flip);  // |a|, |a|
  RealPacket b_all = pselect(evenmask, a_flip, a_abs);  // |b|, |b|

  RealPacket a2 = pmul(a.v, a.v);                    // |a^2, b^2|
  RealPacket a2_flip = pcplxflip(Packet(a2)).v;      // |b^2, a^2|
  RealPacket h = psqrt(padd(a2, a2_flip));           // |sqrt(a^2 + b^2), sqrt(a^2 + b^2)|
  RealPacket h_sq = pmul(h, h);                      // |a^2 + b^2, a^2 + b^2|
  RealPacket a_sq = pselect(evenmask, a2, a2_flip);  // |a^2, a^2|
  RealPacket m_h_sq = pmul(h_sq, cst_minus_one_rp);
  RealPacket m_a_sq = pmul(a_sq, cst_minus_one_rp);
  RealPacket x = psub(psub(pmadd(h, h, m_h_sq), pmadd(b_all, b_all, psub(a_sq, h_sq))), pmadd(a_all, a_all, m_a_sq));
  h = psub(h, pdiv(x, pmul(cst_two_rp, h)));  // |h - x/(2*h), h - x/(2*h)|

  // handle zero-case
  RealPacket iszero = pcmp_eq(por(a_abs, a_flip), cst_zero_rp);

  h = pandnot(h, iszero);  // |sqrt(a^2+b^2), sqrt(a^2+b^2)|
  return Packet(h);        // |sqrt(a^2+b^2), sqrt(a^2+b^2)|
}

template <typename Packet>
struct psign_impl<Packet, std::enable_if_t<!is_scalar<Packet>::value &&
                                           !NumTraits<typename unpacket_traits<Packet>::type>::IsComplex &&
                                           !NumTraits<typename unpacket_traits<Packet>::type>::IsInteger>> {
  static EIGEN_DEVICE_FUNC inline Packet run(const Packet& a) {
    using Scalar = typename unpacket_traits<Packet>::type;
    const Packet cst_one = pset1<Packet>(Scalar(1));
    const Packet cst_zero = pzero(a);

    const Packet abs_a = pabs(a);
    const Packet sign_mask = pandnot(a, abs_a);
    const Packet nonzero_mask = pcmp_lt(cst_zero, abs_a);

    return pselect(nonzero_mask, por(sign_mask, cst_one), abs_a);
  }
};

template <typename Packet>
struct psign_impl<Packet, std::enable_if_t<!is_scalar<Packet>::value &&
                                           !NumTraits<typename unpacket_traits<Packet>::type>::IsComplex &&
                                           NumTraits<typename unpacket_traits<Packet>::type>::IsSigned &&
                                           NumTraits<typename unpacket_traits<Packet>::type>::IsInteger>> {
  static EIGEN_DEVICE_FUNC inline Packet run(const Packet& a) {
    using Scalar = typename unpacket_traits<Packet>::type;
    const Packet cst_one = pset1<Packet>(Scalar(1));
    const Packet cst_minus_one = pset1<Packet>(Scalar(-1));
    const Packet cst_zero = pzero(a);

    const Packet positive_mask = pcmp_lt(cst_zero, a);
    const Packet positive = pand(positive_mask, cst_one);
    const Packet negative_mask = pcmp_lt(a, cst_zero);
    const Packet negative = pand(negative_mask, cst_minus_one);

    return por(positive, negative);
  }
};

template <typename Packet>
struct psign_impl<Packet, std::enable_if_t<!is_scalar<Packet>::value &&
                                           !NumTraits<typename unpacket_traits<Packet>::type>::IsComplex &&
                                           !NumTraits<typename unpacket_traits<Packet>::type>::IsSigned &&
                                           NumTraits<typename unpacket_traits<Packet>::type>::IsInteger>> {
  static EIGEN_DEVICE_FUNC inline Packet run(const Packet& a) {
    using Scalar = typename unpacket_traits<Packet>::type;
    const Packet cst_one = pset1<Packet>(Scalar(1));
    const Packet cst_zero = pzero(a);

    const Packet zero_mask = pcmp_eq(cst_zero, a);
    return pandnot(cst_one, zero_mask);
  }
};

// \internal \returns the the sign of a complex number z, defined as z / abs(z).
template <typename Packet>
struct psign_impl<Packet, std::enable_if_t<!is_scalar<Packet>::value &&
                                           NumTraits<typename unpacket_traits<Packet>::type>::IsComplex &&
                                           unpacket_traits<Packet>::vectorizable>> {
  static EIGEN_DEVICE_FUNC inline Packet run(const Packet& a) {
    typedef typename unpacket_traits<Packet>::type Scalar;
    typedef typename Scalar::value_type RealScalar;
    typedef typename unpacket_traits<Packet>::as_real RealPacket;

    // Step 1. Compute (for each element z = x + i*y in a)
    //     l = abs(z) = sqrt(x^2 + y^2).
    // To avoid over- and underflow, we use the stable formula for each hypotenuse
    //    l = (zmin == 0 ? zmax : zmax * sqrt(1 + (zmin/zmax)**2)),
    // where zmax = max(|x|, |y|), zmin = min(|x|, |y|),
    RealPacket a_abs = pabs(a.v);
    RealPacket a_abs_flip = pcplxflip(Packet(a_abs)).v;
    RealPacket a_max = pmax(a_abs, a_abs_flip);
    RealPacket a_min = pmin(a_abs, a_abs_flip);
    RealPacket a_min_zero_mask = pcmp_eq(a_min, pzero(a_min));
    RealPacket a_max_zero_mask = pcmp_eq(a_max, pzero(a_max));
    RealPacket r = pdiv(a_min, a_max);
    const RealPacket cst_one = pset1<RealPacket>(RealScalar(1));
    RealPacket l = pmul(a_max, psqrt(padd(cst_one, pmul(r, r))));  // [l0, l0, l1, l1]
    // Set l to a_max if a_min is zero, since the roundtrip sqrt(a_max^2) may be
    // lossy.
    l = pselect(a_min_zero_mask, a_max, l);
    // Step 2 compute a / abs(a).
    RealPacket sign_as_real = pandnot(pdiv(a.v, l), a_max_zero_mask);
    Packet sign;
    sign.v = sign_as_real;
    return sign;
  }
};

// TODO(rmlarsen): The following set of utilities for double word arithmetic
// should perhaps be refactored as a separate file, since it would be generally
// useful for special function implementation etc. Writing the algorithms in
// terms if a double word type would also make the code more readable.

// This function splits x into the nearest integer n and fractional part r,
// such that x = n + r holds exactly.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void absolute_split(const Packet& x, Packet& n, Packet& r) {
  n = pround(x);
  r = psub(x, n);
}

// This function computes the sum {s, r}, such that x + y = s_hi + s_lo
// holds exactly, and s_hi = fl(x+y), if |x| >= |y|.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void fast_twosum(const Packet& x, const Packet& y, Packet& s_hi, Packet& s_lo) {
  s_hi = padd(x, y);
  const Packet t = psub(s_hi, x);
  s_lo = psub(y, t);
}

#ifdef EIGEN_VECTORIZE_FMA
// This function implements the extended precision product of
// a pair of floating point numbers. Given {x, y}, it computes the pair
// {p_hi, p_lo} such that x * y = p_hi + p_lo holds exactly and
// p_hi = fl(x * y).
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void twoprod(const Packet& x, const Packet& y, Packet& p_hi, Packet& p_lo) {
  p_hi = pmul(x, y);
  p_lo = pmsub(x, y, p_hi);
}

// A version of twoprod that takes x, y, and fl(x*y) as input and returns the p_lo such that
// x * y = xy + p_lo holds exactly.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet twoprod_low(const Packet& x, const Packet& y, const Packet& xy) {
  return pmsub(x, y, xy);
}

#else

// This function implements the Veltkamp splitting. Given a floating point
// number x it returns the pair {x_hi, x_lo} such that x_hi + x_lo = x holds
// exactly and that half of the significant of x fits in x_hi.
// This is Algorithm 3 from Jean-Michel Muller, "Elementary Functions",
// 3rd edition, Birkh\"auser, 2016.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void veltkamp_splitting(const Packet& x, Packet& x_hi, Packet& x_lo) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  constexpr int shift = (NumTraits<Scalar>::digits() + 1) / 2;
  const Scalar shift_scale = Scalar(uint64_t(1) << shift);  // Scalar constructor not necessarily constexpr.
  const Packet gamma = pmul(pset1<Packet>(shift_scale + Scalar(1)), x);
  Packet rho = psub(x, gamma);
  x_hi = padd(rho, gamma);
  x_lo = psub(x, x_hi);
}

// This function implements Dekker's algorithm for products x * y.
// Given floating point numbers {x, y} computes the pair
// {p_hi, p_lo} such that x * y = p_hi + p_lo holds exactly and
// p_hi = fl(x * y).
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void twoprod(const Packet& x, const Packet& y, Packet& p_hi, Packet& p_lo) {
  Packet x_hi, x_lo, y_hi, y_lo;
  veltkamp_splitting(x, x_hi, x_lo);
  veltkamp_splitting(y, y_hi, y_lo);

  p_hi = pmul(x, y);
  p_lo = pmadd(x_hi, y_hi, pnegate(p_hi));
  p_lo = pmadd(x_hi, y_lo, p_lo);
  p_lo = pmadd(x_lo, y_hi, p_lo);
  p_lo = pmadd(x_lo, y_lo, p_lo);
}

// A version of twoprod that takes x, y, and fl(x*y) as input and returns the p_lo such that
// x * y = xy + p_lo holds exactly.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet twoprod_low(const Packet& x, const Packet& y, const Packet& xy) {
  Packet x_hi, x_lo, y_hi, y_lo;
  veltkamp_splitting(x, x_hi, x_lo);
  veltkamp_splitting(y, y_hi, y_lo);

  Packet p_lo = pmadd(x_hi, y_hi, pnegate(xy));
  p_lo = pmadd(x_hi, y_lo, p_lo);
  p_lo = pmadd(x_lo, y_hi, p_lo);
  p_lo = pmadd(x_lo, y_lo, p_lo);
  return p_lo;
}

#endif  // EIGEN_VECTORIZE_FMA

// This function implements Dekker's algorithm for the addition
// of two double word numbers represented by {x_hi, x_lo} and {y_hi, y_lo}.
// It returns the result as a pair {s_hi, s_lo} such that
// x_hi + x_lo + y_hi + y_lo = s_hi + s_lo holds exactly.
// This is Algorithm 5 from Jean-Michel Muller, "Elementary Functions",
// 3rd edition, Birkh\"auser, 2016.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void twosum(const Packet& x_hi, const Packet& x_lo, const Packet& y_hi,
                                                  const Packet& y_lo, Packet& s_hi, Packet& s_lo) {
  const Packet x_greater_mask = pcmp_lt(pabs(y_hi), pabs(x_hi));
  Packet r_hi_1, r_lo_1;
  fast_twosum(x_hi, y_hi, r_hi_1, r_lo_1);
  Packet r_hi_2, r_lo_2;
  fast_twosum(y_hi, x_hi, r_hi_2, r_lo_2);
  const Packet r_hi = pselect(x_greater_mask, r_hi_1, r_hi_2);

  const Packet s1 = padd(padd(y_lo, r_lo_1), x_lo);
  const Packet s2 = padd(padd(x_lo, r_lo_2), y_lo);
  const Packet s = pselect(x_greater_mask, s1, s2);

  fast_twosum(r_hi, s, s_hi, s_lo);
}

// This is a version of twosum for double word numbers,
// which assumes that |x_hi| >= |y_hi|.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void fast_twosum(const Packet& x_hi, const Packet& x_lo, const Packet& y_hi,
                                                       const Packet& y_lo, Packet& s_hi, Packet& s_lo) {
  Packet r_hi, r_lo;
  fast_twosum(x_hi, y_hi, r_hi, r_lo);
  const Packet s = padd(padd(y_lo, r_lo), x_lo);
  fast_twosum(r_hi, s, s_hi, s_lo);
}

// This is a version of twosum for adding a floating point number x to
// double word number {y_hi, y_lo} number, with the assumption
// that |x| >= |y_hi|.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void fast_twosum(const Packet& x, const Packet& y_hi, const Packet& y_lo,
                                                       Packet& s_hi, Packet& s_lo) {
  Packet r_hi, r_lo;
  fast_twosum(x, y_hi, r_hi, r_lo);
  const Packet s = padd(y_lo, r_lo);
  fast_twosum(r_hi, s, s_hi, s_lo);
}

// This function implements the multiplication of a double word
// number represented by {x_hi, x_lo} by a floating point number y.
// It returns the result as a pair {p_hi, p_lo} such that
// (x_hi + x_lo) * y = p_hi + p_lo hold with a relative error
// of less than 2*2^{-2p}, where p is the number of significand bit
// in the floating point type.
// This is Algorithm 7 from Jean-Michel Muller, "Elementary Functions",
// 3rd edition, Birkh\"auser, 2016.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void twoprod(const Packet& x_hi, const Packet& x_lo, const Packet& y,
                                                   Packet& p_hi, Packet& p_lo) {
  Packet c_hi, c_lo1;
  twoprod(x_hi, y, c_hi, c_lo1);
  const Packet c_lo2 = pmul(x_lo, y);
  Packet t_hi, t_lo1;
  fast_twosum(c_hi, c_lo2, t_hi, t_lo1);
  const Packet t_lo2 = padd(t_lo1, c_lo1);
  fast_twosum(t_hi, t_lo2, p_hi, p_lo);
}

// This function implements the multiplication of two double word
// numbers represented by {x_hi, x_lo} and {y_hi, y_lo}.
// It returns the result as a pair {p_hi, p_lo} such that
// (x_hi + x_lo) * (y_hi + y_lo) = p_hi + p_lo holds with a relative error
// of less than 2*2^{-2p}, where p is the number of significand bit
// in the floating point type.
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void twoprod(const Packet& x_hi, const Packet& x_lo, const Packet& y_hi,
                                                   const Packet& y_lo, Packet& p_hi, Packet& p_lo) {
  Packet p_hi_hi, p_hi_lo;
  twoprod(x_hi, x_lo, y_hi, p_hi_hi, p_hi_lo);
  Packet p_lo_hi, p_lo_lo;
  twoprod(x_hi, x_lo, y_lo, p_lo_hi, p_lo_lo);
  fast_twosum(p_hi_hi, p_hi_lo, p_lo_hi, p_lo_lo, p_hi, p_lo);
}

// This function implements the division of double word {x_hi, x_lo}
// by float y. This is Algorithm 15 from "Tight and rigorous error bounds
// for basic building blocks of double-word arithmetic", Joldes, Muller, & Popescu,
// 2017. https://hal.archives-ouvertes.fr/hal-01351529
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void doubleword_div_fp(const Packet& x_hi, const Packet& x_lo, const Packet& y,
                                                             Packet& z_hi, Packet& z_lo) {
  const Packet t_hi = pdiv(x_hi, y);
  Packet pi_hi, pi_lo;
  twoprod(t_hi, y, pi_hi, pi_lo);
  const Packet delta_hi = psub(x_hi, pi_hi);
  const Packet delta_t = psub(delta_hi, pi_lo);
  const Packet delta = padd(delta_t, x_lo);
  const Packet t_lo = pdiv(delta, y);
  fast_twosum(t_hi, t_lo, z_hi, z_lo);
}

// This function computes log2(x) and returns the result as a double word.
template <typename Scalar>
struct accurate_log2 {
  template <typename Packet>
  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void operator()(const Packet& x, Packet& log2_x_hi, Packet& log2_x_lo) {
    log2_x_hi = plog2(x);
    log2_x_lo = pzero(x);
  }
};

// This specialization uses a more accurate algorithm to compute log2(x) for
// floats in [1/sqrt(2);sqrt(2)] with a relative accuracy of ~6.56508e-10.
// This additional accuracy is needed to counter the error-magnification
// inherent in multiplying by a potentially large exponent in pow(x,y).
// The minimax polynomial used was calculated using the Rminimax tool,
// see https://gitlab.inria.fr/sfilip/rminimax.
// Command line:
//   $ ratapprox --function="log2(1+x)/x"  --dom='[-0.2929,0.41422]'
//   --type=[10,0]
//       --numF="[D,D,SG]" --denF="[SG]" --log --dispCoeff="dec"
//
// The resulting implementation of pow(x,y) is accurate to 3 ulps.
template <>
struct accurate_log2<float> {
  template <typename Packet>
  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void operator()(const Packet& z, Packet& log2_x_hi, Packet& log2_x_lo) {
    // Split the two lowest order constant coefficient into double-word representation.
    constexpr double kC0 = 1.442695041742110273474963832995854318141937255859375e+00;
    constexpr float kC0_hi = static_cast<float>(kC0);
    constexpr float kC0_lo = static_cast<float>(kC0 - static_cast<double>(kC0_hi));
    const Packet c0_hi = pset1<Packet>(kC0_hi);
    const Packet c0_lo = pset1<Packet>(kC0_lo);

    constexpr double kC1 = -7.2134751588268664068692714863573201000690460205078125e-01;
    constexpr float kC1_hi = static_cast<float>(kC1);
    constexpr float kC1_lo = static_cast<float>(kC1 - static_cast<double>(kC1_hi));
    const Packet c1_hi = pset1<Packet>(kC1_hi);
    const Packet c1_lo = pset1<Packet>(kC1_lo);

    constexpr float c[] = {
        9.7010828554630279541015625e-02,  -1.6896486282348632812500000e-01, 1.7200836539268493652343750e-01,
        -1.7892272770404815673828125e-01, 2.0505344867706298828125000e-01,  -2.4046677350997924804687500e-01,
        2.8857553005218505859375000e-01,  -3.6067414283752441406250000e-01, 4.8089790344238281250000000e-01};

    // Evaluate the higher order terms in the polynomial using
    // standard arithmetic.
    const Packet one = pset1<Packet>(1.0f);
    const Packet x = psub(z, one);
    Packet p = ppolevl<Packet, 8>::run(x, c);
    // Evaluate the final two step in Horner's rule using double-word
    // arithmetic.
    Packet p_hi, p_lo;
    twoprod(x, p, p_hi, p_lo);
    fast_twosum(c1_hi, c1_lo, p_hi, p_lo, p_hi, p_lo);
    twoprod(p_hi, p_lo, x, p_hi, p_lo);
    fast_twosum(c0_hi, c0_lo, p_hi, p_lo, p_hi, p_lo);
    // Multiply by x to recover log2(z).
    twoprod(p_hi, p_lo, x, log2_x_hi, log2_x_lo);
  }
};

// This specialization uses a more accurate algorithm to compute log2(x) for
// floats in [1/sqrt(2);sqrt(2)] with a relative accuracy of ~1.27e-18.
// This additional accuracy is needed to counter the error-magnification
// inherent in multiplying by a potentially large exponent in pow(x,y).
// The minimax polynomial used was calculated using the Sollya tool.
// See sollya.org.

template <>
struct accurate_log2<double> {
  template <typename Packet>
  EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE void operator()(const Packet& x, Packet& log2_x_hi, Packet& log2_x_lo) {
    // We use a transformation of variables:
    //    r = c * (x-1) / (x+1),
    // such that
    //    log2(x) = log2((1 + r/c) / (1 - r/c)) = f(r).
    // The function f(r) can be approximated well using an odd polynomial
    // of the form
    //   P(r) = ((Q(r^2) * r^2 + C) * r^2 + 1) * r,
    // For the implementation of log2<double> here, Q is of degree 6 with
    // coefficient represented in working precision (double), while C is a
    // constant represented in extra precision as a double word to achieve
    // full accuracy.
    //
    // The polynomial coefficients were computed by the Sollya script:
    //
    // c = 2 / log(2);
    // trans = c * (x-1)/(x+1);
    // itrans = (1+x/c)/(1-x/c);
    // interval=[trans(sqrt(0.5)); trans(sqrt(2))];
    // print(interval);
    // f = log2(itrans(x));
    // p=fpminimax(f,[|1,3,5,7,9,11,13,15,17|],[|1,DD,double...|],interval,relative,floating);
    const Packet q12 = pset1<Packet>(2.87074255468000586e-9);
    const Packet q10 = pset1<Packet>(2.38957980901884082e-8);
    const Packet q8 = pset1<Packet>(2.31032094540014656e-7);
    const Packet q6 = pset1<Packet>(2.27279857398537278e-6);
    const Packet q4 = pset1<Packet>(2.31271023278625638e-5);
    const Packet q2 = pset1<Packet>(2.47556738444535513e-4);
    const Packet q0 = pset1<Packet>(2.88543873228900172e-3);
    const Packet C_hi = pset1<Packet>(0.0400377511598501157);
    const Packet C_lo = pset1<Packet>(-4.77726582251425391e-19);
    const Packet one = pset1<Packet>(1.0);

    const Packet cst_2_log2e_hi = pset1<Packet>(2.88539008177792677);
    const Packet cst_2_log2e_lo = pset1<Packet>(4.07660016854549667e-17);
    // c * (x - 1)
    Packet t_hi, t_lo;
    // t = c * (x-1)
    twoprod(cst_2_log2e_hi, cst_2_log2e_lo, psub(x, one), t_hi, t_lo);
    // r = c * (x-1) / (x+1),
    Packet r_hi, r_lo;
    doubleword_div_fp(t_hi, t_lo, padd(x, one), r_hi, r_lo);

    // r2 = r * r
    Packet r2_hi, r2_lo;
    twoprod(r_hi, r_lo, r_hi, r_lo, r2_hi, r2_lo);
    // r4 = r2 * r2
    Packet r4_hi, r4_lo;
    twoprod(r2_hi, r2_lo, r2_hi, r2_lo, r4_hi, r4_lo);

    // Evaluate Q(r^2) in working precision. We evaluate it in two parts
    // (even and odd in r^2) to improve instruction level parallelism.
    Packet q_even = pmadd(q12, r4_hi, q8);
    Packet q_odd = pmadd(q10, r4_hi, q6);
    q_even = pmadd(q_even, r4_hi, q4);
    q_odd = pmadd(q_odd, r4_hi, q2);
    q_even = pmadd(q_even, r4_hi, q0);
    Packet q = pmadd(q_odd, r2_hi, q_even);

    // Now evaluate the low order terms of P(x) in double word precision.
    // In the following, due to the increasing magnitude of the coefficients
    // and r being constrained to [-0.5, 0.5] we can use fast_twosum instead
    // of the slower twosum.
    // Q(r^2) * r^2
    Packet p_hi, p_lo;
    twoprod(r2_hi, r2_lo, q, p_hi, p_lo);
    // Q(r^2) * r^2 + C
    Packet p1_hi, p1_lo;
    fast_twosum(C_hi, C_lo, p_hi, p_lo, p1_hi, p1_lo);
    // (Q(r^2) * r^2 + C) * r^2
    Packet p2_hi, p2_lo;
    twoprod(r2_hi, r2_lo, p1_hi, p1_lo, p2_hi, p2_lo);
    // ((Q(r^2) * r^2 + C) * r^2 + 1)
    Packet p3_hi, p3_lo;
    fast_twosum(one, p2_hi, p2_lo, p3_hi, p3_lo);

    // log(z) ~= ((Q(r^2) * r^2 + C) * r^2 + 1) * r
    twoprod(p3_hi, p3_lo, r_hi, r_lo, log2_x_hi, log2_x_lo);
  }
};

// This function implements the non-trivial case of pow(x,y) where x is
// positive and y is (possibly) non-integer.
// Formally, pow(x,y) = exp2(y * log2(x)), where exp2(x) is shorthand for 2^x.
// TODO(rmlarsen): We should probably add this as a packet up 'ppow', to make it
// easier to specialize or turn off for specific types and/or backends.x
template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet generic_pow_impl(const Packet& x, const Packet& y) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  // Split x into exponent e_x and mantissa m_x.
  Packet e_x;
  Packet m_x = pfrexp(x, e_x);

  // Adjust m_x to lie in [1/sqrt(2):sqrt(2)] to minimize absolute error in log2(m_x).
  constexpr Scalar sqrt_half = Scalar(0.70710678118654752440);
  const Packet m_x_scale_mask = pcmp_lt(m_x, pset1<Packet>(sqrt_half));
  m_x = pselect(m_x_scale_mask, pmul(pset1<Packet>(Scalar(2)), m_x), m_x);
  e_x = pselect(m_x_scale_mask, psub(e_x, pset1<Packet>(Scalar(1))), e_x);

  // Compute log2(m_x) with 6 extra bits of accuracy.
  Packet rx_hi, rx_lo;
  accurate_log2<Scalar>()(m_x, rx_hi, rx_lo);

  // Compute the two terms {y * e_x, y * r_x} in f = y * log2(x) with doubled
  // precision using double word arithmetic.
  Packet f1_hi, f1_lo, f2_hi, f2_lo;
  twoprod(e_x, y, f1_hi, f1_lo);
  twoprod(rx_hi, rx_lo, y, f2_hi, f2_lo);
  // Sum the two terms in f using double word arithmetic. We know
  // that |e_x| > |log2(m_x)|, except for the case where e_x==0.
  // This means that we can use fast_twosum(f1,f2).
  // In the case e_x == 0, e_x * y = f1 = 0, so we don't lose any
  // accuracy by violating the assumption of fast_twosum, because
  // it's a no-op.
  Packet f_hi, f_lo;
  fast_twosum(f1_hi, f1_lo, f2_hi, f2_lo, f_hi, f_lo);

  // Split f into integer and fractional parts.
  Packet n_z, r_z;
  absolute_split(f_hi, n_z, r_z);
  r_z = padd(r_z, f_lo);
  Packet n_r;
  absolute_split(r_z, n_r, r_z);
  n_z = padd(n_z, n_r);

  // We now have an accurate split of f = n_z + r_z and can compute
  //   x^y = 2**{n_z + r_z) = exp2(r_z) * 2**{n_z}.
  // Multiplication by the second factor can be done exactly using pldexp(), since
  // it is an integer power of 2.
  const Packet e_r = generic_exp2(r_z);

  // Since we know that e_r is in [1/sqrt(2); sqrt(2)], we can use the fast version
  // of pldexp to multiply by 2**{n_z} when |n_z| is sufficiently small.
  constexpr Scalar kPldExpThresh = std::numeric_limits<Scalar>::max_exponent - 2;
  const Packet pldexp_fast_unsafe = pcmp_lt(pset1<Packet>(kPldExpThresh), pabs(n_z));
  if (predux_any(pldexp_fast_unsafe)) {
    return pldexp(e_r, n_z);
  }
  return pldexp_fast(e_r, n_z);
}

// Generic implementation of pow(x,y).
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS std::enable_if_t<!is_scalar<Packet>::value, Packet> generic_pow(
    const Packet& x, const Packet& y) {
  typedef typename unpacket_traits<Packet>::type Scalar;

  const Packet cst_inf = pset1<Packet>(NumTraits<Scalar>::infinity());
  const Packet cst_zero = pset1<Packet>(Scalar(0));
  const Packet cst_one = pset1<Packet>(Scalar(1));
  const Packet cst_nan = pset1<Packet>(NumTraits<Scalar>::quiet_NaN());

  const Packet x_abs = pabs(x);
  Packet pow = generic_pow_impl(x_abs, y);

  // In the following we enforce the special case handling prescribed in
  // https://en.cppreference.com/w/cpp/numeric/math/pow.

  // Predicates for sign and magnitude of x.
  const Packet x_is_negative = pcmp_lt(x, cst_zero);
  const Packet x_is_zero = pcmp_eq(x, cst_zero);
  const Packet x_is_one = pcmp_eq(x, cst_one);
  const Packet x_has_signbit = psignbit(x);
  const Packet x_abs_gt_one = pcmp_lt(cst_one, x_abs);
  const Packet x_abs_is_inf = pcmp_eq(x_abs, cst_inf);

  // Predicates for sign and magnitude of y.
  const Packet y_abs = pabs(y);
  const Packet y_abs_is_inf = pcmp_eq(y_abs, cst_inf);
  const Packet y_is_negative = pcmp_lt(y, cst_zero);
  const Packet y_is_zero = pcmp_eq(y, cst_zero);
  const Packet y_is_one = pcmp_eq(y, cst_one);
  // Predicates for whether y is integer and odd/even.
  const Packet y_is_int = pandnot(pcmp_eq(pfloor(y), y), y_abs_is_inf);
  const Packet y_div_2 = pmul(y, pset1<Packet>(Scalar(0.5)));
  const Packet y_is_even = pcmp_eq(pround(y_div_2), y_div_2);
  const Packet y_is_odd_int = pandnot(y_is_int, y_is_even);
  // Smallest exponent for which (1 + epsilon) overflows to infinity.
  constexpr Scalar huge_exponent =
      (NumTraits<Scalar>::max_exponent() * Scalar(EIGEN_LN2)) / NumTraits<Scalar>::epsilon();
  const Packet y_abs_is_huge = pcmp_le(pset1<Packet>(huge_exponent), y_abs);

  // *  pow(base, exp) returns NaN if base is finite and negative
  //    and exp is finite and non-integer.
  pow = pselect(pandnot(x_is_negative, y_is_int), cst_nan, pow);

  // * pow(±0, exp), where exp is negative, finite, and is an even integer or
  // a non-integer, returns +∞
  // * pow(±0, exp), where exp is positive non-integer or a positive even
  // integer, returns +0
  // * pow(+0, exp), where exp is a negative odd integer, returns +∞
  // * pow(-0, exp), where exp is a negative odd integer, returns -∞
  // * pow(+0, exp), where exp is a positive odd integer, returns +0
  // * pow(-0, exp), where exp is a positive odd integer, returns -0
  // Sign is flipped by the rule below.
  pow = pselect(x_is_zero, pselect(y_is_negative, cst_inf, cst_zero), pow);

  // pow(base, exp) returns -pow(abs(base), exp) if base has the sign bit set,
  // and exp is an odd integer exponent.
  pow = pselect(pand(x_has_signbit, y_is_odd_int), pnegate(pow), pow);

  // * pow(base, -∞) returns +∞ for any |base|<1
  // * pow(base, -∞) returns +0 for any |base|>1
  // * pow(base, +∞) returns +0 for any |base|<1
  // * pow(base, +∞) returns +∞ for any |base|>1
  // * pow(±0, -∞) returns +∞
  // * pow(-1, +-∞) = 1
  Packet inf_y_val = pselect(por(pand(y_is_negative, x_is_zero), pxor(y_is_negative, x_abs_gt_one)), cst_inf, cst_zero);
  inf_y_val = pselect(pcmp_eq(x, pset1<Packet>(Scalar(-1.0))), cst_one, inf_y_val);
  pow = pselect(y_abs_is_huge, inf_y_val, pow);

  // * pow(+∞, exp) returns +0 for any negative exp
  // * pow(+∞, exp) returns +∞ for any positive exp
  // * pow(-∞, exp) returns -0 if exp is a negative odd integer.
  // * pow(-∞, exp) returns +0 if exp is a negative non-integer or negative
  //     even integer.
  // * pow(-∞, exp) returns -∞ if exp is a positive odd integer.
  // * pow(-∞, exp) returns +∞ if exp is a positive non-integer or positive
  //     even integer.
  auto x_pos_inf_value = pselect(y_is_negative, cst_zero, cst_inf);
  auto x_neg_inf_value = pselect(y_is_odd_int, pnegate(x_pos_inf_value), x_pos_inf_value);
  pow = pselect(x_abs_is_inf, pselect(x_is_negative, x_neg_inf_value, x_pos_inf_value), pow);

  // All cases of NaN inputs return NaN, except the two below.
  pow = pselect(por(pisnan(x), pisnan(y)), cst_nan, pow);

  // * pow(base, 1) returns base.
  // * pow(base, +/-0) returns 1, regardless of base, even NaN.
  // * pow(+1, exp) returns 1, regardless of exponent, even NaN.
  pow = pselect(y_is_one, x, pselect(por(x_is_one, y_is_zero), cst_one, pow));

  return pow;
}

template <typename Scalar>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS std::enable_if_t<is_scalar<Scalar>::value, Scalar> generic_pow(
    const Scalar& x, const Scalar& y) {
  return numext::pow(x, y);
}

namespace unary_pow {

template <typename ScalarExponent, bool IsInteger = NumTraits<ScalarExponent>::IsInteger>
struct exponent_helper {
  using safe_abs_type = ScalarExponent;
  static constexpr ScalarExponent one_half = ScalarExponent(0.5);
  // these routines assume that exp is an integer stored as a floating point type
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ScalarExponent safe_abs(const ScalarExponent& exp) {
    return numext::abs(exp);
  }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE bool is_odd(const ScalarExponent& exp) {
    eigen_assert(((numext::isfinite)(exp) && exp == numext::floor(exp)) && "exp must be an integer");
    ScalarExponent exp_div_2 = exp * one_half;
    ScalarExponent floor_exp_div_2 = numext::floor(exp_div_2);
    return exp_div_2 != floor_exp_div_2;
  }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE ScalarExponent floor_div_two(const ScalarExponent& exp) {
    ScalarExponent exp_div_2 = exp * one_half;
    return numext::floor(exp_div_2);
  }
};

template <typename ScalarExponent>
struct exponent_helper<ScalarExponent, true> {
  // if `exp` is a signed integer type, cast it to its unsigned counterpart to safely store its absolute value
  // consider the (rare) case where `exp` is an int32_t: abs(-2147483648) != 2147483648
  using safe_abs_type = typename numext::get_integer_by_size<sizeof(ScalarExponent)>::unsigned_type;
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE safe_abs_type safe_abs(const ScalarExponent& exp) {
    ScalarExponent mask = numext::signbit(exp);
    safe_abs_type result = safe_abs_type(exp ^ mask);
    return result + safe_abs_type(ScalarExponent(1) & mask);
  }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE bool is_odd(const safe_abs_type& exp) {
    return exp % safe_abs_type(2) != safe_abs_type(0);
  }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE safe_abs_type floor_div_two(const safe_abs_type& exp) {
    return exp >> safe_abs_type(1);
  }
};

template <typename Packet, typename ScalarExponent,
          bool ReciprocateIfExponentIsNegative =
              !NumTraits<typename unpacket_traits<Packet>::type>::IsInteger && NumTraits<ScalarExponent>::IsSigned>
struct reciprocate {
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run(const Packet& x, const ScalarExponent& exponent) {
    using Scalar = typename unpacket_traits<Packet>::type;
    const Packet cst_pos_one = pset1<Packet>(Scalar(1));
    return exponent < 0 ? pdiv(cst_pos_one, x) : x;
  }
};

template <typename Packet, typename ScalarExponent>
struct reciprocate<Packet, ScalarExponent, false> {
  // pdiv not defined, nor necessary for integer base types
  // if the exponent is unsigned, then the exponent cannot be negative
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run(const Packet& x, const ScalarExponent&) { return x; }
};

template <typename Packet, typename ScalarExponent>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet int_pow(const Packet& x, const ScalarExponent& exponent) {
  using Scalar = typename unpacket_traits<Packet>::type;
  using ExponentHelper = exponent_helper<ScalarExponent>;
  using AbsExponentType = typename ExponentHelper::safe_abs_type;
  const Packet cst_pos_one = pset1<Packet>(Scalar(1));
  if (exponent == ScalarExponent(0)) return cst_pos_one;

  Packet result = reciprocate<Packet, ScalarExponent>::run(x, exponent);
  Packet y = cst_pos_one;
  AbsExponentType m = ExponentHelper::safe_abs(exponent);

  while (m > 1) {
    bool odd = ExponentHelper::is_odd(m);
    if (odd) y = pmul(y, result);
    result = pmul(result, result);
    m = ExponentHelper::floor_div_two(m);
  }

  return pmul(y, result);
}

template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE std::enable_if_t<!is_scalar<Packet>::value, Packet> gen_pow(
    const Packet& x, const typename unpacket_traits<Packet>::type& exponent) {
  const Packet exponent_packet = pset1<Packet>(exponent);
  return generic_pow_impl(x, exponent_packet);
}

template <typename Scalar>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE std::enable_if_t<is_scalar<Scalar>::value, Scalar> gen_pow(
    const Scalar& x, const Scalar& exponent) {
  return numext::pow(x, exponent);
}

template <typename Packet, typename ScalarExponent>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet handle_nonint_nonint_errors(const Packet& x, const Packet& powx,
                                                                         const ScalarExponent& exponent) {
  using Scalar = typename unpacket_traits<Packet>::type;

  // non-integer base and exponent case
  const Packet cst_pos_zero = pzero(x);
  const Packet cst_pos_one = pset1<Packet>(Scalar(1));
  const Packet cst_pos_inf = pset1<Packet>(NumTraits<Scalar>::infinity());
  const Packet cst_true = ptrue<Packet>(x);

  const bool exponent_is_not_fin = !(numext::isfinite)(exponent);
  const bool exponent_is_neg = exponent < ScalarExponent(0);
  const bool exponent_is_pos = exponent > ScalarExponent(0);

  const Packet exp_is_not_fin = exponent_is_not_fin ? cst_true : cst_pos_zero;
  const Packet exp_is_neg = exponent_is_neg ? cst_true : cst_pos_zero;
  const Packet exp_is_pos = exponent_is_pos ? cst_true : cst_pos_zero;
  const Packet exp_is_inf = pand(exp_is_not_fin, por(exp_is_neg, exp_is_pos));
  const Packet exp_is_nan = pandnot(exp_is_not_fin, por(exp_is_neg, exp_is_pos));

  const Packet x_is_le_zero = pcmp_le(x, cst_pos_zero);
  const Packet x_is_ge_zero = pcmp_le(cst_pos_zero, x);
  const Packet x_is_zero = pand(x_is_le_zero, x_is_ge_zero);

  const Packet abs_x = pabs(x);
  const Packet abs_x_is_le_one = pcmp_le(abs_x, cst_pos_one);
  const Packet abs_x_is_ge_one = pcmp_le(cst_pos_one, abs_x);
  const Packet abs_x_is_inf = pcmp_eq(abs_x, cst_pos_inf);
  const Packet abs_x_is_one = pand(abs_x_is_le_one, abs_x_is_ge_one);

  Packet pow_is_inf_if_exp_is_neg = por(x_is_zero, pand(abs_x_is_le_one, exp_is_inf));
  Packet pow_is_inf_if_exp_is_pos = por(abs_x_is_inf, pand(abs_x_is_ge_one, exp_is_inf));
  Packet pow_is_one = pand(abs_x_is_one, por(exp_is_inf, x_is_ge_zero));

  Packet result = powx;
  result = por(x_is_le_zero, result);
  result = pselect(pow_is_inf_if_exp_is_neg, pand(cst_pos_inf, exp_is_neg), result);
  result = pselect(pow_is_inf_if_exp_is_pos, pand(cst_pos_inf, exp_is_pos), result);
  result = por(exp_is_nan, result);
  result = pselect(pow_is_one, cst_pos_one, result);
  return result;
}

template <typename Packet, typename ScalarExponent,
          std::enable_if_t<NumTraits<typename unpacket_traits<Packet>::type>::IsSigned, bool> = true>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet handle_negative_exponent(const Packet& x, const ScalarExponent& exponent) {
  using Scalar = typename unpacket_traits<Packet>::type;

  // signed integer base, signed integer exponent case

  // This routine handles negative exponents.
  // The return value is either 0, 1, or -1.
  const Packet cst_pos_one = pset1<Packet>(Scalar(1));
  const bool exponent_is_odd = exponent % ScalarExponent(2) != ScalarExponent(0);
  const Packet exp_is_odd = exponent_is_odd ? ptrue<Packet>(x) : pzero<Packet>(x);

  const Packet abs_x = pabs(x);
  const Packet abs_x_is_one = pcmp_eq(abs_x, cst_pos_one);

  Packet result = pselect(exp_is_odd, x, abs_x);
  result = pselect(abs_x_is_one, result, pzero<Packet>(x));
  return result;
}

template <typename Packet, typename ScalarExponent,
          std::enable_if_t<!NumTraits<typename unpacket_traits<Packet>::type>::IsSigned, bool> = true>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet handle_negative_exponent(const Packet& x, const ScalarExponent&) {
  using Scalar = typename unpacket_traits<Packet>::type;

  // unsigned integer base, signed integer exponent case

  // This routine handles negative exponents.
  // The return value is either 0 or 1

  const Scalar pos_one = Scalar(1);

  const Packet cst_pos_one = pset1<Packet>(pos_one);

  const Packet x_is_one = pcmp_eq(x, cst_pos_one);

  return pand(x_is_one, x);
}

}  // end namespace unary_pow

template <typename Packet, typename ScalarExponent,
          bool BaseIsIntegerType = NumTraits<typename unpacket_traits<Packet>::type>::IsInteger,
          bool ExponentIsIntegerType = NumTraits<ScalarExponent>::IsInteger,
          bool ExponentIsSigned = NumTraits<ScalarExponent>::IsSigned>
struct unary_pow_impl;

template <typename Packet, typename ScalarExponent, bool ExponentIsSigned>
struct unary_pow_impl<Packet, ScalarExponent, false, false, ExponentIsSigned> {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run(const Packet& x, const ScalarExponent& exponent) {
    const bool exponent_is_integer = (numext::isfinite)(exponent) && numext::round(exponent) == exponent;
    if (exponent_is_integer) {
      // The simple recursive doubling implementation is only accurate to 3 ulps
      // for integer exponents in [-3:7]. Since this is a common case, we
      // specialize it here.
      bool use_repeated_squaring =
          (exponent <= ScalarExponent(7) && (!ExponentIsSigned || exponent >= ScalarExponent(-3)));
      return use_repeated_squaring ? unary_pow::int_pow(x, exponent) : generic_pow(x, pset1<Packet>(exponent));
    } else {
      Packet result = unary_pow::gen_pow(x, exponent);
      result = unary_pow::handle_nonint_nonint_errors(x, result, exponent);
      return result;
    }
  }
};

template <typename Packet, typename ScalarExponent, bool ExponentIsSigned>
struct unary_pow_impl<Packet, ScalarExponent, false, true, ExponentIsSigned> {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run(const Packet& x, const ScalarExponent& exponent) {
    return unary_pow::int_pow(x, exponent);
  }
};

template <typename Packet, typename ScalarExponent>
struct unary_pow_impl<Packet, ScalarExponent, true, true, true> {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run(const Packet& x, const ScalarExponent& exponent) {
    if (exponent < ScalarExponent(0)) {
      return unary_pow::handle_negative_exponent(x, exponent);
    } else {
      return unary_pow::int_pow(x, exponent);
    }
  }
};

template <typename Packet, typename ScalarExponent>
struct unary_pow_impl<Packet, ScalarExponent, true, true, false> {
  typedef typename unpacket_traits<Packet>::type Scalar;
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run(const Packet& x, const ScalarExponent& exponent) {
    return unary_pow::int_pow(x, exponent);
  }
};

// This function computes exp2(x) = exp(ln(2) * x).
// To improve accuracy, the product ln(2)*x is computed using the twoprod
// algorithm, such that ln(2) * x = p_hi + p_lo holds exactly. Then exp2(x) is
// computed as exp2(x) = exp(p_hi) * exp(p_lo) ~= exp(p_hi) * (1 + p_lo). This
// correction step this reduces the maximum absolute error as follows:
//
// type   | max error (simple product) | max error (twoprod) |
// -----------------------------------------------------------
// float  |       35 ulps              |       4 ulps        |
// double |      363 ulps              |     110 ulps        |
//
template <typename Packet>
EIGEN_DEFINE_FUNCTION_ALLOWING_MULTIPLE_DEFINITIONS Packet generic_exp2(const Packet& _x) {
  typedef typename unpacket_traits<Packet>::type Scalar;
  constexpr int max_exponent = std::numeric_limits<Scalar>::max_exponent;
  constexpr int digits = std::numeric_limits<Scalar>::digits;
  constexpr Scalar max_cap = Scalar(max_exponent + 1);
  constexpr Scalar min_cap = -Scalar(max_exponent + digits - 1);
  Packet x = pmax(pmin(_x, pset1<Packet>(max_cap)), pset1<Packet>(min_cap));
  Packet p_hi, p_lo;
  twoprod(pset1<Packet>(Scalar(EIGEN_LN2)), x, p_hi, p_lo);
  Packet exp2_hi = pexp(p_hi);
  Packet exp2_lo = padd(pset1<Packet>(Scalar(1)), p_lo);
  return pmul(exp2_hi, exp2_lo);
}

template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet generic_rint(const Packet& a) {
  using Scalar = typename unpacket_traits<Packet>::type;
  using IntType = typename numext::get_integer_by_size<sizeof(Scalar)>::signed_type;
  // Adds and subtracts signum(a) * 2^kMantissaBits to force rounding.
  const IntType kLimit = IntType(1) << (NumTraits<Scalar>::digits() - 1);
  const Packet cst_limit = pset1<Packet>(static_cast<Scalar>(kLimit));
  Packet abs_a = pabs(a);
  Packet sign_a = pandnot(a, abs_a);
  Packet rint_a = padd(abs_a, cst_limit);
  // Don't compile-away addition and subtraction.
  EIGEN_OPTIMIZATION_BARRIER(rint_a);
  rint_a = psub(rint_a, cst_limit);
  rint_a = por(rint_a, sign_a);
  // If greater than limit (or NaN), simply return a.
  Packet mask = pcmp_lt(abs_a, cst_limit);
  Packet result = pselect(mask, rint_a, a);
  return result;
}

template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet generic_floor(const Packet& a) {
  using Scalar = typename unpacket_traits<Packet>::type;
  const Packet cst_1 = pset1<Packet>(Scalar(1));
  Packet rint_a = generic_rint(a);
  // if a < rint(a), then rint(a) == ceil(a)
  Packet mask = pcmp_lt(a, rint_a);
  Packet offset = pand(cst_1, mask);
  Packet result = psub(rint_a, offset);
  return result;
}

template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet generic_ceil(const Packet& a) {
  using Scalar = typename unpacket_traits<Packet>::type;
  const Packet cst_1 = pset1<Packet>(Scalar(1));
  const Packet sign_mask = pset1<Packet>(static_cast<Scalar>(-0.0));
  Packet rint_a = generic_rint(a);
  // if rint(a) < a, then rint(a) == floor(a)
  Packet mask = pcmp_lt(rint_a, a);
  Packet offset = pand(cst_1, mask);
  Packet result = padd(rint_a, offset);
  // Signed zero must remain signed (e.g. ceil(-0.02) == -0).
  result = por(result, pand(sign_mask, a));
  return result;
}

template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet generic_trunc(const Packet& a) {
  Packet abs_a = pabs(a);
  Packet sign_a = pandnot(a, abs_a);
  Packet floor_abs_a = generic_floor(abs_a);
  Packet result = por(floor_abs_a, sign_a);
  return result;
}

template <typename Packet>
EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet generic_round(const Packet& a) {
  using Scalar = typename unpacket_traits<Packet>::type;
  const Packet cst_half = pset1<Packet>(Scalar(0.5));
  const Packet cst_1 = pset1<Packet>(Scalar(1));
  Packet abs_a = pabs(a);
  Packet sign_a = pandnot(a, abs_a);
  Packet floor_abs_a = generic_floor(abs_a);
  Packet diff = psub(abs_a, floor_abs_a);
  Packet mask = pcmp_le(cst_half, diff);
  Packet offset = pand(cst_1, mask);
  Packet result = padd(floor_abs_a, offset);
  result = por(result, sign_a);
  return result;
}

template <typename Packet>
struct nearest_integer_packetop_impl<Packet, /*IsScalar*/ false, /*IsInteger*/ false> {
  using Scalar = typename unpacket_traits<Packet>::type;
  static_assert(packet_traits<Scalar>::HasRound, "Generic nearest integer functions are disabled for this type.");
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_floor(const Packet& x) { return generic_floor(x); }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_ceil(const Packet& x) { return generic_ceil(x); }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_rint(const Packet& x) { return generic_rint(x); }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_round(const Packet& x) { return generic_round(x); }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_trunc(const Packet& x) { return generic_trunc(x); }
};

template <typename Packet>
struct nearest_integer_packetop_impl<Packet, /*IsScalar*/ false, /*IsInteger*/ true> {
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_floor(const Packet& x) { return x; }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_ceil(const Packet& x) { return x; }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_rint(const Packet& x) { return x; }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_round(const Packet& x) { return x; }
  static EIGEN_DEVICE_FUNC EIGEN_STRONG_INLINE Packet run_trunc(const Packet& x) { return x; }
};

}  // end namespace internal
}  // end namespace Eigen

#endif  // EIGEN_ARCH_GENERIC_PACKET_MATH_FUNCTIONS_H
