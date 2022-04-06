/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "transport/SecureSession.h"
#include <app-common/zap-generated/cluster-objects.h>
#include <app/AttributeCache.h>
#include <app/InteractionModelEngine.h>
#include <app/tests/AppTestContext.h>
#include <controller/ReadInteraction.h>
#include <lib/support/ErrorStr.h>
#include <lib/support/UnitTestRegistration.h>
#include <lib/support/logging/CHIPLogging.h>
#include <messaging/tests/MessagingContext.h>
#include <nlunit-test.h>
#include <protocols/interaction_model/Constants.h>

using TestContext = chip::Test::AppContext;

using namespace chip;
using namespace chip::app::Clusters;
using namespace chip::Protocols;

namespace {

constexpr EndpointId kTestEndpointId = 1;
static DataVersion sDataVersion      = 5;

enum ResponseDirective
{
    kSendDataResponse,
    kSendDataError
};

ResponseDirective responseDirective;

// Number of reads of TestCluster::Attributes::Int16u that we have observed.
// Every read will increment this count by 1 and return the new value.
uint16_t totalReadCount = 0;

} // namespace

namespace chip {
namespace app {
CHIP_ERROR ReadSingleClusterData(const Access::SubjectDescriptor & aSubjectDescriptor, bool aIsFabricFiltered,
                                 const ConcreteReadAttributePath & aPath, AttributeReportIBs::Builder & aAttributeReports,
                                 AttributeValueEncoder::AttributeEncodeState * apEncoderState)
{
    if (responseDirective == kSendDataResponse)
    {
        if (aPath.mClusterId == app::Clusters::TestCluster::Id &&
            aPath.mAttributeId == app::Clusters::TestCluster::Attributes::ListFabricScoped::Id)
        {
            AttributeValueEncoder::AttributeEncodeState state =
                (apEncoderState == nullptr ? AttributeValueEncoder::AttributeEncodeState() : *apEncoderState);
            AttributeValueEncoder valueEncoder(aAttributeReports, aSubjectDescriptor.fabricIndex, aPath,
                                               sDataVersion /* data version */, aIsFabricFiltered, state);

            return valueEncoder.EncodeList([aSubjectDescriptor](const auto & encoder) -> CHIP_ERROR {
                app::Clusters::TestCluster::Structs::TestFabricScoped::Type val;
                val.fabricIndex = aSubjectDescriptor.fabricIndex;
                ReturnErrorOnFailure(encoder.Encode(val));
                val.fabricIndex = (val.fabricIndex == 1) ? 2 : 1;
                ReturnErrorOnFailure(encoder.Encode(val));
                return CHIP_NO_ERROR;
            });
        }
        else if (aPath.mClusterId == app::Clusters::TestCluster::Id &&
                 aPath.mAttributeId == app::Clusters::TestCluster::Attributes::Int16u::Id)
        {
            AttributeValueEncoder::AttributeEncodeState state =
                (apEncoderState == nullptr ? AttributeValueEncoder::AttributeEncodeState() : *apEncoderState);
            AttributeValueEncoder valueEncoder(aAttributeReports, aSubjectDescriptor.fabricIndex, aPath,
                                               sDataVersion /* data version */, aIsFabricFiltered, state);

            return valueEncoder.Encode(++totalReadCount);
        }
        else
        {
            AttributeReportIB::Builder & attributeReport = aAttributeReports.CreateAttributeReport();
            ReturnErrorOnFailure(aAttributeReports.GetError());
            AttributeDataIB::Builder & attributeData = attributeReport.CreateAttributeData();
            ReturnErrorOnFailure(attributeReport.GetError());
            TestCluster::Attributes::ListStructOctetString::TypeInfo::Type value;
            TestCluster::Structs::TestListStructOctet::Type valueBuf[4];

            value = valueBuf;

            uint8_t i = 0;
            for (auto & item : valueBuf)
            {
                item.fabricIndex = i;
                i++;
            }

            attributeData.DataVersion(sDataVersion);
            ReturnErrorOnFailure(attributeData.GetError());
            AttributePathIB::Builder & attributePath = attributeData.CreatePath();
            attributePath.Endpoint(aPath.mEndpointId)
                .Cluster(aPath.mClusterId)
                .Attribute(aPath.mAttributeId)
                .EndOfAttributePathIB();
            ReturnErrorOnFailure(attributePath.GetError());

            ReturnErrorOnFailure(DataModel::Encode(*(attributeData.GetWriter()),
                                                   TLV::ContextTag(to_underlying(AttributeDataIB::Tag::kData)), value));
            ReturnErrorOnFailure(attributeData.EndOfAttributeDataIB().GetError());
            return attributeReport.EndOfAttributeReportIB().GetError();
        }
    }
    else
    {
        AttributeReportIB::Builder & attributeReport = aAttributeReports.CreateAttributeReport();
        ReturnErrorOnFailure(aAttributeReports.GetError());
        AttributeStatusIB::Builder & attributeStatus = attributeReport.CreateAttributeStatus();
        AttributePathIB::Builder & attributePath     = attributeStatus.CreatePath();
        attributePath.Endpoint(aPath.mEndpointId).Cluster(aPath.mClusterId).Attribute(aPath.mAttributeId).EndOfAttributePathIB();
        ReturnErrorOnFailure(attributePath.GetError());

        StatusIB::Builder & errorStatus = attributeStatus.CreateErrorStatus();
        ReturnErrorOnFailure(attributeStatus.GetError());
        errorStatus.EncodeStatusIB(StatusIB(Protocols::InteractionModel::Status::Busy));
        attributeStatus.EndOfAttributeStatusIB();
        ReturnErrorOnFailure(attributeStatus.GetError());
        return attributeReport.EndOfAttributeReportIB().GetError();
    }

    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}

bool IsClusterDataVersionEqual(const ConcreteClusterPath & aConcreteClusterPath, DataVersion aRequiredDataVersion)
{
    if (aRequiredDataVersion == sDataVersion)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool IsDeviceTypeOnEndpoint(DeviceTypeId deviceType, EndpointId endpoint)
{
    return false;
}
} // namespace app
} // namespace chip

namespace {

class TestReadInteraction : public app::ReadHandler::ApplicationCallback
{
public:
    TestReadInteraction() {}

