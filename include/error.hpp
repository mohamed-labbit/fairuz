#ifndef ERROR_OR
#define ERROR_OR

#include <variant>

template<typename T, typename E>
class [[nodiscard]] ErrorOr {
public:
    using ResultType = T;
    using ErrorType = E;

    ErrorOr()
        : ValueOrError_()
    {
    }

private:
    std::variant<T, ErrorType> ValueOrError_;
};

#endif // ERROR_OR
