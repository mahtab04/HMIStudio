#pragma once

#include <QString>

#include <tl/expected.hpp>

#include <utility>

namespace hmi
{

// Category of a recoverable failure. Callers branch on the code; the message
// is for logs and the Output panel.
enum class ErrorCode
{
    InvalidArgument,
    TypeMismatch,
    OutOfRange,
    UnknownProperty,
    UnknownType,
    NotFound,
    Duplicate,
};

struct Error
{
    ErrorCode code = ErrorCode::InvalidArgument;
    QString message;
};

// Result of an operation that can fail for reasons outside the programmer's
// control (bad input, missing object). Programming errors use Q_ASSERT.
template <typename T>
using Result = tl::expected<T, Error>;

[[nodiscard]] inline tl::unexpected<Error> makeError(ErrorCode code, QString message)
{
    return tl::unexpected<Error>(Error{code, std::move(message)});
}

} // namespace hmi
