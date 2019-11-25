add-type @"
using System.Net;
using System.Security.Cryptography.X509Certificates;
public class TrustAllCertsPolicy : ICertificatePolicy {
    public bool CheckValidationResult(
        ServicePoint srvPoint, X509Certificate certificate,
        WebRequest request, int certificateProblem) {
        return true;
    }
}
"@
$AllProtocols = [System.Net.SecurityProtocolType]'Ssl3,Tls,Tls11,Tls12'
[System.Net.ServicePointManager]::SecurityProtocol = $AllProtocols
[System.Net.ServicePointManager]::CertificatePolicy = New-Object TrustAllCertsPolicy

$file = "SDL.zip"

Invoke-WebRequest -Uri "https://www.libsdl.org/release/SDL2-2.0.9.zip" -OutFile $file -TimeoutSec 5

# Unzip the file to specified location
$location = Get-Location
$zip_file = (new-object -com shell.application).namespace("$location\$file") 
$destination = (new-object -com shell.application).namespace("$location")
$items = $zip_file.items()
$destination.Copyhere($items)
rename-item "SDL2-2.0.9" "SDL"

$file = "Assimp.zip"

Invoke-WebRequest -Uri "https://github.com/assimp/assimp/archive/v5.0.0.zip" -OutFile $file -TimeoutSec 10

# Unzip the file to specified location
$location = Get-Location
$zip_file = (new-object -com shell.application).namespace("$location\$file") 
$destination = (new-object -com shell.application).namespace("$location")
$items = $zip_file.items()
$destination.Copyhere($items)
rename-item "assimp-5.0.0" "Assimp"

remove-item SDL.zip
remove-item Assimp.zip
