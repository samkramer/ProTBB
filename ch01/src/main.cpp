#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <tbb/tbb.h>
#include <pstl/algorithm>
#include <pstl/execution>
#include "image.h"

using ImagePtr = std::shared_ptr<intel::Image>;

ImagePtr applyGamma(ImagePtr image_ptr, double gamma);
ImagePtr applyTint(ImagePtr image_ptr, const std::array<double, 3> & tints);
ImagePtr applyGammaP(ImagePtr image_ptr, double gamma);
ImagePtr applyTintP(ImagePtr image_ptr, const std::array<double, 3> & tints);
ImagePtr applyGammaS(ImagePtr image_ptr, double gamma);
ImagePtr applyTintS(ImagePtr image_ptr, const std::array<double, 3> & tints);
void writeImage(ImagePtr image_ptr);

void processSerial(const std::vector<ImagePtr>& image_vector) {
	const std::array<double, 3> tint_array = { 0.75, 0, 0 }; // B,G,R
	for (ImagePtr img : image_vector) {
		img = applyGamma(img, 1.4);
		img = applyTint(img, tint_array);
		writeImage(img);
	}
}

// My implementation -- create fractal as part of flow
void processFlowGraph(const std::vector<int>& magnitudes)
{
	const std::array<double, 3> tint_array = { 0.75, 0, 0 };

	tbb::flow::graph graph;

	int i = 0;
	tbb::flow::source_node<ImagePtr> src(graph,
		[&i, &magnitudes](ImagePtr& out) -> bool {
			if (i < magnitudes.size()) {
				int magn = magnitudes[i++];
				const std::string name = std::string("fractal_") + std::to_string(magn);
				out = std::make_shared<intel::Image>(name, intel::IMAGE_WIDTH, intel::IMAGE_HEIGHT);
				out->setMagn(magn);
		
				return true;
			}
			else {
				return false;
			}
		}, false);

	tbb::flow::function_node<ImagePtr, ImagePtr> fractal(graph,
		tbb::flow::unlimited,
		[](ImagePtr img) -> ImagePtr {
			double magn = img->getMagn();
			intel::Fractal fr(img->width(), img->height(), magn);
			// img->fill([&fr](int x, int y) { return fr.calcOnePixel(x, y); });
			img->fill(&fr);
			return img;
		}
	);

	tbb::flow::function_node<ImagePtr, ImagePtr> gamma(graph,
		tbb::flow::unlimited,
		[](ImagePtr img) -> ImagePtr {
			return applyGammaP(img, 1.4);
		}
	);

	tbb::flow::function_node<ImagePtr, ImagePtr> tint(graph,
		tbb::flow::unlimited,
		[tint_array](ImagePtr img) -> ImagePtr {
			return applyTintP(img, tint_array);
		}
	);

	tbb::flow::function_node<ImagePtr> write(graph,
		tbb::flow::unlimited,
		[](ImagePtr img) {
			writeImage(img);
		}
	);

	tbb::flow::make_edge(src, fractal);
	tbb::flow::make_edge(fractal, gamma);
	tbb::flow::make_edge(gamma, tint);
	tbb::flow::make_edge(tint, write);
	src.activate();
	graph.wait_for_all();
}

ImagePtr applyGamma(ImagePtr image_ptr, double gamma) {
	auto output_image_ptr = std::make_shared<intel::Image>(image_ptr->name() + "_gamma", intel::IMAGE_WIDTH, intel::IMAGE_HEIGHT);
	auto in_rows = image_ptr->rows();
	auto out_rows = output_image_ptr->rows();
	const int height = in_rows.size();
	const int width = in_rows[1] - in_rows[0];

	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			const intel::Image::Pixel& pixel = in_rows[i][j];

			double v = 0.3 * pixel.bgra[2] + 0.59 * pixel.bgra[1] + 0.11 * pixel.bgra[0];
			double res = std::pow(v, gamma);
			if (res > intel::MAX_BGR_VALUE) 
				res = intel::MAX_BGR_VALUE;

			out_rows[i][j] = intel::Image::Pixel(res, res, res);
		}
	}

	return output_image_ptr;
}

