/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef MEDIA_WRITER_H_

#define MEDIA_WRITER_H_

#include <utils/RefBase.h>
#include <media/stagefright/MediaSource.h>
#include <media/IMediaRecorderClient.h>
#include <media/mediarecorder.h>

namespace android {

struct MediaWriter : public RefBase {
    MediaWriter()
        : mMaxFileSizeLimitBytes(0),
          mMaxFileDurationLimitUs(0) {
    }

    // Returns true if the file descriptor is opened using a mode
    // which meets minimum writer/muxer requirements.
    static bool isFdOpenModeValid(int fd) {
        // check for invalid file descriptor.
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1) {
            ALOGE("Invalid File Status Flags and/or mode : %d", flags);
            return false;
        }
        // fd must be in read-write mode or write-only mode.
        if ((flags & (O_RDWR | O_WRONLY)) == 0) {
            ALOGE("File must be writable");
            return false;
        }
        // Verify fd is seekable
        off64_t off = lseek64(fd, 0, SEEK_SET);
        if (off < 0) {
            ALOGE("File descriptor is not seekable");
            return false;
        }
        return true;
    }

    // Returns true if the sample data is valid.
    virtual bool isSampleMetadataValid([[maybe_unused]] size_t trackIndex,
                                       [[maybe_unused]] int64_t timeUs) {
        return true;
    }

    virtual status_t addSource(const sp<MediaSource> &source) = 0;
    virtual bool reachedEOS() = 0;
    virtual status_t start(MetaData *params = NULL) = 0;
    virtual status_t stop() = 0;
    virtual status_t pause() = 0;
    virtual status_t setCaptureRate(float /* captureFps */) {
        ALOG(LOG_WARN, "MediaWriter", "setCaptureRate unsupported");
        return ERROR_UNSUPPORTED;
    }

    virtual void setMaxFileSize(int64_t bytes) { mMaxFileSizeLimitBytes = bytes; }
    virtual void setMaxFileDuration(int64_t durationUs) { mMaxFileDurationLimitUs = durationUs; }
    virtual void setListener(const sp<IMediaRecorderClient>& listener) {
        mListener = listener;
    }

    virtual status_t dump(int /*fd*/, const Vector<String16>& /*args*/) {
        return OK;
    }

    virtual void setStartTimeOffsetMs(int /*ms*/) {}
    virtual int32_t getStartTimeOffsetMs() const { return 0; }
    virtual status_t setNextFd(int /*fd*/) { return INVALID_OPERATION; }
    virtual void updateCVODegrees(int32_t /*cvoDegrees*/) {}
    virtual void updatePayloadType(int32_t /*payloadType*/) {}
    virtual void updateSocketNetwork(int64_t /*socketNetwork*/) {}
    virtual uint32_t getSequenceNum() { return 0; }
    virtual uint64_t getAccumulativeBytes() { return 0; }

protected:
    virtual ~MediaWriter() {}
    int64_t mMaxFileSizeLimitBytes;
    int64_t mMaxFileDurationLimitUs;
    sp<IMediaRecorderClient> mListener;

    void notify(int msg, int ext1, int ext2) {
        if (msg == MEDIA_RECORDER_TRACK_EVENT_INFO || msg == MEDIA_RECORDER_TRACK_EVENT_ERROR) {
            uint32_t trackId = (ext1 >> 28) & 0xf;
            int type = ext1 & 0xfffffff;
            ALOG(LOG_VERBOSE, "MediaWriter", "Track event err/info msg:%d, trackId:%u, type:%d,"
                                             "val:%d", msg, trackId, type, ext2);
        } else {
            ALOG(LOG_VERBOSE, "MediaWriter", "Recorder event msg:%d, ext1:%d, ext2:%d",
                                              msg, ext1, ext2);
        }
        if (mListener != nullptr) {
            mListener->notify(msg, ext1, ext2);
        }
    }
private:
    MediaWriter(const MediaWriter &);
    MediaWriter &operator=(const MediaWriter &);
};

}  // namespace android

#endif  // MEDIA_WRITER_H_
