#pragma once
#ifndef MAIN_H
#define MAIN_H
#include <GL/glew.h>


#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#include <opencv2/opencv.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <optional>
#include <filesystem>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string_view>
#include <array>
#include <unordered_set>

#pragma region Consts
const float PREVIEW_WIDTH = 300.0f;
const float maxZoom = 72.0f;
const float thumbWidth = 96.0f;
const float thumbHeight = 54.0f;
static const std::unordered_set<std::string> kExt{
	// bitmap
	".bmp", ".dib",
	// jpeg
	".jpg", ".jpeg", ".jpe", ".jfif",
	// jpeg2000
	".jp2", ".j2k", ".jpf", ".jpx", ".j2c",
	// png
	".png",
	// portable anymap
	".pbm", ".pgm", ".ppm", ".pnm",
	// tiff
	".tif", ".tiff",
	// sun raster / others
	".sr", ".ras",
	// webp (if built with webp)
	".webp",
	// hdr-ish
	".hdr", ".pic",
	// OpenEXR (if built with OpenEXR)
	".exr"
};
#define _S(_LITERAL)    (const char*)u8##_LITERAL
#pragma endregion

#pragma region USINGS
using std::cout;
using std::endl;
using std::vector;
using std::string;
using cv::Mat;
namespace fs = std::filesystem;
#pragma endregion


#pragma region Structs
struct ChannelLabel {
	int index = 0;
	const char* name = nullptr;
};

struct ImageTexture {
	GLuint id = 0;
	int width = 0;
	int height = 0;
};

struct ImageState {
	std::array<char, 512> inputBuffer{};
	ImageTexture texture{};
	ImageTexture texture_thumb{};
	cv::Mat sourceOriginal;
	cv::Mat preview8u;
	cv::Mat previewRGBA;
	std::string currentPath;
	std::string filename;
	std::string depth;
	bool fitToWindow = true;
	int width;
	int height;
	int channels;
	double minVal = 0.0;
	double maxVal = 0.0;
	bool hasMinMax = false;
	float minZoom = -1;
	float zoom = 1.0f;
	ImVec2 pan = ImVec2(0.0f, 0.0f);
};

struct ImageStates {
	bool Link_View = false;
	bool Gray_Image = false;
	bool Auto_Maximize_Contrast = false;
	bool One_Channel_Pseudo_Color = false;
	bool Four_Channel_Ignore_Alpha = false;
	vector<ImageState> states;
	int selected = 0;
};
#pragma endregion





#pragma region Utils

void glfw_error_callback(int error, const char* description);
void drop_callback(GLFWwindow* window, int count, const char** paths);
void release_texture(ImageTexture& texture);
bool load_image_from_path(ImageState& state, std::string& errorOut);
std::string format_pixel_value(const cv::Mat& mat, int x, int y);
void DeleteSelected(ImageStates& states);
#pragma endregion


int ImagePixelViewer();






#endif // MAIN_H

