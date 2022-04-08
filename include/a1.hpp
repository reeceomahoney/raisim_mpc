#include "raisim/World.hpp"

using Eigen::VectorXd;
using Eigen::Vector3d;
using Eigen::MatrixXd;
using Eigen::Matrix3d;
using Eigen::seq;
using std::vector;
using std::map;
using std::string;
using std::cout;
using std::endl;

VectorXd sliceVecDyn(const raisim::VecDyn vec, int start_idx, int end_idx);

class A1 {
    public:
    double time_step;
    VectorXd last_action;
    VectorXd desired_speed;
    int num_legs = 4;
    int num_motors = 12;
    int step_counter = 0;
    MatrixXd hip_offsets {
        {0.183, -0.048,  0.0},
        {0.183, 0.048, 0.0},
        {-0.183, -0.048, 0.0},
        {-0.183, 0.048, 0.0}};

    //Motor gains
    VectorXd motor_kp = 100 * VectorXd::Ones(12);
    VectorXd motor_kd {{1, 2, 2, 1, 2, 2, 1, 2, 2, 1, 2, 2}};

    //MPC parameters
    double mpc_body_mass = 12.454;
    Matrix3d mpc_body_inertia {
        {0.07335, 0, 0},
        {0, 0.25068, 0},
        {0, 0, 0.25447}};
    double mpc_body_height = 0.30;

    raisim::ArticulatedSystem* model;

    A1();
    A1(raisim::ArticulatedSystem* _model, double _time_step);

    void reset();

    MatrixXd getFootPositionsInBaseFrame();
    VectorXd getComPosition();
    VectorXd getComVelocity();
    VectorXd frameTransformation(VectorXd vec);
    VectorXd getBaseRollPitchYaw();
    VectorXd getBaseRollPitchYawRate();

    std::tuple<Eigen::VectorXd, Eigen::VectorXd> 
    getJointAnglesFromLocalFootPosition(int leg_id, VectorXd foot_local_position);
    VectorXd getJointAngles();
    VectorXd getJointVelocities();
    VectorXd getObservation();

    double getTimeSinceReset();

    MatrixXd computeJacobian(int leg_id);
    std::map<int,double> mapContactForceToJointTorques(
        int leg_id, VectorXd contact_force);

    void step(VectorXd actions);
    double getReward();
};