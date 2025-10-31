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
		const std::string dropped = std::string(paths[0]);
		std::filesystem::path p(dropped);
		std::string extLower = to_lower(p.extension().string());
		if (extLower.empty() || !is_opencv_supported_ext(extLower)) {
			showError(("Not a valid image file: " + string(paths[i])).c_str());
			return;
		}
		ImageState state;
		state.pathToLoad = std::string(paths[i]);
		std::cout << *state.pathToLoad << endl;
		states->push_back(state);
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
bool create_texture_from_rgba(ImageTexture& texture, const cv::Mat& rgbaImage, std::string& error) {
	if (rgbaImage.empty()) {
		error = "Cannot create texture: image is empty.";
		return false;
	}
	if (rgbaImage.type() != CV_8UC4) {
		error = "Texture upload expects CV_8UC4 data.";
		return false;
	}

	cv::Mat upload = rgbaImage;
	if (!upload.isContinuous()) {
		upload = upload.clone();
	}

	release_texture(texture);

	glGenTextures(1, &texture.id);
	if (texture.id == 0) {
		error = "Failed to generate OpenGL texture.";
		return false;
	}

	texture.width = upload.cols;
	texture.height = upload.rows;

	glBindTexture(GL_TEXTURE_2D, texture.id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture.width, texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, upload.data);
	glBindTexture(GL_TEXTURE_2D, 0);

	GLenum glError = glGetError();
	if (glError != GL_NO_ERROR) {
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
	case CV_8U:  return "CV_8U";
	case CV_8S:  return "CV_8S";
	case CV_16U: return "CV_16U";
	case CV_16S: return "CV_16S";
	case CV_32S: return "CV_32S";
	case CV_32F: return "CV_32F";
	case CV_64F: return "CV_64F";
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
		if (state.autoNormalize) {
			double minVal = 0.0;
			double maxVal = 0.0;
			cv::minMaxIdx(state.sourceOriginal, &minVal, &maxVal);
			state.minVal = minVal;
			state.maxVal = maxVal;
			state.hasMinMax = true;

			double scale = (maxVal - minVal) != 0.0 ? 255.0 / (maxVal - minVal) : 1.0;
			double shift = -minVal * scale;
			state.sourceOriginal.convertTo(converted, CV_8U, scale, shift);
		}
		else {
			state.sourceOriginal.convertTo(converted, CV_8U);
		}
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

	std::ostringstream oss;
	oss << "original " << describe_mat(state.sourceOriginal)
		<< ", preview " << describe_mat(state.preview8u);
	if (state.sourceOriginal.depth() != CV_8U) {
		if (state.autoNormalize) {
			oss << " (normalized";
			if (state.hasMinMax) {
				oss << ", min=" << state.minVal << ", max=" << state.maxVal;
			}
			oss << ")";
		}
		else {
			oss << " (converted to 8-bit)";
		}
	}

	return oss.str();
}

bool load_image_from_path(ImageState& state, const std::string& path, std::string& errorOut) {
	std::error_code fsError;
	if (!std::filesystem::exists(path, fsError) || !std::filesystem::is_regular_file(path, fsError)) {
		errorOut = "File not found: " + path;
		return false;
	}

	cv::Mat loaded = cv::imread(path, cv::IMREAD_UNCHANGED);
	if (loaded.empty()) {
		errorOut = "Failed to load image via OpenCV.";
		showError(("Cannot load image file: " + *state.pathToLoad).c_str());
		return false;
	}

	state.sourceOriginal = loaded;
	state.currentPath = path;
	copy_path_to_buffer(state, path);

	auto status = update_preview_from_source(state, errorOut);
	if (!status) {
		return false;
	}

	state.zoom = 1.0f;
	state.fitToWindow = true;

	std::filesystem::path fsPath(path);
	std::ostringstream oss;
	std::string displayName = fsPath.filename().string();
	if (displayName.empty()) {
		displayName = fsPath.string();
	}
	oss << "Loaded \"" << displayName << "\" | " << *status;
	state.statusLine = oss.str();

	return true;
}




