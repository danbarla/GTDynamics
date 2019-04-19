/**
 * @file  testTwistAccelFactor.cpp
 * @brief test twistAccel factor
 * @Author: Frank Dellaert and Mandy Xie
 */
#include <gtsam/base/numericalDerivative.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>

#include <CppUnitLite/TestHarness.h>
#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>

#include <DHLink.h>
#include <TwistAccelFactor.h>

#include <cmath>
#include <iostream>

using namespace std;
using namespace gtsam;
using namespace manipulator;

namespace example {
// R link example
DH_Link dh_r = DH_Link(0, 0, 2, 0, 'R', 1, Point3(-1, 0, 0), Vector3(0, 0, 0),
                       -180, 10, 180);
// nosie model
noiseModel::Gaussian::shared_ptr cost_model =
    noiseModel::Gaussian::Covariance(Matrix::Identity(6, 6));
Key qKey = Symbol('q', 0), qVelKey = Symbol('j', 0), qAccelKey = Symbol('a', 0),
    twistKey = Symbol('V', 0), twistAccel_i_key = Symbol('T', 0),
    twistAccel_j_key = Symbol('T', 1);
}  // namespace example

/**
 * Test twistAccel factor for stationary case
 */
TEST(TwistAccelFactor, error) {
  // Create all factors
  Pose3 jMi = Pose3(Rot3(), Point3(-1, 0, 0));
  Vector6 screw_axis;
  screw_axis << 0, 0, 1, 0, 1, 0;

  TwistAccelFactor factor(example::twistKey, example::twistAccel_i_key,
                          example::twistAccel_j_key, example::qKey,
                          example::qVelKey, example::qAccelKey,
                          example::cost_model, jMi, screw_axis);
  double q = M_PI / 4, qVel = 10, qAccel = 10;
  Vector6 twist, twistAccel_i, twistAccel_j;
  twist << 0, 0, 0, 0, 0, 0;
  twistAccel_i << 0, 0, 10, 0, 10, 0;
  twistAccel_j << 0, 0, 20, 7.07106781, 27.0710678, 0;
  Vector6 actual_errors, expected_errors;
  Matrix actual_H1, actual_H2, actual_H3, actual_H4, actual_H5, actual_H6,
      expected_H1, expected_H2, expected_H3, expected_H4, expected_H5,
      expected_H6;

  actual_errors = factor.evaluateError(
      twist, twistAccel_i, twistAccel_j, q, qVel, qAccel, actual_H1, actual_H2,
      actual_H3, actual_H4, actual_H5, actual_H6);
  expected_errors << 0, 0, 0, 0, 0, 0;
  expected_H1 = numericalDerivative11(
      boost::function<Vector(const Vector6 &)>(
          boost::bind(&TwistAccelFactor::evaluateError, factor, _1,
                      twistAccel_i, twistAccel_j, q, qVel, qAccel)),
      twist, 1e-6);
  expected_H2 = numericalDerivative11(
      boost::function<Vector(const Vector6 &)>(
          boost::bind(&TwistAccelFactor::evaluateError, factor, twist, _1,
                      twistAccel_j, q, qVel, qAccel)),
      twistAccel_i, 1e-6);
  expected_H3 = numericalDerivative11(
      boost::function<Vector(const Vector6 &)>(
          boost::bind(&TwistAccelFactor::evaluateError, factor, twist,
                      twistAccel_i, _1, q, qVel, qAccel)),
      twistAccel_j, 1e-6);
  expected_H4 =
      numericalDerivative11(boost::function<Vector(const double &)>(boost::bind(
                                &TwistAccelFactor::evaluateError, factor, twist,
                                twistAccel_i, twistAccel_j, _1, qVel, qAccel)),
                            q, 1e-6);
  expected_H5 =
      numericalDerivative11(boost::function<Vector(const double &)>(boost::bind(
                                &TwistAccelFactor::evaluateError, factor, twist,
                                twistAccel_i, twistAccel_j, q, _1, qAccel)),
                            qVel, 1e-6);
  expected_H6 =
      numericalDerivative11(boost::function<Vector(const double &)>(boost::bind(
                                &TwistAccelFactor::evaluateError, factor, twist,
                                twistAccel_i, twistAccel_j, q, qVel, _1)),
                            qAccel, 1e-6);
  EXPECT(assert_equal(expected_errors, actual_errors, 1e-6));
  EXPECT(assert_equal(expected_H1, actual_H1, 1e-6));
  EXPECT(assert_equal(expected_H2, actual_H2, 1e-6));
  EXPECT(assert_equal(expected_H3, actual_H3, 1e-6));
  EXPECT(assert_equal(expected_H4, actual_H4, 1e-6));
  EXPECT(assert_equal(expected_H5, actual_H5, 1e-6));
  EXPECT(assert_equal(expected_H6, actual_H6, 1e-6));
}

