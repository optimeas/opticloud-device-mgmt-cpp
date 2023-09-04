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
        V5  // separate firmware update task
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

        // v5 protocol extension
        ACKNOWLEDGMENT_FIRMWARE_UPDATE,
        PROGRESS_FIRMWARE_UPDATE,
        RETURN_FIRMWARE_UPDATE
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

        // v5 protocol extension
        TASK_FIRMWARE_UPDATE
    };

    explicit SingleEntryTransfer(const cu::Logger& logger, const std::shared_ptr<ConnectionParameters>& connectionParameters);
    ~SingleEntryTransfer();

    void setProtocolVersion(const ProtocolVersion newProtocolVersion);
    void setRequestType(RequestType newRequestType);
    RequestType requestType();

    // To set the upload content, please use one of the following functions (but NOT both)
    void setUploadFilename(const std::string& fileNameWithPath);
    std::string uploadFileName() const;
    void setUploadData(const char *data,  long size = -1, const std::string &filename = "omCloudService-0.xml");

    // To get the output content, please use one of the following functions (but NOT both)
    std::vector<char> &responseData();
    void setOutputFilename(const std::string& fileNameWithPath);

    std::shared_ptr<curl::CurlAsyncTransfer> prepareTransfer();

    void setCloudCallback(const SingleEntryCallback &newCloudCallback);
    TransferResult result() const;
    CURLcode curlResult() const;
    long httpResponseCode() const;

    // used to match response to request
    int messageId() const;
    void setMessageId(int newMessageId);

    // used in RETURN_FILE to support multiple timelines
    void setReturnFileTag(const std::string &newReturnFileTag);
    void setSslVerification(bool enable = true);

    float transferDuration_s() const;
    uint64_t transferSpeed_BytesPerSecond() const;
    uint64_t transferredBytes() const;

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

    int m_messageId{-1}; // according to Oleh: 1 .. 2^31 - 1
    std::string m_returnFileTag;
};

}
