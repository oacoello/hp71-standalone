#pragma once
#include <string>

namespace Auth {

    struct AuthResult {
        bool success;
        bool is_admin;
        std::string message;
    };

    // Valida credenciales contra auth/users.json
    AuthResult validate(const std::string& user_id, const std::string& password);

}