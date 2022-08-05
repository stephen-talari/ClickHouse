#pragma once

#include <BridgeHelper/IBridgeHelper.h>
#include <IO/ReadWriteBufferFromHTTP.h>
#include <Interpreters/Context.h>

#include <DataTypes/IDataType.h>

#include <Poco/Logger.h>
#include <Poco/Net/HTTPBasicCredentials.h>
#include <Poco/URI.h>

namespace DB
{

class CatBoostLibraryBridgeHelper : public IBridgeHelper
{
public:
    static constexpr inline size_t DEFAULT_PORT = 9012;
    static constexpr inline auto PING_HANDLER = "/catboost_ping";
    static constexpr inline auto MAIN_HANDLER = "/catboost_request";

    CatBoostLibraryBridgeHelper(ContextPtr context_, const String & library_path_, const String & model_path_);

    ~CatBoostLibraryBridgeHelper() override;

    bool initLibrary();

    size_t getTreeCount();

    ColumnPtr evaluate(const ColumnsWithTypeAndName & columns);

protected:
    Poco::URI getPingURI() const override;

    Poco::URI getMainURI() const override;

    bool bridgeHandShake() override;

    void startBridge(std::unique_ptr<ShellCommand> cmd) const override;

    String serviceAlias() const override { return "clickhouse-library-bridge"; }

    String serviceFileName() const override { return serviceAlias(); }

    size_t getDefaultPort() const override { return DEFAULT_PORT; }

    bool startBridgeManually() const override { return false; }

    String configPrefix() const override { return "library_bridge"; }

    const Poco::Util::AbstractConfiguration & getConfig() const override { return config; }

    Poco::Logger * getLog() const override { return log; }

    Poco::Timespan getHTTPTimeout() const override { return http_timeout; }

    Poco::URI createBaseURI() const override;

    ReadWriteBufferFromHTTP::OutStreamCallback getInitLibraryCallback() const;
    ReadWriteBufferFromHTTP::OutStreamCallback getEvaluateLibraryCallback(const String & serialized) const;

private:
    static constexpr inline auto CATBOOST_LIB_NEW_METHOD = "catboost_libNew";
    static constexpr inline auto CATBOOST_LIB_DELETE_METHOD = "catboost_libDelete";
    static constexpr inline auto CATBOOST_GETTREECOUNT_METHOD = "catboost_GetTreeCount";
    static constexpr inline auto CATBOOST_LIB_EVALUATE_METHOD = "catboost_libEvaluate";

    Poco::URI createRequestURI(const String & method) const;

    bool removeLibrary();

    Poco::Logger * log;
    const Poco::Util::AbstractConfiguration & config;
    const Poco::Timespan http_timeout;

    String library_path;
    String model_path;

    String bridge_host;
    size_t bridge_port;
    ConnectionTimeouts http_timeouts;
    Poco::Net::HTTPBasicCredentials credentials{};
};

}