    static void TestReadAttributeResponse(nlTestSuite * apSuite, void * apContext);
    static void TestReadDataVersionFilter(nlTestSuite * apSuite, void * apContext);
    static void TestReadAttributeResponseWithCache(nlTestSuite * apSuite, void * apContext);
    static void TestReadAttributeResponseWithCachePartialRollback(nlTestSuite * apSuite, void * apContext);
    static void TestReadAttributeResponseWithCacheRollback(nlTestSuite * apSuite, void * apContext);
    static void TestSubscribeAttributeResponseWithCache(nlTestSuite * apSuite, void * apContext);
    static void TestReadAttributeError(nlTestSuite * apSuite, void * apContext);
    static void TestReadAttributeTimeout(nlTestSuite * apSuite, void * apContext);
    static void TestReadEventResponse(nlTestSuite * apSuite, void * apContext);
    static void TestReadFabricScopedWithoutFabricFilter(nlTestSuite * apSuite, void * apContext);
    static void TestReadFabricScopedWithFabricFilter(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_MultipleSubscriptions(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_SubscriptionAppRejection(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_MultipleReads(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_OneSubscribeMultipleReads(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_TwoSubscribesMultipleReads(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_MultipleSubscriptionsWithDataVersionFilter(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandler_SubscriptionAlteredReportingIntervals(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandlerResourceExhaustion_MultipleSubscriptions(nlTestSuite * apSuite, void * apContext);
    static void TestReadHandlerResourceExhaustion_MultipleReads(nlTestSuite * apSuite, void * apContext);

private:
    static constexpr uint16_t kTestMinInterval = 33;
    static constexpr uint16_t kTestMaxInterval = 66;

    CHIP_ERROR OnSubscriptionRequested(app::ReadHandler & aReadHandler, Transport::SecureSession & aSecureSession)
    {
        VerifyOrReturnError(!mEmitSubscriptionError, CHIP_ERROR_INVALID_ARGUMENT);

        if (mAlterSubscriptionIntervals)
        {
            ReturnErrorOnFailure(aReadHandler.SetReportingIntervals(kTestMinInterval, kTestMaxInterval));
        }

        return CHIP_NO_ERROR;
    }

    void OnSubscriptionEstablished(app::ReadHandler & aReadHandler) { mNumActiveSubscriptions++; }

    void OnSubscriptionTerminated(app::ReadHandler & aReadHandler) { mNumActiveSubscriptions--; }

    // Issue the given number of reads in parallel and wait for them all to
    // succeed.
    static void MultipleReadHelper(nlTestSuite * apSuite, TestContext & aCtx, size_t aReadCount);

    // Establish the given number of subscriptions, then issue the given number
    // of reads in parallel and wait for them all to succeed.
    static void SubscribeThenReadHelper(nlTestSuite * apSuite, TestContext & aCtx, size_t aSubscribeCount, size_t aReadCount);

private:
    bool mEmitSubscriptionError      = false;
    int32_t mNumActiveSubscriptions  = 0;
    bool mAlterSubscriptionIntervals = false;
};

TestReadInteraction gTestReadInteraction;

class MockInteractionModelApp : public chip::app::AttributeCache::Callback
{
public:
    void OnEventData(const chip::app::EventHeader & aEventHeader, chip::TLV::TLVReader * apData,
                     const chip::app::StatusIB * apStatus) override
    {}

    void OnAttributeData(const chip::app::ConcreteDataAttributePath & aPath, chip::TLV::TLVReader * apData,
                         const chip::app::StatusIB & status) override
    {
        if (status.mStatus == chip::Protocols::InteractionModel::Status::Success)
        {
            mNumAttributeResponse++;
            mGotReport = true;
        }
    }

    void OnError(CHIP_ERROR aError) override
    {
        mError     = aError;
        mReadError = true;
    }

    void OnDone() override {}

    void OnDeallocatePaths(chip::app::ReadPrepareParams && aReadPrepareParams) override
    {
        if (aReadPrepareParams.mpAttributePathParamsList != nullptr)
        {
            delete[] aReadPrepareParams.mpAttributePathParamsList;
        }

        if (aReadPrepareParams.mpEventPathParamsList != nullptr)
        {
            delete[] aReadPrepareParams.mpEventPathParamsList;
        }

        if (aReadPrepareParams.mpDataVersionFilterList != nullptr)
        {
            delete[] aReadPrepareParams.mpDataVersionFilterList;
        }
    }

    int mNumAttributeResponse = 0;
    bool mGotReport           = false;
    bool mReadError           = false;
    CHIP_ERROR mError         = CHIP_NO_ERROR;
};
void TestReadInteraction::TestReadAttributeResponse(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath,
                                                      const auto & dataResponse) {
        uint8_t i = 0;
        NL_TEST_ASSERT(apSuite, attributePath.mDataVersion.HasValue() && attributePath.mDataVersion.Value() == sDataVersion);
        auto iter = dataResponse.begin();
        while (iter.Next())
        {
            auto & item = iter.GetValue();
            NL_TEST_ASSERT(apSuite, item.fabricIndex == i);
            i++;
        }
        NL_TEST_ASSERT(apSuite, i == 4);
        NL_TEST_ASSERT(apSuite, iter.GetStatus() == CHIP_NO_ERROR);
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(&ctx.GetExchangeManager(), sessionHandle,
                                                                                        kTestEndpointId, onSuccessCb, onFailureCb);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, onSuccessCbInvoked && !onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadDataVersionFilter(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath,
                                                      const auto & dataResponse) {
        uint8_t i = 0;
        auto iter = dataResponse.begin();
        while (iter.Next())
        {
            auto & item = iter.GetValue();
            NL_TEST_ASSERT(apSuite, item.fabricIndex == i);
            i++;
        }
        NL_TEST_ASSERT(apSuite, i == 4);
        NL_TEST_ASSERT(apSuite, iter.GetStatus() == CHIP_NO_ERROR);
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        onFailureCbInvoked = true;
    };

    Optional<DataVersion> dataVersion(sDataVersion);
    Controller::ReadAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
        &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, true, dataVersion);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, !onSuccessCbInvoked && !onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadAttributeResponseWithCache(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx  = *static_cast<TestContext *>(apContext);
    auto sessionHandle = ctx.GetSessionBobToAlice();

    responseDirective = kSendDataResponse;

    MockInteractionModelApp delegate;
    chip::app::AttributeCache cache(delegate);
    app::InteractionModelEngine * engine = app::InteractionModelEngine::GetInstance();
    app::ReadPrepareParams readParams(sessionHandle);

    app::AttributePathParams readPaths[3];
    readPaths[0] = app::AttributePathParams(kTestEndpointId, app::Clusters::TestCluster::Id,
                                            TestCluster::Attributes::ListStructOctetString::Id);
    readPaths[1] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::VendorID::Id);
    readPaths[2] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::ProductID::Id);
    readParams.mpAttributePathParamsList    = readPaths;
    readParams.mAttributePathParamsListSize = 3;
    app::ReadClient readClient(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                               chip::app::ReadClient::InteractionType::Read);
    CHIP_ERROR err = readClient.SendRequest(readParams);
    NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 3);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    delegate.mNumAttributeResponse = 0;
    app::ReadClient readClient1(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                                chip::app::ReadClient::InteractionType::Read);
    err = readClient1.SendRequest(readParams);
    NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 0);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadHandlers() == 0);

    // Validate the different data version filter for different attribute with same cluster, that cluster's data version filter
    // would not be created.
    sDataVersion += 1;
    app::ReadClient readClient2(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                                chip::app::ReadClient::InteractionType::Read);
    app::AttributePathParams readPaths2[3];

    readPaths2[0] = app::AttributePathParams(kTestEndpointId, app::Clusters::TestCluster::Id, TestCluster::Attributes::Int16u::Id);
    readPaths2[1] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::VendorID::Id);
    readPaths2[2] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::ProductID::Id);
    readParams.mpAttributePathParamsList    = readPaths2;
    readParams.mAttributePathParamsListSize = 3;

    err = readClient2.SendRequest(readParams);
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 3);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    delegate.mNumAttributeResponse = 0;

