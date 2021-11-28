#pragma once
#include "constants.h"
#include "myTypes.h"
#include <opencv2/core.hpp>

cv::Mat plotScan(cv::Mat scanROI, cv::Mat locEdges, cv::Mat locWin, bool showImage = false);

void plotEdges(const std::vector<cv::Point>& unfiltered, const std::vector<cv::Point>& filtered);