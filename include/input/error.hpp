#pragma once

#include <stdexcept>
#include <string>


namespace mylang {
namespace error {

class FileError: public std::exception
{
 private:
  std::string Imp_;  // Stores the detailed error message

 public:
  // Constructors for initializing the error message
  explicit FileError(const std::string& msg) :
      Imp_("File error occurred: " + msg)
  {
  }
  explicit FileError(const char* msg) :
      Imp_("File error occurred: " + std::string(msg))
  {
  }
  // Copy constructor for safely duplicating error objects
  FileError(const FileError& other) noexcept :
      Imp_(other.imp())
  {
  }

  // Copy assignment operator with self-assignment check
  FileError& operator=(const FileError& other) noexcept
  {
    if (this != &other)
    {
      Imp_ = other.imp();
    }
    return *this;
  }

  // Override base class destructor
  ~FileError() override = default;

  // Override `what()` to return a generic error message (not the detailed message)
  const char* what() const noexcept override { return Imp_.c_str(); }

  // Provides access to the stored detailed message
  const std::string& imp() const { return Imp_; }
};

}
}
