/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  testLink.cpp
 * @brief Test Link class.
 * @author Frank Dellaert, Mandy Xie, Alejandro Escontrela, and Yetong Zhang
 */

#include <CppUnitLite/TestHarness.h>
#include <gtdynamics/universal_robot/Link.h>
#include <gtdynamics/universal_robot/RevoluteJoint.h>
#include <gtdynamics/universal_robot/RobotModels.h>
#include <gtdynamics/utils/utils.h>
#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>
#include <gtsam/base/serializationTestHelpers.h>
#include <gtsam/linear/VectorValues.h>

using namespace gtdynamics;
using gtsam::assert_equal;
using gtsam::Point3;
using gtsam::Pose3;
using gtsam::Rot3;

using namespace gtsam::serializationTestHelpers;

// Construct the same link via Params and ensure all values are as expected.
TEST(Link, params_constructor) {
  Link l1(1, "l1", 100.0, gtsam::Vector3(3, 2, 1).asDiagonal(),
          Pose3(Rot3(), Point3(0, 0, 1)), Pose3());

  // name
  EXPECT(assert_equal("l1", l1.name()));

  // mass
  EXPECT(assert_equal(100, l1.mass()));

  // Check center of mass.
  EXPECT(assert_equal(Pose3(Rot3(), Point3(0, 0, 1)), l1.bMcom()));

  // Check inertia.
  EXPECT(assert_equal(
      (gtsam::Matrix(3, 3) << 3, 0, 0, 0, 2, 0, 0, 0, 1).finished(),
      l1.inertia()));

  // Check general mass matrix.
  EXPECT(assert_equal(
      (gtsam::Matrix(6, 6) << 3, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0,
       0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 100)
          .finished(),
      l1.inertiaMatrix()));

  // Assert correct center of mass in link frame.
  EXPECT(assert_equal(Pose3(Rot3(), Point3(0, 0, 1)), l1.bMcom()));

  // Check that no child links/joints have yet been added.
  EXPECT(assert_equal(0, l1.joints().size()));
}

TEST(Link, NumJoints) {
  auto robot = simple_urdf::getRobot();
  auto l1 = robot.link("l1");
  auto l2 = robot.link("l2");

  EXPECT_LONGS_EQUAL(1, l1->numJoints());

  auto j2 = std::make_shared<RevoluteJoint>(
      123, "j2", Pose3(Rot3(), Point3(0, 0.5, 2)), l1, l2,
      gtsam::Vector3(1, 0, 0), JointParams());

  l1->addJoint(j2);
  EXPECT_LONGS_EQUAL(2, l1->numJoints());
}

#ifdef GTDYNAMICS_ENABLE_BOOST_SERIALIZATION
// Declaration needed for serialization of derived class.
BOOST_CLASS_EXPORT(gtdynamics::RevoluteJoint)

TEST(Link, Serialization) {
  Link link(1, "l1", 100.0, gtsam::Vector3(3, 2, 1).asDiagonal(),
            Pose3(Rot3(), Point3(0, 0, 1)), Pose3());
  EXPECT(equalsObj(link));
  EXPECT(equalsXML(link));
  EXPECT(equalsBinary(link));

  // Test link with joints
  auto robot = simple_urdf::getRobot();
  auto l1 = robot.link("l1");
  EXPECT(equalsDereferenced(l1));
  EXPECT(equalsDereferencedXML(l1));
  EXPECT(equalsDereferencedBinary(l1));
}
#endif

TEST(Link, Print) {
  Link link(1, "l1", 100.0, gtsam::Vector3(3, 2, 1).asDiagonal(),
            Pose3(Rot3(), Point3(0, 0, 1)), Pose3());

  std::string expected =
      "l1, id=1:\n"
      "	com pose:   0 -0  0, 0 0 1\n"
      "	link pose:  0 -0  0, 0 0 0\n";
  EXPECT(gtsam::assert_print_equal(expected, link));
}

int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}
