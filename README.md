# Atlas Engine
![Example scene](images/sponza_rasterized.png)
*Rasterized image using an irradiance volume*
## Introduction
This is a cross platform engine that is available on Linux, Windows and Android.
## Requirements
- OpenGL 4.3 or higher
- OpenGL ES 3.2
## Set up
Before you start compiling make sure to download the dependencies into **./dependencies**. You can either do
this manually or use one of the available scripts. Building the engine is really convenient: You can use your
source code and project across all supported platforms. The only thing that differs are the build tools. 
>**Note:**
>Debugging the resulting application in a debug configuration will result in poor performance.
 
For more information on how to set up the engine have a look at the [Hello World](https://github.com/tippesi/Atlas-Engine/wiki/Hello-World) tutorial.
### Linux and Windows
There are two options available: Start a new project with a predefined
main file which you can edit. The second option is two use the engine as a subproject in an already existing project.
#### New project using the engine
After running CMake you can find the main file at **./src/main.cpp**. Just start your project there, it already
contains a main function.
#### Excisting project using the engine
There exist two options:
- You can use the engine as a CMake subproject. Just go ahead and use **add_subdirectory** in the root
CMakeLists.txt of your project. Afterwards add **target_link_libraries(YOUR_TARGET ... AtlasEngine)**. You should be fine.
- You can compile the engine and all dependencies as a static library (note that some dependencies also have
dynamic libraries). Therefore use the **ATLAS_BUILD_LIBRARY** option when using CMake.
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
### CMake build options
- **ATLAS_BUILD_LIBRARY** Build project as library
- **ATLAS_BUILD_SHARED** Force project to be build as a shared library
- **ATLAS_OPENGL_ES** Use OpenGL ES instead of OpenGL on desktop devices (for Android this option is always on)
- **ATLAS_EXPORT_MAIN** Export the main file to be added as executable to parent project (only if main function cannot be found)
- **ATLAS_NO_APP** Disables the engines main function and app functionality. You have to code the main function and
initialize the engine yourself
## Documentation
If you want more information have a look into the [Documentation](https://tippesi.github.io/Atlas-Engine-Doc/index.html).
## License
The source code is licensed under the MIT license. The license and the copyright notices of the dependencies can be found
in the LICENSE file. 
>**Note:**
>The files in the data folder (except the shaders) use a different license. 
## Code Example
For a code example have a look at the [Wiki](https://github.com/tippesi/Atlas-Engine/wiki/Code-example).
#### Thread safety
Most classes are not designed to be used across different threads by default. These classes have to be protected by you as the user. But there are also classes which are known to be used in several threads. These classes are the EventManager, EventDelegate, Time, AudioManager, AudioStream and AssetLoader. They can all be savely accessed by multiple threads. Note that all methods which are static e.g. OpaqueRenderer::AddConfig(...) are always thread safe.
## Screenshots
![Island scene](images/island.gif) \
*Island demo scene*
![Island scene](images/gun.gif) \
*PBR rendering*
![Example scene](images/sponza_raytraced.png)
*Ray traced image using 3000 samples with 5 bounces*