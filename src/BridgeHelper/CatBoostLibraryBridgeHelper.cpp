#include "CatBoostLibraryBridgeHelper.h"

#include <Common/escapeForFileName.h>

#include <Poco/Net/HTTPRequest.h>

#include <Columns/ColumnsNumber.h> // TODO temporary
#include <Core/Block.h> // TODO temporary
#include <Formats/NativeWriter.h> // TODO temporary
#include <Formats/NativeReader.h> // TODO temporary
#include <IO/ReadBufferFromString.h> // TODO temporary
#include <IO/WriteBufferFromString.h> // TODO temporary
#include <DataTypes/DataTypesNumber.h> // TODO temporary

namespace DB
{

CatBoostLibraryBridgeHelper::CatBoostLibraryBridgeHelper(
        ContextPtr context_,
        const String & library_path_,
        const String & model_path_)
    : IBridgeHelper(context_->getGlobalContext())
    , log(&Poco::Logger::get("CatBoostLibraryBridgeHelper"))
    , config(context_->getConfigRef())
    , http_timeout(context_->getGlobalContext()->getSettingsRef().http_receive_timeout.value)
    , library_path(library_path_)
    , model_path(model_path_)
    , bridge_host(config.getString("library_bridge.host", DEFAULT_HOST))
    , bridge_port(config.getUInt("library_bridge.port", DEFAULT_PORT))
    , http_timeouts(ConnectionTimeouts::getHTTPTimeouts(context_))
{
}

CatBoostLibraryBridgeHelper::~CatBoostLibraryBridgeHelper()
{
    /// try
    /// {
    ///     removeLibrary();
    /// }
    /// catch (...)
    /// {
    ///     tryLogCurrentException(log);
    /// }
}

Poco::URI CatBoostLibraryBridgeHelper::getPingURI() const
{
    auto uri = createBaseURI();
    uri.setPath(PING_HANDLER);
    return uri;
}

Poco::URI CatBoostLibraryBridgeHelper::getMainURI() const
{
    auto uri = createBaseURI();
    uri.setPath(MAIN_HANDLER);
    return uri;
}


Poco::URI CatBoostLibraryBridgeHelper::createRequestURI(const String & method) const
{
    auto uri = getMainURI();
    uri.addQueryParameter("method", method);
    return uri;
}

Poco::URI CatBoostLibraryBridgeHelper::createBaseURI() const
{
    Poco::URI uri;
    uri.setHost(bridge_host);
    uri.setPort(bridge_port);
    uri.setScheme("http");
    return uri;
}

void CatBoostLibraryBridgeHelper::startBridge(std::unique_ptr<ShellCommand> cmd) const
{
    getContext()->addBridgeCommand(std::move(cmd));
}

bool CatBoostLibraryBridgeHelper::bridgeHandShake()
{
    String result;
    try
    {
        ReadWriteBufferFromHTTP buf(getPingURI(), Poco::Net::HTTPRequest::HTTP_GET, {}, http_timeouts, credentials);
        readString(result, buf);
    }
    catch (...)
    {
        tryLogCurrentException(log);
        return false;
    }

    if (result.size() != 1)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Unexpected message from catboost bridge: {}. Check that bridge and server have the same version.", result);

    return true;
}

ReadWriteBufferFromHTTP::OutStreamCallback CatBoostLibraryBridgeHelper::getInitLibraryCallback() const
{
    return [this](std::ostream & os)
    {
        os << "library_path=" << escapeForFileName(library_path) << "&";
        os << "model_path=" << escapeForFileName(model_path);
    };
}

ReadWriteBufferFromHTTP::OutStreamCallback CatBoostLibraryBridgeHelper::getEvaluateLibraryCallback(const String & serialized) const
{
    return [serialized](std::ostream & os)
    {
        os << "data=" << serialized;
    };
}

bool CatBoostLibraryBridgeHelper::initLibrary()
{
    startBridgeSync();

    ReadWriteBufferFromHTTP buf(
        createRequestURI(CATBOOST_LIB_NEW_METHOD),
        Poco::Net::HTTPRequest::HTTP_POST,
        getInitLibraryCallback(),
        http_timeouts, credentials);

    bool res;
    readBoolText(res, buf);
    return res;
}

size_t CatBoostLibraryBridgeHelper::getTreeCount()
{
    startBridgeSync();

    ReadWriteBufferFromHTTP buf(
        createRequestURI(CATBOOST_GETTREECOUNT_METHOD),
        Poco::Net::HTTPRequest::HTTP_POST,
        {},
        http_timeouts, credentials);

    size_t res;
    readIntBinary(res, buf);
    return res;
}

ColumnPtr CatBoostLibraryBridgeHelper::evaluate(const ColumnsWithTypeAndName & columns)
{
    startBridgeSync();

    WriteBufferFromOwnString string_write_buf;
    Block block(columns);
    NativeWriter native_writer(string_write_buf, 0, block);
    native_writer.write(block);

    ReadWriteBufferFromHTTP buf(
        createRequestURI(CATBOOST_LIB_EVALUATE_METHOD),
        Poco::Net::HTTPRequest::HTTP_POST,
        getEvaluateLibraryCallback(string_write_buf.str()),
        http_timeouts, credentials);

    String res;
    readStringBinary(res, buf);
    ReadBufferFromString string_read_buf(res);
    NativeReader native_reader(string_read_buf, 0);
    Block block_read = native_reader.read();

    return block_read.getColumns()[0];
}

bool CatBoostLibraryBridgeHelper::removeLibrary()
{
    if (bridgeHandShake())
    {
        ReadWriteBufferFromHTTP buf(
            createRequestURI(CATBOOST_LIB_DELETE_METHOD),
            Poco::Net::HTTPRequest::HTTP_POST,
            {},
            http_timeouts, credentials);

        bool res;
        readBoolText(res, buf);
        return res;
    }
    return true;
}

}
