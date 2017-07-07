// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#include "pch.h"
#include "httpClient/types.h"
#include "httpClient/httpClient.h"
#include "singleton.h"
#include "asyncop.h"
#include "http_mock.h"
#include "../httpcall.h"

bool DoesMockCallMatch(_In_ const HC_CALL* mockCall, _In_ const HC_CALL* originalCall)
{
    if (mockCall->url.empty())
    {
        return true;
    }
    else
    {
        if (originalCall->url == mockCall->url)
        {
            if (mockCall->requestBodyString.empty())
            {
                return true;
            }
            else
            {
                if (originalCall->requestBodyString == mockCall->requestBodyString)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

int GetIndexOfMock(const std::vector<HC_CALL*>& mocks, HC_CALL* lastMatchingMock)
{
    if (lastMatchingMock == nullptr)
    {
        return -1;
    }

    for (int i = 0; i < mocks.size(); i++)
    {
        if (mocks[i] == lastMatchingMock)
        {
            return i;
        }
    }

    return -1;
}

HC_CALL* GetMatchingMock(
    _In_ HC_CALL_HANDLE originalCall
    )
{
    std::vector<HC_CALL*> mocks;
    HC_CALL* lastMatchingMock = nullptr;
    HC_CALL* matchingMock = nullptr;

    {
        std::lock_guard<std::mutex> guard(get_http_singleton()->m_mocksLock);
        mocks = get_http_singleton()->m_mocks;
        lastMatchingMock = get_http_singleton()->m_lastMatchingMock;
    }

    // ignore last matching call if it doesn't match the current call
    if (lastMatchingMock != nullptr && !DoesMockCallMatch(lastMatchingMock, originalCall))
    {
        lastMatchingMock = nullptr;
    }

    int lastMockIndex = GetIndexOfMock(mocks, lastMatchingMock);
    if (lastMockIndex == -1)
    {
        // if there was no last matching call, then look through all mocks for first match
        for (auto& mockCall : mocks)
        {
            if (DoesMockCallMatch(mockCall, originalCall))
            {
                matchingMock = mockCall;
                break;
            }
        }
    }
    else
    {
        // if there was last matching call, looking through the rest of the mocks to see if there's more that match
        for (int j = lastMockIndex + 1; j < mocks.size(); j++)
        {
            if (DoesMockCallMatch(mocks[j], originalCall))
            {
                matchingMock = mocks[j];
                break;
            }
        }

        // if last after matches, then just keep returning last match
        if (matchingMock == nullptr)
        {
            matchingMock = lastMatchingMock;
        }
    }

    {
        std::lock_guard<std::mutex> guard(get_http_singleton()->m_mocksLock);
        get_http_singleton()->m_lastMatchingMock = matchingMock;
    }

    return matchingMock;
}

bool Mock_Internal_HCHttpCallPerform(
    _In_ HC_CALL_HANDLE originalCall
    )
{
    HC_CALL* matchingMock = GetMatchingMock(originalCall);
    if (matchingMock == nullptr)
    {
        return false;
    }

    PCSTR_T str;
    HCHttpCallResponseGetResponseString(matchingMock, &str);
    HCHttpCallResponseSetResponseString(originalCall, str);

    uint32_t code;
    HCHttpCallResponseGetStatusCode(matchingMock, &code);
    HCHttpCallResponseSetStatusCode(originalCall, code);

    HCHttpCallResponseGetErrorCode(matchingMock, &code);
    HCHttpCallResponseSetErrorCode(originalCall, code);

    HCHttpCallResponseGetErrorMessage(matchingMock, &str);
    HCHttpCallResponseSetErrorMessage(originalCall, str);

    uint32_t numheaders;
    HCHttpCallResponseGetNumHeaders(matchingMock, &numheaders);

    PCSTR_T str1;
    PCSTR_T str2;
    for (uint32_t i = 0; i < numheaders; i++)
    {
        HCHttpCallResponseGetHeaderAtIndex(matchingMock, i, &str1, &str2);
        HCHttpCallResponseSetHeader(originalCall, str1, str2);
    }
    
    return true;
}
