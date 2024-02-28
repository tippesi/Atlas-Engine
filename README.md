# Atlas Engine
[![The MIT License][license-image]][license-url]
[![Code quality][code-quality-image]][code-quality-url]
[![Latest build](https://github.com/tippesi/Atlas-Engine/actions/workflows/build.yml/badge.svg)](https://github.com/tippesi/Atlas-Engine/actions/workflows/build.yml)

[license-image]: https://img.shields.io/badge/License-MIT-yellow.svg
[license-url]: https://opensource.org/licenses/MIT
[code-quality-image]: https://app.codacy.com/project/badge/Grade/0b8608dc5cb349a38b8d64c7fbcbcda6
[code-quality-url]: https://app.codacy.com/gh/tippesi/Atlas-Engine/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade

![GI scene](wiki/images/intel_sponza.gif) <br/>*Realtime Sponza scene with raytraced GI, AO and reflections (model from [Intel Graphics Research Sample Library](https://www.intel.com/content/www/us/en/developer/topic-technology/graphics-research/samples.html))* <br/>
## Introduction
This is a cross platform toy engine developed in my spare time that is available on Linux, Windows and MacOS.
>**Note:**
>The current version (0.2.0) contains many API changes and is still an active WIP
## Requirements
- Vulkan SDK
- C++20 compatible compiler
- CMake
- Vcpkg
## Set up
Dependencies are managed through [vcpkg](https://github.com/microsoft/vcpkg). You can either install the packages manually 
or while doing the build configuration with CMake. To use vcpkg together with CMake follow the steps described [here](https://github.com/microsoft/vcpkg#using-vcpkg-with-cmake).
### Compiling the demo application
Run CMake with the option ATLAS_DEMO=ON to include the demo application in the project. For easier use, the vsbuild.bat does exactly
that and launches Visual Studio afterwards. After launching the IDE, set AtlasEngineDemo as your target.
>**Note:**
>In order to start the application properly you might need to change the asset directory in the [demo source file](https://github.com/tippesi/Atlas-Engine/blob/master/src/demo/App.cpp).
### Including the library into your own project
It is possible to compile the engine either as a shared or static library. Set the ATLAS_BUILD_SHARED option accordingly. To make
the library work with its dependencies, the root CMakeLists.txt of this repository has to be added as a subdirectory. As an entry
point to create an application from scratch take a look at the [Hello World tutorial](https://github.com/tippesi/Atlas-Engine/wiki/Hello-World). For reference, the source folder contains an empty [app header](https://github.com/tippesi/Atlas-Engine/blob/master/src/engine/App.h) and an empty [app source](https://github.com/tippesi/Atlas-Engine/blob/master/src/demo/App.cpp).
<!---
### Android
You can compile the engine using Gradle either with or without AndroidStudio.
The Gradle project can be found in **./platform/android**. Open it before you proceed.
There are also two options available: Start a new project with a predefined
main file which you can edit. The second option is two use the engine as a subproject in an already existing project. 

**Note: Right now there is a bug in the NDK that prevents a successful build. Last version which worked was NDK 18.x. NDK 22.x shouldn't have any problems. To prevent any issues, don't let Android Studio automatically upgrade the NDK or Gradle versions of the project.**
#### New project using the engine
You can find the main file at **./src/main.cpp**. Just start your project there, it already
contains a main function. 
#### Excisting project using the engine
There exist two options:
- You can use the engine as a Gradle subproject.
- You can use the engine as a CMake subproject. Just copy the **./platform** folder to the folder
of the CMake root project. In the **./platform/android/app/src/main/java/com/atlasengine/app** file add the root library name and load
the project with Android Studio. The CMake project has to be compiled as a shared library. Make sure that the path to your data in the
asset directory is correct.
-->
### CMake build options
- **ATLAS_BUILD_SHARED** Force project to be build as a shared library
- **ATLAS_EXPORT_MAIN** Export the main file to be added as executable to parent project (only if main function cannot be found)
- **ATLAS_NO_APP** Disables the engines main function and app functionality. You have to code the main function and
initialize the engine yourself
- **ATLAS_IMGUI** Enables the [ImGui](https://github.com/ocornut/imgui) integration. Is enabled by default if the demo project is build.
- **ATLAS_ASSIMP** Enables the [Assimp](https://github.com/assimp/assimp) integration. Is enabled by default.
- **ATLAS_HEADLESS** Enables the headless support, which means no window needs to be created. Is disabled by default.
- **ATLAS_BINDLESS** Enables support for bindless resources. Might be problematic on MacOS. Enabled by default.
- **ATLAS_TESTS** Generates the testing project and allows to target it.
## Documentation
If you want more information have a look into the [Documentation](https://tippesi.github.io/Atlas-Engine-Doc/index.html).
## License
The source code is licensed under the MIT license. The copyright notices of the dependencies can be found
in the LICENSE.md file in the dependency directory. 
>**Note:**
>The files in the data folder (except the shaders) use a different license. 
## Code Example
For a code example have a look at the [demo application](https://github.com/tippesi/Atlas-Engine/tree/master/src/demo).
## Screenshots
![Editor](wiki/images/editor.png)
*Sponza demo scene opened in the editor* 
![Example scene](wiki/images/sponza_rasterized.png)
*Rasterized image using real time global illumination*
![Example scene](wiki/images/sponza_pathtraced.png)
*Path traced scene*
![Island scene](wiki/images/island.gif)
*Island demo scene using the terrain and ocean systems*
