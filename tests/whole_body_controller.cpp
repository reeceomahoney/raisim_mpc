#include "locomotion_controller.hpp"
#include <Eigen/Dense>
#include "raisim/World.hpp"
#include "raisim/RaisimServer.hpp"
#include <time.h>


vector<double> mpc_weights {1, 1, 0, 0, 0, 50, 0, 0, 1, 0.2, 0.2, 0.1, 0};
vector<double> inertia = {0.07335, 0, 0, 0, 0.25068, 0, 0, 0, 0.25447};
double mass = 12.454;

double sim_freq = 1000;
int mpc_freq = 50;
int max_time = 24;

GaitGenerator gait_generator;
SwingController sw_controller;
StanceController st_controller;

//Helper function to interpolate velocity commands
VectorXd interp1d(VectorXd time_points, MatrixXd speed_points, double t) {
    for (int i = 0; i < time_points.size(); i++) {
        if ((t>=time_points[i]) && (t<=time_points[i+1])) {
            return speed_points.row(i);
        };
    };
};

class GaitProfile {
    public:
    Vector4d stance_duration, duty_factor, init_leg_phase;
    Vector4i init_leg_state;
    GaitProfile(string gait) {
        if (gait == "standing") {
            stance_duration << 0.3, 0.3, 0.3, 0.3;
            duty_factor << 1, 1, 1, 1;
            init_leg_phase << 0, 0, 0, 0;
            init_leg_state << 1, 1, 1, 1;
        };
        if (gait == "trotting") {
            stance_duration << 0.3, 0.3, 0.3, 0.3;
            duty_factor << 0.6, 0.6, 0.6, 0.6;
            init_leg_phase << 0.9, 0, 0, 0.9;
            init_leg_state << 0, 1, 1, 0;
        };
    };
};

LocomotionController setupController(A1* robot_, GaitGenerator* gg, SwingController* sw, StanceController* st,
    string gait) {
    
    Vector3d desired_speed {0, 0, 0};
    double desired_twisting_speed = 0;

    // Standing or trotting
    GaitProfile gait_profile(gait);

    *gg = GaitGenerator(gait_profile.stance_duration, gait_profile.duty_factor, 
        gait_profile.init_leg_state, gait_profile.init_leg_phase);
    
    *sw = SwingController(robot_, gg, desired_speed, desired_twisting_speed, 
        robot_->mpc_body_height, 0.01);

    *st = StanceController(robot_, gg, desired_speed, desired_twisting_speed,
        robot_->mpc_body_height, robot_->mpc_body_mass);

    LocomotionController controller(robot_, gg, sw, st);

    return controller;
}

//Creates a speed profile
VectorXd getCommand(double t) {
    double vx = 1.5;
    double vy = 0.75;
    double wz = 2.0;

    VectorXd time_points {{0, 3, 6, 9, 12, 15, 18}};
    MatrixXd speed_points {{vx, 0, 0, 0}, {-vx, 0, 0, 0},{0, vy, 0, 0},
        {0, -vy, 0, 0}, {0, 0, 0, wz}, {0, 0, 0, -wz}};
    
    return interp1d(time_points, speed_points, t);
};

int main() {
    //Construct simulator
    raisim::World world;
    double time_step = 1/sim_freq;
    world.setTimeStep(time_step);
    raisim::RaisimServer server(&world);
    world.addGround();
    server.launchServer(8080);

    //Create a1 class
    string urdf_path = "/home/romahoney/4yp/raisim_mpc/a1_data/urdf/a1.urdf";
    auto model = world.addArticulatedSystem(urdf_path);
    A1 robot(model, time_step);
    
    //Setup controller
    auto controller = setupController(&robot, &gait_generator, &sw_controller, &st_controller, "trotting");
    controller.reset();
    
    auto start_time = robot.getTimeSinceReset();
    auto current_time = start_time;

    //Initialise so MPC always calls on the first step
    int mpc_count = sim_freq / mpc_freq - 1;
    bool mpc_step;

    //Main loop
    while ((current_time - start_time) < max_time) {
        
        //Update the controller parameters.
        auto desired_speed = getCommand(current_time);
        controller.update(desired_speed(seq(0,2)), desired_speed(3));

        //Store desired speed for reward function calcuation
        robot.desired_speed = desired_speed;

        //Flag to enforce MPC frequency
        mpc_count += 1;
        mpc_step = false;
        if (mpc_count == (int(sim_freq) / mpc_freq)) {
            mpc_count = 0;
            mpc_step = true;
        }

        //Apply action
        auto hybrid_action = controller.getAction(mpc_step, mpc_weights, mass, inertia);
        robot.step(hybrid_action);
        world.integrate();

        current_time = robot.getTimeSinceReset();
        std::this_thread::sleep_for(std::chrono::microseconds(300));
        if (fmod(current_time,5.) == 0.) {    
            cout<<"Time: "<<current_time<<"s"<<endl;
        };
    };
    server.killServer();
};