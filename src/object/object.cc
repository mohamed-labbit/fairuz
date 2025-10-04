#include "object/object.h"


template<typename... _Args>
bool _Object<_Args...>::operator==(const _Object& __other) const
{
    if (__type_ != __other.__type_)
    {
        return false;
    }

    switch (__type_)
    {
    case _Type::Integer :
        return __int_ == __other.__int_;
    case _Type::Float :
        return __float_ == __other.__float_;
    case _Type::STRING :
        return __str_ == __other.__str_;
    case _Type::Function :
        // Functions are equal if they point to the same address
        return static_cast<bool>(
          __func_.target_type() == __other.__func_.target_type()
          && __func_.template target<_Object (*)(std::vector<_Object>)>()
               == __other.__func_.template target<_Object (*)(std::vector<_Object>)>());
    case _Type::Boolean :
        return __bool_ == __other.__bool_;
    case _Type::None :
        return true;
    default :
        return false;
    }
}

template<typename... _Args>
bool _Object<_Args...>::operator!=(const _Object& __other) const
{
    return !(*this == __other);
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::operator+(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        return _Object(__float_ + __other.__float_);
    }
    else if (__type_ == _Type::STRING && __other.__type_ == _Type::STRING)
    {
        return _Object(__str_ + __other.__str_);
    }
    else
    {
        return _Object();
    }
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::operator-(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        return _Object(__float_ - __other.__float_);
    }
    else
    {
        return _Object();
    }
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::operator*(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        return _Object(__float_ * __other.__float_);
    }
    else
    {
        return _Object();
    }
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::operator/(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        if (__other.__float_ == 0)
        {
            return _Object();
        }

        return _Object(__int_ / __other.__int_);
    }
    else
    {
        return _Object();
    }
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::pow(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        return _Object(std::pow(__float_, __other.__float_));
    }
    else
    {
        return _Object();
    }
}

template<typename... _Args>
bool _Object<_Args...>::less_than(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        return _Object(static_cast<bool>(__float_ < __other.__float_));
    }
    else if (__type_ == _Type::Function && __other.__type_ == _Type::Function)
    {
        return _Object(static_cast<bool>(
          __func_.target_type() == __other.__func_.target_type()
          && __func_.template target<_Object (*)(std::vector<_Object>)>()
               < __other.__func_.template target<_Object (*)(std::vector<_Object>)>()));
    }
    else if (__type_ == _Type::STRING && __other.__type_ == _Type::STRING)
    {
        return _Object(static_cast<bool>(__str_ < __other.__str_));
    }
}

template<typename... _Args>
bool _Object<_Args...>::greater_than(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        return _Object(static_cast<bool>(__float_ > __other.__float_));
    }
    else if (__type_ == _Type::Function && __other.__type_ == _Type::Function)
    {
        return _Object(static_cast<bool>(
          __func_.target_type() == __other.__func_.target_type()
          && __func_.template target<_Object (*)(std::vector<_Object>)>()
               > __other.__func_.template target<_Object (*)(std::vector<_Object>)>()));
    }
    else if (__type_ == _Type::STRING && __other.__type_ == _Type::STRING)
    {
        return _Object(static_cast<bool>(__str_ > __other.__str_));
    }
}

template<typename... _Args>
bool _Object<_Args...>::less_equal(const _Object& __other) const
{
    return !(__other.greater_than(*this));
}

template<typename... _Args>
bool _Object<_Args...>::greater_equal(const _Object& __other) const
{
    return !(__other.less_than(*this));
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::operator&(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        return _Object(__int_ & __other.__int_);
    }
    else if (__type_ == _Type::Boolean && __other.__type_ == _Type::Boolean)
    {
        return _Object(__bool_ & __other.__bool_);
    }
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::operator|(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        return _Object(__int_ | __other.__int_);
    }
    else if (__type_ == _Type::Boolean && __other.__type_ == _Type::Boolean)
    {
        return _Object(__bool_ | __other.__bool_);
    }
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::operator^(const _Object& __other) const
{
    if ((__type_ == _Type::Integer && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Integer)
        || (__type_ == _Type::Float && __other.__type_ == _Type::Float)
        || (__type_ == _Type::Integer && __other.__type_ == _Type::Integer))
    {
        return _Object(__int_ ^ __other.__int_);
    }
    else if (__type_ == _Type::Boolean && __other.__type_ == _Type::Boolean)
    {
        return _Object(__bool_ ^ __other.__bool_);
    }
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::operator~() const
{
    if ((__type_ == _Type::Integer) || (__type_ == _Type::Float))
    {
        return _Object(~__int_);
    }
    else if (__type_ == _Type::Boolean)
    {
        return _Object(~__bool_);
    }
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::operator!() const
{
    if (__type_ == _Type::Boolean)
    {
        return _Object(!__bool_);
    }
    else if (__type_ == _Type::Function)
    {
        return _Object(!__func_);
    }
}

template<typename... _Args>
_Object<_Args...> _Object<_Args...>::func_call(std::vector<_Object> args)
{
    if (__type_ == _Type::Function)
    {
        return __func_(args);
    }

    return _Object();
}