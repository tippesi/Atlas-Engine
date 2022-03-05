$file = "Cornell.zip"

Invoke-WebRequest -Uri "https://casual-effects.com/g3d/data10/common/model/CornellBox/CornellBox.zip" -OutFile $file -TimeoutSec 600

# Unzip the file to specified location
$location = Get-Location
$zip_file = (new-object -com shell.application).namespace("$location\$file")
New-Item -Path . -Name "cornell" -ItemType "directory"
$destination = (new-object -com shell.application).namespace("$location\cornell")
$items = $zip_file.items()
$destination.Copyhere($items)

$file = "SanMiguel.zip"

Invoke-WebRequest -Uri "https://casual-effects.com/g3d/data10/research/model/San_Miguel/San_Miguel.zip" -OutFile $file -TimeoutSec 600

# Unzip the file to specified location
$location = Get-Location
$zip_file = (new-object -com shell.application).namespace("$location\$file")
New-Item -Path . -Name "sanmiguel" -ItemType "directory"
$destination = (new-object -com shell.application).namespace("$location\sanmiguel")
$items = $zip_file.items()
$destination.Copyhere($items)

$file = "Bistro.zip"

# Invoke-WebRequest -Uri "https://casual-effects.com/g3d/data10/research/model/bistro/Exterior.zip" -OutFile $file -TimeoutSec 600

# Unzip the file to specified location
# $location = Get-Location
# $zip_file = (new-object -com shell.application).namespace("$location\$file")
# New-Item -Path . -Name "bistro" -ItemType "directory"
# $destination = (new-object -com shell.application).namespace("$location\bistro")
# $items = $zip_file.items()
# $destination.Copyhere($items)

remove-item SanMiguel.zip
remove-item Cornell.zip
# remove-item Bistro.zip