    app::ReadClient readClient3(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                                chip::app::ReadClient::InteractionType::Read);
    err = readClient3.SendRequest(readParams);
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 1);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    sDataVersion -= 1;
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadAttributeResponseWithCachePartialRollback(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx  = *static_cast<TestContext *>(apContext);
    auto sessionHandle = ctx.GetSessionBobToAlice();

    responseDirective = kSendDataResponse;

    MockInteractionModelApp delegate;
    chip::app::AttributeCache cache(delegate);
    app::InteractionModelEngine * engine = app::InteractionModelEngine::GetInstance();
    app::ReadPrepareParams readParams(sessionHandle);

    app::AttributePathParams readPaths[3];
    readPaths[0] = app::AttributePathParams(kTestEndpointId, app::Clusters::TestCluster::Id,
                                            TestCluster::Attributes::ListStructOctetString::Id);
    readPaths[1] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::VendorID::Id);
    readPaths[2] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::ProductID::Id);
    readParams.mpAttributePathParamsList    = readPaths;
    readParams.mAttributePathParamsListSize = 3;
    app::ReadClient readClient(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                               chip::app::ReadClient::InteractionType::Read);
    CHIP_ERROR err = readClient.SendRequest(readParams);
    NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 3);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    delegate.mNumAttributeResponse = 0;
    app::ReadClient readClient1(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                                chip::app::ReadClient::InteractionType::Read);
    chip::app::EventPathParams eventPathParams[100];
    readParams.mpEventPathParamsList = eventPathParams;

    readParams.mEventPathParamsListSize = 78;
    for (uint32_t index = 0; index < readParams.mEventPathParamsListSize; index++)
    {
        readParams.mpEventPathParamsList[index].mEndpointId = kTestEndpointId;
        readParams.mpEventPathParamsList[index].mClusterId  = app::Clusters::TestCluster::Id;
        readParams.mpEventPathParamsList[index].mEventId    = 0;
    }
    err = readClient1.SendRequest(readParams);
    NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 1);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadAttributeResponseWithCacheRollback(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx  = *static_cast<TestContext *>(apContext);
    auto sessionHandle = ctx.GetSessionBobToAlice();

    responseDirective = kSendDataResponse;

    MockInteractionModelApp delegate;
    chip::app::AttributeCache cache(delegate);
    app::InteractionModelEngine * engine = app::InteractionModelEngine::GetInstance();
    app::ReadPrepareParams readParams(sessionHandle);

    app::AttributePathParams readPaths[3];
    readPaths[0] = app::AttributePathParams(kTestEndpointId, app::Clusters::TestCluster::Id,
                                            TestCluster::Attributes::ListStructOctetString::Id);
    readPaths[1] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::VendorID::Id);
    readPaths[2] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::ProductID::Id);
    readParams.mpAttributePathParamsList    = readPaths;
    readParams.mAttributePathParamsListSize = 3;
    app::ReadClient readClient(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                               chip::app::ReadClient::InteractionType::Read);
    CHIP_ERROR err = readClient.SendRequest(readParams);
    NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 3);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    delegate.mNumAttributeResponse = 0;
    app::ReadClient readClient1(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                                chip::app::ReadClient::InteractionType::Read);
    chip::app::EventPathParams eventPathParams[100];
    readParams.mpEventPathParamsList = eventPathParams;

    readParams.mEventPathParamsListSize = 79;
    for (uint32_t index = 0; index < readParams.mEventPathParamsListSize; index++)
    {
        readParams.mpEventPathParamsList[index].mEndpointId = kTestEndpointId;
        readParams.mpEventPathParamsList[index].mClusterId  = app::Clusters::TestCluster::Id;
        readParams.mpEventPathParamsList[index].mEventId    = 0;
    }
    err = readClient1.SendRequest(readParams);
    NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 3);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestSubscribeAttributeResponseWithCache(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx  = *static_cast<TestContext *>(apContext);
    auto sessionHandle = ctx.GetSessionBobToAlice();

    responseDirective = kSendDataResponse;

    MockInteractionModelApp delegate;
    chip::app::AttributeCache cache(delegate);
    app::InteractionModelEngine * engine = app::InteractionModelEngine::GetInstance();
    app::ReadPrepareParams readParams(sessionHandle);

    app::AttributePathParams readPaths[3];
    readPaths[0] = app::AttributePathParams(kTestEndpointId, app::Clusters::TestCluster::Id,
                                            TestCluster::Attributes::ListStructOctetString::Id);
    readPaths[1] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::VendorID::Id);
    readPaths[2] = app::AttributePathParams(kTestEndpointId, app::Clusters::Basic::Id, Basic::Attributes::ProductID::Id);
    readParams.mpAttributePathParamsList    = readPaths;
    readParams.mAttributePathParamsListSize = 3;
    readParams.mMinIntervalFloorSeconds     = 2;
    readParams.mMaxIntervalCeilingSeconds   = 5;
    readParams.mKeepSubscriptions           = true;
    app::ReadClient readClient(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                               chip::app::ReadClient::InteractionType::Subscribe);
    CHIP_ERROR err = readClient.SendRequest(readParams);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Controller, "Failed to send read request for testing clusters");
    }
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 3);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    delegate.mNumAttributeResponse = 0;
    app::ReadClient readClient1(engine, &ctx.GetExchangeManager(), cache.GetBufferedCallback(),
                                chip::app::ReadClient::InteractionType::Subscribe);
    err = readClient1.SendRequest(readParams);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(Controller, "Failed to send read request for testing clusters");
    }
    ctx.DrainAndServiceIO();
    NL_TEST_ASSERT(apSuite, delegate.mNumAttributeResponse == 0);
    NL_TEST_ASSERT(apSuite, !delegate.mReadError);
    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadClients() == 2);
    NL_TEST_ASSERT(apSuite, engine->GetNumActiveReadHandlers() == 2);
    engine->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 0);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    engine->UnregisterReadHandlerAppCallback();
}

