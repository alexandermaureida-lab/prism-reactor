#include "VideoExport.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <thread>
#include <chrono>

namespace
{
bool waitExport (AVAssetExportSession* ex, juce::String& err)
{
    dispatch_semaphore_t sem = dispatch_semaphore_create (0);
    [ex exportAsynchronouslyWithCompletionHandler:^{ dispatch_semaphore_signal (sem); }];
    dispatch_semaphore_wait (sem, DISPATCH_TIME_FOREVER);
    if (ex.status != AVAssetExportSessionStatusCompleted)
    {
        err = juce::String::fromUTF8 (ex.error != nil ? [[ex.error localizedDescription] UTF8String] : "export failed");
        return false;
    }
    return true;
}
}

class MacVideoSink : public VideoSink
{
public:
    ~MacVideoSink() override { abort(); }

    bool begin (const juce::File& out, int w, int h, int f, double mbps, const juce::File& audio, int codec, juce::String& err) override
    {
        @autoreleasepool
        {
            outFile = out; audioFile = audio; fps = f; isMov = (codec == 2); srcWidth = w; srcHeight = h;
            width = (w + 1) & ~1; height = (h + 1) & ~1;
            tmpVideo = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("prism-video", codec == 2 ? ".mov" : ".mp4", false);
            tmpAudio = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("prism-audio", ".m4a", false);
            tmpVideo.deleteFile();

            NSError* e = nil;
            writer = [[AVAssetWriter alloc] initWithURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:tmpVideo.getFullPathName().toRawUTF8()]]
                                               fileType:(isMov ? AVFileTypeQuickTimeMovie : AVFileTypeMPEG4) error:&e];
            if (writer == nil) { err = "Cannot create video file"; return false; }

