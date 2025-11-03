#include "ImagePixelViewer.h"
#include "tinyfiledialogs.h"

void showError(const char* message) {
	tinyfd_messageBox(
		"",          // Title
		message,            // Message
		"ok",               // Buttons ("ok", "yesno", "okcancel")
		"error",          // Icon ("info", "warning", "error", "question")
		1                   // Default button (1 = "ok" or "yes")
	);
}

void glfw_error_callback(int error, const char* description) {
	std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}


// Common OpenCV-readable extensions (depends on your build/codecs)
bool is_opencv_supported_ext(const std::string& extLower) {
	return kExt.count(extLower) != 0;
}
std::string to_lower(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return s;
}

void drop_callback(GLFWwindow* window, int count, const char** paths) {
	if (count <= 0 || paths == nullptr) {
		return;
	}

	auto* states = static_cast<ImageStates*>(glfwGetWindowUserPointer(window));
	if (!states) {
		return;
	}
	for (int i = 0; i < count; i++) {
		const std::string dropped = paths[i];
		std::filesystem::path p = std::filesystem::u8path(dropped);
		std::string extLower = to_lower(p.extension().string());
		if (extLower.empty() || !is_opencv_supported_ext(extLower)) {
			showError(("Not a valid image file: " + string(paths[i])).c_str());
			continue;
		}
		ImageState state;
		state.currentPath = p.string();
		state.filename = p.filename().u8string();
		std::cout << state.currentPath << endl;
		string load_error;
		if (!load_image_from_path(state, load_error)) {
			showError(load_error.c_str());
			continue;
		}
		states->states.push_back(state);
	}
}

void release_texture(ImageTexture& texture) {
	if (texture.id != 0) {
		glDeleteTextures(1, &texture.id);
		texture.id = 0;
		texture.width = 0;
		texture.height = 0;
	}
}

void copy_path_to_buffer(ImageState& state, const std::string& path) {
	std::fill(state.inputBuffer.begin(), state.inputBuffer.end(), '\0');
	const size_t capacity = state.inputBuffer.size() > 0 ? state.inputBuffer.size() - 1 : 0;
	if (capacity == 0) {
		return;
	}

	std::string_view slice(path);
	if (slice.size() > capacity) {
		slice = slice.substr(slice.size() - capacity);
	}

	std::memcpy(state.inputBuffer.data(), slice.data(), slice.size());
}
//bool create_texture_from_rgba(ImageTexture& texture, const cv::Mat& rgbaImage, std::string& error) {
//	if (rgbaImage.empty()) {
//		error = "Cannot create texture: image is empty.";
//		return false;
//	}
//	if (rgbaImage.type() != CV_8UC4) {
//		error = "Texture upload expects CV_8UC4 data.";
//		return false;
//	}
//
//	cv::Mat upload = rgbaImage;
//	if (!upload.isContinuous()) {
//		upload = upload.clone();
//	}
//
//	release_texture(texture);
//
//	glGenTextures(1, &texture.id);
//	if (texture.id == 0) {
//		error = "Failed to generate OpenGL texture.";
//		return false;
//	}
//
//	texture.width = upload.cols;
//	texture.height = upload.rows;
//
//	glBindTexture(GL_TEXTURE_2D, texture.id);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, upload.data);
//	glBindTexture(GL_TEXTURE_2D, 0);
//
//	GLenum glError = glGetError();
//	if (glError != GL_NO_ERROR) {
//		release_texture(texture);
//		std::ostringstream oss;
//		oss << "OpenGL error during texture upload: 0x" << std::hex << glError;
//		error = oss.str();
//		return false;
//	}
//
//	return true;
//}
bool create_texture_from_rgba(ImageTexture& texture, const cv::Mat& rgbaImage, std::string& error) {
	if (rgbaImage.empty()) {
		error = "Cannot create texture: image is empty.";
		return false;
	}
	if (rgbaImage.type() != CV_8UC4) {
		error = "Texture upload expects CV_8UC4 data.";
		return false;
	}

	const cv::Mat* src = &rgbaImage;
	cv::Mat upload;
	if (!rgbaImage.isContinuous()) {
		upload = rgbaImage.clone();
		src = &upload;
	}

	release_texture(texture);

	glGenTextures(1, &texture.id);
	if (texture.id == 0) {
		error = "Failed to generate OpenGL texture.";
		return false;
	}

	texture.width = src->cols;
	texture.height = src->rows;

	glBindTexture(GL_TEXTURE_2D, texture.id);

	// --- CRISP PIXELS: nearest filtering, no mipmaps ---
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);        // no smoothing
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);        // no smoothing
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);                 // disable mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);                  // disable mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);      // safe edges
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Upload (RGBA8)
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, src->data);

	glBindTexture(GL_TEXTURE_2D, 0);

	if (GLenum glError = glGetError(); glError != GL_NO_ERROR) {
		release_texture(texture);
		std::ostringstream oss;
		oss << "OpenGL error during texture upload: 0x" << std::hex << glError;
		error = oss.str();
		return false;
	}
	return true;
}

