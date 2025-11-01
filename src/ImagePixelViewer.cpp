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
	GLFWwindow* window = glfwCreateWindow(static_cast<int>(900 * mainScale), static_cast<int>(500 * mainScale), "ImagePixelViewer", nullptr, nullptr);
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

	string fontPath = fs::current_path().string() + "/assets/NotoSansSC-Regular.ttf";
	string fontPath_Debug = fs::current_path().string() + "/Debug/assets/NotoSansSC-Regular.ttf";
	std::cout << fs::current_path().string() << endl;
	if (fs::exists(fontPath)) {
		std::cout << "Loading Font: " << fontPath << endl;
		ImFont* font_arabic = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
	}
	if (fs::exists(fontPath_Debug)) {
		std::cout << "Loading Font: " << fontPath_Debug << endl;
		ImFont* font_arabic = io.Fonts->AddFontFromFileTTF(fontPath_Debug.c_str(), 16.0f);
	}

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

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		{ // image preview window
			ImGuiWindowFlags preview_window_flag = 0;
			preview_window_flag |= ImGuiWindowFlags_NoTitleBar;
			preview_window_flag |= ImGuiWindowFlags_NoCollapse;
			preview_window_flag |= ImGuiWindowFlags_NoMove;
			preview_window_flag |= ImGuiWindowFlags_NoResize;

			ImGuiViewport* main_viewport = ImGui::GetMainViewport();

			// Set position and size relative to main window
			ImVec2 preview_pos = ImVec2(main_viewport->WorkPos.x, main_viewport->WorkPos.y);
			ImVec2 preview_size = ImVec2(PREVIEW_WIDTH, main_viewport->WorkSize.y);

			// Apply position and size before Begin()
			ImGui::SetNextWindowPos(preview_pos);
			ImGui::SetNextWindowSize(preview_size);

			// Push custom background color (#262626)
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.149f, 0.149f, 0.149f, 1.0f));

			ImGui::Begin("Metadata", nullptr, preview_window_flag);






			// left column: thumbnails + list
			ImGui::BeginChild("left_panel", ImVec2(PREVIEW_WIDTH, 0), true);
			ImGui::Text("Images");
			ImGui::Separator();

			const float thumbSize = 96.0f;
			for (int i = 0; i < (int)states.states.size(); ++i) {
				ImageState& img = states.states[i];

				float aspect = (img.height > 0) ? (float)img.width / (float)img.height : 1.0f;
				ImVec2 disp(thumbSize, thumbSize);
				if (aspect > 1.0f) { disp.y = thumbSize / aspect; }
				else { disp.x = thumbSize * aspect; }

				ImGui::PushID(i);
				if (img.texture.id) {
					// ImGui::ImageButton uses (void*)(intptr_t)textureId for OpenGL
					if (ImGui::ImageButton("thumb", (void*)(intptr_t)img.texture.id, disp, ImVec2(0, 1), ImVec2(1, 0))) {
						states.selected = i;
					}
				}
				else {
					ImGui::Button("NoTex", disp);
				}
				ImGui::SameLine();
				ImGui::BeginGroup();
				ImGui::TextWrapped("%s", img.filename.c_str());
				ImGui::Text("%d x %d", img.width, img.height);
				if (states.selected == i) ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Selected");


				ImGui::EndGroup();
				ImGui::PopID();

				ImGui::Spacing();
			}
			if (states.states.empty()) {
				ImGui::TextDisabled("Drop & Drop images");
			}
			ImGui::EndChild(); // left_panel









			ImGui::End();

			ImGui::PopStyleColor();
		}




		ImGui::Render();
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	for (auto& state : states.states)
		release_texture(state.texture);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();


	return 0;
}