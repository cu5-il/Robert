#pragma once
#include <vector>
#include "myTypes.h"
#include "raster.h"
#include <opencv2/core.hpp>
#include "MaterialModel.h"
#include "input.h"

#include <iterator> 
#include <algorithm>

#ifndef MULTILAYER_H
#define MULTILAYER_H

///////////////////////////////////////  MultiLayerScaffold  ///////////////////////////////////////
class MultiLayerScaffold
{
public:
	// default constructor
	MultiLayerScaffold() {}
	MultiLayerScaffold(TableInput input, Raster _raster, double _omega);

	std::vector<std::vector<Path>> path, pathScan;
	Raster raster;
	std::vector<Segment> segments, segmentsScan;
	
	void leadout(double length);

	//TODO: set theta for entire path
	//void theta(double angle);
	//void theta(std::vector<double> angle);

private:
	void _interpPathPoints(std::vector<cv::Point2i> inPts, double wayptSpc, std::vector<cv::Point2i>& outPts);
	void _makePath(TableInput input, Raster _raster, double _omega, std::vector<Segment>& _segments, std::vector<std::vector<Path>>& _path);
	
};

inline MultiLayerScaffold::MultiLayerScaffold(TableInput input, Raster _raster, double _omega)
{
	raster = _raster;
	_makePath(input, raster, _omega, segments, path);
	// make the scanning path 
	if (input.layers == 1)
	{
		TableInput input2 = input;
		// set the scanning speed
		input2.F = 1;
		double scanSpacing = raster.width() == 0 ? raster.spacing() : raster.width() / ceil(raster.width() / 13.0);
		
		Raster rasterScan(raster.length() + 2 * raster.rodWidth(), raster.width(), scanSpacing, raster.rodWidth(), raster.border());
		rasterScan.offset(cv::Point2d(input.initPos.x, input.initPos.y));
		input2.initPos+=cv::Point3d(0, 0, 1); // raise path
		switch (input.startLayer)
		{
		default:
			break;
		case 0:
			rasterScan.offset(cv::Point2d(-SCAN_OFFSET_X - raster.rodWidth(), raster.spacing() / 2));
			break;
		case 1:
			rasterScan.offset(cv::Point2d(raster.spacing() / 2, -SCAN_OFFSET_X - raster.rodWidth()));
			break;
		case 2:
			rasterScan.offset(cv::Point2d(SCAN_OFFSET_X - raster.rodWidth(), raster.spacing() / 2));
			break;
		case 3:
			rasterScan.offset(cv::Point2d(raster.spacing() / 2, SCAN_OFFSET_X - raster.rodWidth()));
			break;
		}
		_makePath(input2, rasterScan, _omega, segmentsScan, pathScan);
		// Set the theta position to be constant
		for (int i = 0; i < pathScan.size(); i++) {
			for (int j = 0; j < pathScan[i].size(); j++) {
				pathScan[i][j].T = 90.0 * input.startLayer;
			}
		}

	}
}

inline void MultiLayerScaffold::leadout(double length)
{
	switch (segments.back().dir())
	{
	case printDir::X_POS: 
		segments.back().setScanDonePt(segments.back().scanDonePt() + cv::Point2d(length, 0));
		break;
	case printDir::X_NEG: 
		segments.back().setScanDonePt(segments.back().scanDonePt() - cv::Point2d(length, 0));
		break;
	case printDir::Y_POS:
		segments.back().setScanDonePt(segments.back().scanDonePt() + cv::Point2d(0, length));
		break;
	case printDir::Y_NEG:
		segments.back().setScanDonePt(segments.back().scanDonePt() - cv::Point2d(0, length));
		break;
	}
}

inline void MultiLayerScaffold::_interpPathPoints(std::vector<cv::Point2i> inPts, double wayptSpc, std::vector<cv::Point2i>& outPts)
{
	cv::Point2d diff, delta;
	double L;

	// Interpolate the points
	for (auto it = inPts.begin(); it != std::prev(inPts.end(), 1); ++it) {
		diff = *std::next(it, 1) - *it;
		L = cv::norm(diff);
		delta = (diff / L) * (double)MM2PIX(wayptSpc);
		for (int i = 0; cv::norm(i * delta) < L; i++) {
			outPts.push_back(*it + cv::Point2i(i * delta));
		}
	}
}

