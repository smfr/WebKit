/*
 * Copyright (C) 2014 Igalia S.L
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if WK_HAVE_C_SPI

#if ENABLE(MEDIA_STREAM)
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKUserMediaPermissionCheck.h>
#include <string.h>
#include <vector>

namespace TestWebKitAPI {

static bool didCrash;
static bool wasPrompted;

static void decidePolicyForUserMediaPermissionRequestCallBack(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKSecurityOriginRef, WKUserMediaPermissionRequestRef permissionRequest, const void* /* clientInfo */)
{
    WKRetainPtr<WKArrayRef> audioDeviceUIDs = WKUserMediaPermissionRequestAudioDeviceUIDs(permissionRequest);
    WKRetainPtr<WKArrayRef> videoDeviceUIDs = WKUserMediaPermissionRequestVideoDeviceUIDs(permissionRequest);

    if (WKArrayGetSize(videoDeviceUIDs.get()) || WKArrayGetSize(audioDeviceUIDs.get())) {
        WKRetainPtr<WKStringRef> videoDeviceUID;
        if (WKArrayGetSize(videoDeviceUIDs.get()))
            videoDeviceUID = reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(videoDeviceUIDs.get(), 0));
        else
            videoDeviceUID = WKStringCreateWithUTF8CString("");

        WKRetainPtr<WKStringRef> audioDeviceUID;
        if (WKArrayGetSize(audioDeviceUIDs.get()))
            audioDeviceUID = reinterpret_cast<WKStringRef>(WKArrayGetItemAtIndex(audioDeviceUIDs.get(), 0));
        else
            audioDeviceUID = WKStringCreateWithUTF8CString("");

        WKUserMediaPermissionRequestAllow(permissionRequest, audioDeviceUID.get(), videoDeviceUID.get());
    }

    wasPrompted = true;
}

TEST(WebKit, UserMediaBasic)
{
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    WKPageUIClientV6 uiClient;
    zeroBytes(uiClient);
    uiClient.base.version = 6;
    uiClient.decidePolicyForUserMediaPermissionRequest = decidePolicyForUserMediaPermissionRequestCallBack;

    PlatformWebView webView(context.get());
    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    auto configuration = adoptWK(WKPageCopyPageConfiguration(webView.page()));
    auto* preferences = WKPageConfigurationGetPreferences(configuration.get());
    WKPreferencesSetMediaDevicesEnabled(preferences, true);
    WKPreferencesSetFileAccessFromFileURLsAllowed(preferences, true);
    WKPreferencesSetMediaCaptureRequiresSecureConnection(preferences, false);
    WKPreferencesSetMockCaptureDevicesEnabled(preferences, true);
    WKPreferencesSetGetUserMediaRequiresFocus(preferences, false);

    wasPrompted = false;
    auto url = adoptWK(Util::createURLForResource("getUserMedia", "html"));
    ASSERT(url.get());

    WKPageLoadURL(webView.page(), url.get());

    Util::run(&wasPrompted);
}

static void didCrashCallback(WKPageRef, const void*)
{
    didCrash = true;
    // Set wasPrompted to true to speed things up, we know the test failed.
    wasPrompted = true;
}

TEST(WebKit, OnDeviceChangeCrash)
{
    auto configuration = adoptWK(WKPageConfigurationCreate());
    auto preferences = adoptWK(WKPreferencesCreate());
    WKPreferencesSetMediaDevicesEnabled(preferences.get(), true);
    WKPreferencesSetFileAccessFromFileURLsAllowed(preferences.get(), true);
    WKPreferencesSetMediaCaptureRequiresSecureConnection(preferences.get(), false);
    WKPreferencesSetMockCaptureDevicesEnabled(preferences.get(), true);
    WKPreferencesSetGetUserMediaRequiresFocus(preferences.get(), false);
    WKPageConfigurationSetPreferences(configuration.get(), preferences.get());

    WKPageUIClientV6 uiClient;
    zeroBytes(uiClient);
    uiClient.base.version = 6;
    uiClient.decidePolicyForUserMediaPermissionRequest = decidePolicyForUserMediaPermissionRequestCallBack;

    PlatformWebView webView(configuration.get());
    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    // Load a page registering ondevicechange handler.
    auto url = adoptWK(Util::createURLForResource("ondevicechange", "html"));
    ASSERT(url.get());

    WKPageLoadURL(webView.page(), url.get());

    // Load a second page in same process.
    PlatformWebView webView2(webView.page());
    WKPageSetPageUIClient(webView2.page(), &uiClient.base);
    WKPageNavigationClientV0 navigationClient;
    zeroBytes(navigationClient);
    navigationClient.base.version = 0;
    navigationClient.webProcessDidCrash = didCrashCallback;
    WKPageSetPageNavigationClient(webView2.page(), &navigationClient.base);

    wasPrompted = false;
    url = adoptWK(Util::createURLForResource("getUserMedia", "html"));
    WKPageLoadURL(webView2.page(), url.get());
    Util::run(&wasPrompted);
    EXPECT_EQ(WKPageGetProcessIdentifier(webView.page()), WKPageGetProcessIdentifier(webView2.page()));

    didCrash = false;

    // Close first page.
    WKPageClose(webView.page());

    wasPrompted = false;
    url = adoptWK(Util::createURLForResource("getUserMedia", "html"));
    WKPageLoadURL(webView2.page(), url.get());
    Util::run(&wasPrompted);
    // Verify pages process did not crash.
    EXPECT_TRUE(!didCrash);
}

static bool didReceiveMessage;
static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didReceiveMessage = true;
}

TEST(WebKit, EnumerateDevicesCrash)
{
    auto configuration = adoptWK(WKPageConfigurationCreate());
    auto preferences = adoptWK(WKPreferencesCreate());
    WKPageConfigurationSetPreferences(configuration.get(), preferences.get());

    WKPreferencesSetMediaDevicesEnabled(preferences.get(), true);
    WKPreferencesSetFileAccessFromFileURLsAllowed(preferences.get(), true);
    WKPreferencesSetMediaCaptureRequiresSecureConnection(preferences.get(), false);
    WKPreferencesSetMockCaptureDevicesEnabled(preferences.get(), true);
    WKPreferencesSetGetUserMediaRequiresFocus(preferences.get(), false);

    WKPageUIClientV6 uiClient;
    // We want uiClient.checkUserMediaPermissionForOrigin to be null.
    zeroBytes(uiClient);
    uiClient.base.version = 6;

    WKPageNavigationClientV3 loaderClient;
    zeroBytes(loaderClient);
    loaderClient.base.version = 3;
    loaderClient.didFinishNavigation = didFinishNavigation;

    PlatformWebView webView(configuration.get());
    WKPageSetPageUIClient(webView.page(), &uiClient.base);
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    // Load a page doing enumerateDevices.
    didReceiveMessage = false;
    auto url = adoptWK(Util::createURLForResource("getUserMedia", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didReceiveMessage);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_STREAM)

#endif // WK_HAVE_C_SPI

