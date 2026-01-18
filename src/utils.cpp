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

static std::string normalize_path(const fs::path& path) {
	std::error_code ec;
	fs::path absolutePath = fs::absolute(path, ec);
	if (ec) {
		absolutePath = path;
	}
	return absolutePath.lexically_normal().string();
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
		fs::path p = fs::u8path(dropped);
		std::string extLower = to_lower(p.extension().string());
		if (extLower.empty() || !is_opencv_supported_ext(extLower)) {
			showError(("Not a valid image file: " + string(paths[i])).c_str());
			continue;
		}
		const std::string normalizedPath = normalize_path(p);
		bool alreadyLoaded = false;
		for (const auto& existing : states->states) {
			if (normalize_path(fs::path(existing.currentPath)) == normalizedPath) {
				alreadyLoaded = true;
				break;
			}
		}
		if (alreadyLoaded) {
			continue;
		}
		ImageState state;
		state.grayApplied = states->Gray_Image;
		state.autoContrastApplied = states->Auto_Maximize_Contrast;
		state.pseudoColorApplied = states->One_Channel_Pseudo_Color;
		state.ignoreAlphaApplied = states->Four_Channel_Ignore_Alpha;
		state.currentPath = p.string();
		state.filename = p.filename().u8string();
		std::cout << state.currentPath << endl;
		string load_error;
		if (!load_image_from_path(state, load_error, true)) {
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
	GLint internalFormat = 0;
	GLenum dataType = 0;
	switch (rgbaImage.type()) {
	case CV_8UC4:
		internalFormat = GL_RGBA8;
		dataType = GL_UNSIGNED_BYTE;
		break;
	case CV_16UC4:
		internalFormat = GL_RGBA16;
		dataType = GL_UNSIGNED_SHORT;
		break;
	case CV_32FC4:
		internalFormat = GL_RGBA32F;
		dataType = GL_FLOAT;
		break;
	default:
		error = "Texture upload expects CV_8UC4, CV_16UC4, or CV_32FC4 data.";
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

	// Upload (RGBA)
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texture.width, texture.height, 0,
		GL_RGBA, dataType, src->data);

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

	cv::Mat previewMat;
	state.hasMinMax = false;
	state.minVal = 0.0;
	state.maxVal = 0.0;

	if (!state.autoContrastApplied) {
		switch (state.sourceOriginal.depth()) {
		case CV_8U:
			previewMat = state.sourceOriginal;
			break;
		case CV_8S: {
			cv::Mat converted;
			state.sourceOriginal.convertTo(converted, CV_8U);
			previewMat = converted;
			break;
		}
		case CV_16U:
			previewMat = state.sourceOriginal;
			break;
		case CV_16S: {
			cv::Mat converted;
			state.sourceOriginal.convertTo(converted, CV_16U);
			previewMat = converted;
			break;
		}
		case CV_32S:
		case CV_32F:
		case CV_64F: {
			cv::Mat converted;
			state.sourceOriginal.convertTo(converted, CV_32F);
			previewMat = converted;
			break;
		}
		default:
			errorOut = "Unsupported image depth.";
			return std::nullopt;
		}
	}
	else {
		std::vector<cv::Mat> channels;
		cv::split(state.sourceOriginal, channels);
		if (channels.empty()) {
			errorOut = "Failed to split image channels.";
			return std::nullopt;
		}

		double minAcross = std::numeric_limits<double>::infinity();
		double maxAcross = -std::numeric_limits<double>::infinity();

		for (size_t i = 0; i < channels.size(); ++i) {
			if (channels.size() == 4 && i == 3) {
				cv::Mat alpha8;
				channels[i].convertTo(alpha8, CV_8U);
				channels[i] = alpha8;
				continue;
			}

			cv::Mat floatChan;
			channels[i].convertTo(floatChan, CV_32F);
			double minVal = 0.0;
			double maxVal = 0.0;
			cv::minMaxLoc(floatChan, &minVal, &maxVal);
			minAcross = std::min(minAcross, minVal);
			maxAcross = std::max(maxAcross, maxVal);

			cv::Mat normalized8;
			if (minVal == maxVal) {
				normalized8 = cv::Mat::zeros(floatChan.size(), CV_8U);
			}
			else {
				floatChan.convertTo(normalized8, CV_8U, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
			}
			channels[i] = normalized8;
		}

		state.minVal = std::isfinite(minAcross) ? minAcross : 0.0;
		state.maxVal = std::isfinite(maxAcross) ? maxAcross : 0.0;
		state.hasMinMax = true;

		cv::merge(channels, previewMat);
	}

	if (state.grayApplied && previewMat.channels() > 1) {
		cv::Mat gray;
		if (previewMat.channels() == 3) {
			cv::cvtColor(previewMat, gray, cv::COLOR_BGR2GRAY);
		}
		else if (previewMat.channels() == 4) {
			cv::cvtColor(previewMat, gray, cv::COLOR_BGRA2GRAY);
		}
		previewMat = gray;
	}

	bool previewIsRGBA = false;
	if (state.pseudoColorApplied && previewMat.channels() == 1) {
		cv::Mat gray8;
		if (previewMat.depth() == CV_8U) {
			gray8 = previewMat;
		}
		else {
			previewMat.convertTo(gray8, CV_8U);
		}
		cv::Mat colorBgr;
		cv::applyColorMap(gray8, colorBgr, cv::COLORMAP_TURBO);
		cv::cvtColor(colorBgr, previewMat, cv::COLOR_BGR2RGBA);
		previewIsRGBA = true;
	}

	if (state.ignoreAlphaApplied && previewMat.channels() == 4) {
		std::vector<cv::Mat> channels;
		cv::split(previewMat, channels);
		if (channels.size() == 4) {
			switch (previewMat.depth()) {
			case CV_8U:
				channels[3].setTo(255);
				break;
			case CV_16U:
				channels[3].setTo(65535);
				break;
			case CV_32F:
				channels[3].setTo(1.0f);
				break;
			default:
				break;
			}
			cv::merge(channels, previewMat);
		}
	}

	state.preview8u = previewMat;

	cv::Mat rgba;
	if (previewIsRGBA) {
		rgba = previewMat;
	}
	else {
		switch (previewMat.channels()) {
		case 1:
			cv::cvtColor(previewMat, rgba, cv::COLOR_GRAY2RGBA);
			break;
		case 3:
			cv::cvtColor(previewMat, rgba, cv::COLOR_BGR2RGBA);
			break;
		case 4:
			cv::cvtColor(previewMat, rgba, cv::COLOR_BGRA2RGBA);
			break;
		default:
			errorOut = "Unsupported channel count: " + std::to_string(previewMat.channels());
			return std::nullopt;
		}
	}

	state.previewRGBA = rgba;

	std::string textureError;
	if (!create_texture_from_rgba(state.texture, state.previewRGBA, textureError)) {
		errorOut = textureError;
		return std::nullopt;
	}

	cv::Mat thumbSource = rgba;
	if (thumbSource.type() != CV_8UC4) {
		cv::Mat thumb8;
		thumbSource.convertTo(thumb8, CV_8U);
		thumbSource = thumb8;
	}
	Mat thumb_img = makeThumbnailLetterboxed(thumbSource);
	create_texture_from_rgba(state.texture_thumb, thumb_img, textureError);
	std::ostringstream oss;
	oss << "original " << describe_mat(state.sourceOriginal)
		<< ", preview " << describe_mat(state.preview8u);

	return oss.str();
}

static bool read_file_stamp(const std::string& path,
	fs::file_time_type& outWriteTime,
	std::uintmax_t& outFileSize,
	std::string& errorOut) {
	std::error_code fsError;
	if (!std::filesystem::exists(path, fsError) || !std::filesystem::is_regular_file(path, fsError)) {
		errorOut = "File not found: " + path;
		return false;
	}
	outWriteTime = std::filesystem::last_write_time(path, fsError);
	if (fsError) {
		errorOut = "Failed to read file timestamp: " + path;
		return false;
	}
	outFileSize = std::filesystem::file_size(path, fsError);
	if (fsError) {
		errorOut = "Failed to read file size: " + path;
		return false;
	}
	return true;
}

bool load_image_from_path(ImageState& state, std::string& errorOut, bool showErrors) {
	string path = state.currentPath;
	std::error_code fsError;
	if (!std::filesystem::exists(path, fsError) || !std::filesystem::is_regular_file(path, fsError)) {
		errorOut = "File not found: " + path;
		return false;
	}

	cv::Mat loaded = cv::imread(path, cv::IMREAD_UNCHANGED);
	if (loaded.empty()) {
		errorOut = "Failed to load image via OpenCV.";
		if (showErrors) {
			showError(("Cannot load image file: " + state.currentPath).c_str());
		}
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
	fs::file_time_type writeTime{};
	std::uintmax_t fileSize = 0;
	std::string stampError;
	if (read_file_stamp(path, writeTime, fileSize, stampError)) {
		state.lastWriteTime = writeTime;
		state.lastFileSize = fileSize;
		state.hasFileStamp = true;
	}
	return true;
}

bool rebuild_preview_from_source(ImageState& state, bool grayImage, bool autoMaximizeContrast, bool oneChannelPseudoColor, bool ignoreAlpha, std::string& errorOut) {
	if (state.sourceOriginal.empty()) {
		errorOut = "No source image available.";
		return false;
	}
	state.grayApplied = grayImage;
	state.autoContrastApplied = autoMaximizeContrast;
	state.pseudoColorApplied = oneChannelPseudoColor;
	state.ignoreAlphaApplied = ignoreAlpha;
	auto status = update_preview_from_source(state, errorOut);
	return status.has_value();
}

bool refresh_image_if_changed(ImageState& state, std::string& errorOut) {
	if (state.currentPath.empty()) {
		return false;
	}

	fs::file_time_type writeTime{};
	std::uintmax_t fileSize = 0;
	std::string stampError;
	if (!read_file_stamp(state.currentPath, writeTime, fileSize, stampError)) {
		return false;
	}

	if (!state.hasFileStamp) {
		state.lastWriteTime = writeTime;
		state.lastFileSize = fileSize;
		state.hasFileStamp = true;
		return false;
	}

	if (writeTime == state.lastWriteTime && fileSize == state.lastFileSize) {
		return false;
	}

	const bool wasFit = state.fitToWindow;
	const float oldZoom = state.zoom;
	const float oldMinZoom = state.minZoom;
	const ImVec2 oldPan = state.pan;

	std::string loadError;
	if (!load_image_from_path(state, loadError, false)) {
		errorOut = loadError;
		return false;
	}

	state.lastWriteTime = writeTime;
	state.lastFileSize = fileSize;
	state.hasFileStamp = true;

	if (wasFit) {
		state.fitToWindow = true;
		state.minZoom = -1.0f;
	}
	else {
		state.fitToWindow = false;
		state.zoom = oldZoom;
		state.pan = oldPan;
		state.minZoom = oldMinZoom;
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