inline void MultiLayerScaffold::_makePath(TableInput input, Raster _raster, double _omega, std::vector<Segment>& _segments, std::vector<std::vector<Path>>& _path)
{
	cv::Rect roi;
	std::vector<cv::Point2i> wp_px;
	std::vector<cv::Point2d> wp_mm;
	cv::Point2d scanDonePt;
	int pixRodWth = MM2PIX(_raster.rodWidth());
	int direction = 1;
	cv::Point2d origin = _raster.origin();
	std::vector<Path> tmp;

	double z = input.initPos.z;
	double T = 90.0 * ((4 - input.startLayer) % 4);
	double e = input.E;
	double f = input.F;
	double w = 0;

	double desThetaPos = T;
	double omega = _omega;
	double dTheta = input.wayptSpc / f * omega;
	cv::Point2d prevPt;

	double rodDoneOffset = std::min(_raster.length(), 2 * fabs(SCAN_OFFSET_X) + 1) - _raster.length();

	// Rods
	for (int layer = input.startLayer; layer < input.layers + input.startLayer; layer++)
	{
		for (auto it = _raster.px(layer).begin(); it != std::prev(_raster.px(layer).end()); ++it)
		{
			// Generate the pixel waypoints
			_interpPathPoints(std::vector<cv::Point2i> {*it, * std::next(it)}, input.wayptSpc, wp_px);
			//// if long rods
			//if (std::distance(_raster.px(layer).begin(), it) % 2 == 0)
			//{
			//	_interpPathPoints(std::vector<cv::Point2i> {*it, * std::next(it)}, input.wayptSpc, wp_px);
			//}
			//else
			//{
			//	std::vector<cv::Point2i> wp_px_temp;
			//	_interpPathPoints(std::vector<cv::Point2i> {*it, * std::next(it)}, input.wayptSpc, wp_px_temp);
			//	wp_px.push_back(wp_px_temp.front());
			//	wp_px.push_back(wp_px_temp.back());
			//}

			// add the final point of the pattern
			if (std::next(it) == std::prev(_raster.px(layer).end())) {
				wp_px.push_back(*std::next(it));
			}

			// Determining the theta positions
			if (std::distance(_raster.px(layer).begin(), it) % 2 == 1 )
			{
				desThetaPos = 90.0 * (double)((_segments.back().dir() + 2) % 4);
			}

			// making the path
			wp_mm.resize(wp_px.size());
			std::transform(wp_px.begin(), wp_px.end(), wp_mm.begin(), [&origin](cv::Point& pt) {return (PIX2MM(cv::Point2d(pt)) + origin); });
			for (auto it2 = wp_mm.begin(); it2 != wp_mm.end(); ++it2) 
			{
				//if ((std::distance(wp_mm.begin(), it2) > 0) || (std::distance(_raster.px(layer).begin(), it) % 2 == 0)){
				//	if (fabs(desThetaPos - T) > dTheta){ T += copysign(dTheta, desThetaPos - T); }
				//	else { T = desThetaPos; }
				//}
				if (std::distance(_raster.px(layer).begin(), it) > 0 ) {
					dTheta = cv::norm(prevPt - (*it2)) / f * omega;
					if (fabs(desThetaPos - T) > dTheta) { T += copysign(dTheta, desThetaPos - T); }
					else { T = desThetaPos; }
				}
				tmp.push_back(Path(*it2, z, T, f, e, w));
				prevPt = *it2;
			}
			_path.push_back(tmp);

			// Determine the direction 
			if (((*std::next(it)).x - (*it).x) > 0) { direction = printDir::X_POS; }	// positive x direction
			else if (((*std::next(it)).x - (*it).x) < 0) { direction = printDir::X_NEG; }	// negative x direction
			else if (((*std::next(it)).y - (*it).y) > 0) { direction = printDir::Y_POS; }	// positive y direction
			else if (((*std::next(it)).y - (*it).y) < 0) { direction = printDir::Y_NEG; }	// negative y direction

			// Setting the scan done point and the segment roi
			scanDonePt = wp_mm.front();
			// ----------- FIRST/THIRD LAYER -----------
			if (layer % 2 == 0) 
			{
				switch (direction % 2) {
				case 0: // Horizontal lines
					roi = cv::Rect(*it - cv::Point(0, pixRodWth / 2), *std::next(it, 1) + cv::Point(0, pixRodWth / 2));
					// shifting and stretching the roi
					//roi -= cv::Point(pixRodWth / 2, 0);
					//roi += cv::Size(pixRodWth / 1, 0);
					roi -= cv::Point(pixRodWth / 4, 0);
					roi += cv::Size(pixRodWth / 2, 0);
					//roi += cv::Point(pixRodWth / 2, 0);
					//roi -= cv::Size(pixRodWth, 0);
					// defining the point when the region has been completely scanned as the end of the next horizontal line
					scanDonePt += (layer % 4 == 0) ? cv::Point2d(rodDoneOffset, _raster.spacing()) : cv::Point2d(rodDoneOffset, -_raster.spacing());
					if (!_raster.roi(layer).contains(scanDonePt)) {
						scanDonePt -= cv::Point2d(rodDoneOffset * 2, 0);
					}

					// if it is the final segment
					if (std::next(it) == std::prev(_raster.px(layer).end())) {
						scanDonePt = wp_mm.back();
					}
					break;
				case 1: // vertical lines
					roi = cv::Rect(0, 0, 1, 1);
					//roi = cv::Rect(*it - cv::Point(pixRodWth / 4, 0), *std::next(it, 1) + cv::Point(pixRodWth / 2, 0));
					//roi -= cv::Point(0, pixRodWth / 4);
					//roi += cv::Size(0, pixRodWth /2);
					//roi = cv::Rect(*it - cv::Point(pixRodWth / 2, 0), *std::next(it, 1) + cv::Point(pixRodWth / 2, 0));
					//roi -= cv::Point(0, pixRodWth / 2);
					//roi += cv::Size(0, pixRodWth);
					// defining the point when the region has been completely scanned as the midpoint of the next vertical line

					// if it's not the last vertical rod
					if (std::next(it, 2) != std::prev(_raster.px(layer).end())) {
						scanDonePt += (layer % 4 == 0) ? cv::Point2d(_raster.length(), 1.5 * _raster.spacing()) : cv::Point2d(_raster.length(), -1.5 * _raster.spacing());
					}
					else {
						scanDonePt += (layer % 4 == 0) ? cv::Point2d(_raster.length(), _raster.spacing()) : cv::Point2d(_raster.length(), -_raster.spacing());
					}
					if (!_raster.roi(layer).contains(scanDonePt)) {
						scanDonePt -= cv::Point2d(_raster.length() * 2, 0);
					}
					break;
				}
			}
			// ----------- SECOND/FOURTH LAYER -----------
			else if (layer % 2 == 1)
			{
				switch (direction % 2) {
				case 0: // Horizontal lines
					roi = cv::Rect(0, 0, 1, 1);
					
					// defining the point when the region has been completely scanned as the midpoint of the next horizontal line
					// if it's not the last horizontal rod
					if (std::next(it, 2) != std::prev(_raster.px(layer).end())) {
						scanDonePt += (layer % 4 == 1) ? cv::Point2d(1.5 * _raster.spacing(), _raster.length()) : cv::Point2d(-1.5 * _raster.spacing(), _raster.length());
					}
					else {
						scanDonePt += (layer % 4 == 1) ? cv::Point2d(_raster.spacing(), _raster.length()) : cv::Point2d(-_raster.spacing(), _raster.length());
					}
					if (!_raster.roi(layer).contains(scanDonePt)) {
						scanDonePt -= cv::Point2d(0, _raster.length() * 2);
					}
					break;
				case 1: // vertical lines
					roi = cv::Rect(*it - cv::Point(pixRodWth / 2, 0), *std::next(it, 1) + cv::Point(pixRodWth / 2, 0));
					// // shifting and stretching the roi
					//roi -= cv::Point(0, pixRodWth / 4);
					//roi += cv::Size(0, pixRodWth / 2);
					roi += cv::Point(0, pixRodWth / 2);
					roi -= cv::Size(0, pixRodWth);
					// defining the point when the region has been completely scanned as the end of the next vertical line
					scanDonePt += (layer % 4 == 1) ? cv::Point2d(_raster.spacing(), rodDoneOffset) : cv::Point2d(-_raster.spacing(), rodDoneOffset);
					if (!_raster.roi(layer).contains(scanDonePt)) {
						scanDonePt -= cv::Point2d(0, rodDoneOffset * 2);
					}
					// if it is the final segment
					if (std::next(it) == std::prev(_raster.px(layer).end())) {
						scanDonePt = wp_mm.back();
					}
					break;
				}
			}
			_segments.push_back(Segment(roi, wp_px, scanDonePt, direction, layer));
			wp_px.clear();
			wp_mm.clear();
			tmp.clear();
		}
		z += input.height;
	}
}


