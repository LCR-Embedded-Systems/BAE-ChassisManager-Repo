#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <optional>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>
#include <future>
#include <bitset>
#include <cmath>
#include <fstream>
#include <variant>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <fmt/format.h>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <iostream>
#include <nlohmann/json.hpp>


extern std::ofstream ADC_dbgFile;

std::string currentTimestamp();\

int obtain_raw_voltage(std::string path);

std::string readFile(std::string path);



namespace phosphor
{
namespace interface
{
namespace util
{
namespace detail
{
namespace errors = sdbusplus::xyz::openbmc_project::Common::Error;
} // namespace detail


/**
 * @class DBusError
 *
 * The base class for the exceptions thrown on fails in the various
 * SDBusPlus calls.  Used so that a single catch statement can catch
 * any type of these exceptions.
 *
 * None of these exceptions will log anything when they are created,
 * it is up to the handler to do that if desired.
 */
class DBusError : public std::runtime_error
{
  public:
    explicit DBusError(const std::string& msg) : std::runtime_error(msg)
    {}
};

/**
 * @class DBusMethodError
 *
 * Thrown on a DBus Method call failure
 */
class DBusMethodError : public DBusError
{
  public:
    DBusMethodError(const std::string& busName, const std::string& path,
                    const std::string& interface, const std::string& method) :
        DBusError(fmt::format("DBus method failed: {} {} {} {}", busName, path,
                              interface, method)),
        busName(busName), path(path), interface(interface), method(method)
    {}

    const std::string busName;
    const std::string path;
    const std::string interface;
    const std::string method;
};

/**
 * @class DBusServiceError
 *
 * Thrown when a service lookup fails.  Usually this points to
 * the object path not being present in D-Bus.
 */
class DBusServiceError : public DBusError
{
  public:
    DBusServiceError(const std::string& path, const std::string& interface) :
        DBusError(
            fmt::format("DBus service lookup failed: {} {}", path, interface)),
        path(path), interface(interface)
    {}

    const std::string path;
    const std::string interface;
};


/**
 * @class DBusPropertyError
 *
 * Thrown when a set/get property fails.
 */
class DBusPropertyError : public DBusError
{
  public:
    DBusPropertyError(const std::string& msg, const std::string& busName,
                      const std::string& path, const std::string& interface,
                      const std::string& property) :
        DBusError(msg + fmt::format(": {} {} {} {}", busName, path, interface,
                                    property)),
        busName(busName), path(path), interface(interface), property(property)
    {}

