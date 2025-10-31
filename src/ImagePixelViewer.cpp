#include "ImagePixelViewer.h"

int ImagePixelViewer() {
	float minZoom = -1;

	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		return 1;
	}
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	float mainScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
	GLFWwindow* window = glfwCreateWindow(static_cast<int>(900 * mainScale), static_cast<int>(500 * mainScale), "Image Watch (ImGui)", nullptr, nullptr);
	if (window == nullptr) {
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	glewExperimental = GL_TRUE;
	GLenum glewErr = glewInit();
	glGetError(); // Clear the benign GL_INVALID_ENUM set by glewInit on core profiles.
	if (glewErr != GLEW_OK) {
		std::fprintf(stderr, "GLEW init failed: %s\n", glewGetErrorString(glewErr));
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(mainScale);
	style.FontScaleDpi = mainScale;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ImageStates states;
	glfwSetWindowUserPointer(window, &states);
	glfwSetDropCallback(window, drop_callback);


	ImVec4 clear_color = ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
	std::string errorMessage;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		if (states.size() > 0) {
			std::string loadError;
			if (!load_image_from_path(states[0], *states[0].pathToLoad, loadError)) {
				states[0].statusLine = loadError;
			}
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();






		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	for (auto& state : states)
		release_texture(state.texture);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();


	return 0;
}