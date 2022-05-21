# Download CPP-YAML version 0.7.0
curl -O "https://codeload.github.com/jbeder/yaml-cpp/zip/refs/tags/yaml-cpp-0.7.0"
unzip yaml-cpp-0.7.0
mv yaml-cpp-yaml-cpp-0.7.0 YAML
rm yaml-cpp-0.7.0

# Download SDL2 version 2.0.20
curl -O "https://www.libsdl.org/release/SDL2-2.0.20.zip"
unzip SDL2-2.0.20.zip
mv SDL2-2.0.20 SDL
rm SDL2-2.0.20.zip

# Download Assimp version 5.2.3
curl -O "https://codeload.github.com/assimp/assimp/zip/refs/tags/v5.2.3"
unzip v5.2.3
mv assimp-5.2.3 Assimp
rm v5.2.3

# Download Imgui version 1.87
curl -O "https://codeload.github.com/ocornut/imgui/zip/refs/tags/v1.87"
unzip v1.87
mv imgui-1.87 Imgui/imgui
rm v1.87