    const std::string busName;
    const std::string path;
    const std::string interface;
    const std::string property;
};



/** @brief Alias for PropertiesChanged signal callbacks. */
template <typename... T>
using Properties = std::map<std::string, std::variant<T...>>;

template <typename... Args>
static auto callMethod(sdbusplus::bus_t& bus, const std::string& busName,
                       const std::string& path,
                       const std::string& interface,
                       const std::string& method, Args&&... args)
{

    auto reqMsg = bus.new_method_call(busName.c_str(), path.c_str(),
                                      interface.c_str(), method.c_str());
    reqMsg.append(std::forward<Args>(args)...);
    try
    {
        auto respMsg = bus.call(reqMsg);
        if (respMsg.is_method_error())
        {
            throw DBusMethodError{busName, path, interface, method};
        }
        return respMsg;
    }
    catch (const sdbusplus::exception_t&)
    {
        throw DBusMethodError{busName, path, interface, method};
    }
}

template <typename Ret, typename... Args>
static auto
callMethodAndRead(sdbusplus::bus_t& bus, const std::string& busName,
                  const std::string& path, const std::string& interface,
                  const std::string& method, Args&&... args)
{
    sdbusplus::message_t respMsg = callMethod<Args...>(
        bus, busName, path, interface, method, std::forward<Args>(args)...);
    Ret resp;
    respMsg.read(resp);
    return resp;
}


/** @brief Get subtree paths from the mapper without checking response. */
static auto getSubTreePathsRaw(sdbusplus::bus_t& bus,
    const std::string& path,
    const std::string& interface, int32_t depth)
{
    using namespace std::literals::string_literals;

    using Path = std::string;
    using Intf = std::string;
    using Intfs = std::vector<Intf>;
    using ObjectPaths = std::vector<Path>;
    Intfs intfs = {interface};

    return callMethodAndRead<ObjectPaths>(
        bus, "xyz.openbmc_project.ObjectMapper"s,
        "/xyz/openbmc_project/object_mapper"s,
        "xyz.openbmc_project.ObjectMapper"s, "GetSubTreePaths"s, path,
        depth, intfs);
}

/** @brief Get subtree paths from the mapper. */
static auto getSubTreePaths(sdbusplus::bus_t& bus, const std::string& path,
     const std::string& interface, int32_t depth)
{
    auto mapperResp = getSubTreePathsRaw(bus, path, interface, depth);
    if (mapperResp.empty())
    {
        phosphor::logging::log<phosphor::logging::level::ERR>(
            "Empty response from mapper GetSubTreePaths",
            phosphor::logging::entry("SUBTREE=%s", path.c_str()),
            phosphor::logging::entry("INTERFACE=%s", interface.c_str()),
            phosphor::logging::entry("DEPTH=%u", depth));
            phosphor::logging::elog<detail::errors::InternalFailure>();
    }
    return mapperResp;
}



/** @brief Get subtree from the mapper without checking response. */
static auto getSubTreeRaw(sdbusplus::bus_t& bus, const std::string& path,
    const std::string& interface, int32_t depth)
{
    using namespace std::literals::string_literals;

    using Path = std::string;
    using Intf = std::string;
    using Serv = std::string;
    using Intfs = std::vector<Intf>;
    using Objects = std::map<Path, std::map<Serv, Intfs>>;
    Intfs intfs = {interface};

    return callMethodAndRead<Objects>(bus,
                        "xyz.openbmc_project.ObjectMapper"s,
                        "/xyz/openbmc_project/object_mapper"s,
                        "xyz.openbmc_project.ObjectMapper"s,
                        "GetSubTree"s, path, depth, intfs);
}

/** @brief Get service from the mapper without checking response. */
static auto getServiceRaw(sdbusplus::bus_t& bus, const std::string& path,
    const std::string& interface)
{
    using namespace std::literals::string_literals;
    using GetObject = std::map<std::string, std::vector<std::string>>;

    return callMethodAndRead<GetObject>(
        bus, "xyz.openbmc_project.ObjectMapper"s,
        "/xyz/openbmc_project/object_mapper"s,
        "xyz.openbmc_project.ObjectMapper"s, "GetObject"s, path,
        GetObject::mapped_type{interface});
}

/** @brief Get service from the mapper. */
static auto getService(sdbusplus::bus_t& bus, const std::string& path,
    const std::string& interface)
{
    try
    {
        auto mapperResp = getServiceRaw(bus, path, interface);

        if (mapperResp.empty())
        {
            // Should never happen.  A missing object would fail
            // in callMethodAndRead()
            phosphor::logging::log<phosphor::logging::level::ERR>(
            "Empty mapper response on service lookup");
            throw DBusServiceError{path, interface};
        }
        return mapperResp.begin()->first;
    }
    catch (const DBusMethodError& e)
    {
        throw DBusServiceError{path, interface};
    }
}

/** @brief Get a property with mapper lookup. */
template <typename Property>
static auto getProperty(sdbusplus::bus_t& bus, const std::string& path,
                        const std::string& interface,
                        const std::string& property)
{
    using namespace std::literals::string_literals;

    auto service = getService(bus, path, interface);
    auto msg =
        callMethod(bus, service, path, "org.freedesktop.DBus.Properties"s,
                    "Get"s, interface, property);
    if (msg.is_method_error())
    {
        throw DBusPropertyError{"DBus get property failed", service, path,
                                interface, property};
    }
    std::variant<Property> value;
    msg.read(value);
    return std::get<Property>(value);
}

/** @brief Invoke a method and return without checking for error. */
template <typename... Args>
static auto callMethodAndReturn(sdbusplus::bus_t& bus,
                                const std::string& busName,
                                const std::string& path,
                                const std::string& interface,
                                const std::string& method, Args&&... args)
{
    auto reqMsg = bus.new_method_call(busName.c_str(), path.c_str(),
                                        interface.c_str(), method.c_str());
    reqMsg.append(std::forward<Args>(args)...);
    auto respMsg = bus.call(reqMsg);

    return respMsg;
}

/** @brief Set a property with mapper lookup. */
template <typename Property>
static void setProperty(sdbusplus::bus_t& bus, const std::string& path,
                        const std::string& interface,
                        const std::string& property, Property&& value)
{
    using namespace std::literals::string_literals;

    std::variant<Property> varValue(std::forward<Property>(value));

    auto service = getService(bus, path, interface);
    auto msg = callMethodAndReturn(bus, service, path,
                                    "org.freedesktop.DBus.Properties"s,
                                    "Set"s, interface, property, varValue);
    if (msg.is_method_error())
    {
        throw DBusPropertyError{"DBus set property failed", service, path,
                                interface, property};
    }
}

}
}
}
