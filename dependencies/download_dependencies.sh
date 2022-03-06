# Download SDL2 version 2.0.9
curl -O "https://www.libsdl.org/release/SDL2-2.0.9.zip"
unzip SDL2-2.0.9.zip
mv SDL2-2.0.9 SDL
rm SDL2-2.0.9.zip

# Download Assimp version 5.0.1
curl -O "https://codeload.github.com/assimp/assimp/zip/refs/tags/v5.0.1"
unzip v5.0.1
mv assimp-5.0.1 Assimp
rm v5.0.1

# Download Imgui version 1.87
curl -O "https://codeload.github.com/ocornut/imgui/zip/refs/tags/v1.87"
unzip v1.87
mv imgui-1.87 Imgui/imgui
rm v1.87