// Fork-Join Layer using tbb::parallel_for
ImagePtr applyGammaP(ImagePtr image_ptr, double gamma) {
	auto output_image_ptr =	std::make_shared<intel::Image>(image_ptr->name() + "_gamma", intel::IMAGE_WIDTH, intel::IMAGE_HEIGHT);
	auto in_rows = image_ptr->rows();
	auto out_rows = output_image_ptr->rows();
	const int height = in_rows.size();
	const int width = in_rows[1] - in_rows[0];

	tbb::parallel_for(0, height,
		[&in_rows, &out_rows, width, gamma](int i) {
			for (int j = 0; j < width; ++j) {
				const intel::Image::Pixel& pixel = in_rows[i][j];

				double v = 0.3 * pixel.bgra[2] + 0.59 * pixel.bgra[1] + 0.11 * pixel.bgra[0];
				double res = std::pow(v, gamma);
				if (res > intel::MAX_BGR_VALUE)
					res = intel::MAX_BGR_VALUE;

				out_rows[i][j] = intel::Image::Pixel(res, res, res);
			}
		}
	);

	return output_image_ptr;
}

// SIMD Layer using Parallel STL Transform
ImagePtr applyGammaS(ImagePtr image_ptr, double gamma) {
	auto output_image_ptr =	std::make_shared<intel::Image>(image_ptr->name() + "_gamma", intel::IMAGE_WIDTH, intel::IMAGE_HEIGHT);
	auto in_rows = image_ptr->rows();
	auto out_rows = output_image_ptr->rows();
	const int height = in_rows.size();
	const int width = in_rows[1] - in_rows[0];

	tbb::parallel_for(0, height,
		[&in_rows, &out_rows, width, gamma](int i) {
			auto in_row = in_rows[i];
			auto out_row = out_rows[i];
			std::transform(pstl::execution::unseq, in_row, in_row + width,
				out_row, [gamma](const intel::Image::Pixel& pixel) {
					double v = 0.3 * pixel.bgra[2] + 0.59 * pixel.bgra[1] + 0.11 * pixel.bgra[0];
					double res = std::pow(v, gamma);
					if (res > intel::MAX_BGR_VALUE) 
						res = intel::MAX_BGR_VALUE;

					return intel::Image::Pixel(res, res, res);
				});
		}
	);

	return output_image_ptr;
}

ImagePtr applyTint(ImagePtr image_ptr, const std::array<double, 3>& tints) {
	auto output_image_ptr = std::make_shared<intel::Image>(image_ptr->name() + "_tinted", intel::IMAGE_WIDTH, intel::IMAGE_HEIGHT);
	auto in_rows = image_ptr->rows();
	auto out_rows = output_image_ptr->rows();
	const int height = in_rows.size();
	const int width = in_rows[1] - in_rows[0];

	for (int i = 0; i < height; ++i) {
		for (int j = 0; j < width; ++j) {
			const intel::Image::Pixel& pixel = in_rows[i][j];

			double db = (double)pixel.bgra[0] + (intel::MAX_BGR_VALUE - pixel.bgra[0]) * tints[0];
			double dg = (double)pixel.bgra[1] + (intel::MAX_BGR_VALUE - pixel.bgra[1]) * tints[1];
			double dr = (double)pixel.bgra[2] + (intel::MAX_BGR_VALUE - pixel.bgra[2]) * tints[2];
			std::uint8_t b = static_cast<std::uint8_t>( (db > intel::MAX_BGR_VALUE) ? intel::MAX_BGR_VALUE : db );
			std::uint8_t g = static_cast<std::uint8_t>( (dg > intel::MAX_BGR_VALUE) ? intel::MAX_BGR_VALUE : dg );
			std::uint8_t r = static_cast<std::uint8_t>( (dr > intel::MAX_BGR_VALUE) ? intel::MAX_BGR_VALUE : dr );

			out_rows[i][j] = intel::Image::Pixel(b, g, r);
		}
	}

	return output_image_ptr;
}

