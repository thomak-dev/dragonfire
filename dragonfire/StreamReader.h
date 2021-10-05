/* Based on Microsoft sample */
#pragma once

#include <GLTFSDK/GLTFResourceReader.h>

// The glTF SDK is decoupled from all file I/O by the IStreamReader (and IStreamWriter)
// interface(s) and the C++ stream-based I/O library. This allows the glTF SDK to be used in
// sandboxed environments, such as WebAssembly modules and UWP apps, where any file I/O code
// must be platform or use-case specific.
class StreamReader : public Microsoft::glTF::IStreamReader
{
public:
    StreamReader(std::filesystem::path pathBase) noexcept : pathBase(std::move(pathBase)) { assert(this->pathBase.has_root_path()); }

    // Resolves the relative URIs of any external resources declared in the glTF manifest
    std::shared_ptr<std::istream> GetInputStream(const std::string& filename) const override;

private:
    std::filesystem::path pathBase;
};
