#include "image.h"
#include <iostream>
#include <algorithm>
#include <fstream>

namespace intel {

// -- Fractal --

double Fractal::calcOnePixel(int x0, int y0)
{
	double fx0 = double(x0) - double(mySize[0]) / 2;
	double fy0 = double(y0) - double(mySize[1]) / 2;
	fx0 = fx0 / myMagn + cx;
	fy0 = fy0 / myMagn + cy;

	double res = 0;
	double x = 0;
	double y = 0;
	for (int iter = 0; x * x + y * y <= 4 && iter < maxIter; ++iter) {
		const double val = x * x - y * y + fx0;
		y = 2 * x * y + fy0;
		x = val;
		res += exp(-sqrt(x * x + y * y));
	}

	return res;
}

// -- Image --

void Image::write(const char* fname) const
{
	if (myData.empty()) {
		std::cout << "Warning: Image is empty.\n";
		return;
	}
	std::ofstream stream{ fname };
	stream.write((char*)& file.type, file.sizeRest);
	stream.write((char*)& info, info.size);
	stream.write((char*)myData[0].bgra, myData.size() * sizeof(myData[0]));
}

void Image::fill(std::uint8_t r, std::uint8_t g, std::uint8_t b, int x, int y) 
{
	if (myData.empty())
		return;

	if (x < 0 && y < 0) //fill whole Image
		std::fill(myData.begin(), myData.end(), Pixel(b, g, r));
	else {
		auto& bgra = myData[myWidth * x + y].bgra;
		bgra[3] = 0, bgra[2] = r, bgra[1] = g, bgra[0] = b;
	}
}

void Image::fill(Fractal* frac) 
{
	if (myData.empty())
		reset(myWidth, myHeight);

	int i = -1;
	int w = this->myWidth;
	std::for_each(myData.begin(), myData.end(), [&i, w, &frac](Image::Pixel& p) {
		++i;
		int x = i / w, y = i % w;
		// auto val = frac(x, y);
		auto val = frac->calcOnePixel(x, y);
		if (val > 255)
			val = 255;
		uint8_t uval = static_cast<uint8_t>(val);
		p = Image::Pixel(uval, uval, uval);
		});
}

// private method
void Image::reset(int w, int h)
{
	if (w <= 0 || h <= 0) {
		std::cerr << "Warning: Invalid Image size.\n";
		return;
	}

	myWidth = w;
	myHeight = h;

	// reset raw data
	myData.resize(myWidth * myHeight);
	myRows.resize(myHeight);

	// reset rows
	for (int i = 0; i < myRows.size(); ++i) {
		myRows[i] = &myData[0] + (i * myWidth);
	}

	int myPadSize = (4 - (w * sizeof(myData[0])) % 4) % 4;
	int sizeData = w * h * sizeof(myData[0]) + h * myPadSize;
	int sizeAll = sizeData + sizeof(file) + sizeof(info);

	// BITMAPFILEHEADER
	file.sizeRest = 14;
	file.type = 0x4d42; //same as 'BM' in ASCII
	file.size = sizeAll;
	file.reserved = 0;
	file.offBits = 54;

	// BITMAPINFOHEADER
	info.size = 40;
	info.width = w;
	info.height = h;
	info.planes = 1;
	info.bitCount = 32;
	info.compression = 0;
	info.sizeImage = sizeData;
	info.yPelsPerMeter = 0;
	info.xPelsPerMeter = 0;
	info.clrUsed = 0;
	info.clrImportant = 0;
}

std::shared_ptr<Image> makeFractalImage(double magn)
{
	const std::string name = std::string("fractal_") + std::to_string((int)magn);
	auto image_ptr = std::make_shared<Image>(name, IMAGE_WIDTH, IMAGE_HEIGHT);

	Fractal fr(image_ptr->width(), image_ptr->height(), magn);
	// image_ptr->fill([&fr](int x, int y) { return fr.calcOnePixel(x, y); });
	image_ptr->fill(&fr);

	return image_ptr;
}

} // end namespace intel