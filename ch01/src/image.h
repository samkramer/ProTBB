#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <memory>

namespace intel {

//! Fractal class
class Fractal {
public:
	//! Constructor
	Fractal(int x, int y, double m) : mySize{ x, y }, myMagn(m) {}
	~Fractal() = default;

	// don't allow copying
	Fractal(const Fractal&) = delete;
	Fractal& operator=(const Fractal&) = delete;

	//! One pixel calculation routine
	double calcOnePixel(int x0, int y0);

private:
	//! Size of the Fractal area
	const std::array<int, 2> mySize;
	//! Fractal properties
	const double cx = -0.7436;
	const double cy = 0.1319;
	const double myMagn;
	const int maxIter = 1000;
};

class Image {
public:
	struct Pixel {
		Pixel() = default;
		~Pixel() = default;

		Pixel(const Pixel&) = default;
		Pixel& operator=(const Pixel&) = default;

		Pixel(std::uint8_t b, std::uint8_t g, std::uint8_t r) {
			bgra[0] = b;
			bgra[1] = g;
			bgra[2] = r;
			bgra[3] = 0;
		}

		uint8_t bgra[4];
	};

	Image(const std::string& n, int w = 1920, int h = 1080) : myName(n) {
		reset(w, h);
		magn = 0;
	}
	~Image() = default;

	// don't allow copying
	Image(const Image&) = delete;
	Image& operator=(const Image&) = delete;

	std::string name() const { return myName; }
	std::string setName(const std::string& n) { return myName = n; }

	int width() const { return myWidth; }
	int height() const { return myHeight; }

	void setMagn(double m) { magn = m; }
	double getMagn() const { return magn; }

	void write(const char* fname) const;
	void fill(std::uint8_t r, std::uint8_t g, std::uint8_t b, int x = -1, int y = -1);
	void fill(Fractal* frac);

	std::vector<Pixel*>& rows() { return myRows; }

private:
	void reset(int w, int h);

private:
	std::string myName;
	int myWidth;
	int myHeight;
	double magn;

	std::vector<Pixel> myData; //raw raster data
	std::vector<Pixel*> myRows;

	//data structures 'file' and 'info' are using to store an Image as BMP file
	//for more details see https://en.wikipedia.org/wiki/BMP_file_format
	using BITMAPFILEHEADER = struct {
		std::uint16_t sizeRest; // field is not from specification, 
				  // was added for alignemt. store size of rest of the fields
		std::uint16_t type;
		std::uint32_t size;
		std::uint32_t reserved;
		std::uint32_t offBits;
	};
	BITMAPFILEHEADER file;

	using BITMAPINFOHEADER = struct {
		std::uint32_t size;
		std::int32_t width;
		std::int32_t height;
		std::uint16_t planes;
		std::uint16_t bitCount;
		std::uint32_t compression;
		std::uint32_t sizeImage;
		std::int32_t xPelsPerMeter;
		std::int32_t yPelsPerMeter;
		std::uint32_t clrUsed;
		std::uint32_t clrImportant;
	};
	BITMAPINFOHEADER info;
};

const int IMAGE_WIDTH = 800;
const int IMAGE_HEIGHT = 800;
const int MAX_BGR_VALUE = 255;

std::shared_ptr<Image> makeFractalImage(double magn);

} // end namespace intel
