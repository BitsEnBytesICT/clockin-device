#pragma once

#include "services/api/ApiTypes.hpp"

#include <cstdint>
#include <string>

namespace clockin {

class IApiService {
public:
    virtual ~IApiService() = default;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual std::uint64_t SubmitHealth() = 0;
    virtual std::uint64_t SubmitScan(const std::string& uid) = 0;
    virtual std::uint64_t SubmitAttendance(const std::string& uid) = 0;
    virtual std::uint64_t SubmitSignature(const std::string& uid,
                                          const std::string& signature) = 0;
    virtual bool TryPop(ApiResult& result) = 0;
    virtual bool HasApiKey() const = 0;
    virtual std::string BaseUrl() const = 0;
    // Applies to jobs started after the call; a request already in flight
    // finishes against the previous address.
    virtual void SetBaseUrl(const std::string& base_url) = 0;
};

}  // namespace clockin