void TestReadInteraction::TestReadEventResponse(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &onSuccessCbInvoked](const app::EventHeader & eventHeader, const auto & EventResponse) {
        // TODO: Need to add check when IM event server integration completes
        IgnoreUnusedVariable(apSuite);
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked](const app::EventHeader * eventHeader, CHIP_ERROR aError) {
        onFailureCbInvoked = true;
    };

    Controller::ReadEvent<TestCluster::Events::TestEvent::DecodableType>(&ctx.GetExchangeManager(), sessionHandle, kTestEndpointId,
                                                                         onSuccessCb, onFailureCb);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, !onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadAttributeError(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataError;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked, apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        NL_TEST_ASSERT(apSuite, aError.IsIMStatus() && app::StatusIB(aError).mStatus == Protocols::InteractionModel::Status::Busy);
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(&ctx.GetExchangeManager(), sessionHandle,
                                                                                        kTestEndpointId, onSuccessCb, onFailureCb);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, !onSuccessCbInvoked && onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadAttributeTimeout(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataError;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked, apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        NL_TEST_ASSERT(apSuite, aError == CHIP_ERROR_TIMEOUT);
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(&ctx.GetExchangeManager(), sessionHandle,
                                                                                        kTestEndpointId, onSuccessCb, onFailureCb);

    ctx.ExpireSessionAliceToBob();

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 1);

    ctx.ExpireSessionBobToAlice();

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, !onSuccessCbInvoked && onFailureCbInvoked);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);

    //
    // Let's put back the sessions so that the next tests (which assume a valid initialized set of sessions)
    // can function correctly.
    //
    ctx.CreateSessionAliceToBob();
    ctx.CreateSessionBobToAlice();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadHandler_MultipleSubscriptions(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx                        = *static_cast<TestContext *>(apContext);
    auto sessionHandle                       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        //
        // We shouldn't be encountering any failures in this test.
        //
        NL_TEST_ASSERT(apSuite, false);
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
    };

    //
    // Test the application callback as well to ensure we get the right number of SubscriptionEstablishment/Termination
    // callbacks.
    //
    app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&gTestReadInteraction);

    //
    // Try to issue parallel subscriptions that will exceed the value for CHIP_IM_MAX_NUM_READ_HANDLER.
    // If heap allocation is correctly setup, this should result in it successfully servicing more than the number
    // present in that define.
    //
    for (int i = 0; i < (CHIP_IM_MAX_NUM_READ_HANDLER + 1); i++)
    {
        NL_TEST_ASSERT(apSuite,
                       Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                           &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                           onSubscriptionEstablishedCb, false, true) == CHIP_NO_ERROR);
    }

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));
    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 0);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->UnregisterReadHandlerAppCallback();
}

