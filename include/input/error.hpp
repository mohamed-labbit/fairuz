#pragma once

#include <stdexcept>
#include <string>


namespace mylang {
namespace error {

class file_error: public std::exception
{
   private:
    std::string imp_;  // Stores the detailed error message

   public:
    // Constructors for initializing the error message
    explicit file_error(const std::string& msg) :
        imp_("File error occurred: " + msg)
    {
    }
    explicit file_error(const char* msg) :
        imp_("File error occurred: " + std::string(msg))
    {
    }
    // Copy constructor for safely duplicating error objects
    file_error(const file_error& other) noexcept :
        imp_(other.imp())
    {
    }
    // Copy assignment operator with self-assignment check
    file_error& operator=(const file_error& other) noexcept
    {
        if (this != &other)
            imp_ = other.imp();
        return *this;
    }
    // Override base class destructor
    ~file_error() override = default;
    // Override `what()` to return a generic error message (not the detailed message)
    const char* what() const noexcept override { return imp_.c_str(); }
    // Provides access to the stored detailed message
    const std::string& imp() const { return imp_; }
};

}
}
