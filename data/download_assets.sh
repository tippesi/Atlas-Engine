function download_file() {
    url="$1"
    directory="$2"
    out="download.zip"
    curl $url -o $out
    unzip $out -d $directory
    rm $out
}

download_file "https://casual-effects.com/g3d/data10/common/model/CornellBox/CornellBox.zip" "cornell"
download_file "https://casual-effects.com/g3d/data10/research/model/San_Miguel/San_Miguel.zip" "sanmiguel"
download_file "https://cdrdv2.intel.com/v1/dl/getContent/726594" "newsponza"
download_file "https://cdrdv2.intel.com/v1/dl/getContent/726650" "newsponza"
download_file "https://cdrdv2.intel.com/v1/dl/getContent/726656" "newsponza"
download_file "https://cdrdv2.intel.com/v1/dl/getContent/726676" "newsponza"