void TestReadInteraction::TestReadHandler_SubscriptionAppRejection(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx                        = *static_cast<TestContext *>(apContext);
    auto sessionHandle                       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numFailureCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&numFailureCalls](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        numFailureCalls++;
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
    };

    //
    // Test the application callback as well to ensure we get the right number of SubscriptionEstablishment/Termination
    // callbacks.
    //
    app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&gTestReadInteraction);

    //
    // Test the application rejecting subscriptions.
    //
    gTestReadInteraction.mEmitSubscriptionError = true;

    NL_TEST_ASSERT(apSuite,
                   Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                       &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                       onSubscriptionEstablishedCb, false, true) == CHIP_NO_ERROR);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == 0);

    //
    // Failures won't get routed to us here since re-subscriptions are enabled by default in the Controller::SubscribeAttribute
    // implementation.
    //
    NL_TEST_ASSERT(apSuite, numFailureCalls == 0);
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == 0);
    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 0);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 0);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->UnregisterReadHandlerAppCallback();
    gTestReadInteraction.mEmitSubscriptionError = false;
}

void TestReadInteraction::TestReadHandler_SubscriptionAlteredReportingIntervals(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx                        = *static_cast<TestContext *>(apContext);
    auto sessionHandle                       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numFailureCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&numFailureCalls](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        numFailureCalls++;
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls, &apSuite](const app::ReadClient & readClient) {
        uint16_t minInterval = 0, maxInterval = 0;

        CHIP_ERROR err = readClient.GetReportingIntervals(minInterval, maxInterval);

        NL_TEST_ASSERT(apSuite, err == CHIP_NO_ERROR);

        NL_TEST_ASSERT(apSuite, minInterval == kTestMinInterval);
        NL_TEST_ASSERT(apSuite, maxInterval == kTestMaxInterval);

        numSubscriptionEstablishedCalls++;
    };

    //
    // Test the application callback as well to ensure we get the right number of SubscriptionEstablishment/Termination
    // callbacks.
    //
    app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&gTestReadInteraction);

    //
    // Test the server-side application altering the subscription intervals.
    //
    gTestReadInteraction.mAlterSubscriptionIntervals = true;

    NL_TEST_ASSERT(apSuite,
                   Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                       &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                       onSubscriptionEstablishedCb, false, true) == CHIP_NO_ERROR);

    ctx.DrainAndServiceIO();

    //
    // Failures won't get routed to us here since re-subscriptions are enabled by default in the Controller::SubscribeAttribute
    // implementation.
    //
    NL_TEST_ASSERT(apSuite, numSuccessCalls != 0);
    NL_TEST_ASSERT(apSuite, numFailureCalls == 0);
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == 1);
    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 1);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, gTestReadInteraction.mNumActiveSubscriptions == 0);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->UnregisterReadHandlerAppCallback();
    gTestReadInteraction.mAlterSubscriptionIntervals = false;
}

