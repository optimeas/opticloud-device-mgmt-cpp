#include "opticloud-device-mgmt-cpp/singleentrytransfer.hpp"
#include "libcurl-wrapper/urlutils.hpp"

#include <fmt/core.h>
#include <fmt/format.h>

#include <filesystem>
#include <regex>

namespace opticloud
{

SingleEntryTransfer::SingleEntryTransfer(const cu::Logger &logger, const std::shared_ptr<ConnectionParameters>& connectionParameters)
    : m_logger(logger)
    , m_connectionParameters(connectionParameters)
    , m_transfer(std::make_shared<curl::CurlHttpTransfer>(logger))
{
    m_transfer->setTransferCallback([&](curl::CurlAsyncTransfer *transfer)
    {
        m_messageId = -1;
        if(transfer->asyncResult() != curl::AsyncResult::CURL_DONE)
        {
            switch(transfer->asyncResult())
            {
            case curl::AsyncResult::NONE:
            case curl::AsyncResult::RUNNING:
            case curl::AsyncResult::CURL_DONE:
                m_result = UNKNOWN_ERROR;
                break;
            case curl::AsyncResult::CANCELED:
                m_result = ASYNC_CANCELED;
                break;
            case curl::AsyncResult::TIMEOUT:
                m_result = ASYNC_TIMEOUT;
                break;
            }

            if(m_cloudCallback)
                m_cloudCallback(this);

            return;
        }

        if(transfer->curlResult() != CURLE_OK)
        {
            m_result = CURL_ERROR;
            m_curlResult = transfer->curlResult();

            if(m_cloudCallback)
                m_cloudCallback(this);

            return;
        }

        parseResponse();
    });
}

SingleEntryTransfer::~SingleEntryTransfer()
{
    curl_mime_free(m_multiPartContainer);
}

void SingleEntryTransfer::setProtocolVersion(const ProtocolVersion newProtocolVersion)
{
    m_protocolVersion = newProtocolVersion;
}

void SingleEntryTransfer::setRequestType(RequestType newRequestType)
{
    m_requestType = newRequestType;
}

SingleEntryTransfer::RequestType SingleEntryTransfer::requestType()
{
    return m_requestType;
}

void SingleEntryTransfer::setUploadFilename(const std::string &fileNameWithPath)
{
    m_uploadDataPointer = nullptr;
    m_uploadDataSize = -1;
    m_uploadDataFileName.clear();

    m_uploadFileName = fileNameWithPath;
}

void SingleEntryTransfer::setUploadData(const char *data, long size, const std::string &filename)
{
    m_uploadFileName.clear();

    m_uploadDataPointer = data;
    m_uploadDataSize = size;
    m_uploadDataFileName = filename;
}

std::vector<char> &SingleEntryTransfer::responseData()
{
    return m_transfer->responseData();
}

void SingleEntryTransfer::setOutputFilename(const std::string &fileNameWithPath)
{
    m_outputFileName = fileNameWithPath;
}

std::shared_ptr<curl::CurlAsyncTransfer> SingleEntryTransfer::prepareTransfer()
{
    std::string url = m_connectionParameters->url;
    if(curl::urlReplacePath(url, "status.php", false) == false) // overwrite path only when it does not exist
    {
        std::string errMsg = "bad url";
        m_logger->error(errMsg);
        throw std::invalid_argument(errMsg);
    }

    if(m_connectionParameters->accessToken.empty())
    {
        std::string errMsg = "accessToken is empty";
        m_logger->error(errMsg);
        throw std::invalid_argument(errMsg);
    }

    std::string request;
    switch(m_requestType)
    {
    case PING:                           request = "PING";                           break;
    case ATTENTION:                      request = "ATTN";                           break;
    case UPLOAD:                         request = "UPLOAD";                         break;
    case RETURN_ERROR:                   request = "RETURN_ERROR";                   break;
    case RETURN_SCPI:                    request = "RETURN_SCPI";                    break;
    case RETURN_SCRIPT:                  request = "RETURN_BASH";                    break;
    case RETURN_LIST:                    request = "RETURN_LIST";                    break;
    case RETURN_FILE:                    request = "RETURN_FILE";                    break;
    case ACKNOWLEDGMENT_FIRMWARE_UPDATE: request = "ACKNOWLEDGMENT_FIRMWARE_UPDATE"; break;
    case PROGRESS_FIRMWARE_UPDATE:       request = "PROGRESS_FIRMWARE_UPDATE";       break;
    case RETURN_FIRMWARE_UPDATE:         request = "RETURN_FIRMWARE_UPDATE";         break;
    }

    std::string protocolVersion;
    switch(m_protocolVersion)
    {
    case V4: protocolVersion = "v4"; break;
    case V5: protocolVersion = "v5"; break;
    }

    // Since C++20 the start time of the system_clock is no longer unspecified, instead it is defined as UNIX-epoch (1970-01-01 00:00:00 UTC)
    // See: http://eel.is/c++draft/time.clock.system#overview-1
    const auto secondsSinceUnixEpoch = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::string timestamp = std::to_string(secondsSinceUnixEpoch);

    url += "?F=" + request + "&ID=" + m_connectionParameters->accessToken + "&P=" + protocolVersion + "&T=" + timestamp;
    m_transfer->setUrl(url);
    m_transfer->setFollowRedirects(true);
    m_transfer->setProgressTimeout_s(m_connectionParameters->progressTimeout_s);

    m_transfer->setHeader("Cache-Control", "no-cache, no-store"); // HTTP 1.1
    m_transfer->setHeader("Pragma", "no-cache"); // HTTP 1.0

    m_transfer->setVerifySslCertificates(m_connectionParameters->doVerifySslCertificates);
    m_transfer->setReuseExistingConnection(m_connectionParameters->doReuseExistingConnection);

    if(!m_connectionParameters->hostAlias.empty())
        m_transfer->setHeader("Host", m_connectionParameters->hostAlias);

    if(!m_outputFileName.empty())
        m_transfer->setOutputFilename(m_outputFileName);

    if(m_messageId > 0)
    {
        std::string contentDescription = fmt::format("Message-ID: {}", m_messageId);
        m_messageId = -1; // try to prevent wrong usage

        if((m_requestType == RETURN_FILE) && (!m_returnFileTag.empty()))
        {
            contentDescription.append(fmt::format(", File-Tag: {}", m_returnFileTag));
            m_returnFileTag.clear(); // try to prevent wrong usage
        }

        m_transfer->setHeader("Content-Description", contentDescription);
    }

    curl_mime_free(m_multiPartContainer);
    m_multiPartContainer = curl_mime_init(m_transfer->curl().handle);
    curl_mimepart *multiPartEntry = curl_mime_addpart(m_multiPartContainer);
    curl_mime_name(multiPartEntry, "data");

    std::string mimeType;
    if(!m_uploadFileName.empty())
    {
        // content from file
        mimeType = mimeTypeFromFileExtension(m_uploadFileName);
        curl_mime_filedata(multiPartEntry, m_uploadFileName.c_str());
    }
    else
    {
        // content from memory
        mimeType = mimeTypeFromFileExtension(m_uploadDataFileName);

        if(m_uploadDataSize == -1)
            curl_mime_data(multiPartEntry, m_uploadDataPointer, CURL_ZERO_TERMINATED);
        else
            curl_mime_data(multiPartEntry, m_uploadDataPointer, m_uploadDataSize);

        curl_mime_filename(multiPartEntry, m_uploadDataFileName.c_str());
    }

    curl_mime_type(multiPartEntry, mimeType.c_str());
    curl_easy_setopt(m_transfer->curl().handle, CURLOPT_MIMEPOST, m_multiPartContainer);

    return m_transfer;
}

std::string SingleEntryTransfer::mimeTypeFromFileExtension(const std::string &filenameWithPath)
{
    std::filesystem::path fileToUpload = filenameWithPath;
    std::string extension = fileToUpload.extension();
    if(extension[0] == '.')
        extension.erase(extension.begin());

    std::string mimeType = "image/" + extension;
    return mimeType;
}

void SingleEntryTransfer::parseResponse()
{
    m_httpResponseCode = m_transfer->responseCode();
    if(m_httpResponseCode == 204)
    {
        m_result = RETURN_OK;
    }
    else if(m_httpResponseCode == 200)
    {
        std::string contentType = m_transfer->responseHeader("Content-Type");
        if(contentType.compare("application/om-request-list") == 0)
            m_result = TASK_REQUEST_LIST;
        else if(contentType.compare("application/om-request-file") == 0)
            m_result = TASK_REQUEST_FILE;
        else if(contentType.compare("application/om-bash") == 0)
            m_result = TASK_EXECUTE_SCRIPT;
        else if(contentType.compare("application/om-scpi-dev") == 0)
            m_result = TASK_SCPI_DEVICE_MANAGER;
        else if(contentType.compare("application/om-scpi-app") == 0)
            m_result = TASK_SCPI_APPLICATION;
        else if(contentType.compare("application/om-firmware-update") == 0)
            m_result = TASK_FIRMWARE_UPDATE;
        else
            m_result = UNKNOWN_RESPONSE;

        std::string contentDescription = m_transfer->responseHeader("Content-Description");
        if(!contentDescription.empty())
        {
            auto const regexSplit = std::regex(R"(Message-ID: *([-+]?[[:digit:]]+))", std::regex_constants::icase);
            std::smatch matches;
            if(std::regex_search(contentDescription, matches, regexSplit))
            {
                if(matches.size() == 2)
                     m_messageId = stoi(matches[1].str());
            }
        }
    }
    else
        m_result = UNKNOWN_RESPONSE;

    if(m_cloudCallback)
        m_cloudCallback(this);
}

std::shared_ptr<ConnectionParameters> SingleEntryTransfer::connectionParameters() const
{
    return m_connectionParameters;
}

void SingleEntryTransfer::setConnectionParameters(const std::shared_ptr<ConnectionParameters> &newConnectionParameters)
{
    m_connectionParameters = newConnectionParameters;
}

std::string SingleEntryTransfer::uploadFileName() const
{
    return m_uploadFileName;
}

void SingleEntryTransfer::setReturnFileTag(const std::string &newReturnFileTag)
{
    m_returnFileTag = newReturnFileTag;
}

float SingleEntryTransfer::transferDuration_s() const
{
    return m_transfer->transferDuration_s();
}

uint64_t SingleEntryTransfer::transferSpeed_BytesPerSecond() const
{
    return m_transfer->transferSpeed_BytesPerSecond();
}

uint64_t SingleEntryTransfer::transferredBytes() const
{
    return m_transfer->transferredBytes();
}

int SingleEntryTransfer::messageId() const
{
    return m_messageId;
}

void SingleEntryTransfer::setMessageId(int newMessageId)
{
    m_messageId = newMessageId;
}

long SingleEntryTransfer::httpResponseCode() const
{
    return m_httpResponseCode;
}

CURLcode SingleEntryTransfer::curlResult() const
{
    return m_curlResult;
}

SingleEntryTransfer::TransferResult SingleEntryTransfer::result() const
{
    return m_result;
}

void SingleEntryTransfer::setCloudCallback(const SingleEntryCallback &newCloudCallback)
{
    m_cloudCallback = newCloudCallback;
}

}
