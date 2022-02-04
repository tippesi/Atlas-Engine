$file = "SDL.zip"

Invoke-WebRequest -Uri "https://www.libsdl.org/release/SDL2-2.0.9.zip" -OutFile $file -TimeoutSec 120

# Unzip the file to specified location
$location = Get-Location
$zip_file = (new-object -com shell.application).namespace("$location\$file") 
$destination = (new-object -com shell.application).namespace("$location")
$items = $zip_file.items()
$destination.Copyhere($items)
rename-item "SDL2-2.0.9" "SDL"

$file = "Assimp.zip"

Invoke-WebRequest -Uri "https://github.com/assimp/assimp/archive/v5.0.1.zip" -OutFile $file -TimeoutSec 120

# Unzip the file to specified location
$location = Get-Location
$zip_file = (new-object -com shell.application).namespace("$location\$file") 
$destination = (new-object -com shell.application).namespace("$location")
$items = $zip_file.items()
$destination.Copyhere($items)
rename-item "assimp-5.0.1" "Assimp"

remove-item SDL.zip
remove-item Assimp.zip