void TestReadInteraction::TestReadHandler_MultipleReads(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx = *static_cast<TestContext *>(apContext);

    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT <= CHIP_IM_MAX_NUM_READ_HANDLER,
                  "How can we have more reports in flight than read handlers?");

    MultipleReadHelper(apSuite, ctx, CHIP_IM_MAX_REPORTS_IN_FLIGHT);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();
}

void TestReadInteraction::TestReadHandler_OneSubscribeMultipleReads(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx = *static_cast<TestContext *>(apContext);

    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT <= CHIP_IM_MAX_NUM_READ_HANDLER,
                  "How can we have more reports in flight than read handlers?");
    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT > 1, "We won't do any reads");

    SubscribeThenReadHelper(apSuite, ctx, 1, CHIP_IM_MAX_REPORTS_IN_FLIGHT - 1);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();
}

void TestReadInteraction::TestReadHandler_TwoSubscribesMultipleReads(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx = *static_cast<TestContext *>(apContext);

    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT <= CHIP_IM_MAX_NUM_READ_HANDLER,
                  "How can we have more reports in flight than read handlers?");
    static_assert(CHIP_IM_MAX_REPORTS_IN_FLIGHT > 2, "We won't do any reads");

    SubscribeThenReadHelper(apSuite, ctx, 2, CHIP_IM_MAX_REPORTS_IN_FLIGHT - 2);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();
}

void TestReadInteraction::SubscribeThenReadHelper(nlTestSuite * apSuite, TestContext & aCtx, size_t aSubscribeCount,
                                                  size_t aReadCount)
{
    auto sessionHandle                       = aCtx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        //
        // We shouldn't be encountering any failures in this test.
        //
        NL_TEST_ASSERT(apSuite, false);
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls, &apSuite, &aCtx, aSubscribeCount,
                                        aReadCount](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
        if (numSubscriptionEstablishedCalls == aSubscribeCount)
        {
            MultipleReadHelper(apSuite, aCtx, aReadCount);
        }
    };

    for (size_t i = 0; i < aSubscribeCount; ++i)
    {
        NL_TEST_ASSERT(apSuite,
                       Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                           &aCtx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                           onSubscriptionEstablishedCb, false, true) == CHIP_NO_ERROR);
    }

    aCtx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == aSubscribeCount);
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == aSubscribeCount);
}

void TestReadInteraction::MultipleReadHelper(nlTestSuite * apSuite, TestContext & aCtx, size_t aReadCount)
{
    auto sessionHandle       = aCtx.GetSessionBobToAlice();
    uint32_t numSuccessCalls = 0;
    uint32_t numFailureCalls = 0;

    responseDirective = kSendDataResponse;

    uint16_t firstExpectedResponse = totalReadCount + 1;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite, &numFailureCalls](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        numFailureCalls++;

        NL_TEST_ASSERT(apSuite, attributePath == nullptr);
    };

    for (size_t i = 0; i < aReadCount; ++i)
    {
        // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise,
        // it's not safe to do so.
        auto onSuccessCb = [&numSuccessCalls, &apSuite, firstExpectedResponse,
                            i](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
            NL_TEST_ASSERT(apSuite, dataResponse == firstExpectedResponse + i);
            numSuccessCalls++;
        };

        NL_TEST_ASSERT(apSuite,
                       Controller::ReadAttribute<TestCluster::Attributes::Int16u::TypeInfo>(
                           &aCtx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb) == CHIP_NO_ERROR);
    }

    aCtx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == aReadCount);
    NL_TEST_ASSERT(apSuite, numFailureCalls == 0);
}

