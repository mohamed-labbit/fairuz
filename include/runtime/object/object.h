#pragma once

#include "runtime/allocator/arena.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace mylang {
namespace runtime {
namespace object {

template<class _Alloc = allocator::ArenaAllocator, typename... _Args>
class _PrimitiveObject
{
   public:
    enum class _PrimitiveType { __INTEGER__, __FLOAT__, __STRING__, __FUNCTION__, __BOOLEAN__, __NONE__ };

   protected:
    bool __bool_;
    long long __int_;
    double __float_;
    std::string __str_;
    std::function<_PrimitiveObject(std::vector<_Object>)> __func_;
    _PrimitiveType __type_;
    _Alloc* __allocator_;

   public:
    // Default constructor
    _PrimitiveObject() :
        __type_(_PrimitiveType::__NONE__),
        __int_(0),
        __float_(0.0),
        __str_(""),
        __func_(nullptr),
        __bool_(false),
        __allocator_(nullptr)
    {
    }

    // Constructor with allocator
    _PrimitiveObject(_Alloc& __alloc) :
        __type_(_PrimitiveType::__NONE__),
        __int_(0),
        __float_(0.0),
        __str_(""),
        __func_(nullptr),
        __bool_(false),
        __allocator_(&__alloc)
    {
    }

    // Variadic constructor for values
    template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, _Alloc>>>
    _PrimitiveObject(T&& __arg)
    {
        __allocator_ = nullptr;

        if constexpr (std::is_same_v<std::decay_t<T>, long long> || std::is_integral_v<std::decay_t<T>>)
        {
            __type_ = _PrimitiveType::__INTEGER__;
            __int_ = static_cast<long long>(__arg);
            __float_ = static_cast<double>(__int_);
            __str_ = "";
            __func_ = nullptr;
            __bool_ = __int_ != 0;
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, double> || std::is_floating_point_v<std::decay_t<T>>)
        {
            __type_ = _PrimitiveType::__FLOAT__;
            __float_ = static_cast<double>(__arg);
            __int_ = static_cast<long long>(__float_);
            __str_ = "";
            __func_ = nullptr;
            __bool_ = __float_ != 0.0;
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<std::decay_t<T>, const char*>
          || std::is_same_v<std::decay_t<T>, char*>)
        {
            __type_ = _PrimitiveType::__STRING__;
            __int_ = 0;
            __float_ = 0.0;
            __str_ = std::forward<T>(__arg);
            __func_ = nullptr;
            __bool_ = !__str_.empty();
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, std::function<_PrimitiveObject(std::vector<_PrimitiveObject>)>>)
        {
            __type_ = _PrimitiveType::__FUNCTION__;
            __int_ = 0;
            __float_ = 0.0;
            __str_ = "";
            __func_ = std::forward<T>(__arg);
            __bool_ = __func_ != nullptr;
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, bool>)
        {
            __type_ = _PrimitiveType::__BOOLEAN__;
            __bool_ = __arg;
            __int_ = __bool_ ? 1 : 0;
            __float_ = static_cast<double>(__int_);
            __str_ = "";
            __func_ = nullptr;
        }
        else
        {
            *this = _PrimitiveObject();
        }
    }

    // Constructor with allocator and value
    template<typename T>
    _PrimitiveObject(_Alloc& __alloc, T&& __arg) :
        _PrimitiveObject(std::forward<T>(__arg))
    {
        __allocator_ = &__alloc;
    }

    ~_PrimitiveObject() = default;

    // Getters for type checking and access
    _PrimitiveType type() const { return __type_; }
    bool as_bool() const { return __bool_; }
    long long as_int() const { return __int_; }
    double as_float() const { return __float_; }
    const std::string& as_string() const { return __str_; }

    bool operator==(const _Object& __other) const
    {
        if (__type_ != __other.__type_)
            return false;
        switch (__type_)
        {
        case _PrimitiveType::__INTEGER__ :
            return __int_ == __other.__int_;
        case _PrimitiveType::__FLOAT__ :
            return __float_ == __other.__float_;
        case _PrimitiveType::__STRING__ :
            return __str_ == __other.__str_;
        case _PrimitiveType::__FUNCTION__ :
            return __func_.target_type() == __other.__func_.target_type();
        case _PrimitiveType::__BOOLEAN__ :
            return __bool_ == __other.__bool_;
        case _PrimitiveType::__NONE__ :
            return true;
        default :
            return false;
        }
    }

