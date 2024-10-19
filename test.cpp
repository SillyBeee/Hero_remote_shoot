#include "detect_bullet.hpp"
#include <iostream>
#include <vector>
#include <Eigen/Geometry>
int main(){
    cv::VideoCapture cap("/home/ma/sillybee/videos/7月14日.mp4");
    if (cap.isOpened() == false){
        std::cerr<<"cannot open the video"<<std::endl;
        return -1;
    }
    else{
        aimer::aim::DetectBullet detector;
        std::vector<aimer::aim::ImageBullet> 
        while(true){
            cv::Mat frame;
            cap >> frame;
            
            detector.process_new_frame(frame,Eigen::Quaterniond (1,0,0,0));
        }
    }
}