            NSDictionary* settings;
            if (codec == 2)
                settings = @{ AVVideoCodecKey: AVVideoCodecTypeAppleProRes422, AVVideoWidthKey: @(width), AVVideoHeightKey: @(height) };
            else
                settings = @{ AVVideoCodecKey: (codec == 1 ? AVVideoCodecTypeHEVC : AVVideoCodecTypeH264),
                              AVVideoWidthKey: @(width), AVVideoHeightKey: @(height),
                              AVVideoCompressionPropertiesKey: @{ AVVideoAverageBitRateKey: @((int) (mbps * 1.0e6)),
                                                                  AVVideoExpectedSourceFrameRateKey: @(fps) } };
            input = [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:settings];
            input.expectsMediaDataInRealTime = NO;
            NSDictionary* attrs = @{ (__bridge id) kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
                                     (__bridge id) kCVPixelBufferWidthKey: @(width), (__bridge id) kCVPixelBufferHeightKey: @(height) };
            adaptor = [AVAssetWriterInputPixelBufferAdaptor assetWriterInputPixelBufferAdaptorWithAssetWriterInput:input sourcePixelBufferAttributes:attrs];
            if (! [writer canAddInput:input]) { err = "H.264 settings not supported"; return false; }
            [writer addInput:input];
            if (! [writer startWriting]) { err = juce::String::fromUTF8 ([[writer.error localizedDescription] UTF8String]); return false; }
            [writer startSessionAtSourceTime:kCMTimeZero];
            started = true;
        }
        return true;
    }

    bool addFrame (const juce::uint8* rgba, juce::String& err) override
    {
        @autoreleasepool
        {
            for (int i = 0; i < 4000 && ! input.readyForMoreMediaData; ++i)
            {
                if (writer.status == AVAssetWriterStatusFailed) break;
                std::this_thread::sleep_for (std::chrono::milliseconds (2));
            }
            if (writer.status == AVAssetWriterStatusFailed || ! input.readyForMoreMediaData)
            {
                err = juce::String::fromUTF8 (writer.error != nil ? [[writer.error localizedDescription] UTF8String] : "encoder is stuck");
                return false;
            }
            CVPixelBufferRef pb = nullptr;
            if (CVPixelBufferPoolCreatePixelBuffer (nullptr, adaptor.pixelBufferPool, &pb) != kCVReturnSuccess || pb == nullptr)
            { err = "Cannot allocate video frame"; return false; }

            CVPixelBufferLockBaseAddress (pb, 0);
            auto* dst = (juce::uint8*) CVPixelBufferGetBaseAddress (pb);
            const size_t stride = CVPixelBufferGetBytesPerRow (pb);
            const int srcW = srcWidth > 0 ? srcWidth : width, srcH = srcHeight > 0 ? srcHeight : height;
            for (int y = 0; y < height; ++y)
            {
                const int sy = juce::jmin (srcH - 1, y);
                const juce::uint8* s = rgba + (size_t) (srcH - 1 - sy) * (size_t) srcW * 4;
                juce::uint8* d = dst + (size_t) y * stride;
                for (int x = 0; x < width; ++x)
                {
                    const juce::uint8* px = s + (size_t) juce::jmin (srcW - 1, x) * 4;
                    d[x * 4 + 0] = px[2]; d[x * 4 + 1] = px[1]; d[x * 4 + 2] = px[0]; d[x * 4 + 3] = 255;
                }
            }
            CVPixelBufferUnlockBaseAddress (pb, 0);
            const bool ok = [adaptor appendPixelBuffer:pb withPresentationTime:CMTimeMake (frames, fps)];
            CVPixelBufferRelease (pb);
            if (! ok) { err = juce::String::fromUTF8 (writer.error != nil ? [[writer.error localizedDescription] UTF8String] : "append failed"); return false; }
            ++frames;
        }
        return true;
    }

    bool finish (juce::String& err) override
    {
        @autoreleasepool
        {
            if (! started) { err = "not started"; return false; }
            started = false;
            [input markAsFinished];
            dispatch_semaphore_t sem = dispatch_semaphore_create (0);
            [writer finishWritingWithCompletionHandler:^{ dispatch_semaphore_signal (sem); }];
            dispatch_semaphore_wait (sem, DISPATCH_TIME_FOREVER);
            if (writer.status != AVAssetWriterStatusCompleted)
            { err = juce::String::fromUTF8 (writer.error != nil ? [[writer.error localizedDescription] UTF8String] : "video encode failed"); cleanup(); return false; }

            outFile.deleteFile();
            if (! audioFile.existsAsFile())
            {
                const bool ok = tmpVideo.moveFileTo (outFile);
                if (! ok) err = "Cannot write the output file";
                cleanup(); return ok;
            }

            const double dur = (double) frames / (double) fps;
            NSURL* vurl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:tmpVideo.getFullPathName().toRawUTF8()]];
            NSURL* aurl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:audioFile.getFullPathName().toRawUTF8()]];
            NSURL* murl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:tmpAudio.getFullPathName().toRawUTF8()]];

            // 1) audio -> AAC .m4a (any decodable format), trimmed to the video length
            AVURLAsset* aAsset = [AVURLAsset URLAssetWithURL:aurl options:nil];
            AVAssetExportSession* ea = [[AVAssetExportSession alloc] initWithAsset:aAsset presetName:AVAssetExportPresetAppleM4A];
            ea.outputURL = murl; ea.outputFileType = AVFileTypeAppleM4A;
            ea.timeRange = CMTimeRangeMake (kCMTimeZero, CMTimeMakeWithSeconds (dur, 600));
            if (! waitExport (ea, err)) { err = "Audio conversion failed: " + err; cleanup(); return false; }

            // 2) video + audio -> MP4 (passthrough, no re-encode)
            AVURLAsset* vAsset = [AVURLAsset URLAssetWithURL:vurl options:nil];
            AVURLAsset* mAsset = [AVURLAsset URLAssetWithURL:murl options:nil];
            AVMutableComposition* comp = [AVMutableComposition composition];
            NSError* e = nil;
            AVAssetTrack* vt = [[vAsset tracksWithMediaType:AVMediaTypeVideo] firstObject];
            AVAssetTrack* at = [[mAsset tracksWithMediaType:AVMediaTypeAudio] firstObject];
            if (vt == nil) { err = "No video track"; cleanup(); return false; }
            AVMutableCompositionTrack* cv = [comp addMutableTrackWithMediaType:AVMediaTypeVideo preferredTrackID:kCMPersistentTrackID_Invalid];
            [cv insertTimeRange:CMTimeRangeMake (kCMTimeZero, vAsset.duration) ofTrack:vt atTime:kCMTimeZero error:&e];
            if (at != nil)
            {
                AVMutableCompositionTrack* ca = [comp addMutableTrackWithMediaType:AVMediaTypeAudio preferredTrackID:kCMPersistentTrackID_Invalid];
                CMTime ad = CMTimeMinimum (mAsset.duration, vAsset.duration);
                [ca insertTimeRange:CMTimeRangeMake (kCMTimeZero, ad) ofTrack:at atTime:kCMTimeZero error:&e];
            }
            AVAssetExportSession* ev = [[AVAssetExportSession alloc] initWithAsset:comp presetName:AVAssetExportPresetPassthrough];
            ev.outputURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:outFile.getFullPathName().toRawUTF8()]];
            ev.outputFileType = isMov ? AVFileTypeQuickTimeMovie : AVFileTypeMPEG4;
            ev.shouldOptimizeForNetworkUse = YES;
            const bool ok = waitExport (ev, err);
            if (! ok) err = "Mux failed: " + err;
            cleanup();
            return ok;
        }
    }

    void abort() override
    {
        if (started && writer != nil) { [writer cancelWriting]; started = false; }
        cleanup();
    }

    void cleanup() { tmpVideo.deleteFile(); tmpAudio.deleteFile(); }

private:
    int srcWidth = 0, srcHeight = 0;
    juce::File outFile, audioFile, tmpVideo, tmpAudio;
    AVAssetWriter* writer = nil;
    AVAssetWriterInput* input = nil;
    AVAssetWriterInputPixelBufferAdaptor* adaptor = nil;
    int width = 0, height = 0, fps = 30;
    bool isMov = false;
    juce::int64 frames = 0;
    bool started = false;
};

std::shared_ptr<VideoSink> VideoSink::create()
{
    return std::make_shared<MacVideoSink>();
}
bool VideoSink::isAvailable() { return true; }
