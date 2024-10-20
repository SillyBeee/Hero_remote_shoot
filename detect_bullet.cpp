#ifndef AIMER_AUTO_AIM_PREDICTOR_AIM_DETECT_BULLET_CPP
#define AIMER_AUTO_AIM_PREDICTOR_AIM_DETECT_BULLET_CPP

#include "detect_bullet.hpp"

#include <ctime>
#include <opencv2/imgproc.hpp>

#include "do_reproj.hpp"

namespace aimer::aim {
const float WEIGHTS[3] = { 4, 4, 2 }; //计算颜色差异的权重
const uint8_t DIFF_STEP = 5;  //颜色差异的步长
const uint8_t DIFF_THRESHOLD = 30;  //颜色差异的阈值
const cv::Size KERNEL1_SIZE = cv::Size(10, 10);  //膨胀运算核大小
const cv::Size KERNEL2_SIZE = cv::Size(4, 4); //开运算核大小
const cv::Scalar COLOR_LOWB = cv::Scalar(25, 40, 40); //颜色范围的下限  //棕色
const cv::Scalar COLOR_UPB = cv::Scalar(90, 255, 255); //颜色范围的上限  //青绿色
const cv::Scalar MIN_VUE = cv::Scalar(0, 255 * .1, 255 * .2); //最小的视觉亮度

// 测试如果一个轮廓中某个像素满足 test_is_bullet_color，那么这个就是弹丸
bool test_is_bullet_color(const cv::Vec3b& hsv_col) { // H代表色相，S代表饱和度，V代表亮度,根据hsv的值判断是否为弹丸
    return hsv_col[2] > 50  //饱和度大于50
        && fabs((int)hsv_col[0] - 50) < 10 + .5 * exp((hsv_col[1] + hsv_col[2]) / 100);
}

cv::Mat DoFrameDifference::get_diff(// 做帧差（并与原来的取交）（可获取弹道轨迹）
    const cv::Mat& s1,
    const cv::Mat& s2,
    const cv::Mat& ref,
    const cv::Mat& lst_fr_bullets) {
    this->tme -= (double)clock() / CLOCKS_PER_SEC;  //获取程序运行实际时间
    // cv::imshow("s1", s1);
    // cv::imshow("s2", s2);
    cv::Mat res = cv::Mat::zeros(s1.rows, s1.cols, CV_8U);  //新建空白图像
    for (size_t y = 0; y < s1.rows; y += DIFF_STEP) {
        for (size_t x = 0; x < s1.cols; x += DIFF_STEP) {  //步长为5
            cv::Point p(x, y);  //提取第一张图像坐标点像素值
            if (!ref.at<uint8_t>(p) || (!lst_fr_bullets.empty() && lst_fr_bullets.at<uint8_t>(p)))
                continue; // 如果该点在参考图像中为0，或者上一帧为空或该点在上一帧弹丸图像中为1，则跳过
            const cv::Vec3b& c1 = s1.at<cv::Vec3b>(p);  //c1引用p点像素值
            bool flag = true;   
            for (int dy = -0; dy < 1 && flag; ++dy) {  
                int ty = y + dy; 
                if (ty < 0 || ty >= s1.rows)
                    continue;
                for (int dx = -0; dx < 1 && flag; ++dx) {
                    int tx = x + dx;
                    if (tx < 0 || tx >= s1.cols)
                        continue;
                    const cv::Vec3b& c2 = s2.at<cv::Vec3b>(cv::Point(tx, ty));  //遍历获取第二张图像的邻域像素点
                    uint8_t tmp = (WEIGHTS[0] * abs(c1[0] - c2[0]) + WEIGHTS[1] * abs(c1[1] - c2[1])
                                   + WEIGHTS[2] * abs(c1[2] - c2[2]))   //使用权重与2张图片的做差除以权重和作为判断条件
                        / (WEIGHTS[0] + WEIGHTS[1] + WEIGHTS[2]);
                    if (tmp < DIFF_THRESHOLD)  //如果小于阈值则标记为false
                        flag = false;  
                }
            }
            res.at<uint8_t>(p) = flag ? 255 : 0;  //如果flag为true，则标记为255，否则为0
        }   //即两张图片差距大的地方会标黑
    }
    cv::dilate(res, res, this->kernel1);  //膨胀，使效果更明显
    if (!lst_fr_bullets.empty()) {   //如果上一帧有弹丸
        res |= lst_fr_bullets;  //将上一帧弹丸图像与当前图像取并集
    }
    // cv::imshow("diff", res);
    this->tme += (double)clock() / CLOCKS_PER_SEC;  //获取程序运行实际时间
    return res;
}

DetectBullet::DetectBullet() {  //DetectBullet类的构造函数
    this->kernel1 = cv::getStructuringElement(cv::MORPH_ELLIPSE, KERNEL1_SIZE);
    this->kernel2 = cv::getStructuringElement(cv::MORPH_CROSS, KERNEL2_SIZE);
}

void DetectBullet::init(const DoReproj& do_reproj) {
    this->do_reproj = do_reproj;
}

DetectBullet::DetectBullet(const DoReproj& do_reproj) {
    this->init(do_reproj);
}

// 找出可能是子弹的区域（调用了帧差和重投影） 
void DetectBullet::get_possible() {
    tme_get_possible -= (double)clock() / CLOCKS_PER_SEC;  //

    // 对上一帧的 hsv 进行重投影
    assert(!this->lst_hsv.empty());  //assert用于检查上一帧的hsv是否为空
    //对上一帧图像进行重投影
    cv::Mat lst_reproj = this->do_reproj.reproj(this->lst_hsv, this->lst_fr_q, this->cur_fr_q); //获取上一帧的透视变换
    cv::Mat res, msk_not_dark;
    // 先根据弹丸颜色判断可能是弹丸的部分
    // 在一个大概的绿色范围内
    cv::inRange(this->cur_hsv, COLOR_LOWB, COLOR_UPB, res);
    // 不能太黑
    cv::inRange(this->cur_hsv, MIN_VUE, cv::Scalar(255, 255, 255), msk_not_dark);
    res &= msk_not_dark;   //做交集获取结果，即弹丸可能的位置
    // cv::Mat tmp1 = res.clone();

    // 根据 hsv 作帧差
    cv::Mat mat_diff = this->do_diff.get_diff(this->cur_hsv, lst_reproj, res, this->lst_msk);   //这里的res可能区域用作参考
    // 如果上一帧某个位置有弹丸，需要考虑这一帧某颗弹丸跑到了同样的位置上
    res &= mat_diff;  //可能区域和帧差做交集

    // cv::Mat show_mat = res + (tmp1 - res) * .5;
    // cv::imshow("show_mat", show_mat);

    // 进行滤波
    cv::morphologyEx(res, res, cv::MORPH_OPEN, this->kernel2);  //进行开运算

    tme_get_possible += (double)clock() / CLOCKS_PER_SEC;  //获取get_possible的运行时间

    // 寻找轮廓
    tme_find_contours -= (double)clock() / CLOCKS_PER_SEC;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(res, this->contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    tme_find_contours += (double)clock() / CLOCKS_PER_SEC;
    for (const cv::Vec4i& vc: hierarchy) {
        if (~vc[3]) {  //检查是否有父轮廓
            // std::cerr << "Nested contours!" << std::endl;
        }
    }
}

void DetectBullet::sort_points(std::vector<cv::Point>& vec) { // 对点进行排序，会返回排序好的vec
    tme_sort_points -= (double)clock() / CLOCKS_PER_SEC;

    if (this->sort_pts.empty()) {
        // 如果 sort_pts 没有初始化，那么先初始化
        this->sort_pts = std::vector<std::vector<uint32_t>>(this->cur_frame.cols);// 二维vector数组，大小为当前图像的列
    }

    uint32_t mn_x = this->cur_frame.cols, mx_x = 0;
    // 计算 vec 中所有点的 x 坐标范围
    // 物理意义：遍历图像的所有点，将x坐标相同的点存入sort_pts的x行中，并对y值排序
    for (const cv::Point& pt: vec) {  // 遍历vec中所有点
        uint32_t x = pt.x, y = pt.y;    // 赋值给临时变量
        this->sort_pts[x].emplace_back(y);  // 将y值存入二维vector数组的x行中
        if (x < mn_x)
            mn_x = x; // 记录最小的x
        if (x > mx_x)
            mx_x = x;  // 记录最大的x
    }
    std::vector<cv::Point>().swap(vec); // 清空vec的内存
    for (uint32_t x = mn_x; x <= mx_x; ++x) {// 遍历minx到maxx的点
        std::vector<uint32_t>& vc_x = this->sort_pts[x];  // 提取sort_pts的x行存入vc_x中
        if (vc_x.size() > 10) { // 如果vc_x的长度大于10，则使用sort函数，否则使用冒泡排序
            sort(vc_x.begin(), vc_x.end());
        } else {
            for (uint32_t i = 0; i < vc_x.size(); ++i) {
                for (uint32_t j = 0; j < i; ++j) {
                    if (vc_x[j] > vc_x[i])
                        std::swap(vc_x[i], vc_x[j]);
                }
            }
        }
        for (uint32_t y: vc_x) {
            vec.emplace_back(x, y);  // 将vc_x（排序好的）的值存入vec的x行中
        }
        std::vector<uint32_t>().swap(vc_x); // 清空vc_x的内存
    }

    tme_sort_points += (double)clock() / CLOCKS_PER_SEC;
}

// 找出一个轮廓中最亮的部分
bool DetectBullet::test_is_bullet(std::vector<cv::Point> contour) {  //输入一个点集
    this->sort_points(contour); //对轮廓内的点进行排序
    tme_get_brightest -= (double)clock() / CLOCKS_PER_SEC;

    this->sort_points(contour);
    bool flag = false;

    for (uint32_t i = 0, j = 0; i < contour.size() && !flag; i = j) {
        int x = contour[i].x;//获取
        while (j < contour.size() && x == contour[j].x)
            ++j;
        assert(i < j);
        for (int y = contour[i].y; y <= contour[j - 1].y && !flag; ++y) {
            if (test_is_bullet_color(this->cur_hsv.at<cv::Vec3b>(cv::Point(x, y)))) {
                flag = true;
            }
        }
    }
    tme_get_brightest += (double)clock() / CLOCKS_PER_SEC;
    return flag;
}

// 获取子弹位置、半径
void DetectBullet::get_bullets() {
    bullets.clear();  //清空子弹vector
    this->lst_msk = cv::Mat::zeros(this->cur_frame.rows, this->cur_frame.cols, CV_8U); //根据当前帧大小初始化一个全黑图像

    // std::cerr << "contours size = " << this->contours.size() << std::endl;
    for (uint32_t i = 0; i < this->contours.size(); ++i) { //遍历所有轮廓
        const std::vector<cv::Point>& contour = this->contours[i];  //
        cv::RotatedRect rect = cv::minAreaRect(contour); // 获取最小外接矩形
        cv::Size rect_size = rect.size;
        if (rect_size.area() < 30)  //如果太小，则排除掉
            continue;
        double ratio = cv::contourArea(contour) / rect_size.area(); 
        if (ratio < 0.5) //如果轮廓面积与最小外接矩形面积之比小于0.5，即形状不符合则排除掉
            continue;
        if (this->test_is_bullet(contour)) {// 判断是否是子弹
            this->bullets.emplace_back( 
                rect.center,
                std::min(rect_size.height, rect_size.width) * .5); // 存入子弹vector
            cv::drawContours(  //在lst_msk中绘制轮廓，lst_msk用来给帧差做参考
                this->lst_msk,
                contours,
                i,
                255,
                cv::FILLED); // 在下一帧中作为上一帧识别到的子弹
        }
    }
}

// 输出标出子弹的图像
cv::Mat DetectBullet::print_bullets() {
    cv::Mat res = this->cur_frame.clone();
    for (const ImageBullet& bul: this->bullets) { // 遍历所有子弹
        cv::circle(res, bul.center, bul.radius, cv::Scalar(255, 255, 255)); // 在图像上绘制子弹
    }
    // cv::imshow("actual_res", res);
    // cv::waitKey(0);
    return res;
}

std::vector<ImageBullet>
DetectBullet::process_new_frame(const cv::Mat& new_frame, const Eigen::Quaterniond& q) { // 输入新的图像和当前相机的姿态,返回子弹vector
    tme_total -= (double)clock() / CLOCKS_PER_SEC;
    //更新参数
    this->lst_hsv = this->cur_hsv.clone();
    this->lst_frame = this->cur_frame.clone();
    this->cur_frame = new_frame.clone();
    this->lst_fr_q = this->cur_fr_q;
    this->cur_fr_q = q;
    
    cv::cvtColor(this->cur_frame, this->cur_hsv, cv::COLOR_BGR2HSV);
    // 处理新帧
    if (!this->lst_frame.empty()) {
        this->get_possible();
        this->get_bullets();
    }

    tme_total += (double)clock() / CLOCKS_PER_SEC;

    return this->bullets;
}
} // namespace aimer::aim

#endif /* AIMER_AUTO_AIM_PREDICTOR_AIM_DETECT_BULLET_CPP */
