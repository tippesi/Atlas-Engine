#ifndef AE_SYSTEM_H
#define AE_SYSTEM_H

#define _CRT_SECURE_NO_WARNINGS

// There are three available defines to change some engine behavior:
// AE_INSTANT_SHADER_RELOAD: Tracks shader changes and reloads them on change (high performance impact)
// AE_SHOW_LOG: Prints out general information to the console or debug window (low performance impact)
// AE_SHOW_API_DEBUG_LOG: Prints out graphics API specific things (medium performance impact)

#define AE_SHOW_LOG
// #define AE_SHOW_API_DEBUG_LOG

#include <stdint.h>
#include <string>
#include <math.h>
#include <exception>

#if  defined(AE_OS_ANDROID)

#define GL_GLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <GLES2/gl2ext.h>

#include <jni.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#define AtlasLog(...) __android_log_print(ANDROID_LOG_INFO, "ATLAS_LOG", __VA_ARGS__)

#elif defined(AE_OS_WINDOWS) || defined(AE_OS_LINUX) || defined(AE_OS_MACOS)

// GLAD
#ifdef AE_OS_WINDOWS
#include <direct.h>
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <Windows.h>
#endif
#include <Glad/glad/glad.h>

#define AtlasLog(...) printf(__VA_ARGS__); printf("\n");

#endif

// GLM
#include "libraries/glm/glm.hpp"
#include "libraries/glm/gtc/type_ptr.hpp"
#include "libraries/glm/gtx/common.hpp"
#include "libraries/glm/gtx/transform.hpp"

// Important definitions
using glm::vec4;
using glm::vec3;
using glm::vec2;
using glm::ivec2;
using glm::ivec3;
using glm::ivec4;

using glm::mat4;
using glm::mat3;

typedef short float16;

class AtlasException : public std::exception {

public:
	AtlasException(const char* message) : msg(message) { }

	const char* what() const throw()
	{
		return msg;
	}

private:
	const char* msg;

};

#endif