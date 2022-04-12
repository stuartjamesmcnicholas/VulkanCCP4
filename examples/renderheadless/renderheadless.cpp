/*
* Vulkan Example - Minimal headless rendering example
*
* Copyright (C) 2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#if defined(_WIN32)
#pragma comment(linker, "/subsystem:console")
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <android/log.h>
#include "VulkanAndroid.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <array>
#include <iostream>
#include <algorithm>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "VulkanTools.h"
#include "VulkanglTFModel.h"
#include "camera.hpp"

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
android_app* androidapp;
#endif

#define DEBUG (!NDEBUG)

#define BUFFER_ELEMENTS 32

#include "renderheadless.h"

#ifdef _HEADLESS_MAIN_

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
void handleAppCommand(android_app * app, int32_t cmd) {
	if (cmd == APP_CMD_INIT_WINDOW) {
		VulkanHeadless *vulkanExample = new VulkanHeadless();
		delete(vulkanExample);
		ANativeActivity_finish(app->activity);
	}
}
void android_main(android_app* state) {
	androidapp = state;
	androidapp->onAppCmd = handleAppCommand;
	int ident, events;
	struct android_poll_source* source;
	while ((ident = ALooper_pollAll(-1, NULL, &events, (void**)&source)) >= 0) {
		if (source != NULL)	{
			source->process(androidapp, source);
		}
		if (androidapp->destroyRequested != 0) {
			break;
		}
	}
}
#else
int main(int argc, char* argv[]) {
    int width = 2048;
    int height = 1024;
    Camera camera;
    camera.type = Camera::CameraType::lookat;
    camera.setPosition(glm::vec3(0,0,-60));
    camera.setRotation(glm::vec3(-0.0f, -0.0f, 0.0f));
    camera.setPerspective(30.0f, (float)width / (float)height, 1.0f, 256.0f);

    PushConsts pc;
    pc.view = camera.matrices.view;
    pc.projection = camera.matrices.perspective;
    pc.model = glm::mat4(1.0f);
    pc.lightPos = glm::vec4();
    pc.lightPos.x = 0.0;
    pc.lightPos.y = 0.0;
    pc.lightPos.z = 1000.0;
    pc.fog_start= 40;
    pc.fog_end = 90;
    pc.fogColor = glm::vec4(1.0);

    VulkanHeadless *vulkanExample = new VulkanHeadless(std::string(argv[1]),std::string(argv[2]),pc,width,height);
    std::cout << "Finished" << std::endl;
    delete(vulkanExample);
    return 0;
}
#endif
#endif