// Fork-Join Layer using tbb::parallel_for
ImagePtr applyTintP(ImagePtr image_ptr, const std::array<double, 3> & tints) {
	auto output_image_ptr = std::make_shared<intel::Image>(image_ptr->name() + "_tinted", intel::IMAGE_WIDTH, intel::IMAGE_HEIGHT);
	auto in_rows = image_ptr->rows();
	auto out_rows = output_image_ptr->rows();
	const int height = in_rows.size();
	const int width = in_rows[1] - in_rows[0];

	tbb::parallel_for(0, height,
		[&in_rows, &out_rows, width, tints](int i) {
			for (int j = 0; j < width; ++j) {
				const intel::Image::Pixel& pixel = in_rows[i][j];

				double db = (double)pixel.bgra[0] + (intel::MAX_BGR_VALUE - pixel.bgra[0]) * tints[0];
				double dg = (double)pixel.bgra[1] + (intel::MAX_BGR_VALUE - pixel.bgra[1]) * tints[1];
				double dr = (double)pixel.bgra[2] + (intel::MAX_BGR_VALUE - pixel.bgra[2]) * tints[2];
				std::uint8_t b = static_cast<std::uint8_t>((db > intel::MAX_BGR_VALUE) ? intel::MAX_BGR_VALUE : db);
				std::uint8_t g = static_cast<std::uint8_t>((dg > intel::MAX_BGR_VALUE) ? intel::MAX_BGR_VALUE : dg);
				std::uint8_t r = static_cast<std::uint8_t>((dr > intel::MAX_BGR_VALUE) ? intel::MAX_BGR_VALUE : dr);

				out_rows[i][j] = intel::Image::Pixel(b, g, r);
			}
		}
	);

	return output_image_ptr;
}

// SIMD Layer using Parallel STL Transform
ImagePtr applyTintS(ImagePtr image_ptr, const std::array<double, 3> & tints) {
	auto output_image_ptr =
		std::make_shared<intel::Image>(image_ptr->name() + "_tinted",
			intel::IMAGE_WIDTH, intel::IMAGE_HEIGHT);
	auto in_rows = image_ptr->rows();
	auto out_rows = output_image_ptr->rows();
	const int height = in_rows.size();
	const int width = in_rows[1] - in_rows[0];

	tbb::parallel_for(0, height,
		[&in_rows, &out_rows, width, tints](int i) {
			auto in_row = in_rows[i];
			auto out_row = out_rows[i];
			std::transform(pstl::execution::unseq, in_row, in_row + width,
				out_row, [tints](const intel::Image::Pixel& pixel) {
					double db = (double)pixel.bgra[0] + (intel::MAX_BGR_VALUE - pixel.bgra[0]) * tints[0];
					double dg = (double)pixel.bgra[1] + (intel::MAX_BGR_VALUE - pixel.bgra[1]) * tints[1];
					double dr = (double)pixel.bgra[2] + (intel::MAX_BGR_VALUE - pixel.bgra[2]) * tints[2];
					std::uint8_t b = static_cast<std::uint8_t>((db > intel::MAX_BGR_VALUE) ? intel::MAX_BGR_VALUE : db);
					std::uint8_t g = static_cast<std::uint8_t>((dg > intel::MAX_BGR_VALUE) ? intel::MAX_BGR_VALUE : dg);
					std::uint8_t r = static_cast<std::uint8_t>((dr > intel::MAX_BGR_VALUE) ? intel::MAX_BGR_VALUE : dr);

					return intel::Image::Pixel(b, g, r);
				});
		}
	);
	return output_image_ptr;
}

void writeImage(ImagePtr image_ptr) {
	std::string path = std::string("output/") + image_ptr->name() + ".bmp";
	image_ptr->write(path.c_str());
}

// ~11 seconds
void runSerial()
{
	tbb::tick_count t0 = tbb::tick_count::now();

	std::vector<ImagePtr> image_vector;
	for (int i = 2000; i < 20000000; i *= 10) {
		image_vector.push_back(intel::makeFractalImage(i));
	}
	// image_vector.push_back(intel::makeFractalImage(20000000));

	processSerial(image_vector);

	tbb::tick_count t1 = tbb::tick_count::now();
	std::cout << "Time : " << (t1 - t0).seconds() << " seconds" << std::endl;
}

// ~ 6 seconds
void runFlowGraph()
{
	tbb::tick_count t0 = tbb::tick_count::now();

	// warmup the scheduler
	tbb::parallel_for(0, tbb::task_scheduler_init::default_num_threads(), [](int) {
		tbb::tick_count tn = tbb::tick_count::now();
		while ((tbb::tick_count::now() - tn).seconds() < 0.01);
	});

	std::vector<int> magnitudes;
	for (int i = 2000; i < 20000000; i *= 10) {
		magnitudes.push_back(i);
	}

	processFlowGraph(magnitudes);

	tbb::tick_count t1 = tbb::tick_count::now();
	std::cout << "Time : " << (t1 - t0).seconds() << " seconds" << std::endl;
}

int main(int argc, char* argv[])
{
	// runSerial();
	runFlowGraph();
}

