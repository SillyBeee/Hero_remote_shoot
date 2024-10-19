#include "detect_bullet.hpp"
#include <iostream>
#include <vector>
#include <Eigen/Geometry>
#include <unistd.h>
int main(){
    cv::VideoCapture cap("/home/ma/桌面/C0165.MP4");
    if (cap.isOpened() == false){
        std::cerr<<"cannot open the video"<<std::endl;
        return -1;
    }
    else{
        aimer::aim::DetectBullet detector;
        
        while(true){
            cv::Mat frame;
            cap >> frame;
            cv::Mat draw;
            detector.process_new_frame(frame,Eigen::Quaterniond (0,0,0,0));
            draw = detector.print_bullets();
            cv::resize(draw,draw,cv::Size(1920,));
            cv::imshow("draw",draw);
            cv::waitKey(0.01);
        }
    }
}