static const char* depth_to_string(int depth) {
	switch (depth) {
	case CV_8U:  return "8U";
	case CV_8S:  return "8S";
	case CV_16U: return "16U";
	case CV_16S: return "16S";
	case CV_32S: return "32S";
	case CV_32F: return "32F";
	case CV_64F: return "64F";
	default:     return "Unknown";
	}
}
std::string describe_mat(const cv::Mat& mat) {
	if (mat.empty()) {
		return "empty";
	}

	std::ostringstream oss;
	oss << mat.cols << "x" << mat.rows << " x" << mat.channels() << " " << depth_to_string(mat.depth());
	return oss.str();
}
static inline cv::Mat makeThumbnailLetterboxed(const cv::Mat& srcRGBA,
	int thumbW = thumbWidth,
	int thumbH = thumbHeight,
	cv::Scalar padColor = cv::Scalar(114, 114, 114, 255)) // BGRA
{
	CV_Assert(!srcRGBA.empty());
	CV_Assert(srcRGBA.type() == CV_8UC4); // RGBA/BGRA

	const int srcW = srcRGBA.cols;
	const int srcH = srcRGBA.rows;

	// Scale to fit (no stretch): scale = min(target/src)
	const double sx = static_cast<double>(thumbW) / static_cast<double>(srcW);
	const double sy = static_cast<double>(thumbH) / static_cast<double>(srcH);
	const double scale = std::min(sx, sy);

	// Compute resized dimensions
	int newW = std::max(1, static_cast<int>(std::round(srcW * scale)));
	int newH = std::max(1, static_cast<int>(std::round(srcH * scale)));

	// Choose interpolation: AREA for downscale, NEAREST for upscale (keeps pixels crisp)
	int interp = (scale < 1.0) ? cv::INTER_AREA : cv::INTER_NEAREST;

	cv::Mat resized;
	cv::resize(srcRGBA, resized, cv::Size(newW, newH), 0, 0, interp);

	// Create target canvas and center the resized image on it
	cv::Mat canvas(thumbH, thumbW, CV_8UC4, padColor);
	int x = (thumbW - newW) / 2;
	int y = (thumbH - newH) / 2;

	// In case rounding causes off-by-one, clamp ROI
	x = std::max(0, std::min(x, thumbW - newW));
	y = std::max(0, std::min(y, thumbH - newH));

	resized.copyTo(canvas(cv::Rect(x, y, newW, newH)));
	return canvas;
}
std::optional<std::string> update_preview_from_source(ImageState& state, std::string& errorOut) {
	if (state.sourceOriginal.empty()) {
		errorOut = "No source image available.";
		return std::nullopt;
	}

	cv::Mat eightBit;
	state.hasMinMax = false;

	if (state.sourceOriginal.depth() == CV_8U) {
		eightBit = state.sourceOriginal;
	}
	else {
		cv::Mat converted;
		state.sourceOriginal.convertTo(converted, CV_8U);
		eightBit = converted;
	}

	state.preview8u = eightBit;

	cv::Mat rgba;
	switch (eightBit.channels()) {
	case 1:
		cv::cvtColor(eightBit, rgba, cv::COLOR_GRAY2RGBA);
		break;
	case 3:
		cv::cvtColor(eightBit, rgba, cv::COLOR_BGR2RGBA);
		break;
	case 4:
		cv::cvtColor(eightBit, rgba, cv::COLOR_BGRA2RGBA);
		break;
	default:
		errorOut = "Unsupported channel count: " + std::to_string(eightBit.channels());
		return std::nullopt;
	}

	state.previewRGBA = rgba;

	std::string textureError;
	if (!create_texture_from_rgba(state.texture, state.previewRGBA, textureError)) {
		errorOut = textureError;
		return std::nullopt;
	}

	Mat thumb_img = makeThumbnailLetterboxed(rgba);
	create_texture_from_rgba(state.texture_thumb, thumb_img, textureError);
	std::ostringstream oss;
	oss << "original " << describe_mat(state.sourceOriginal)
		<< ", preview " << describe_mat(state.preview8u);

	return oss.str();
}

