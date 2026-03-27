#include "car_control.h"

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    ros::init(argc, argv, "control_main_node");
    car::CarMove carmove_node_;
    carmove_node_.init();
    carmove_node_.run();
    ros::spin();

    return 0;
}


