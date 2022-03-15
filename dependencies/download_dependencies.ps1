$file = "Imgui.zip"

Invoke-WebRequest -Uri "https://github.com/ocornut/imgui/archive/refs/tags/v1.87.zip" -OutFile $file -TimeoutSec 120

# Unzip the file to specified location
$location = Get-Location
$zip_file = (new-object -com shell.application).namespace("$location\$file")
$destination = (new-object -com shell.application).namespace("$location\Imgui")
$items = $zip_file.items()
$destination.Copyhere($items)

rename-item "Imgui/imgui-1.87" "imgui"

$file = "SDL.zip"

Invoke-WebRequest -Uri "https://www.libsdl.org/release/SDL2-2.0.20.zip" -OutFile $file -TimeoutSec 120

# Unzip the file to specified location
$location = Get-Location
$zip_file = (new-object -com shell.application).namespace("$location\$file") 
$destination = (new-object -com shell.application).namespace("$location")
$items = $zip_file.items()
$destination.Copyhere($items)
rename-item "SDL2-2.0.20" "SDL"

$file = "Assimp.zip"

Invoke-WebRequest -Uri "https://github.com/assimp/assimp/archive/v5.2.2.zip" -OutFile $file -TimeoutSec 120

# Unzip the file to specified location
$location = Get-Location
$zip_file = (new-object -com shell.application).namespace("$location\$file") 
$destination = (new-object -com shell.application).namespace("$location")
$items = $zip_file.items()
$destination.Copyhere($items)
rename-item "assimp-5.2.2" "Assimp"

remove-item SDL.zip
remove-item Assimp.zip
remove-item Imgui.zip