void TestReadInteraction::TestReadHandler_MultipleSubscriptionsWithDataVersionFilter(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx                        = *static_cast<TestContext *>(apContext);
    auto sessionHandle                       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &numSuccessCalls](const app::ConcreteDataAttributePath & attributePath,
                                                   const auto & dataResponse) {
        NL_TEST_ASSERT(apSuite, attributePath.mDataVersion.HasValue() && attributePath.mDataVersion.Value() == sDataVersion);
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        //
        // We shouldn't be encountering any failures in this test.
        //
        NL_TEST_ASSERT(apSuite, false);
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
    };

    //
    // Try to issue parallel subscriptions that will exceed the value for CHIP_IM_MAX_NUM_READ_HANDLER.
    // If heap allocation is correctly setup, this should result in it successfully servicing more than the number
    // present in that define.
    //
    chip::Optional<chip::DataVersion> dataVersion(1);
    for (int i = 0; i < (CHIP_IM_MAX_NUM_READ_HANDLER + 1); i++)
    {
        NL_TEST_ASSERT(apSuite,
                       Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                           &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                           onSubscriptionEstablishedCb, false, true, dataVersion) == CHIP_NO_ERROR);
    }

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == (CHIP_IM_MAX_NUM_READ_HANDLER + 1));

    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadHandlerResourceExhaustion_MultipleSubscriptions(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx                        = *static_cast<TestContext *>(apContext);
    auto sessionHandle                       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls                 = 0;
    uint32_t numFailureCalls                 = 0;
    uint32_t numSubscriptionEstablishedCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite, &numFailureCalls](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        numFailureCalls++;

        NL_TEST_ASSERT(apSuite, aError == CHIP_IM_GLOBAL_STATUS(ResourceExhausted));
        NL_TEST_ASSERT(apSuite, attributePath == nullptr);
    };

    auto onSubscriptionEstablishedCb = [&numSubscriptionEstablishedCalls](const app::ReadClient & readClient) {
        numSubscriptionEstablishedCalls++;
    };

    //
    // Artifically limit the capacity to 2 ReadHandlers. This will also validate reservation of handlers for Reads,
    // since the second subscription below should fail correctly.
    //
    app::InteractionModelEngine::GetInstance()->SetHandlerCapacity(2);
    NL_TEST_ASSERT(apSuite,
                   Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                       &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                       onSubscriptionEstablishedCb, false, true) == CHIP_NO_ERROR);

    NL_TEST_ASSERT(apSuite,
                   Controller::SubscribeAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                       &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, 0, 10,
                       onSubscriptionEstablishedCb, false, true) == CHIP_NO_ERROR);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == 1);
    NL_TEST_ASSERT(apSuite, numSubscriptionEstablishedCalls == 1);
    // Resubscription is happening for second subscribe call
    NL_TEST_ASSERT(apSuite, numFailureCalls == 0);

    app::InteractionModelEngine::GetInstance()->SetHandlerCapacity(-1);
    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadHandlerResourceExhaustion_MultipleReads(nlTestSuite * apSuite, void * apContext)
{
    TestContext & ctx        = *static_cast<TestContext *>(apContext);
    auto sessionHandle       = ctx.GetSessionBobToAlice();
    uint32_t numSuccessCalls = 0;
    uint32_t numFailureCalls = 0;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [&numSuccessCalls](const app::ConcreteDataAttributePath & attributePath, const auto & dataResponse) {
        numSuccessCalls++;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&apSuite, &numFailureCalls](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        numFailureCalls++;

        NL_TEST_ASSERT(apSuite, aError == CHIP_IM_GLOBAL_STATUS(ResourceExhausted));
        NL_TEST_ASSERT(apSuite, attributePath == nullptr);
    };

    app::InteractionModelEngine::GetInstance()->SetHandlerCapacity(0);

    NL_TEST_ASSERT(apSuite,
                   Controller::ReadAttribute<TestCluster::Attributes::ListStructOctetString::TypeInfo>(
                       &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb) == CHIP_NO_ERROR);

    ctx.DrainAndServiceIO();

    app::InteractionModelEngine::GetInstance()->SetHandlerCapacity(-1);
    app::InteractionModelEngine::GetInstance()->ShutdownActiveReads();

    NL_TEST_ASSERT(apSuite, numSuccessCalls == 0);
    NL_TEST_ASSERT(apSuite, numFailureCalls == 1);

    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadFabricScopedWithoutFabricFilter(nlTestSuite * apSuite, void * apContext)
{
    /**
     *  TODO: we cannot implement the e2e read tests w/ fabric filter since the test session has only one session, and the
     * ReadSingleClusterData is not the one in real applications. We should be able to move some logic out of the ember library and
     * make it possible to have more fabrics in test setup so we can have a better test coverage.
     *
     *  NOTE: Based on the TODO above, the test is testing two separate logics:
     *   - When a fabric filtered read request is received, the server is able to pass the required fabric index to the response
     * encoder.
     *   - When a fabric filtered read request is received, the response encoder is able to encode the attribute correctly.
     */
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath,
                                                      const auto & dataResponse) {
        size_t len = 0;

        NL_TEST_ASSERT(apSuite, dataResponse.ComputeSize(&len) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, len > 1);

        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListFabricScoped::TypeInfo>(
        &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, false /* fabric filtered */);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, onSuccessCbInvoked && !onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

void TestReadInteraction::TestReadFabricScopedWithFabricFilter(nlTestSuite * apSuite, void * apContext)
{
    /**
     *  TODO: we cannot implement the e2e read tests w/ fabric filter since the test session has only one session, and the
     * ReadSingleClusterData is not the one in real applications. We should be able to move some logic out of the ember library and
     * make it possible to have more fabrics in test setup so we can have a better test coverage.
     *
     *  NOTE: Based on the TODO above, the test is testing two separate logics:
     *   - When a fabric filtered read request is received, the server is able to pass the required fabric index to the response
     * encoder.
     *   - When a fabric filtered read request is received, the response encoder is able to encode the attribute correctly.
     */
    TestContext & ctx       = *static_cast<TestContext *>(apContext);
    auto sessionHandle      = ctx.GetSessionBobToAlice();
    bool onSuccessCbInvoked = false, onFailureCbInvoked = false;

    responseDirective = kSendDataResponse;

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onSuccessCb = [apSuite, &onSuccessCbInvoked](const app::ConcreteDataAttributePath & attributePath,
                                                      const auto & dataResponse) {
        size_t len = 0;

        NL_TEST_ASSERT(apSuite, dataResponse.ComputeSize(&len) == CHIP_NO_ERROR);
        NL_TEST_ASSERT(apSuite, len == 1);

        // TODO: Uncomment the following code after we have fabric support in unit tests.
        /*
        auto iter = dataResponse.begin();
        if (iter.Next())
        {
            auto & item = iter.GetValue();
            NL_TEST_ASSERT(apSuite, item.fabricIndex == 1);
        }
        */
        onSuccessCbInvoked = true;
    };

    // Passing of stack variables by reference is only safe because of synchronous completion of the interaction. Otherwise, it's
    // not safe to do so.
    auto onFailureCb = [&onFailureCbInvoked](const app::ConcreteDataAttributePath * attributePath, CHIP_ERROR aError) {
        onFailureCbInvoked = true;
    };

    Controller::ReadAttribute<TestCluster::Attributes::ListFabricScoped::TypeInfo>(
        &ctx.GetExchangeManager(), sessionHandle, kTestEndpointId, onSuccessCb, onFailureCb, true /* fabric filtered */);

    ctx.DrainAndServiceIO();

    NL_TEST_ASSERT(apSuite, onSuccessCbInvoked && !onFailureCbInvoked);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadClients() == 0);
    NL_TEST_ASSERT(apSuite, app::InteractionModelEngine::GetInstance()->GetNumActiveReadHandlers() == 0);
    NL_TEST_ASSERT(apSuite, ctx.GetExchangeManager().GetNumActiveExchanges() == 0);
}

// clang-format off
const nlTest sTests[] =
{
    NL_TEST_DEF("TestReadAttributeResponse", TestReadInteraction::TestReadAttributeResponse),
    NL_TEST_DEF("TestReadDataVersionFilter", TestReadInteraction::TestReadDataVersionFilter),
    NL_TEST_DEF("TestReadEventResponse", TestReadInteraction::TestReadEventResponse),
    NL_TEST_DEF("TestReadAttributeError", TestReadInteraction::TestReadAttributeError),
    NL_TEST_DEF("TestReadFabricScopedWithoutFabricFilter", TestReadInteraction::TestReadFabricScopedWithoutFabricFilter),
    NL_TEST_DEF("TestReadFabricScopedWithFabricFilter", TestReadInteraction::TestReadFabricScopedWithFabricFilter),
    NL_TEST_DEF("TestReadHandler_MultipleSubscriptions", TestReadInteraction::TestReadHandler_MultipleSubscriptions),
    NL_TEST_DEF("TestReadHandler_SubscriptionAppRejection", TestReadInteraction::TestReadHandler_SubscriptionAppRejection),
    NL_TEST_DEF("TestReadHandler_MultipleSubscriptionsWithDataVersionFilter", TestReadInteraction::TestReadHandler_MultipleSubscriptionsWithDataVersionFilter),
    NL_TEST_DEF("TestReadHandler_MultipleReads", TestReadInteraction::TestReadHandler_MultipleReads),
    NL_TEST_DEF("TestReadHandler_OneSubscribeMultipleReads", TestReadInteraction::TestReadHandler_OneSubscribeMultipleReads),
    NL_TEST_DEF("TestReadHandler_TwoSubscribesMultipleReads", TestReadInteraction::TestReadHandler_TwoSubscribesMultipleReads),
    NL_TEST_DEF("TestReadHandlerResourceExhaustion_MultipleSubscriptions", TestReadInteraction::TestReadHandlerResourceExhaustion_MultipleSubscriptions),
    NL_TEST_DEF("TestReadHandlerResourceExhaustion_MultipleReads", TestReadInteraction::TestReadHandlerResourceExhaustion_MultipleReads),
    NL_TEST_DEF("TestReadAttributeTimeout", TestReadInteraction::TestReadAttributeTimeout),
    NL_TEST_DEF("TestReadHandler_SubscriptionAlteredReportingIntervals", TestReadInteraction::TestReadHandler_SubscriptionAlteredReportingIntervals),
    NL_TEST_DEF("TestReadAttributeResponseWithCache", TestReadInteraction::TestReadAttributeResponseWithCache),
    NL_TEST_DEF("TestReadAttributeResponseWithCachePartialRollback", TestReadInteraction::TestReadAttributeResponseWithCachePartialRollback),
    NL_TEST_DEF("TestReadAttributeResponseWithCacheRollback", TestReadInteraction::TestReadAttributeResponseWithCacheRollback),
    NL_TEST_DEF("TestSubscribeAttributeResponseWithCache", TestReadInteraction::TestSubscribeAttributeResponseWithCache),
    NL_TEST_SENTINEL()
};
// clang-format on

// clang-format off
nlTestSuite sSuite =
{
    "TestRead",
    &sTests[0],
    TestContext::InitializeAsync,
    TestContext::Finalize
};
// clang-format on

} // namespace

int TestReadInteractionTest()
{
    TestContext gContext;
    nlTestRunner(&sSuite, &gContext);
    return (nlTestRunnerStats(&sSuite));
}

CHIP_REGISTER_TEST_SUITE(TestReadInteractionTest)
