#pragma once

#include "opticloud-device-mgmt-cpp/connectionparameters.hpp"
#include "libcurl-wrapper/curlhttptransfer.hpp"

#include <string>

namespace opticloud
{

class SingleEntryTransfer;
using SingleEntryCallback = std::function<void (SingleEntryTransfer *transfer)>;

class SingleEntryTransfer
{
public:
    enum ProtocolVersion
    {
        V4,
        V5, // serial number in PING content
        V6  // separate firmware update task
    };

    enum RequestType
    {
        PING,
        ATTENTION,
        UPLOAD,
        RETURN_ERROR,
        RETURN_SCPI,
        RETURN_SCRIPT,
        RETURN_LIST,
        RETURN_FILE,

        // v6 protocol extension
        ACKNOWLEDGMENT_FIRMWARE_UPDATE,
        PROGRESS_FIRMWARE_UPDATE,
        RETURN_FIRMWARE_UPDATE,
        RETURN_ABORT_ASYNCHRONOUS_TASK
    };

    enum TransferResult
    {
        NONE,
        RUNNING,

        ASYNC_CANCELED,
        ASYNC_TIMEOUT,

        UNKNOWN_ERROR,
        CURL_ERROR,

        UNKNOWN_RESPONSE,

        RETURN_OK, // no request from cloud

        TASK_REQUEST_LIST,
        TASK_REQUEST_FILE,
        TASK_EXECUTE_SCRIPT,
        TASK_SCPI_DEVICE_MANAGER,
        TASK_SCPI_APPLICATION,

        // v6 protocol extension
        TASK_FIRMWARE_UPDATE,
        ABORT_ASYNCHRONOUS_TASK
    };

    explicit SingleEntryTransfer(const cu::Logger& logger, const std::shared_ptr<ConnectionParameters>& connectionParameters);
    ~SingleEntryTransfer();

    void setProtocolVersion(const ProtocolVersion newProtocolVersion);

    void setRequestType(RequestType newRequestType);
    RequestType requestType();

    // To set the upload content, please use one of the following functions (but NOT both)
    void setUploadFilename(const std::string& fileNameWithPath);
    std::string const& uploadFileName() const;
    void setUploadData(const char *data,  long size = -1, const std::string &filename = "omCloudService-0.xml");

    // To get the output content, please use one of the following functions (but NOT both)
    std::vector<char> &responseData();
    void setOutputFilename(const std::string& fileNameWithPath);

    std::shared_ptr<curl::CurlAsyncTransfer> prepareTransfer();
    std::shared_ptr<curl::CurlHttpTransfer> transfer() const;

    void setCloudCallback(const SingleEntryCallback &newCloudCallback);
    TransferResult result() const;
    CURLcode curlResult() const;
    long httpResponseCode() const;

    // used to match response to request
    std::string const& messageToken() const;
    void setMessageToken(const std::string &newMessageToken);

    // used in RETURN_FILE to support multiple timelines
    void setReturnFileTag(const std::string &newReturnFileTag);

    float transferDuration_s() const;
    uint64_t transferSpeed_BytesPerSecond() const;
    uint64_t transferredBytes() const;
    void setLogPrefix(const std::string &newLogPrefix);

    std::shared_ptr<ConnectionParameters> connectionParameters() const;
    void setConnectionParameters(const std::shared_ptr<ConnectionParameters> &newConnectionParameters);

private:
    std::string mimeTypeFromFileExtension(const std::string &filenameWithPath);
    void parseResponse();

    cu::Logger m_logger;
    std::shared_ptr<ConnectionParameters> m_connectionParameters;
    std::shared_ptr<curl::CurlHttpTransfer> m_transfer;

    SingleEntryCallback m_cloudCallback;
    TransferResult m_result{NONE};
    CURLcode   m_curlResult{CURL_LAST}; // result from the curl transfer; only valid if LegacyResult == CURL_ERROR
    long m_httpResponseCode{0};

    ProtocolVersion m_protocolVersion{V4};
    RequestType m_requestType{PING};

    std::string m_uploadFileName;
    std::string m_outputFileName;

    const char *m_uploadDataPointer{nullptr};
    long m_uploadDataSize{-1};
    std::string m_uploadDataFileName{"omCloudService-0.xml"};

    curl_mime *m_multiPartContainer{nullptr};

    std::string m_messageToken;  // The cloud currently uses an int32 implementation (like the former PHP Cloud and older CloudService versions). Value range: according to Oleh: 1 .. 2^31 - 1
                                 // However, since there are regular overflows with a cloud-wide global 32-bit value, which is stored in the database for months/years, we are preparing for a changeover in the future from version 22.16 onwards.
    std::string m_returnFileTag;
};

}
