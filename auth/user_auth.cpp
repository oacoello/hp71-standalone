#include "user_auth.h"

namespace Auth
{

AuthResult validate(const std::string&, const std::string&)
{
    AuthResult r;

    // LOGIN DESACTIVADO PARA DESARROLLO
    r.success = true;

    return r;
}

}