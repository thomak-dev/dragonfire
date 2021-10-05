#include "pch.h"

#include "StreamReader.h"

#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>

std::shared_ptr<std::istream> StreamReader::GetInputStream(const std::string& filename) const
{
    // In order to construct a valid stream:
    // 1. The filename argument will be encoded as UTF-8 so use filesystem::u8path to
    //    correctly construct a path instance.
    // 2. Generate an absolute path by concatenating m_pathBase with the specified filename
    //    path. The filesystem::operator/ uses the platform's preferred directory separator
    //    if appropriate.
    // 3. Always open the file stream in binary mode. The glTF SDK will handle any text
    //    encoding issues for us.
    auto streamPath = pathBase / std::filesystem::u8path(filename);
    auto stream = std::make_shared<std::ifstream>(streamPath, std::ios_base::binary);

    // Check if the stream has no errors and is ready for I/O operations
    if (!stream || !(*stream))
    {
        throw std::runtime_error("Unable to create a valid input stream for uri: " + filename);
    }

    return stream;
}