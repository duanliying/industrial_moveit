/*
 * Software License Agreement (Apache License)
 *
 * Copyright (c) 2016, Southwest Research Institute
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * stomp.h
 *
 *  Created on: Mar 7, 2016
 *      Author: Jorge Nicho
 */

#ifndef INDUSTRIAL_MOVEIT_STOMP_CORE_INCLUDE_STOMP_CORE_STOMP_H_
#define INDUSTRIAL_MOVEIT_STOMP_CORE_INCLUDE_STOMP_CORE_STOMP_H_

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <XmlRpc.h>
#include "stomp_core/stomp_core_utils.h"
#include "stomp_core/task.h"
#include "stomp_core/multivariate_gaussian.h"

namespace stomp_core
{

namespace TrajectoryInitializations
{
enum TrajectoryInitialization
{
  LINEAR_INTERPOLATION = 1,
  CUBIC_POLYNOMIAL_INTERPOLATION,
  MININUM_CONTROL_COST
};
}

struct StompConfiguration
{
  // General settings
  int num_iterations;
  int num_iterations_after_valid;   /**< Stomp will stop optimizing this many iterations after finding a valid solution */
  int num_timesteps;
  int num_dimensions;               /** parameter dimensionality */
  double delta_t;               /** time change between consecutive points */
  int initialization_method; /** TrajectoryInitializations::TrajectoryInitialization */

  // Noisy trajectory generation
  int num_rollouts; /**< Number of noisy trajectories*/
  int min_rollouts; /**< There be no less than min_rollouts computed on each iteration */
  int max_rollouts; /**< The combined number of new and old rollouts during each iteration shouldn't exceed this value */
  NoiseGeneration noise_generation;

  // Cost calculation
  double control_cost_weight;  /**< Percentage of the trajectory accelerations cost to be applied in the total cost calculation >*/
};

using MultivariateGaussianPtr = boost::shared_ptr<MultivariateGaussian>;
class Stomp
{
public:
  Stomp(const StompConfiguration& config,TaskPtr task);
  virtual ~Stomp();

  bool solve(const std::vector<double>& first,const std::vector<double>& last,
             Eigen::MatrixXd& parameters_optimized);
  bool solve(const Eigen::MatrixXd& initial_parameters,
             Eigen::MatrixXd& parameters_optimized);
  bool cancel();

  static bool parseConfig(XmlRpc::XmlRpcValue config,StompConfiguration& stomp_config);

protected:

  // initialization methods
  bool resetVariables();
  bool computeInitialTrajectory(const std::vector<double>& first,const std::vector<double>& last);

  // optimization methods
  bool runSingleIteration();
  bool generateNoisyRollouts();
  bool filterNoisyRollouts();
  bool computeNoisyRolloutsCosts();
  bool computeRolloutsStateCosts();
  bool computeRolloutsControlCosts();
  bool computeProbabilities();
  bool updateParameters();
  bool filterUpdatedParameters();
  bool computeOptimizedCost();

  // thread safe methods
  void setProceed(bool proceed);
  bool getProceed();

  // noise generation variables
  void updateNoiseStddev();

protected:

  // process control
  boost::mutex proceed_mutex_;
  bool proceed_;
  TaskPtr task_;
  StompConfiguration config_;
  unsigned int current_iteration_;

  // optimized parameters
  bool parameters_valid_;         /**< whether or not the optimized parameters are valid */
  double parameters_total_cost_;  /**< Total cost of the optimized parameters */
  Eigen::MatrixXd initial_control_cost_parameters_;    /**< [Dimensions][timesteps]*/
  Eigen::MatrixXd parameters_optimized_;               /**< [Dimensions][timesteps]*/
  Eigen::VectorXd parameters_state_costs_;                          /**< [timesteps]*/
  Eigen::MatrixXd parameters_control_costs_;           /**< [Dimensions][timesteps]*/
  Eigen::VectorXd temp_parameter_updates_;                          /**< [timesteps]*/

  // noise generation
  std::vector<double> noise_stddevs_;
  std::vector<MultivariateGaussianPtr > random_dist_generators_;
  Eigen::VectorXd temp_noise_array_;


  // rollouts
  std::vector<Rollout> noisy_rollouts_;
  std::vector<Rollout> reused_rollouts_;     /**< Used for reordering arrays based on cost */
  int num_active_rollouts_;

  // finite difference and optimization matrices
  int num_timesteps_padded_;                        /**< timesteps + 2*(FINITE_DIFF_RULE_LENGTH - 1) */
  int start_index_padded_;                          /** index corresponding to the start of the non-paded section in the padded arrays */
  Eigen::MatrixXd finite_diff_matrix_A_padded_;
  Eigen::MatrixXd control_cost_matrix_R_padded_;
  Eigen::MatrixXd finite_diff_matrix_A_;            /**< [timesteps x timesteps], Referred to as 'A' in the literature */
  Eigen::MatrixXd control_cost_matrix_R_;           /**< [timesteps x timesteps], Referred to as 'R = A x A_transpose' in the literature */
  Eigen::MatrixXd inv_control_cost_matrix_R_;       /**< [timesteps x timesteps], R^-1 ' matrix */
  Eigen::MatrixXd projection_matrix_M_;             /**< [timesteps x timesteps], Projection smoothing matrix  M > */
  Eigen::MatrixXd inv_projection_matrix_M_;         /**< [timesteps x timesteps], Inverse projection smoothing matrix M-1 >*/


};

} /* namespace stomp */

#endif /* INDUSTRIAL_MOVEIT_STOMP_CORE_INCLUDE_STOMP_CORE_STOMP_H_ */