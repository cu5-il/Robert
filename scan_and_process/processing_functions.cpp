#include <iostream>
#include <fstream>
#include <cmath>
#include <vector> 

#include "constants.h"
#include "myTypes.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>


void processData(double data[][NUM_DATA_SAMPLES], Coords* fbk, cv::Mat& profile) {
	int fbIdx[2];
	int profileStartIdx;
	cv::Mat profileVoltage_8U, profileEdges, profileEdgesIdx;

	cv::Mat dataMat(NUM_PROFILE_PTS, NUM_DATA_SAMPLES, CV_64F, data); // copying the collected data into a matrix
	
	// Get the position feedback when the laser was triggered
	cv::minMaxIdx(dataMat.row(1), NULL, NULL, fbIdx, NULL); //find the rising edge of the trigger signal sent to the laser
	fbk->x = dataMat.at<double>(2, fbIdx[1]); // assigning the position feedback values
	fbk->y = dataMat.at<double>(3, fbIdx[1]);
	fbk->z = dataMat.at<double>(4, fbIdx[1]);
	fbk->T = dataMat.at<double>(5, fbIdx[1]);

	// Finding the voltage header of the scanner signal, i.e find the start of the scanned profile
	// Search only the data after the triggering signal was sent (after index fbIdx[1])
	cv::normalize(dataMat(cv::Range(0, 1), cv::Range(fbIdx[1], dataMat.cols)), profileVoltage_8U, 0, 255, cv::NORM_MINMAX, CV_8U);
	cv::Canny(profileVoltage_8U, profileEdges, 10, 30, 3);
	cv::findNonZero(profileEdges, profileEdgesIdx);

	profileStartIdx = profileEdgesIdx.at<int>(1, 0) + fbIdx[1]; // Starting index of the scan profile
	profile = dataMat(cv::Rect(profileStartIdx, 0, NUM_PROFILE_PTS, 1)).clone() / OPAMP_GAIN; // Isolating the scanned profile and converting to height

	return;
}

void local2globalScan(int profileCols, const Coords fbk, const std::vector<double>& printROI, cv::Point &profileStart, cv::Point& profileEnd , cv::Range& profileROIRange) {
	std::cout << "print roi = ";
	for (auto i : printROI)
		std::cout << i << ", ";
	std::cout << std::endl;
	std::vector<double> XY_start(2),XY_end(2);
	double X, Y;
	double R = SCAN_OFFSET;
	double local_x;
	int startIdx = -1, endIdx = 0;

	for (int i = 0; i < profileCols; i++) {
		// Local coordinate of the scanned point
		local_x = -SCAN_WIDTH / 2 + i * SCAN_WIDTH / (profileCols - 1);
		// Transforming local coordinate to global coordinate
		X = fbk.x - R * cos(fbk.T * PI / 180) - local_x * sin(fbk.T * PI / 180);
		Y = fbk.y - R * sin(fbk.T * PI / 180) + local_x * cos(fbk.T * PI / 180);
		// Check if scanned point in outside the print ROI 
		if ((X < printROI[0] || printROI[2] < X || Y < printROI[1] || printROI[3] < Y)) {
		}
		else if (startIdx == -1) {
			startIdx = i;
			XY_start = { X, Y };
		}
		else { 
			endIdx = i; 
			XY_end = { X, Y };
		}
	}
	//convert the start and end (X,Y) coordinates of the scan to points on the image
	cv::Point startPx(std::round((XY_start[1] - printROI[1]) / PIX2MM), std::round((XY_start[0] - printROI[0]) / PIX2MM));
	cv::Point endPx(std::round((XY_end[1] - printROI[1]) / PIX2MM), std::round((XY_end[0] - printROI[0]) / PIX2MM));

	profileStart = cv::Point (std::round((XY_start[1] - printROI[1]) / PIX2MM), std::round((XY_start[0] - printROI[0]) / PIX2MM));
	profileEnd = cv::Point (std::round((XY_end[1] - printROI[1]) / PIX2MM), std::round((XY_end[0] - printROI[0]) / PIX2MM));
	profileROIRange = cv::Range(startIdx, endIdx);
	return;
}

void findEdges(cv::Mat edgeBoundary, cv::Point profileStart, cv::Point profileEnd, cv::Mat& scanROI, cv::Mat& gblEdges, cv::Mat& locEdges, double heightThresh) {

	cv::LineIterator it(edgeBoundary, profileStart, profileEnd, 8);
	std::vector<cv::Point> windowPts;
	windowPts.reserve(it.count);
	uchar lastVal = 0;
	uchar curVal = 0;

	// find the intersection of the scan and the edge boundary using a line iterator
	for (int i = 0; i < it.count; i++, ++it) {
		curVal = *(const uchar*)*it;
		if ((curVal == 255) && (lastVal == 0) && (i != 0)) { // find rising edges
			windowPts.push_back(it.pos());
		}
		if ((curVal == 0) && (lastVal == 255) && (i != 0)) { // find falling edges
			windowPts.push_back(it.pos());
		}
		lastVal = curVal;
	}
	if ((windowPts.size() % 2) != 0) {
		std::cout << "ERROR: odd number of edges" << std::endl;
		return ;
	}

	// create a height mask for the scan profile to remove all edges below a height threshold
	cv::Mat heightMask;
	cv::threshold(scanROI, heightMask, heightThresh, 1, cv::THRESH_BINARY);
	cv::normalize(heightMask, heightMask, 0, 255, cv::NORM_MINMAX, CV_8U);


	// Search within the edges of the dialated raster for the actual edges
	cv::Mat searchWindow;
	cv::Mat edges;
	std::vector<cv::Point> edgeCoords;

	for (int i = 0; i < windowPts.size(); i = i + 2) { // loop through all the search windows

		searchWindow = scanROI(cv::Range::all(), cv::Range(windowPts[i].x, windowPts[(int)i + 1].x)); // isolate the area around a single raster rod
		cv::normalize(searchWindow, searchWindow, 0, 255, cv::NORM_MINMAX, CV_8U); // Normalize the search window
		cv::Canny(searchWindow, edges, 10, 20, 7);
		cv::findNonZero(edges, edgeCoords);

		if (edgeCoords.size() == 2) { //verify that only two edges were found

		}

		// mark edges on local profile and global ROI
		for (int j = 0; j < edgeCoords.size(); j++) {
			if (heightMask.at<uchar>(cv::Point(edgeCoords[j].x + windowPts[i].x, 0)) == 255) { // check if edges are within height mask
				locEdges.at<uchar>(cv::Point(edgeCoords[j].x + windowPts[i].x , 0)) = 255;
				gblEdges.at<uchar>(cv::Point(edgeCoords[j].x + windowPts[i].x + profileStart.x, profileStart.y)) = 255;
			}

		}

	}
}