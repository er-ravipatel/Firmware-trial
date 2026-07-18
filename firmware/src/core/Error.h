// Error codes for the Lumen Frame firmware.
// Bare metal: no C++ exceptions — fallible operations return Result<T> (see Result.h).
#pragma once
#include <cstdint>

namespace lf {

enum class Error : uint16_t {
    None = 0,
    InvalidArg,
    OutOfMemory,
    OutOfRange,
    NotFound,
    Full,
    Empty,
    IoError,
    CorruptData,
    BadMagic,             // bundle/file magic mismatch
    UnsupportedVersion,   // format version we don't understand
    IntegrityFail,        // SHA-256 mismatch
    SignatureFail,        // Ed25519 verify failed
    VersionRejected,      // anti-downgrade / anti-loop
    Timeout,
    Unavailable,
};

inline const char* to_string(Error e) {
    switch (e) {
        case Error::None:               return "None";
        case Error::InvalidArg:         return "InvalidArg";
        case Error::OutOfMemory:        return "OutOfMemory";
        case Error::OutOfRange:         return "OutOfRange";
        case Error::NotFound:           return "NotFound";
        case Error::Full:               return "Full";
        case Error::Empty:              return "Empty";
        case Error::IoError:            return "IoError";
        case Error::CorruptData:        return "CorruptData";
        case Error::BadMagic:           return "BadMagic";
        case Error::UnsupportedVersion: return "UnsupportedVersion";
        case Error::IntegrityFail:      return "IntegrityFail";
        case Error::SignatureFail:      return "SignatureFail";
        case Error::VersionRejected:    return "VersionRejected";
        case Error::Timeout:            return "Timeout";
        case Error::Unavailable:        return "Unavailable";
    }
    return "Unknown";
}

}  // namespace lf
