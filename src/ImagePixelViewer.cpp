#include "ImagePixelViewer.h"

int ImagePixelViewer() {

	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		return 1;
	}
#ifdef __APPLE__
	const char* glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

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

	//ImGui::StyleColorsClassic();
	ImGui::StyleColorsLight();
	//ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(mainScale);
	style.FontScaleDpi = mainScale;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	ImageStates states;
	states.Gray_Image_Last = states.Gray_Image;
	states.Auto_Maximize_Contrast_Last = states.Auto_Maximize_Contrast;
	states.One_Channel_Pseudo_Color_Last = states.One_Channel_Pseudo_Color;
	states.Four_Channel_Ignore_Alpha_Last = states.Four_Channel_Ignore_Alpha;
	glfwSetWindowUserPointer(window, &states);
	glfwSetDropCallback(window, drop_callback);


	ImVec4 clear_color = ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
	std::string errorMessage;



	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		for (auto& state : states.states) {
			std::string reloadError;
			refresh_image_if_changed(state, reloadError);
		}

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


			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
				// Fire once per press (no auto-repeat). If you want repeat, pass true.
				if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
					DeleteSelected(states);
				}
			}

			// left column: thumbnails + list
			ImGui::Text("Images");

			for (int i = 0; i < (int)states.states.size(); ++i) {
				ImGui::Separator();

				ImageState& img = states.states[i];

				float aspect = (img.height > 0) ? (float)img.width / (float)img.height : 1.0f;
				ImVec2 disp(thumbWidth, thumbHeight);
				if (aspect > 1.0f) { disp.y = thumbHeight / aspect; }
				else { disp.x = thumbWidth * aspect; }

				ImGui::PushID(i);
				int pushed = 0;
				if (img.texture_thumb.id) {
					if (states.selected == i) {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.15f, 1.0f)); pushed++;
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.25f, 1.0f)); pushed++;
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.55f, 0.10f, 1.0f)); pushed++;
					}
					if (ImGui::ImageButton("thumb", (void*)(intptr_t)img.texture_thumb.id, disp, ImVec2(0, 0), ImVec2(1, 1))) {
						states.selected = i;
					}
					if (ImGui::BeginPopupContextItem(("thumb_ctx##" + std::to_string(i)).c_str(),
						ImGuiPopupFlags_MouseButtonRight)) {
						if (ImGui::MenuItem("Delete")) {
							// release GL resources first
							release_texture(states.states[i].texture);
							release_texture(states.states[i].texture_thumb);

							// fix selected index after erase
							int oldSelected = states.selected;
							states.states.erase(states.states.begin() + i);

							if (states.states.empty()) {
								states.selected = 0;
							}
							else {
								if (oldSelected == i) {
									// if we deleted the selected one, select the previous item (or 0)
									states.selected = std::min(i, (int)states.states.size() - 1);
								}
								else if (oldSelected > i) {
									// shift selection left since indices moved
									states.selected = oldSelected - 1;
								}
								else {
									states.selected = oldSelected;
								}
							}

							ImGui::CloseCurrentPopup();
							// Important: early return from UI branch to avoid using stale 'img' reference this frame
							ImGui::EndPopup();
							if (pushed > 0) ImGui::PopStyleColor(pushed);
							ImGui::PopID();
							// Skip the rest of the per-item UI after deletion
							continue;
						}
						ImGui::EndPopup();
					}
				}
				else {
					ImGui::Button("NoTex", disp);
				}
				if (pushed > 0) ImGui::PopStyleColor(pushed);
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.00f));
				ImGui::BeginGroup();
				ImGui::TextWrapped("%s", img.filename.c_str());
				ImGui::Text("%d x %d", img.width, img.height);
				ImGui::Text("%d x %s", img.channels, img.depth.c_str());
				ImGui::PopStyleColor();

				ImGui::EndGroup();
				ImGui::PopID();
				ImGui::Spacing();
			}
			if (states.states.empty()) {
				ImGui::TextDisabled("Drop & Drop images");
			}


			ImGui::End();

			ImGui::PopStyleColor();
		}

		{ // image view windows (canvas) — full window, no scrollbars, cursor-centered zoom
			ImGuiWindowFlags view_window_flag = 0;
			view_window_flag |= ImGuiWindowFlags_NoTitleBar;
			view_window_flag |= ImGuiWindowFlags_NoCollapse;
			view_window_flag |= ImGuiWindowFlags_NoMove;
			view_window_flag |= ImGuiWindowFlags_NoResize;
			view_window_flag |= ImGuiWindowFlags_NoScrollWithMouse; // per request
			view_window_flag |= ImGuiWindowFlags_NoScrollbar;       // per request

			ImGuiViewport* main_viewport = ImGui::GetMainViewport();

			ImVec2 winPos = ImVec2(main_viewport->WorkPos.x + PREVIEW_WIDTH, main_viewport->WorkPos.y);
			ImVec2 winSize = ImVec2(main_viewport->WorkSize.x - PREVIEW_WIDTH, main_viewport->WorkSize.y);

			ImGui::SetNextWindowPos(winPos);
			ImGui::SetNextWindowSize(winSize);

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.506f, 0.506f, 0.506f, 1.0f)); // 129,129,129
			ImGui::Begin("Canvas", nullptr, view_window_flag);

			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 origin = ImGui::GetCursorScreenPos(); // top-left of drawable area in screen space

			std::optional<std::pair<int, int>> hoveredPixel;
			std::string hoveredOriginalValue;
			std::string hoveredPreviewValue;
			bool hoveredForTooltip = false;

			if (states.states.size() > 0) {
				auto& state = states.states[states.selected];

				if (state.texture.id != 0 && !state.previewRGBA.empty()) {
					// Right-click anywhere in this window to open the menu
					if (ImGui::BeginPopupContextWindow("canvas_ctx",
						ImGuiPopupFlags_MouseButtonRight /* open on RMB */
					/*| ImGuiPopupFlags_NoOpenOverItems*/)) // uncomment to block opening over items
					{
						if (ImGui::MenuItem("Reset Zoom&Pan")) {
							for (auto& one_state : states.states) {
								one_state.pan = ImVec2(0, 0);
								one_state.zoom = one_state.minZoom;
								one_state.fitToWindow = true;
							}
						}
						if (ImGui::MenuItem("Link View", nullptr, &states.Link_View));
						ImGui::Separator();
						if (ImGui::MenuItem("Gray Image", nullptr, &states.Gray_Image));
						if (ImGui::MenuItem("Auto Maximize Contrast", nullptr, &states.Auto_Maximize_Contrast));
						if (ImGui::MenuItem("1-Channel Pseudo Color", nullptr, &states.One_Channel_Pseudo_Color));
						if (ImGui::MenuItem("4-Channel Ignore Alpha", nullptr, &states.Four_Channel_Ignore_Alpha));
						ImGui::Separator();
						if (ImGui::MenuItem("Copy Pixel Position"));
						ImGui::EndPopup();
					}
					// Compute visual zoom and image size
					float renderedZoom = state.zoom;
					if (state.fitToWindow && state.texture.width > 0 && state.texture.height > 0 && avail.x > 0.0f && avail.y > 0.0f) {
						float sx = avail.x / (float)state.texture.width;
						float sy = avail.y / (float)state.texture.height;
						renderedZoom = std::max(state.minZoom, std::min(sx, sy));
					}
					renderedZoom = std::clamp(renderedZoom, state.minZoom, maxZoom);

					// set minZoom to when image fit width
					if (state.minZoom == -1) {
						state.minZoom = renderedZoom;
					}
					ImVec2 imageSize(
						std::max(1.0f, (float)state.texture.width * renderedZoom),
						std::max(1.0f, (float)state.texture.height * renderedZoom));


					// Where to draw the image (screen space)
					// - In fit mode: centered (ignore state.pan)
					// - In manual mode: origin + state.pan
					ImVec2 base = origin;
					ImVec2 centered = ImVec2(
						base.x + (avail.x - imageSize.x) * 0.5f,
						base.y + (avail.y - imageSize.y) * 0.5f);

					ImVec2 imageTopLeft = state.fitToWindow ? centered : ImVec2(base.x + state.pan.x, base.y + state.pan.y);

					// Draw the image
					const ImTextureID texId = (ImTextureID)(uintptr_t)state.texture.id;
					ImGui::SetCursorScreenPos(imageTopLeft);
					ImGui::Image(texId, imageSize);

					// Hover/interaction
					ImGuiIO& io = ImGui::GetIO();
					const bool winHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_None);
					const bool imgHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_None);

					// --- Cursor-centered zoom on wheel (no scrollbars, no BeginChild) ---
					if (winHovered && io.MouseWheel != 0.0f)
					{
						// Capture the "old" numbers
						float oldRenderedZoom = renderedZoom;

						// If we were in fit mode, freeze current visual zoom & placement as the starting point for manual mode.
						if (state.fitToWindow) {
							state.fitToWindow = false;
							state.zoom = oldRenderedZoom;
							// Initialize pan so that the current centered view becomes the manual baseline.
							state.pan = ImVec2(centered.x - base.x, centered.y - base.y);
						}

						// Recompute imageTopLeft based on current manual state before changing zoom
						imageTopLeft = ImVec2(base.x + state.pan.x, base.y + state.pan.y);

						// Compute focus point in image space under the mouse BEFORE changing zoom
						ImVec2 mouseLocal = ImVec2(io.MousePos.x - imageTopLeft.x, io.MousePos.y - imageTopLeft.y);
						ImVec2 focusContent = ImVec2(
							mouseLocal.x / state.zoom,
							mouseLocal.y / state.zoom);

						// Apply zoom
						const float zoomFactor = std::pow(1.55f, io.MouseWheel);
						state.zoom = std::clamp(state.zoom * zoomFactor, state.minZoom, maxZoom);

						// Keep that focus point under the mouse: newTopLeft = mouse - focus*newZoom
						ImVec2 newTopLeft = ImVec2(
							io.MousePos.x - focusContent.x * state.zoom,
							io.MousePos.y - focusContent.y * state.zoom);

						// Update pan accordingly
						state.pan = ImVec2(newTopLeft.x - base.x, newTopLeft.y - base.y);
					}

					// --- Panning with drag (LMB or MMB) ---
					if (winHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)
						|| ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)))
					{
						if (state.fitToWindow) {
							// Leave fit mode on first drag so user can pan freely
							state.fitToWindow = false;
							state.zoom = renderedZoom;
							// Start from the centered placement
							state.pan = ImVec2(centered.x - base.x, centered.y - base.y);
						}

						state.pan.x += io.MouseDelta.x;
						state.pan.y += io.MouseDelta.y;
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
					}

					// --- Pixel hover / grid / crosshair (re-using your logic) ---
					// Derive item rect from what we just drew:
					ImVec2 itemMin = imageTopLeft;
					ImVec2 itemMax = ImVec2(imageTopLeft.x + imageSize.x, imageTopLeft.y + imageSize.y);
					ImVec2 itemSize = ImVec2(itemMax.x - itemMin.x, itemMax.y - itemMin.y);

					float pixelWidth = (state.previewRGBA.cols > 0) ? (itemSize.x / (float)state.previewRGBA.cols) : 0.0f;
					float pixelHeight = (state.previewRGBA.rows > 0) ? (itemSize.y / (float)state.previewRGBA.rows) : 0.0f;

					std::optional<std::pair<int, int>> hoveredPixel;
					std::string hoveredOriginalValue;

					if (imgHovered && !state.previewRGBA.empty()) {
						ImVec2 rel(io.MousePos.x - itemMin.x, io.MousePos.y - itemMin.y);
						if (rel.x >= 0.0f && rel.y >= 0.0f && rel.x < itemSize.x && rel.y < itemSize.y) {
							float u = rel.x / itemSize.x;
							float v = rel.y / itemSize.y;
							int px = (int)std::clamp(std::floor(u * (float)state.previewRGBA.cols), 0.0f, (float)(state.previewRGBA.cols - 1));
							int py = (int)std::clamp(std::floor(v * (float)state.previewRGBA.rows), 0.0f, (float)(state.previewRGBA.rows - 1));
							hoveredPixel = std::make_pair(px, py);
							hoveredForTooltip = imgHovered;

							hoveredOriginalValue = format_pixel_value(state.sourceOriginal, px, py);

							if (!ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
								ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
						}
					}

					// Grid (only when pixels are large enough)
					if ((pixelWidth >= 4.0f || pixelHeight >= 4.0f) && state.previewRGBA.cols > 0 && state.previewRGBA.rows > 0) {
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						const ImU32 gridColor = IM_COL32(255, 255, 255, 40);
						for (int cx = 1; cx < state.previewRGBA.cols; ++cx) {
							float xPos = itemMin.x + (float)cx * pixelWidth;
							if (xPos > itemMax.x) break;
							drawList->AddLine(ImVec2(xPos, itemMin.y), ImVec2(xPos, itemMax.y), gridColor, 1.0f);
						}
						for (int cy = 1; cy < state.previewRGBA.rows; ++cy) {
							float yPos = itemMin.y + (float)cy * pixelHeight;
							if (yPos > itemMax.y) break;
							drawList->AddLine(ImVec2(itemMin.x, yPos), ImVec2(itemMax.x, yPos), gridColor, 1.0f);
						}
					}

					// Crosshair for hovered pixel
					if (hoveredPixel) {
						int px = hoveredPixel->first;
						int py = hoveredPixel->second;

						ImDrawList* dl = ImGui::GetWindowDrawList();
						const ImU32 overlay = IM_COL32(255, 255, 0, 200);

						ImVec2 pxTL(itemMin.x + px * pixelWidth, itemMin.y + py * pixelHeight);
						ImVec2 pxBR(pxTL.x + pixelWidth, pxTL.y + pixelHeight);
						ImVec2 pxC(pxTL.x + pixelWidth * 0.5f, pxTL.y + pixelHeight * 0.5f);

						dl->AddRect(pxTL, pxBR, overlay, 0.0f, 0, 1.0f);
						dl->AddLine(ImVec2(pxC.x, itemMin.y), ImVec2(pxC.x, itemMax.y), overlay, 1.0f);
						dl->AddLine(ImVec2(itemMin.x, pxC.y), ImVec2(itemMax.x, pxC.y), overlay, 1.0f);
					}
					if (hoveredForTooltip) {
						std::string tooltip = hoveredOriginalValue;
						if (!hoveredPreviewValue.empty() && hoveredPreviewValue != hoveredOriginalValue) {
							if (!tooltip.empty()) {
								tooltip.append("\n");
							}
							tooltip.append(hoveredPreviewValue);
						}
						if (!tooltip.empty()) {
							ImGui::SetTooltip("%s", tooltip.c_str());
						}
					}

					// Link View
					if (states.Link_View) {
						auto& selected_state = states.states[states.selected];

						for (int i = 0; i < (int)states.states.size(); ++i) {
							if (i == states.selected) continue;

							auto& cur_state = states.states[i];
							if (selected_state.width == cur_state.width &&
								selected_state.height == cur_state.height) {
								// Ensure minZoom is initialized on targets (so clamping works)
								if (cur_state.minZoom < 0.0f) {
									// If their minZoom hasn't been set by fit pass yet, copy selected's
									cur_state.minZoom = selected_state.minZoom;
								}

								// Copy zoom/pan
								cur_state.zoom = std::clamp(selected_state.zoom, cur_state.minZoom, maxZoom);
								cur_state.pan = selected_state.pan;

								// IMPORTANT: make them obey the copied zoom/pan immediately
								cur_state.fitToWindow = selected_state.fitToWindow;
								// If you always want manual view when linking, force:
								// cur_state.fitToWindow = false;

								// (Optional) any other flags you want synced
								cur_state.hasMinMax = selected_state.hasMinMax;
							}
						}
					}
				}
				else {
					ImGui::Dummy(ImVec2(0.0f, 120.0f));
					ImGui::TextUnformatted("Drag & Drop");
				}
			}





			ImGui::End();
			ImGui::PopStyleColor();
		}


		if (states.Gray_Image != states.Gray_Image_Last
			|| states.Auto_Maximize_Contrast != states.Auto_Maximize_Contrast_Last
			|| states.One_Channel_Pseudo_Color != states.One_Channel_Pseudo_Color_Last
			|| states.Four_Channel_Ignore_Alpha != states.Four_Channel_Ignore_Alpha_Last) {
			for (auto& state : states.states) {
				std::string updateError;
				rebuild_preview_from_source(state, states.Gray_Image, states.Auto_Maximize_Contrast, states.One_Channel_Pseudo_Color, states.Four_Channel_Ignore_Alpha, updateError);
			}
			states.Gray_Image_Last = states.Gray_Image;
			states.Auto_Maximize_Contrast_Last = states.Auto_Maximize_Contrast;
			states.One_Channel_Pseudo_Color_Last = states.One_Channel_Pseudo_Color;
			states.Four_Channel_Ignore_Alpha_Last = states.Four_Channel_Ignore_Alpha;
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
