/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file sdf.cpp
 * @brief Implementation of SDF related utilities.
 * @author Frank Dellaert, Alejandro Escontrela, Stephanie McCormick
 */

#include "gtdynamics/universal_robot/sdf.h"

#include <sdf/parser_urdf.hh>

#include "gtdynamics/universal_robot/PrismaticJoint.h"
#include "gtdynamics/universal_robot/RevoluteJoint.h"
#include "gtdynamics/universal_robot/ScrewJoint.h"
#include "gtdynamics/universal_robot/ScrewJointBase.h"

namespace gtdynamics {

using gtsam::Pose3;

sdf::Model get_sdf(std::string sdf_file_path, std::string model_name) {
  auto sdf = sdf::readFile(sdf_file_path);

  sdf::Model model = sdf::Model();
  model.Load(sdf->Root()->GetElement("model"));

  // Check whether this is a world file, in which case we have to first
  // access the world element then check whether one of its child models
  // corresponds to model_name.
  if (model.Name() != "__default__") return model;

  // Load the world element.
  sdf::World world = sdf::World();
  world.Load(sdf->Root()->GetElement("world"));

  for (uint i = 0; i < world.ModelCount(); i++) {
    sdf::Model curr_model = *world.ModelByIndex(i);
    if (curr_model.Name() == model_name) return curr_model;
  }

  throw std::runtime_error("Model not found in: " + sdf_file_path);
}

gtsam::Pose3 parse_ignition_pose(ignition::math::Pose3d ignition_pose) {
  gtsam::Pose3 parsed_pose =
      gtsam::Pose3(gtsam::Rot3(gtsam::Quaternion(
                       ignition_pose.Rot().W(), ignition_pose.Rot().X(),
                       ignition_pose.Rot().Y(), ignition_pose.Rot().Z())),
                   gtsam::Point3(ignition_pose.Pos()[0], ignition_pose.Pos()[1],
                                 ignition_pose.Pos()[2]));

  return parsed_pose;
}

Joint::Parameters ParametersFromSdfJoint(const sdf::Joint &sdf_joint) {
  Joint::Parameters parameters;

  parameters.scalar_limits.value_lower_limit = sdf_joint.Axis()->Lower();
  parameters.scalar_limits.value_upper_limit = sdf_joint.Axis()->Upper();
  parameters.velocity_limit = sdf_joint.Axis()->MaxVelocity();
  parameters.torque_limit = sdf_joint.Axis()->Effort();
  parameters.damping_coefficient = sdf_joint.Axis()->Damping();

  return parameters;
}

Link::Params ParametersFromSdfLink(const sdf::Link &sdf_link) {
  Link::Params parameters;
  parameters.name = sdf_link.Name();
  parameters.mass = sdf_link.Inertial().MassMatrix().Mass();
  parameters.inertia << sdf_link.Inertial().Moi()(0, 0),
             sdf_link.Inertial().Moi()(0, 1), sdf_link.Inertial().Moi()(0, 2),
             sdf_link.Inertial().Moi()(1, 0), sdf_link.Inertial().Moi()(1, 1),
             sdf_link.Inertial().Moi()(1, 2), sdf_link.Inertial().Moi()(2, 0),
             sdf_link.Inertial().Moi()(2, 1), sdf_link.Inertial().Moi()(2, 2);
  parameters.wTl = parse_ignition_pose(sdf_link.Pose());
  parameters.lTcom = parse_ignition_pose(sdf_link.Inertial().Pose());
  return parameters;
}

Link::Params LinkParamsByName(const sdf::Model& sdf_model, const std::string& name) {
  return ParametersFromSdfLink(*sdf_model.LinkByName(name));
}

Pose3 GetJointFrame(const sdf::Joint &sdf_joint,
                    const LinkSharedPtr &parent_link,
                    const LinkSharedPtr &child_link) {
  auto frame = sdf_joint.PoseFrame();
  if (frame == "" || frame == child_link->name()) {
    if (sdf_joint.Pose() == ignition::math::Pose3d())
      return child_link->wTl();
    else
      return child_link->wTl() * parse_ignition_pose(sdf_joint.Pose());
  } else if (frame == parent_link->name()) {
    if (sdf_joint.Pose() == ignition::math::Pose3d())
      return parent_link->wTl();
    else
      return parent_link->wTl() * parse_ignition_pose(sdf_joint.Pose());
  } else if (frame == "world") {
    return parse_ignition_pose(sdf_joint.Pose());
  } else {
    // TODO(gchen328): get pose frame from name. Need sdf::Model to do that
    throw std::runtime_error(
        "joint pose frames other than world, parent, or "
        "child not yet supported");
  }
}

gtsam::Vector3 GetSdfAxis(const sdf::Joint &sdf_joint) {
  auto axis = sdf_joint.Axis()->Xyz();
  return gtsam::Vector3(axis[0], axis[1], axis[2]);
}

/**
 * @fn Construct all Link and Joint objects from an input sdf::ElementPtr.
 * @param sdf_ptr a shared pointer to a sdf::ElementPtr containing the model.
 * @return LinkMap and JointMap as a pair
 */
static LinkJointPair ExtractRobotFromSdf(const sdf::Model &sdf) {
  // Loop through all links in the sdf interface and construct Link
  // objects without parents or children.
  LinkMap name_to_link;
  for (uint i = 0; i < sdf.LinkCount(); i++) {
    LinkSharedPtr link = boost::make_shared<Link>(ParametersFromSdfLink(*sdf.LinkByIndex(i)));
    link->setID(i);
    name_to_link.emplace(link->name(), link);
  }

  // Create Joint objects and update list of parent and child links/joints.
  JointMap name_to_joint;
  for (uint j = 0; j < sdf.JointCount(); j++) {
    sdf::Joint sdf_joint = *sdf.JointByIndex(j);

    // Get this joint's parent and child links.
    std::string parent_link_name = sdf_joint.ParentLinkName();
    std::string child_link_name = sdf_joint.ChildLinkName();
    if (parent_link_name == "world") {
      // This joint fixes the child link in the world frame.
      LinkSharedPtr child_link = name_to_link[child_link_name];
      Pose3 fixed_pose = child_link->wTcom();
      child_link->fix(fixed_pose);
      continue;
    }
    LinkSharedPtr parent_link = name_to_link[parent_link_name];
    LinkSharedPtr child_link = name_to_link[child_link_name];

    // Construct Joint and insert into name_to_joint.
    JointSharedPtr joint;

    // Generate a joint parameters struct with values from the SDF.
    Joint::Parameters parameters = ParametersFromSdfJoint(sdf_joint);

    std::string name(sdf_joint.Name());
    Pose3 wTj = GetJointFrame(sdf_joint, parent_link, child_link);

    const gtsam::Vector3 axis = GetSdfAxis(sdf_joint);
    switch (sdf_joint.Type()) {
      case sdf::JointType::PRISMATIC:
        joint = boost::make_shared<PrismaticJoint>(
            name, wTj, parent_link, child_link, parameters, axis);
        break;
      case sdf::JointType::REVOLUTE:
        joint = boost::make_shared<RevoluteJoint>(name, wTj, parent_link,
                                                  child_link, parameters, axis);
        break;
      case sdf::JointType::SCREW:
        joint = boost::make_shared<ScrewJoint>(name, wTj, parent_link,
                                               child_link, parameters, axis,
                                               sdf_joint.ThreadPitch());
        break;
      default:
        throw std::runtime_error("Joint type for [" + name +
                                 "] not yet supported");
    }

    name_to_joint.emplace(name, joint);
    joint->setID(j);

    // Update list of parent and child links/joints for each Link.
    parent_link->addJoint(joint);
    child_link->addJoint(joint);
  }

  return std::make_pair(name_to_link, name_to_joint);
}

/**
 * @fn Construct all Link and Joint objects from an input urdf or sdf file.
 * @param[in] file_path absolute path to the urdf or sdf file containing the
 * robot description.
 * @param[in] model_name name of the robot we care about. Must be specified in
 * case sdf_file_path points to a world file.
 * @return LinkMap and JointMap as a pair
 */
static LinkJointPair ExtractRobotFromFile(const std::string &file_path,
                                          const std::string &model_name) {
  std::string file_ext = file_path.substr(file_path.find_last_of(".") + 1);
  std::transform(file_ext.begin(), file_ext.end(), file_ext.begin(), ::tolower);

  if (file_ext == "urdf")
    return ExtractRobotFromSdf(get_sdf(file_path));
  else if (file_ext == "sdf")
    return ExtractRobotFromSdf(get_sdf(file_path, model_name));

  throw std::runtime_error("Invalid file extension.");
}

Robot CreateRobotFromFile(const std::string file_path, std::string model_name) {
  return Robot(ExtractRobotFromFile(file_path, model_name));
}

}  // namespace gtdynamics
