// Program: extract_color_channel
// Description: Reads an image using OpenCV, extracts a specified color channel (B,G,R)
//              pixel-by-pixel and writes out a single-channel grayscale image containing
//              only that channel's intensity values.
// Usage: ./extract_color_channel <input_image> <output_image> <channel_index>
//        channel_index: 0 = Blue, 1 = Green, 2 = Red (OpenCV default BGR order)
// Example: ./extract_color_channel imagens\ PPM/lena.ppm lena_red.pgm 2
// Notes: The output is saved as an 8-bit single channel image. Works with 3-channel color images.

#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
	if (argc != 4) {
		std::cerr << "Usage: " << argv[0] << " <input_image> <output_image> <channel_index>\n"
				  << "channel_index: 0=Blue 1=Green 2=Red (OpenCV BGR)\n";
		return 1;
	}

	std::string inputPath = argv[1];
	std::string outputPath = argv[2];
	int channelIndex;
	try {
		channelIndex = std::stoi(argv[3]);
	} catch (const std::exception&) {
		std::cerr << "Invalid channel index (must be an integer 0-2).\n";
		return 1;
	}

	if (channelIndex < 0 || channelIndex > 2) {
		std::cerr << "Channel index out of range (expected 0,1,2).\n";
		return 1;
	}

	// Read the input image as color (this ensures we have 3 channels in BGR order)
	cv::Mat color = cv::imread(inputPath, cv::IMREAD_COLOR);
	if (color.empty()) {
		std::cerr << "Failed to read image: " << inputPath << "\n";
		return 1;
	}

	if (color.channels() != 3) {
		std::cerr << "Input image must have 3 channels (BGR). Got " << color.channels() << " channels.\n";
		return 1;
	}

	// Create single-channel output image with same width/height, 8-bit
	cv::Mat singleChannel(color.rows, color.cols, CV_8UC1);

	// Pixel-by-pixel extraction
	for (int r = 0; r < color.rows; ++r) {
		const cv::Vec3b* srcRow = color.ptr<cv::Vec3b>(r);
		unsigned char* dstRow = singleChannel.ptr<unsigned char>(r);
		for (int c = 0; c < color.cols; ++c) {
			dstRow[c] = srcRow[c][channelIndex];
		}
	}

	// Write output image
	if (!cv::imwrite(outputPath, singleChannel)) {
		std::cerr << "Failed to write output image: " << outputPath << "\n";
		return 1;
	}

	std::cout << "Extracted channel " << channelIndex << " (" << (channelIndex==0?"Blue":channelIndex==1?"Green":"Red")
			  << ") from '" << inputPath << "' to '" << outputPath << "'\n";
	return 0;
}

