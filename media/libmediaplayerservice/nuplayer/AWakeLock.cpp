/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "AWakeLock"
#include <utils/Log.h>

#include "AWakeLock.h"

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <media/stagefright/foundation/ADebug.h>
#include <powermanager/PowerManager.h>


namespace android {

const int32_t INVALID_DISPLAY_ID = -1;

AWakeLock::AWakeLock() :
    mPowerManager(NULL),
    mWakeLockToken(NULL),
    mWakeLockCount(0),
    mDeathRecipient(new PMDeathRecipient(this)){}

AWakeLock::~AWakeLock() {
    if (mPowerManager != NULL) {
        sp<IBinder> binder = IInterface::asBinder(mPowerManager);
        binder->unlinkToDeath(mDeathRecipient);
    }
    clearPowerManager();
}

bool AWakeLock::acquire() {
    if (mWakeLockCount == 0) {
        CHECK(mWakeLockToken == NULL);
        if (mPowerManager == NULL) {
            // use checkService() to avoid blocking if power service is not up yet
            sp<IBinder> binder =
                defaultServiceManager()->checkService(String16("power"));
            if (binder == NULL) {
                ALOGW("could not get the power manager service");
            } else {
                mPowerManager = interface_cast<os::IPowerManager>(binder);
                binder->linkToDeath(mDeathRecipient);
            }
        }
        if (mPowerManager != NULL) {
            sp<IBinder> binder = new BBinder();
            int64_t token = IPCThreadState::self()->clearCallingIdentity();
            binder::Status status = mPowerManager->acquireWakeLock(
                binder,
                /*flags= */ POWERMANAGER_PARTIAL_WAKE_LOCK,
                /*tag=*/ String16("AWakeLock"),
                /*packageName=*/ String16("media"),
                /*ws=*/ {},
                /*historyTag=*/ {},
                /*displayId=*/ INVALID_DISPLAY_ID,
                /*callback=*/NULL);
            IPCThreadState::self()->restoreCallingIdentity(token);
            if (status.isOk()) {
                mWakeLockToken = binder;
                mWakeLockCount++;
                ALOGI("AwakeLock acquired");
                return true;
            }
        }
    } else {
        mWakeLockCount++;
        return true;
    }
    return false;
}

void AWakeLock::release(bool force) {
    if (mWakeLockCount == 0) {
        return;
    }
    if (force) {
        // Force wakelock release below by setting reference count to 1.
        mWakeLockCount = 1;
    }
    if (--mWakeLockCount == 0) {
        CHECK(mWakeLockToken != NULL);
        if (mPowerManager != NULL) {
            int64_t token = IPCThreadState::self()->clearCallingIdentity();
            mPowerManager->releaseWakeLock(mWakeLockToken, 0 /* flags */);
            IPCThreadState::self()->restoreCallingIdentity(token);
        }
        mWakeLockToken.clear();
        ALOGI("AwakeLock released");
    }
}

void AWakeLock::clearPowerManager() {
    release(true);
    mPowerManager.clear();
}

void AWakeLock::PMDeathRecipient::binderDied(const wp<IBinder>& who __unused) {
    if (mWakeLock != NULL) {
        mWakeLock->clearPowerManager();
    }
}

}  // namespace android
