#pragma once

#include <stdexcept>
#include <string>

namespace fanqie {

enum class SourceErrorCode {
    PluginLoadFailed,
    PluginInvalidManifest,
    PluginMissingMethod,
    PluginConfigError,
    PluginRuntimeError,
    InvalidReturnType,
    InvalidReturnField,
    NetworkError,
    SourceNotSelected,
};

struct SourceError {
    SourceErrorCode code = SourceErrorCode::PluginRuntimeError;
    std::string     source_id;
    std::string     plugin_path;
    std::string     operation;
    std::string     message;
};

class SourceException : public std::runtime_error {
public:
    explicit SourceException(SourceError error)
        : std::runtime_error(error.message), error_(std::move(error)) {}

    const SourceError& error() const { return error_; }

private:
    SourceError error_;
};

} // namespace fanqie
