#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 6;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 1;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
  is_initialized_ = false;
  n_x_ = 5;
  n_aug_ = 7;
  lambda_ = 3 - n_aug_;
  n_z_lidar_ = 2;
  n_z_radar_ = 3;

  x_ << 0,0,0,0,0;
  P_ <<  1, 0, 0, 0, 0,
         0, 1, 0, 0, 0,
         0, 0, 1, 0, 0,
         0, 0, 0, 1, 0,
         0, 0, 0, 0, 1;
  Xsig_aug_ = MatrixXd(n_aug_, 2*n_aug_+1);
  Xsig_pred_ = MatrixXd(n_x_, 2*n_aug_+1);
  Zsig_lidar_ = MatrixXd(n_z_lidar_, 2*n_aug_ + 1);
  Zsig_radar_ = MatrixXd(n_z_radar_, 2*n_aug_ + 1);
  z_lidar_ = VectorXd(n_z_lidar_);
  z_radar_ = VectorXd(n_z_radar_);
  S_lidar_ = MatrixXd(n_z_lidar_, n_z_lidar_);
  S_radar_ = MatrixXd(n_z_radar_, n_z_radar_);

  R_lidar_ = MatrixXd(n_z_lidar_, n_z_lidar_);
  R_lidar_ << std_laspx_*std_laspx_, 0,
              0, std_laspy_*std_laspy_;
  R_radar_ = MatrixXd(n_z_radar_, n_z_radar_);
  R_radar_ << std_radr_*std_radr_, 0, 0,
              0, std_radphi_*std_radphi_, 0,
              0, 0, std_radrd_*std_radrd_;

  weights_ = VectorXd(2*n_aug_+1);
  double w_0 = lambda_/(lambda_+n_aug_);
  double w = 0.5/(n_aug_+lambda_);
  weights_(0) = w_0;
  for (int i=1; i<2*n_aug_+1; i++){
    weights_(i) = w;
  }
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  if (!is_initialized_) {
    if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      // initialize x
      float rho = meas_package.raw_measurements_[0];
      float phi = meas_package.raw_measurements_[1];
      float rho_dot = meas_package.raw_measurements_[2];
      float px =  rho * cos(phi);
      float py =  rho * sin(phi);
      // avoid zero division
      if (px == 0 || py == 0){
        return;
      }
      x_ << px, py, rho_dot, phi, 0;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      // Initialize x
      float px =  meas_package.raw_measurements_[0];
      float py =  meas_package.raw_measurements_[1];
      // avoid zero division
      if (px == 0 || py == 0){
        return;
      }
      x_ << px, py, 0, 0, 0;
    }

    time_us_ = meas_package.timestamp_;
    is_initialized_ = true;
    return;
  }

  double dt = (meas_package.timestamp_ - time_us_) / 1000000.0; //dt - expressed in seconds
  time_us_ = meas_package.timestamp_;

  // Predict
  Prediction(dt);
  // Update
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
    UpdateRadar(meas_package);
  }else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
    UpdateLidar(meas_package);
  }
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  AugmentedSigmaPoints();
  SigmaPointPrediction(delta_t);
  PredictMeanAndCovariance();
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  PredictLidarMeasurement();
  UpdateLidarState(meas_package);
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  PredictRadarMeasurement();
  UpdateRadarState(meas_package);
}

void UKF::AugmentedSigmaPoints() {
  VectorXd x_aug = VectorXd(n_aug_);
  MatrixXd P_aug = MatrixXd(n_aug_,n_aug_);
  //create augmented mean state
  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;
  //create augmented covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = P_;
  P_aug(5,5) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;
  //create square root matrix
  MatrixXd L = P_aug.llt().matrixL();
  //create augmented sigma points
  Xsig_aug_.col(0)  = x_aug;
  for (int i = 0; i< n_aug_; i++)
  {
    Xsig_aug_.col(i+1)       = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug_.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }
}

