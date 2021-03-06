/* Function declarations*/
#pragma once
#include "constants.h"
#include "myTypes.h"
#include <opencv2/core.hpp>

#ifndef DRAW_H
#define DRAW_H

/**
 * @brief callback function for when mouse button is clicked on an image
*/
void mouse_callback(int  event, int  x, int  y, int  flag, void* param);

static cv::Scalar randomColor(cv::RNG& rng);

/**
 * @brief Adds a scale bar to an image
 * @param image[in/out] Mat containing the image
 * @param length[in] length of the scale bar in mm
 * @param offset[in] Offset from the bottom left corner of the image where the scale will be placed
*/
void addScale(cv::Mat& image, double length = 1, cv::Point offset = cv::Point(25, 25), double fontScale = 0.5);

void drawEdges(cv::Mat src, cv::Mat& dst, cv::Mat edges, const cv::Scalar& color, const int pointSz = 1);

void drawErrors(cv::Mat src, cv::Mat& dst, std::vector<Segment>& seg, int layer = 0);

void drawMaterial(cv::Mat src, cv::Mat& dst, std::vector<Segment>& seg, std::vector<std::vector<Path>> path, int layer = 0);

void drawSegments(cv::Mat src, cv::Mat& dst, std::vector<Segment>& seg, cv::Point2d origin, int layer = 0, const int pointSz = 1);

void drawMaterialSegments(cv::Mat src, cv::Mat& dst, std::vector<Segment>& seg, std::vector<std::vector<Path>> path, int layer = 0);

void drawOutlines(cv::Mat src, cv::Mat& dst, std::vector<Segment>& seg, int layer);

#endif // !DRAW_H