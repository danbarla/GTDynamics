/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  initialize_solution_utils.cpp
 * @brief Utility methods for initializing trajectory optimization solutions.
 * @authors Alejandro Escontrela, Yetong Zhang, Varun Agrawal
 */

#include <gtdynamics/dynamics/DynamicsGraph.h>
#include <gtdynamics/factors/MinTorqueFactor.h>
#include <gtdynamics/universal_robot/Robot.h>
#include <gtdynamics/utils/Initializer.h>
#include <gtdynamics/utils/values.h>
#include <gtsam/base/Value.h>
#include <gtsam/base/Vector.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>

#include <string>
#include <utility>
#include <vector>

using gtsam::Point3;
using gtsam::Pose3;
using gtsam::Rot3;
using gtsam::Sampler;
using gtsam::Values;
using gtsam::Vector;
using gtsam::Vector3;
using gtsam::Vector6;

namespace gtdynamics {

Pose3 Initializer::AddGaussianNoiseToPose(const Pose3& T,
                                          const Sampler& sampler) const {
  Vector6 xi = sampler.sample();
  return T.expmap(xi);
}

std::vector<Pose3> Initializer::InterpolatePoses(
    const Pose3& wTl_i, const std::vector<Pose3>& wTl_t, double t_i,
    const std::vector<double>& timesteps, double dt) {
  std::vector<Pose3> wTl_dt;
  Pose3 wTl = wTl_i;

  for (size_t i = 0; i < timesteps.size(); i++) {
    double t_f = timesteps[i];  // Final time for each step.

    for (double t = t_i; t <= t_f; t += dt) {
      // Normalized phase progress.
      double s = (t - t_i) / (t_f - t_i);

      // Compute interpolated pose for link.
      Pose3 wTl_s = gtsam::interpolate<Pose3>(wTl, wTl_t[i], s);

      wTl_dt.push_back(wTl_s);
    }
    wTl = wTl_t[i];
  }
  wTl_dt.push_back(wTl_t[wTl_t.size() - 1]);  // Add the final pose.

  return wTl_dt;
}

Values Initializer::InitializePosesAndJoints(
    const Robot& robot, const Pose3& wTl_i, const std::vector<Pose3>& wTl_t,
    const std::string& link_name, double t_i,
    const std::vector<double>& timesteps, double dt, const Sampler& sampler,
    std::vector<Pose3>* wTl_dt) {
  // Linearly interpolated pose for link at each discretized timestep.
  *wTl_dt = InterpolatePoses(wTl_i, wTl_t, t_i, timesteps, dt);

  for (auto& pose : *wTl_dt) {
    pose = AddGaussianNoiseToPose(pose, sampler);
  }

  // TODO(frank): I'm not really understanding all this. Refactor in future?
  Values values, fk_input;

  // Initial pose and joint angles are known a priori.
  for (auto&& joint : robot.joints()) {
    InsertJointAngle(&fk_input, joint->id(), sampler.sample()[0]);
    InsertJointVel(&fk_input, joint->id(), sampler.sample()[0]);
    InsertJointAngle(&values, joint->id(), 0, sampler.sample()[0]);
  }

  // Compute forward dynamics to obtain remaining link poses.
  Pose3 wTl_i_processed = AddGaussianNoiseToPose(wTl_i, sampler);
  InsertPose(&fk_input, robot.link(link_name)->id(), wTl_i_processed);
  InsertTwist(&fk_input, robot.link(link_name)->id(), gtsam::Z_6x1);
  auto fk_results = robot.forwardKinematics(fk_input, 0, link_name);

  for (auto&& link : robot.links()) {
    size_t i = link->id();
    InsertPose(&values, i, Pose(fk_results, i));
  }
  return values;
}

Values Initializer::InitializeSolutionInterpolation(
    const Robot& robot, const std::string& link_name, const Pose3& wTl_i,
    const Pose3& wTl_f, double T_s, double T_f, double dt,
    double gaussian_noise, const std::optional<PointOnLinks>& contact_points) {
  auto sampler_noise_model =
      gtsam::noiseModel::Isotropic::Sigma(6, gaussian_noise);
  Sampler sampler(sampler_noise_model);

  // Initial and final discretized timesteps.
  int n_steps_init = std::lround(T_s / dt);
  int n_steps_final = std::lround(T_f / dt);

  double t_elapsed = T_s;

  Values init_vals;

  // Initialize joint angles and velocities to 0.
  for (int t = n_steps_init; t <= n_steps_final; t++) {
    double s = (t_elapsed - T_s) / (T_f - T_s);

    // Compute interpolated pose for link.
    Pose3 wTl_t = AddGaussianNoiseToPose(
        gtsam::interpolate<Pose3>(wTl_i, wTl_f, s), sampler);

    // Compute forward dynamics to obtain remaining link poses.
    // TODO(Alejandro): forwardKinematics needs to get passed prev link twist
    for (auto&& joint : robot.joints()) {
      InsertJointAngle(&init_vals, joint->id(), t, sampler.sample()[0]);
      InsertJointVel(&init_vals, joint->id(), t, sampler.sample()[0]);
    }

    auto link = robot.link(link_name);
    if (link->isFixed()) {
      throw std::invalid_argument("InitializeSolutionInterpolation: Link " +
                                  link_name + " is fixed.");
    }
    InsertPose(&init_vals, link->id(), t, wTl_t);
    InsertTwist(&init_vals, link->id(), t, gtsam::Z_6x1);
    init_vals = robot.forwardKinematics(init_vals, t, link_name);

    const Values zero_values =
        ZeroValues(robot, t, gaussian_noise, contact_points);
    for (auto&& key : zero_values.keys()) {
      if (!init_vals.exists(key)) {
        init_vals.insert(key, zero_values.at(key));
      }
    }

    t_elapsed += dt;
  }

  return init_vals;
}

Values Initializer::InitializeSolutionInterpolationMultiPhase(
    const Robot& robot, const std::string& link_name, const Pose3& wTl_i,
    const std::vector<Pose3>& wTl_t, const std::vector<double>& ts, double dt,
    double gaussian_noise, const std::optional<PointOnLinks>& contact_points) {
  Values init_vals;
  Pose3 pose = wTl_i;
  double curr_t = 0.0;
  for (size_t i = 0; i < wTl_t.size(); i++) {
    Values phase_vals = InitializeSolutionInterpolation(
        robot, link_name, pose, wTl_t[i], curr_t, ts[i], dt, gaussian_noise,
        contact_points);

    for (auto&& key : phase_vals.keys()) {
      if (!init_vals.exists(key)) {
        init_vals.insert(key, phase_vals.at(key));
      }
    }
    pose = wTl_t[i];
    curr_t = ts[i];
  }
  return init_vals;
}

Values Initializer::InitializeSolutionInverseKinematics(
    const Robot& robot, const std::string& link_name, const Pose3& wTl_i,
    const std::vector<Pose3>& wTl_t, const std::vector<double>& timesteps,
    double dt, double gaussian_noise,
    const std::optional<PointOnLinks>& contact_points) {
  double t_i = 0.0;  // Time elapsed.

  Vector3 gravity(0, 0, -9.8);

  auto sampler_noise_model =
      gtsam::noiseModel::Isotropic::Sigma(6, gaussian_noise);
  Sampler sampler(sampler_noise_model);

  // Link pose at each step
  std::vector<Pose3> wTl_dt;

  // Iteratively solve the inverse kinematics problem while statisfying
  // the contact pose constraint.
  Values init_vals,
      values = InitializePosesAndJoints(robot, wTl_i, wTl_t, link_name, t_i,
                                        timesteps, dt, sampler, &wTl_dt);

  DynamicsGraph dgb(gravity);
  for (int t = 0; t <= std::round(timesteps[timesteps.size() - 1] / dt); t++) {
    auto kfg = dgb.qFactors(robot, t, contact_points);
    kfg.addPrior(PoseKey(robot.link(link_name)->id(), t), wTl_dt[t],
                 gtsam::noiseModel::Isotropic::Sigma(6, 0.001));

    gtsam::LevenbergMarquardtOptimizer optimizer(kfg, values);
    Values results = optimizer.optimize();

    // Add zero initial values for remaining variables.
    init_vals.insert(ZeroValues(robot, t, gaussian_noise, contact_points));

    // Update with the results of the optimizer.
    init_vals.update(results);

    // Update initial values for next timestep.
    values.clear();

    for (auto&& link : robot.links()) {
      InsertPose(&values, link->id(), t + 1, Pose(results, link->id(), t));
    }
    for (auto&& joint : robot.joints()) {
      InsertJointAngle(&values, joint->id(), t + 1,
                       JointAngle(results, joint->id(), t));
    }
  }

  return init_vals;
}

Values Initializer::MultiPhaseZeroValuesTrajectory(
    const Robot& robot, const std::vector<int>& phase_steps,
    std::vector<Values> transition_graph_init, double dt_i,
    double gaussian_noise,
    const std::optional<std::vector<PointOnLinks>>& phase_contact_points)
    const {
  Values values;
  int num_phases = phase_steps.size();

  // Return either PointOnLinks or None if none specified for phase p
  auto contact_points =
      [&phase_contact_points](int p) -> std::optional<PointOnLinks> {
    if (phase_contact_points) return (*phase_contact_points)[p];
    return {};
  };

  // First slice, k==0
  values.insert(ZeroValues(robot, 0, gaussian_noise, contact_points(0)));

  int k = 0;
  for (int p = 0; p < num_phases; p++) {
    // in-phase
    for (int step = 0; step < phase_steps[p] - 1; step++) {
      values.insert(ZeroValues(robot, ++k, gaussian_noise, contact_points(p)));
    }

    if (p == num_phases - 1) {
      values.insert(ZeroValues(robot, ++k, gaussian_noise, contact_points(p)));
    } else {
      values.insert(transition_graph_init[p]);
      k++;
    }
  }

  for (int p = 0; p < num_phases; p++) {
    values.insert(PhaseKey(p), dt_i);
  }

  return values;
}

Values Initializer::MultiPhaseInverseKinematicsTrajectory(
    const Robot& robot, const std::string& link_name,
    const std::vector<int>& phase_steps, const Pose3& wTl_i,
    const std::vector<Pose3>& wTl_t, const std::vector<double>& ts,
    std::vector<Values> transition_graph_init, double dt, double gaussian_noise,
    const std::optional<std::vector<PointOnLinks>>& phase_contact_points) {
  double t_i = 0;  // Time elapsed.
  Vector3 gravity = (Vector(3) << 0, 0, -9.8).finished();

  auto sampler_noise_model =
      gtsam::noiseModel::Isotropic::Sigma(6, gaussian_noise);
  Sampler sampler(sampler_noise_model);

  std::vector<Pose3> wTl_dt;

  // Iteratively solve the inverse kinematics problem while statisfying
  // the contact pose constraint.
  Values init_vals,
      values = InitializePosesAndJoints(robot, wTl_i, wTl_t, link_name, t_i, ts,
                                        dt, sampler, &wTl_dt);

  DynamicsGraph dgb(gravity);

  int t = 0;
  int num_phases = phase_steps.size();

  for (int phase = 0; phase < num_phases; phase++) {
    // In-phase.
    int curr_phase_steps =
        phase == (num_phases - 1) ? phase_steps[phase] + 1 : phase_steps[phase];
    for (int phase_step = 0; phase_step < curr_phase_steps; phase_step++) {
      auto kfg = dgb.qFactors(robot, t, (*phase_contact_points)[phase]);

      kfg.addPrior(PoseKey(robot.link(link_name)->id(), t), wTl_dt[t],
                   gtsam::noiseModel::Isotropic::Sigma(6, 0.001));

      gtsam::LevenbergMarquardtOptimizer optimizer(kfg, values);
      Values results = optimizer.optimize();

      init_vals.insert(results);

      // Update initial values for next timestep.
      values.clear();
      for (auto&& link : robot.links()) {
        int link_id = link->id();
        InsertPose(&values, link_id, t + 1, Pose(results, link_id, t));
      }

      for (auto&& joint : robot.joints()) {
        int joint_id = joint->id();
        InsertJointAngle(&values, joint_id, t + 1,
                         JointAngle(results, joint_id, t));
      }
      t++;
    }
  }

  Values zero_values =
      MultiPhaseZeroValuesTrajectory(robot, phase_steps, transition_graph_init,
                                     dt, gaussian_noise, phase_contact_points);

  for (auto&& key : zero_values.keys()) {
    if (!init_vals.exists(key)) {
      init_vals.insert(key, zero_values.at(key));
    }
  }

  return init_vals;
}

Values Initializer::ZeroValues(
    const Robot& robot, const int t, double gaussian_noise,
    const std::optional<PointOnLinks>& contact_points) const {
  Values values;

  auto sampler_noise_model =
      gtsam::noiseModel::Isotropic::Sigma(6, gaussian_noise);
  Sampler sampler(sampler_noise_model);

  // Initialize link dynamics to 0.
  for (auto&& link : robot.links()) {
    int i = link->id();
    InsertPose(&values, i, t, AddGaussianNoiseToPose(link->bMcom(), sampler));
    InsertTwist(&values, i, t, sampler.sample());
    InsertTwistAccel(&values, i, t, sampler.sample());
  }

  // Initialize joint kinematics/dynamics to 0.
  for (auto&& joint : robot.joints()) {
    int j = joint->id();
    InsertWrench(&values, joint->parent()->id(), j, t, sampler.sample());
    InsertWrench(&values, joint->child()->id(), j, t, sampler.sample());
    std::vector<DynamicsSymbol> keys = {TorqueKey(j, t), JointAngleKey(j, t),
                                        JointVelKey(j, t), JointAccelKey(j, t)};
    for (size_t i = 0; i < keys.size(); i++)
      values.insert(keys[i], sampler.sample()[0]);
  }

  if (contact_points) {
    for (auto&& cp : *contact_points) {
      // TODO(frank): allow multiple contact points on one link, id = 0,1,2...
      values.insert(ContactWrenchKey(cp.link->id(), 0, t), sampler.sample());
    }
  }

  return values;
}

Values Initializer::ZeroValuesTrajectory(
    const Robot& robot, const int num_steps, const int num_phases,
    double gaussian_noise, const std::optional<PointOnLinks>& contact_points) {
  Values z_values;
  for (int t = 0; t <= num_steps; t++)
    z_values.insert(ZeroValues(robot, t, gaussian_noise, contact_points));
  if (num_phases > 0) {
    for (int phase = 0; phase <= num_phases; phase++)
      z_values.insert(PhaseKey(phase), 0.0);
  }
  return z_values;
}

}  // namespace gtdynamics
