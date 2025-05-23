/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "CoreIPCNSURLRequest.h"

#import "GeneratedSerializers.h"
#import <WebCore/ResourceLoadPriority.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(COCOA) && HAVE(WK_SECURE_CODING_NSURLREQUEST)

@interface NSURLRequest (WKSecureCoding)
- (NSDictionary *)_webKitPropertyListData;
- (instancetype)_initWithWebKitPropertyListData:(NSDictionary *)plist;
@end

namespace WebKit {

#define SET_NSURLREQUESTDATA(NAME, CLASS, WRAPPER)  \
    RetainPtr<id> NAME = dict.get()[@#NAME];        \
    if ([NAME isKindOfClass:CLASS.class]) {         \
        auto var = WRAPPER(NAME.get());             \
        m_data.NAME = WTFMove(var);                 \
    }

#define SET_NSURLREQUESTDATA_PRIMITIVE(NAME, CLASS, PRIMITIVE) \
    RetainPtr<id> NAME = dict.get()[@#NAME];                   \
    if ([NAME isKindOfClass:CLASS.class])                      \
        m_data.NAME = [NAME PRIMITIVE##Value];

#define SET_DICT_FROM_OPTIONAL_MEMBER(KEY) \
    if (m_data.KEY) { \
        if (auto var##KEY = m_data.KEY->toID()) \
            [dict setObject:var##KEY.get() forKey:@#KEY]; \
    }

#define SET_DICT_FROM_MEMBER(KEY) \
    if (auto var##KEY = m_data.KEY.toID()) \
        [dict setObject:var##KEY.get() forKey:@#KEY] \

#define SET_DICT_FROM_OPTIONAL_PRIMITIVE(KEY, CLASS, PRIMITIVE)                    \
    if (m_data.KEY)                                                                \
        [dict setObject:[NSNumber numberWith##PRIMITIVE:*m_data.KEY] forKey:@#KEY]

#define SET_DICT_FROM_PRIMITIVE(KEY, CLASS, PRIMITIVE) \
    [dict setObject:[NSNumber numberWith##PRIMITIVE:m_data.KEY] forKey:@#KEY]

WTF_MAKE_TZONE_ALLOCATED_IMPL(CoreIPCNSURLRequest);

CoreIPCNSURLRequest::CoreIPCNSURLRequest(NSURLRequest *request)
{
    RetainPtr dict = [request _webKitPropertyListData];

    SET_NSURLREQUESTDATA(protocolProperties, NSDictionary, CoreIPCPlistDictionary);
    SET_NSURLREQUESTDATA_PRIMITIVE(isMutable, NSNumber, bool);

    RetainPtr<id> url = dict.get()[@"URL"];
    if ([url isKindOfClass:NSURL.class]) {
        auto var = CoreIPCURL(url.get());
        m_data.url = WTFMove(var);
    }

    SET_NSURLREQUESTDATA_PRIMITIVE(timeout, NSNumber, double);

    RetainPtr<id> cachePolicy = dict.get()[@"cachePolicy"];
    if ([cachePolicy isKindOfClass:[NSNumber class]]) {
        auto val = [cachePolicy unsignedCharValue];
        if (isValidEnum<NSURLRequestCachePolicy>(val))
            m_data.cachePolicy = static_cast<NSURLRequestCachePolicy>(val);
    }

    SET_NSURLREQUESTDATA(mainDocumentURL, NSURL, CoreIPCURL);
    SET_NSURLREQUESTDATA_PRIMITIVE(shouldHandleHTTPCookies, NSNumber, bool);

    RetainPtr<id> explicitFlags = dict.get()[@"explicitFlags"];
    if ([explicitFlags isKindOfClass:[NSNumber class]]) {
        auto val = [explicitFlags shortValue];
        auto optionSet = OptionSet<NSURLRequestFlags>::fromRaw(val);
        if (isValidOptionSet<NSURLRequestFlags>(optionSet))
            m_data.explicitFlags = optionSet;
    }

    SET_NSURLREQUESTDATA_PRIMITIVE(allowCellular, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(preventsIdleSystemSleep, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(timeWindowDelay, NSNumber, double);
    SET_NSURLREQUESTDATA_PRIMITIVE(timeWindowDuration, NSNumber, double);

    RetainPtr<id> networkServiceType = dict.get()[@"networkServiceType"];
    if ([networkServiceType isKindOfClass:[NSNumber class]]) {
        auto val = [networkServiceType unsignedCharValue];
        if (isValidEnum<NSURLRequestNetworkServiceType>(val))
            m_data.networkServiceType = static_cast<NSURLRequestNetworkServiceType>(val);
    }

    SET_NSURLREQUESTDATA_PRIMITIVE(requestPriority, NSNumber, int);
    SET_NSURLREQUESTDATA_PRIMITIVE(isHTTP, NSNumber, bool);
    SET_NSURLREQUESTDATA(httpMethod, NSString, CoreIPCString);

    RetainPtr<NSDictionary> headerFields = dict.get()[@"headerFields"];
    if ([headerFields isKindOfClass:[NSDictionary class]]) {
        Vector<CoreIPCNSURLRequestData::HeaderField> vector;
        vector.reserveInitialCapacity(headerFields.get().count);
        for (id key in headerFields.get()) {
            RetainPtr<id> value = [headerFields objectForKey:key];
            if (RetainPtr valueArray = dynamic_objc_cast<NSArray>(value.get())) {
                Vector<String> valueVector;
                valueVector.reserveInitialCapacity(valueArray.get().count);
                for (id item in valueArray.get()) {
                    if (RetainPtr nsString = dynamic_objc_cast<NSString>(item))
                        valueVector.append(nsString.get());
                }
                vector.append({ key, valueVector });
            }
            if (RetainPtr nsString = dynamic_objc_cast<NSString>(value.get()))
                vector.append({ key, nsString.get() });
        }
        vector.shrinkToFit();
        m_data.headerFields = WTFMove(vector);
    }

    SET_NSURLREQUESTDATA(body, NSData, CoreIPCData);

    RetainPtr<NSArray> bodyParts = dict.get()[@"bodyParts"];
    if ([bodyParts isKindOfClass:[NSArray class]]) {
        Vector<CoreIPCNSURLRequestData::BodyParts> vector;
        vector.reserveInitialCapacity(bodyParts.get().count);
        for (id element in bodyParts.get()) {
            if ([element isKindOfClass:[NSString class]]) {
                CoreIPCString tooAdd(element);
                vector.append(WTFMove(tooAdd));
            }
            if ([element isKindOfClass:[NSData class]]) {
                CoreIPCData tooAdd(element);
                vector.append(WTFMove(tooAdd));
            }
        }
        vector.shrinkToFit();
        m_data.bodyParts = WTFMove(vector);
    }

    SET_NSURLREQUESTDATA_PRIMITIVE(startTimeoutTime, NSNumber, double);
    SET_NSURLREQUESTDATA_PRIMITIVE(requiresShortConnectionTimeout, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(payloadTransmissionTimeout, NSNumber, double);

    RetainPtr<id> allowedProtocolTypes = dict.get()[@"allowedProtocolTypes"];
    if ([allowedProtocolTypes isKindOfClass:[NSNumber class]]) {
        auto val = [allowedProtocolTypes unsignedCharValue];
        auto optionSet = OptionSet<NSURLRequestAllowedProtocolTypes>::fromRaw(val);
        if (isValidOptionSet<NSURLRequestAllowedProtocolTypes>(optionSet))
            m_data.allowedProtocolTypes = optionSet;
    }

    SET_NSURLREQUESTDATA(boundInterfaceIdentifier, NSString, CoreIPCString);
    SET_NSURLREQUESTDATA_PRIMITIVE(allowsExpensiveNetworkAccess, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(allowsConstrainedNetworkAccess, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(allowsUCA, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(assumesHTTP3Capable, NSNumber, bool);

    RetainPtr<id> attribution = dict.get()[@"attribution"];
    if ([attribution isKindOfClass:[NSNumber class]]) {
        auto val = [allowedProtocolTypes unsignedCharValue];
        if (isValidEnum<NSURLRequestAttribution>(val))
            m_data.attribution = static_cast<NSURLRequestAttribution>(val);
    }

    SET_NSURLREQUESTDATA_PRIMITIVE(knownTracker, NSNumber, bool);
    SET_NSURLREQUESTDATA(trackerContext, NSString, CoreIPCString);
    SET_NSURLREQUESTDATA_PRIMITIVE(privacyProxyFailClosed, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(privacyProxyFailClosedForUnreachableNonMainHosts, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(privacyProxyFailClosedForUnreachableHosts, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(requiresDNSSECValidation, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(allowsPersistentDNS, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(prohibitPrivacyProxy, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(allowPrivateAccessTokensForThirdParty, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(useEnhancedPrivacyMode, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(blockTrackers, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(failInsecureLoadWithHTTPSDNSRecord, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(isWebSearchContent, NSNumber, bool);
    SET_NSURLREQUESTDATA_PRIMITIVE(allowOnlyPartitionedCookies, NSNumber, bool);

    RetainPtr<NSArray> contentDispositionEncodingFallbackArray = dict.get()[@"contentDispositionEncodingFallbackArray"];
    if ([contentDispositionEncodingFallbackArray isKindOfClass:[NSArray class]]) {
        Vector<CoreIPCNumber> vector;
        vector.reserveInitialCapacity(contentDispositionEncodingFallbackArray.get().count);
        for (id element in contentDispositionEncodingFallbackArray.get()) {
            if ([element isKindOfClass:[NSNumber class]])
                vector.append(CoreIPCNumber(element));
        }
        vector.shrinkToFit();
        m_data.contentDispositionEncodingFallbackArray = WTFMove(vector);
    }
}

CoreIPCNSURLRequest::CoreIPCNSURLRequest(CoreIPCNSURLRequestData&& data)
    : m_data(WTFMove(data)) { }

CoreIPCNSURLRequest::CoreIPCNSURLRequest(const RetainPtr<NSURLRequest>& request)
    : CoreIPCNSURLRequest(request.get()) { }


RetainPtr<id> CoreIPCNSURLRequest::toID() const
{
    RetainPtr dict = adoptNS([[NSMutableDictionary alloc] initWithCapacity:CoreIPCNSURLRequestData::numberOfFields]);

    SET_DICT_FROM_OPTIONAL_MEMBER(protocolProperties);
    SET_DICT_FROM_PRIMITIVE(isMutable, NSNumber, Bool);

    if (auto nsURL = m_data.url.toID())
        [dict setObject:nsURL.get() forKey:@"URL"];

    SET_DICT_FROM_PRIMITIVE(timeout, NSNumber, Double);

    [dict setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(m_data.cachePolicy)] forKey:@"cachePolicy"];

    SET_DICT_FROM_OPTIONAL_MEMBER(mainDocumentURL);
    SET_DICT_FROM_PRIMITIVE(shouldHandleHTTPCookies, NSNumber, Bool);

    [dict setObject:[NSNumber numberWithShort:static_cast<int16_t>(m_data.explicitFlags.toRaw())] forKey:@"explicitFlags"];

    SET_DICT_FROM_PRIMITIVE(allowCellular, NSNumber, Bool);
    SET_DICT_FROM_PRIMITIVE(preventsIdleSystemSleep, NSNumber, Bool);
    SET_DICT_FROM_PRIMITIVE(timeWindowDelay, NSNumber, Double);
    SET_DICT_FROM_PRIMITIVE(timeWindowDuration, NSNumber, Double);

    [dict setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(m_data.networkServiceType)] forKey:@"networkServiceType"];

    int clampedRequestPriority = std::min(std::max(m_data.requestPriority, -1),
        static_cast<int>(WTF::enumToUnderlyingType(WebCore::ResourceLoadPriority::Highest)));
    [dict setObject:[NSNumber numberWithInt:clampedRequestPriority] forKey:@"requestPriority"];

    SET_DICT_FROM_OPTIONAL_PRIMITIVE(isHTTP, NSNumber, Bool);
    SET_DICT_FROM_OPTIONAL_MEMBER(httpMethod);

    if (m_data.headerFields) {
        auto headerFields = adoptNS([[NSMutableDictionary alloc] initWithCapacity:(*m_data.headerFields).size()]);
        for (auto& headerPair : *m_data.headerFields) {
            WTF::switchOn(headerPair.second,
                [&] (const String& string) {
                    auto array = adoptNS([[NSMutableArray alloc] initWithCapacity:1]);
                    if (!string.isNull() && !headerPair.first.isNull()) {
                        [array addObject:string.createNSString().get()];
                        [headerFields setObject:array.get() forKey:headerPair.first.createNSString().get()];
                    }
                },
                [&] (const Vector<String>& vector) {
                    auto array = adoptNS([[NSMutableArray alloc] initWithCapacity:vector.size()]);
                    for (auto& item : vector)
                        [array addObject:item.createNSString().get()];
                    if (!headerPair.first.isNull())
                        [headerFields setObject:array.get() forKey:headerPair.first.createNSString().get()];
                }
            );
        }
        [dict setObject:headerFields.get() forKey:@"headerFields"];
    }

    SET_DICT_FROM_OPTIONAL_MEMBER(body);

    if (m_data.bodyParts) {
        auto array = adoptNS([[NSMutableArray alloc] initWithCapacity:(*m_data.bodyParts).size()]);
        for (auto& value : *m_data.bodyParts) {
            WTF::switchOn(value,
                [&] (const auto& d) {
                    if (auto obj = d.toID())
                        [array addObject:obj.get()];
                }
            );
        }
        [dict setObject:array.get() forKey:@"bodyParts"];
    }

    SET_DICT_FROM_PRIMITIVE(startTimeoutTime, NSInteger, Double);
    SET_DICT_FROM_PRIMITIVE(requiresShortConnectionTimeout, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(payloadTransmissionTimeout, NSInteger, Double);

    [dict setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(m_data.allowedProtocolTypes.toRaw())] forKey:@"allowedProtocolTypes"];

    SET_DICT_FROM_OPTIONAL_MEMBER(boundInterfaceIdentifier);
    SET_DICT_FROM_OPTIONAL_PRIMITIVE(allowsExpensiveNetworkAccess, NSInteger, Bool);
    SET_DICT_FROM_OPTIONAL_PRIMITIVE(allowsConstrainedNetworkAccess, NSInteger, Bool);
    SET_DICT_FROM_OPTIONAL_PRIMITIVE(allowsUCA, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(assumesHTTP3Capable, NSInteger, Bool);

    [dict setObject:[NSNumber numberWithUnsignedChar:static_cast<uint8_t>(m_data.attribution)] forKey:@"attribution"];

    SET_DICT_FROM_PRIMITIVE(knownTracker, NSInteger, Bool);
    SET_DICT_FROM_OPTIONAL_MEMBER(trackerContext);
    SET_DICT_FROM_PRIMITIVE(privacyProxyFailClosed, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(privacyProxyFailClosedForUnreachableNonMainHosts, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(privacyProxyFailClosedForUnreachableHosts, NSInteger, Bool);
    SET_DICT_FROM_OPTIONAL_PRIMITIVE(requiresDNSSECValidation, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(allowsPersistentDNS, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(prohibitPrivacyProxy, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(allowPrivateAccessTokensForThirdParty, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(useEnhancedPrivacyMode, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(blockTrackers, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(failInsecureLoadWithHTTPSDNSRecord, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(isWebSearchContent, NSInteger, Bool);
    SET_DICT_FROM_PRIMITIVE(allowOnlyPartitionedCookies, NSInteger, Bool);

    if (m_data.contentDispositionEncodingFallbackArray) {
        auto array = adoptNS([[NSMutableArray alloc] initWithCapacity:m_data.contentDispositionEncodingFallbackArray->size()]);
        for (auto& value : *m_data.contentDispositionEncodingFallbackArray) {
            if (auto valueObj = value.toID())
                [array addObject:valueObj.get()];
        }
        [dict setObject:array.get() forKey:@"contentDispositionEncodingFallbackArray"];
    }

    return adoptNS([[NSURLRequest alloc] _initWithWebKitPropertyListData:dict.get()]);
}

} // namespace WebKit

#undef SET_NSURLREQUESTDATA
#undef SET_NSURLREQUESTDATA_PRIMITIVE
#undef SET_DICT_FROM_MEMBER
#undef SET_DICT_FROM_OPTIONAL_MEMBER
#undef SET_DICT_FROM_PRIMITIVE
#undef SET_DICT_FROM_OPTIONAL_PRIMITIVE

#endif // PLATFORM(COCOA) && HAVE(WK_SECURE_CODING_NSURLREQUEST)
