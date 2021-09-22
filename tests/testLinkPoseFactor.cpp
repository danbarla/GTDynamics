/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  testLinkPoseFactor.cpp
 * @brief Test LinkPoseFactor.
 * @author Varun Agrawal
 */

#include <CppUnitLite/TestHarness.h>
#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>
#include <gtsam/base/numericalDerivative.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/nonlinear/factorTesting.h>

#include <iostream>

#include "gtdynamics/factors/LinkPoseFactor.h"
#include "gtdynamics/universal_robot/RevoluteJoint.h"
#include "gtdynamics/universal_robot/RobotModels.h"
#include "gtdynamics/utils/values.h"

using namespace gtdynamics;
using namespace gtsam;
using gtsam::assert_equal;

const Key key0 = gtdynamics::internal::PoseKey(0),
          key1 = gtdynamics::internal::PoseKey(1);

auto kModel = noiseModel::Isotropic::Sigma(6, 0.1);

auto robot = simple_rr::getRobot();
size_t t = 0;

gtsam::Values zeroValues() {
  gtsam::Values joint_angles;
  for (auto&& joint : robot.joints()) {
    InsertJointAngle(&joint_angles, joint->id(), 0.0);
  }
  return joint_angles;
}

TEST(LinkPoseFactor, Constructor) {
  LinkPoseFactor<RevoluteJoint>(key0, key1, kModel, robot.joints()[0], 0.0, t);
}

TEST(LinkPoseFactor, Error) {
  LinkPoseFactor<RevoluteJoint> factor(key0, key1, kModel, robot.joints()[0],
                                       0.0, t);

  auto link0 = robot.links()[0];
  auto link1 = robot.links()[1];

  // Error at rest
  Vector error = factor.evaluateError(link0->bMcom(), link1->bMcom());
  EXPECT(assert_equal(Vector::Zero(6), error, 1e-9));

  // Error when the elbow is bent to 90 degrees
  LinkPoseFactor<RevoluteJoint> factor2(key0, key1, kModel, robot.joints()[0],
                                        M_PI, t);

  Pose3 wTl1(Rot3::Rz(M_PI), gtsam::Point3(0, 0, 0.5));
  Vector error2 = factor2.evaluateError(link0->bMcom(), wTl1);

  EXPECT(assert_equal(Vector::Zero(6), error2, 1e-9));
}

TEST(LinkPoseFactor, Jacobians) {
  LinkPoseFactor<RevoluteJoint> factor(key0, key1, kModel, robot.joints()[0],
                                       0.0, t);

  auto link0 = robot.links()[0];
  auto link1 = robot.links()[1];

  Pose3 wTl0 = link0->bMcom(), wTl1 = link1->bMcom();

  Values values;
  InsertPose(&values, link0->id(), wTl0);
  InsertPose(&values, link1->id(), wTl1);

  // Check Jacobians when joint angle is 0.0
  EXPECT_CORRECT_FACTOR_JACOBIANS(factor, values, 1e-7, 1e-5);

  // Non-trivial joint angle
  double angle = M_PI;
  LinkPoseFactor<RevoluteJoint> factor2(key0, key1, kModel, robot.joints()[0],
                                        angle, t);
  wTl1 = Pose3(Rot3::Rz(angle), gtsam::Point3(0, 0, 0.5));
  values.clear();
  InsertPose(&values, link0->id(), wTl0);
  InsertPose(&values, link1->id(), wTl1);

  Vector error2 = factor2.evaluateError(wTl0, wTl1);
  EXPECT(assert_equal(Vector::Zero(6), error2, 1e-9));
  // values.print("", GTDKeyFormatter);
  // Check Jacobians
  EXPECT_CORRECT_FACTOR_JACOBIANS(factor2, values, 1e-7, 1e-3);
}

TEST(ForwardKinematicsFactor, ArbitraryTime) {
  // Robot robot =
  //     CreateRobotFromFile(kUrdfPath + std::string("/test/simple_urdf.urdf"));
  // std::string base_link = "l1", end_link = "l2";

  // size_t t = 81;
  // Values joint_angles;
  // InsertJointAngle(&joint_angles, robot.joint("j1")->id(), t, M_PI_2);

  // ForwardKinematicsFactor factor(key0, key1, robot, base_link, end_link,
  //                                joint_angles, kModel, t);

  // Pose3 actual = factor.measured();
  // Values fk = robot.forwardKinematics(joint_angles, t, base_link);
  // Pose3 wTl1 = Pose(fk, robot.link("l1")->id(), t),
  //       wTl2 = Pose(fk, robot.link("l2")->id(), t);
  // Pose3 expected = wTl1.between(wTl2);
  // EXPECT(assert_equal(expected, actual));
}

int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