    bool operator!=(const _Object& __other) const { return !(*this == __other); }

    _PrimitiveObject operator+(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
            return _PrimitiveObject(__float_ + __other.__float_);
        else if (__type_ == _PrimitiveType::__STRING__ && __other.__type_ == _PrimitiveType::__STRING__)
            return _PrimitiveObject(__str_ + __other.__str_);
        return _PrimitiveObject();
    }

    _PrimitiveObject operator-(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
            return _PrimitiveObject(__float_ - __other.__float_);
        return _PrimitiveObject();
    }

    _PrimitiveObject operator*(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
            return _PrimitiveObject(__float_ * __other.__float_);
        return _PrimitiveObject();
    }

    _PrimitiveObject operator/(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
        {
            if (__other.__float_ == 0.0)
                return _PrimitiveObject();
            return _PrimitiveObject(__float_ / __other.__float_);
        }
        return _PrimitiveObject();
    }

    _PrimitiveObject operator&(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
            return _PrimitiveObject(__int_ & __other.__int_);
        else if (__type_ == _PrimitiveType::__BOOLEAN__ && __other.__type_ == _PrimitiveType::__BOOLEAN__)
            return _PrimitiveObject(__bool_ & __other.__bool_);
        return _PrimitiveObject();
    }

    _PrimitiveObject operator|(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
            return _PrimitiveObject(__int_ | __other.__int_);
        else if (__type_ == _PrimitiveType::__BOOLEAN__ && __other.__type_ == _PrimitiveType::__BOOLEAN__)
            return _PrimitiveObject(__bool_ | __other.__bool_);
        return _PrimitiveObject();
    }

    _PrimitiveObject operator^(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
            return _PrimitiveObject(__int_ ^ __other.__int_);
        else if (__type_ == _PrimitiveType::__BOOLEAN__ && __other.__type_ == _PrimitiveType::__BOOLEAN__)
            return _PrimitiveObject(__bool_ ^ __other.__bool_);
        return _PrimitiveObject();
    }

    _PrimitiveObject operator~() const
    {
        if (__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
            return _PrimitiveObject(~__int_);
        else if (__type_ == _PrimitiveType::__BOOLEAN__)
            return _PrimitiveObject(!__bool_);
        return _PrimitiveObject();
    }

    _PrimitiveObject operator!() const
    {
        if (__type_ == _PrimitiveType::__BOOLEAN__)
            return _PrimitiveObject(!__bool_);
        else if (__type_ == _PrimitiveType::__FUNCTION__)
            return _PrimitiveObject(__func_ == nullptr);
        return _PrimitiveObject();
    }

    _PrimitiveObject pow(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
            return _PrimitiveObject(std::pow(__float_, __other.__float_));
        return _PrimitiveObject();
    }

    bool greater_than(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
            return __float_ > __other.__float_;
        else if (__type_ == _PrimitiveType::__STRING__ && __other.__type_ == _PrimitiveType::__STRING__)
            return __str_ > __other.__str_;
        return false;
    }

    bool greater_equal(const _Object& __other) const { return greater_than(__other) || (*this == __other); }

    bool less_than(const _Object& __other) const
    {
        if ((__type_ == _PrimitiveType::__INTEGER__ || __type_ == _PrimitiveType::__FLOAT__)
          && (__other.__type_ == _PrimitiveType::__INTEGER__ || __other.__type_ == _PrimitiveType::__FLOAT__))
            return __float_ < __other.__float_;
        else if (__type_ == _PrimitiveType::__STRING__ && __other.__type_ == _PrimitiveType::__STRING__)
            return __str_ < __other.__str_;
        return false;
    }

    bool less_equal(const _Object& __other) const { return less_than(__other) || (*this == __other); }

    _PrimitiveObject func_call(std::vector<_Object> args)
    {
        if (__type_ == _PrimitiveType::__FUNCTION__ && __func_)
            return __func_(args);
        return _PrimitiveObject();
    }
};

}
}
}