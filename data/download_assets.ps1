function Download-File {
    param (
        $url,
        $directory
    )

    $out = "download.zip"
    Invoke-WebRequest -Uri $url -OutFile $out -TimeoutSec 600
    # Unzip the file to specified location
    $location = Get-Location
    $zip_file = (new-object -com shell.application).namespace("$location\$out")
    
    if ((Test-Path -Path $directory) -eq $false) {
        New-Item -Path . -Name $directory -ItemType "directory"
    }

    $destination = (new-object -com shell.application).namespace("$location\$directory")
    $items = $zip_file.items()
    $destination.Copyhere($items)

    remove-item $out
}

Download-File -url "https://casual-effects.com/g3d/data10/common/model/CornellBox/CornellBox.zip" -directory "cornell"
Download-File -url "https://casual-effects.com/g3d/data10/research/model/San_Miguel/San_Miguel.zip" -directory "sanmiguel"
Download-File -url "https://cdrdv2.intel.com/v1/dl/getContent/726594" -directory "newsponza"
Download-File -url "https://cdrdv2.intel.com/v1/dl/getContent/726650" -directory "newsponza"
Download-File -url "https://cdrdv2.intel.com/v1/dl/getContent/726656" -directory "newsponza"
Download-File -url "https://cdrdv2.intel.com/v1/dl/getContent/726676" -directory "newsponza"