///////////////////////////////////////  FunGenScaf  ///////////////////////////////////////

class FunGenScaf : public MultiLayerScaffold
{
public:
	FunGenScaf() {};
	FunGenScaf(TableInput input, Raster _raster, double _omega, MaterialModel matModel);

private:
	bool _makeFGS(char scafType, double range[2]);
	void _setInput(MaterialModel matModel);


};

FunGenScaf::FunGenScaf(TableInput input, Raster _raster, double _omega, MaterialModel matModel)
	: MultiLayerScaffold(input, _raster, _omega)
{
	if (_makeFGS(input.type, input.range)) { _setInput(matModel); }
}

inline bool FunGenScaf::_makeFGS(char scafType, double range[2])
{
	int numPts;
	double delta;
	double width = range[0];

	switch (scafType)
	{
	default:
		std::cout << "ERROR: unknown scaffold type" << std::endl;
		return false;
		break;
	case 'b': // bowtie scaffold
	{
		numPts = path[0].size() - 1;
		delta = (range[1] - range[0]) / (numPts / 2.0);

		for (auto it_seg = path.begin(); it_seg != path.end(); ++it_seg) {
			for (auto it_rod = (*it_seg).begin(); it_rod != (*it_seg).end(); ++it_rod) {

				// Modify the width
				(*it_rod).w = width;
				// if long rods
				if (std::distance(path.begin(), it_seg) % 2 == 0) {
					// decrease for first half of rod then increase for second half
					if (std::distance((*it_seg).begin(), it_rod) < (numPts / 2.0)) {
						width += delta;
					}
					else {
						width -= delta;
					}
				}
				else {
					width = range[0];
				}
			}
		}
		break;
	}
	case 'g': // gradient scaffold
	{
		numPts = path[0].size() - 1;
		delta = (range[1] - range[0]) / numPts;

		for (auto it_seg = path.begin(); it_seg != path.end(); ++it_seg) {
			for (auto it_rod = (*it_seg).begin(); it_rod != (*it_seg).end(); ++it_rod) {

				// Modify the width
				(*it_rod).w = width;
				switch (std::distance(path.begin(), it_seg) % 4)
				{
				case 0: // positive x direction rods
					width += delta;
					break;
				case 1: // y direction rods at max x
					width = range[1];
					break;
				case 2: // negative x direction rods
					width -= delta;
					break;
				case 3: // y direction rods at min x
					width = range[0];
					break;
				}
			}
		}
		break;
	}
	case 'c': // continuous gradient scaffold
	{
		numPts = ceil((path[0].size() - 1) * ceil(path.size() / 2.0));
		delta = (range[1] - range[0]) / numPts;

		for (auto it_seg = path.begin(); it_seg != path.end(); ++it_seg) {
			for (auto it_rod = (*it_seg).begin(); it_rod != (*it_seg).end(); ++it_rod) {

				// Modify the width
				(*it_rod).w = width;
				// if long rods
				if (std::distance(path.begin(), it_seg) % 2 == 0) {
					width += delta;
				}
			}
		}
		break;
	}
	case 'p': // process map print
	{
		numPts = ceil((path.size() - 1) / 2.0);
		delta = (range[1] - range[0]) / numPts;

		for (auto it_seg = path.begin(); it_seg != path.end(); ++it_seg) {
			for (auto it_rod = (*it_seg).begin(); it_rod != (*it_seg).end(); ++it_rod) {
				// Modify the width
				(*it_rod).w = width;
			}
			if (std::distance(path.begin(), it_seg) % 2 == 0) {
				width += delta;
			}
		}
		break;
	}
	}
	return true;
}

inline void FunGenScaf::_setInput(MaterialModel matModel)
{
	// modify the inputs
	for (auto it_seg = path.begin(); it_seg != path.end(); ++it_seg) {
		for (auto it_rod = (*it_seg).begin(); it_rod != (*it_seg).end(); ++it_rod) {

			// Modify the width
			switch (matModel.type())
			{
			case MaterialModel::VELOCITY:
				(*it_rod).f = matModel.output((*it_rod).w, (*it_rod).e);
				break;
			case MaterialModel::AUGER:
				(*it_rod).e = matModel.output((*it_rod).w, (*it_rod).f);
				break;
			}
		}
	}
}


#endif // !MULTILAYER_H