void UKF::SigmaPointPrediction(double delta_t) {
  //predict sigma points
  for (int i=0; i < 2*n_aug_+1; i++)
  {
    //extract values for better readability
    double p_x = Xsig_aug_(0,i);
    double p_y = Xsig_aug_(1,i);
    double v = Xsig_aug_(2,i);
    double yaw = Xsig_aug_(3,i);
    double yawd = Xsig_aug_(4,i);
    double nu_a = Xsig_aug_(5,i);
    double nu_yawdd = Xsig_aug_(6,i);
    //predicted state values
    double px_p, py_p;
    //avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
        py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
    }
    else {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
    }
    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;
    //add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    v_p = v_p + nu_a*delta_t;
    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;
    //write predicted sigma point into right column
    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }
}

void UKF::PredictMeanAndCovariance() {
  //predicted state mean
  x_.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {  //iterate over sigma points
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }
  //predicted state covariance matrix
  P_.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {  //iterate over sigma points
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;
    P_ = P_ + weights_(i) * x_diff * x_diff.transpose() ;
  }
}

void UKF::PredictLidarMeasurement() {
  for (int i=0; i < 2*n_aug_+1; i++) {  //2n+1 simga points
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    Zsig_lidar_(0,i) = p_x;
    Zsig_lidar_(1,i) = p_y;
  }
  //mean predicted measurement
  z_lidar_.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_lidar_ = z_lidar_ + weights_(i) * Zsig_lidar_.col(i);
  }
  //measurement covariance matrix S
  S_lidar_.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig_lidar_.col(i) - z_lidar_;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
    S_lidar_ = S_lidar_ + weights_(i) * z_diff * z_diff.transpose();
  }
  S_lidar_ = S_lidar_ + R_lidar_;
}

void UKF::UpdateLidarState(MeasurementPackage meas_package) {
  VectorXd z = meas_package.raw_measurements_;
  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z_lidar_);
  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig_lidar_.col(i) - z_lidar_;
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S_lidar_.inverse();
  //residual
  VectorXd z_diff = z - z_lidar_;
  //update state mean and covariance matrix
  x_ = x_ + K*z_diff;
  P_ = P_ - K*S_lidar_*K.transpose();
  NIS_lidar_ = z_diff.transpose()*S_lidar_.inverse()*z_diff;
}

void UKF::PredictRadarMeasurement() {
  //transform sigma points into measurement space
  for (int i=0; i < 2*n_aug_+1; i++) {  //2n+1 simga points
    // extract values for better readibility
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);
    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;
    // measurement model
    double rho = sqrt(p_x*p_x + p_y*p_y);
    Zsig_radar_(0,i) = rho;
    if (fabs(rho) > 0.001) {
      Zsig_radar_(1,i) = atan2(p_y,p_x);
      Zsig_radar_(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);
    } else {
      Zsig_radar_(1,i) = 0;
      Zsig_radar_(2,i) = 0;
    }
  }

  //mean predicted measurement
  z_radar_.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_radar_ = z_radar_ + weights_(i) * Zsig_radar_.col(i);
  }
  //measurement covariance matrix S
  S_radar_.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig_radar_.col(i) - z_radar_;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
    S_radar_ = S_radar_ + weights_(i) * z_diff * z_diff.transpose();
  }
  S_radar_ = S_radar_ + R_radar_;
}

void UKF::UpdateRadarState(MeasurementPackage meas_package) {
  VectorXd z = meas_package.raw_measurements_;
  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z_radar_);
  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig_radar_.col(i) - z_radar_;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S_radar_.inverse();
  //residual
  VectorXd z_diff = z - z_radar_;
  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;
  //update state mean and covariance matrix
  x_ = x_ + K*z_diff;
  P_ = P_ - K*S_radar_*K.transpose();
  NIS_radar_ = z_diff.transpose()*S_radar_.inverse()*z_diff;
}
