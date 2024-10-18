#include "detect_bullet.hpp"
#include <iostream>
int main(){
    cv::VideoCapture cap("/sillybee/videos/7月14日.mp4");
    if (cap.isOpened() == false){
        std::cerr<<"cannot open the video"<<std::endl;
        return -1;
    }
    else{
        while(true){
            cv::Mat frame;
            cap >> frame;
            
        }
    }
}