/**
 * Test twistAccel factor for stationary case
 */
TEST(TwistAccelFactor, error_1) {
  // Create all factors
  Pose3 jMi = Pose3(Rot3(), Point3(-1, 0, 0));
  Vector6 screw_axis = (Vector(6) << 0, 0, 1, 0, 1, 0).finished();

  TwistAccelFactor factor(example::twistKey, example::twistAccel_i_key,
                          example::twistAccel_j_key, example::qKey,
                          example::qVelKey, example::qAccelKey,
                          example::cost_model, jMi, screw_axis);
  double q = 0, qVel = 0, qAccel = -9.8;
  Vector6 twist, twistAccel_i, twistAccel_j;
  twist = (Vector(6) << 0, 0, 0, 0, 0, 0).finished();
  twistAccel_i = (Vector(6) << 0, 0, 0, 0, 9.8, 0).finished();
  twistAccel_j = (Vector(6) << 0, 0, -9.8, 0, 0, 0).finished();
  Vector6 actual_errors, expected_errors;
  Matrix actual_H1, actual_H2, actual_H3, actual_H4, actual_H5, actual_H6,
      expected_H1, expected_H2, expected_H3, expected_H4, expected_H5,
      expected_H6;

  actual_errors = factor.evaluateError(
      twist, twistAccel_i, twistAccel_j, q, qVel, qAccel, actual_H1, actual_H2,
      actual_H3, actual_H4, actual_H5, actual_H6);
  expected_errors = (Vector(6) << 0, 0, 0, 0, 0, 0).finished();
  expected_H1 = numericalDerivative11(
      boost::function<Vector(const Vector6 &)>(
          boost::bind(&TwistAccelFactor::evaluateError, factor, _1,
                      twistAccel_i, twistAccel_j, q, qVel, qAccel)),
      twist, 1e-6);
  expected_H2 = numericalDerivative11(
      boost::function<Vector(const Vector6 &)>(
          boost::bind(&TwistAccelFactor::evaluateError, factor, twist, _1,
                      twistAccel_j, q, qVel, qAccel)),
      twistAccel_i, 1e-6);
  expected_H3 = numericalDerivative11(
      boost::function<Vector(const Vector6 &)>(
          boost::bind(&TwistAccelFactor::evaluateError, factor, twist,
                      twistAccel_i, _1, q, qVel, qAccel)),
      twistAccel_j, 1e-6);
  expected_H4 =
      numericalDerivative11(boost::function<Vector(const double &)>(boost::bind(
                                &TwistAccelFactor::evaluateError, factor, twist,
                                twistAccel_i, twistAccel_j, _1, qVel, qAccel)),
                            q, 1e-6);
  expected_H5 =
      numericalDerivative11(boost::function<Vector(const double &)>(boost::bind(
                                &TwistAccelFactor::evaluateError, factor, twist,
                                twistAccel_i, twistAccel_j, q, _1, qAccel)),
                            qVel, 1e-6);
  expected_H6 =
      numericalDerivative11(boost::function<Vector(const double &)>(boost::bind(
                                &TwistAccelFactor::evaluateError, factor, twist,
                                twistAccel_i, twistAccel_j, q, qVel, _1)),
                            qAccel, 1e-6);
  EXPECT(assert_equal(expected_errors, actual_errors, 1e-6));
  EXPECT(assert_equal(expected_H1, actual_H1, 1e-6));
  EXPECT(assert_equal(expected_H2, actual_H2, 1e-6));
  EXPECT(assert_equal(expected_H3, actual_H3, 1e-6));
  EXPECT(assert_equal(expected_H4, actual_H4, 1e-6));
  EXPECT(assert_equal(expected_H5, actual_H5, 1e-6));
  EXPECT(assert_equal(expected_H6, actual_H6, 1e-6));
}

int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