bool load_image_from_path(ImageState& state, std::string& errorOut) {
	string path = state.currentPath;
	std::error_code fsError;
	if (!std::filesystem::exists(path, fsError) || !std::filesystem::is_regular_file(path, fsError)) {
		errorOut = "File not found: " + path;
		return false;
	}

	cv::Mat loaded = cv::imread(path, cv::IMREAD_UNCHANGED);
	if (loaded.empty()) {
		errorOut = "Failed to load image via OpenCV.";
		showError(("Cannot load image file: " + state.currentPath).c_str());
		return false;
	}

	state.sourceOriginal = loaded;
	state.width = loaded.cols;
	state.height = loaded.cols;
	state.channels = loaded.channels();
	state.depth = depth_to_string(loaded.depth());
	copy_path_to_buffer(state, path);

	auto status = update_preview_from_source(state, errorOut);
	if (!status) {
		return false;
	}

	state.zoom = 1.0f;
	std::filesystem::path fsPath(path);
	std::ostringstream oss;
	std::string displayName = fsPath.filename().string();
	if (displayName.empty()) {
		displayName = fsPath.string();
	}
	return true;
}
static std::vector<ChannelLabel> make_channel_labels(const cv::Mat& mat) {
	std::vector<ChannelLabel> labels;
	int channels = mat.channels();
	labels.reserve(channels);

	if (channels == 1) {
		labels.push_back({ 0, "Gray" });
	}
	else if (channels == 3) {
		labels.push_back({ 2, "R" });
		labels.push_back({ 1, "G" });
		labels.push_back({ 0, "B" });
	}
	else if (channels == 4) {
		labels.push_back({ 2, "R" });
		labels.push_back({ 1, "G" });
		labels.push_back({ 0, "B" });
		labels.push_back({ 3, "A" });
	}
	else {
		for (int i = 0; i < channels; ++i) {
			labels.push_back({ i, nullptr });
		}
	}

	return labels;
}
template <typename T>
void append_pixel_components(std::ostringstream& oss, const cv::Mat& mat, int x, int y, const std::vector<ChannelLabel>& labels) {
	const T* ptr = mat.ptr<T>(y) + static_cast<size_t>(x) * mat.channels();
	for (size_t i = 0; i < labels.size(); ++i) {
		if (i > 0) {
			oss << ", ";
		}
		int channelIndex = std::clamp(labels[i].index, 0, mat.channels() - 1);
		if (labels[i].name) {
			oss << labels[i].name;
		}
		else {
			oss << "C" << channelIndex;
		}
		oss << "=" << static_cast<long double>(ptr[channelIndex]);
	}
}
std::string format_pixel_value(const cv::Mat& mat, int x, int y) {
	if (mat.empty() || x < 0 || y < 0 || x >= mat.cols || y >= mat.rows) {
		return "";
	}

	std::ostringstream oss;
	oss << "(" << x << ", " << y << ") [";
	std::vector<ChannelLabel> labels = make_channel_labels(mat);

	switch (mat.depth()) {
	case CV_8U:
		append_pixel_components<unsigned char>(oss, mat, x, y, labels);
		break;
	case CV_8S:
		append_pixel_components<signed char>(oss, mat, x, y, labels);
		break;
	case CV_16U:
		append_pixel_components<uint16_t>(oss, mat, x, y, labels);
		break;
	case CV_16S:
		append_pixel_components<int16_t>(oss, mat, x, y, labels);
		break;
	case CV_32S:
		append_pixel_components<int32_t>(oss, mat, x, y, labels);
		break;
	case CV_32F:
		append_pixel_components<float>(oss, mat, x, y, labels);
		break;
	case CV_64F:
		append_pixel_components<double>(oss, mat, x, y, labels);
		break;
	default:
		oss << "unsupported depth";
		break;
	}

	oss << "]";
	return oss.str();
}


void DeleteSelected(ImageStates& states) {
    if (states.states.empty()) return;
    int del = states.selected;
    if (del < 0 || del >= (int)states.states.size()) return;

    // Release GL resources
    release_texture(states.states[del].texture);
    release_texture(states.states[del].texture_thumb);

    // Remove from vector
    states.states.erase(states.states.begin() + del);

    // Fix selection
    if (states.states.empty()) {
        states.selected = 0;
    } else {
        states.selected = std::min(del, (int)states.states.size() - 1);
    }
}





