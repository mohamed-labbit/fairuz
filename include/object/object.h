#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>


template<typename... _Args>
class _Object
{
  enum class _Type {
    Integer,
    Float,
    STRING,
    Function,
    Boolean,
    None
  };

 protected:
  bool                                         __bool_;
  long long                                    __int_;
  double                                       __float_;
  std::string                                  __str_;
  std::function<_Object(std::vector<_Object>)> __func_;
  _Type                                        __type_;

  _Object() :
      __type_(_Type::None),
      __int_(0),
      __float_(0.0),
      __str_(""),
      __func_(nullptr) {}

  _Object(_Args&&... __args) {
    if ((... || (typeid(__args) == typeid(long long))))
    {
      __type_  = _Type::Integer;
      __int_   = std::get<0>(std::forward_as_tuple(__args...));
      __float_ = static_cast<float>(__int_);
      __str_   = "";
      __func_  = nullptr;
      __bool_  = __int_ == 0 ? false : true;
    }
    else if ((... || (typeid(__args) == typeid(double))))
    {
      __type_  = _Type::Float;
      __float_ = std::get<0>(std::forward_as_tuple(__args...));
      __int_   = static_cast<long long>(__float_);
      __str_   = "";
      __func_  = nullptr;
      __bool_  = __float_ == 0.0f ? false : true;
    }
    else if ((... || (typeid(__args) == typeid(std::string))))
    {
      __type_  = _Type::STRING;
      __int_   = 0;
      __float_ = 0.0;
      __str_   = std::get<0>(std::forward_as_tuple(__args...));
      __func_  = nullptr;
      __bool_  = __str_.empty() ? false : true;
    }
    else if ((... || (typeid(__args) == typeid(std::function<_Object(std::vector<_Object>)>))))
    {
      __type_  = _Type::Function;
      __int_   = 0;
      __float_ = 0.0;
      __str_   = "";
      __func_  = std::get<0>(std::forward_as_tuple(__args...));
      __bool_  = __func_ == nullptr ? false : true;
    }
    else if ((... || (typeid(__args) == typeid(bool))))
    {
      __type_  = _Type::Boolean;
      __bool_  = std::get<0>(std::forward_as_tuple(__args...));
      __int_   = __bool_ ? 1 : 0;
      __float_ = static_cast<float>(__int_);
      __str_   = "";
      __func_  = nullptr;
    }
    else
    {
      *this = _Object();
    }
  }

  ~_Object() = default;

  bool operator==(const _Object& __other) const;
  bool operator!=(const _Object& __other) const;

  _Object operator+(const _Object& __other) const;
  _Object operator-(const _Object& __other) const;
  _Object operator*(const _Object& __other) const;
  _Object operator/(const _Object& __other) const;
  _Object operator&(const _Object& __other) const;
  _Object operator|(const _Object& __other) const;
  _Object operator^(const _Object& __other) const;

  _Object operator~() const;
  _Object operator!() const;

  _Object pow(const _Object& __other) const;

  bool greater_than(const _Object& __other) const;
  bool greater_equal(const _Object& __other) const;
  bool less_than(const _Object& __other) const;
  bool less_equal(const _Object& __other) const;

  _Object func_call(std::vector<_Object> args);
};
