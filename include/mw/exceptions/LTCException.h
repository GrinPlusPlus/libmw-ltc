#pragma once

//#include <mw/common/Logger.h>

#include <stdexcept>
#include <string>

class LTCException : public std::exception
{
public:
    ~LTCException() = default;

    virtual const char* what() const throw()
    {
        return m_what.c_str();
    }

    const std::string& GetMsg() const { return m_message; }

protected:
    LTCException(const std::string& type, const std::string& message, const std::string& function)
    {
        m_type = type;
        m_message = message;
        m_function = function;
        m_what = function + ": " + message;

        //LOG_WARNING_F("{} thrown at {}: {}", type, function, message);
    }

private:
    std::string m_type;
    std::string m_message;
    std::string m_function;
    std::string m_what;
};