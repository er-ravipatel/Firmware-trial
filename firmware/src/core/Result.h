// Result<T> — the error-handling primitive for the whole firmware.
// Bare metal has no exceptions; every fallible call returns a Result that is either
// a value or an Error. Build this first (IMPLEMENTATION §3.5).
#pragma once
#include "Error.h"

// Freestanding move (avoids <utility>, which is unavailable under Circle's -nostdinc++).
namespace lf {
template <class T> constexpr T&& move_(T& t) { return static_cast<T&&>(t); }
}

namespace lf {

// Empty value for operations that either succeed or fail with no payload.
struct Unit {};

template <class T>
class Result {
public:
    Result(const T& v) : ok_(true), err_(Error::None), val_(v) {}
    Result(T&& v) : ok_(true), err_(Error::None), val_(static_cast<T&&>(v)) {}
    Result(Error e) : ok_(false), err_(e), val_() {}  // implicit: `return Error::X;`

    bool is_ok() const { return ok_; }
    bool is_err() const { return !ok_; }
    explicit operator bool() const { return ok_; }

    // Precondition: is_ok(). Caller must check first.
    const T& value() const { return val_; }
    T& value() { return val_; }
    T value_or(const T& fallback) const { return ok_ ? val_ : fallback; }

    Error error() const { return err_; }

private:
    bool ok_;
    Error err_;
    T val_;
};

// Status = a Result with no payload. Use Ok() for success.
using Status = Result<Unit>;
inline Status Ok() { return Status(Unit{}); }

}  // namespace lf
