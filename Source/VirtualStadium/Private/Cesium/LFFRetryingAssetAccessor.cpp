#include "Cesium/LFFRetryingAssetAccessor.h"

#include "VirtualStadium.h"

#include "CesiumRuntime.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreMisc.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumAsync/Promise.h>

#include <mutex>
#include <stdexcept>
#include <vector>

using CesiumAsync::AsyncSystem;
using CesiumAsync::Future;
using CesiumAsync::IAssetAccessor;
using CesiumAsync::IAssetRequest;

namespace
{
    struct FAttemptOutcome
    {
        std::shared_ptr<IAssetRequest> Request;
        bool bFailed = false;
        std::string Error;
    };

    struct FPendingDelay
    {
        double DueSeconds;
        CesiumAsync::Promise<void> Promise;
    };

    // Delays are resolved from tick() rather than an engine ticker: Cesium pumps the accessor's tick() while it
    // blocks for in-flight loads (for example in ACesium3DTileset::IsReadyForFinishDestroy), so a retry waiting
    // on an engine ticker that isn't running could never finish.
    std::mutex GPendingDelaysMutex;
    std::vector<FPendingDelay> GPendingDelays;

    Future<void> DelayUntilTicked(const AsyncSystem& InAsyncSystem, float Seconds)
    {
        CesiumAsync::Promise<void> Promise = InAsyncSystem.createPromise<void>();
        Future<void> Result = Promise.getFuture();
        {
            std::lock_guard<std::mutex> Lock(GPendingDelaysMutex);
            GPendingDelays.push_back(FPendingDelay{FPlatformTime::Seconds() + Seconds, std::move(Promise)});
        }
        return Result;
    }

    void ResolveDueDelays()
    {
        std::vector<CesiumAsync::Promise<void>> Due;
        {
            std::lock_guard<std::mutex> Lock(GPendingDelaysMutex);
            if (GPendingDelays.empty())
            {
                return;
            }
            const double Now = FPlatformTime::Seconds();
            // Never keep a shutting-down engine waiting on a backoff.
            const bool bFlushAll = IsEngineExitRequested();
            for (auto It = GPendingDelays.begin(); It != GPendingDelays.end();)
            {
                if (bFlushAll || It->DueSeconds <= Now)
                {
                    Due.push_back(std::move(It->Promise));
                    It = GPendingDelays.erase(It);
                }
                else
                {
                    ++It;
                }
            }
        }
        for (const CesiumAsync::Promise<void>& Promise : Due)
        {
            Promise.resolve();
        }
    }

    bool IsLocalFileUrl(const std::string& Url)
    {
        return Url.rfind("file:", 0) == 0;
    }

    FString ShortUrl(const std::string& Url)
    {
        // Strip the query string: it carries the API key and session.
        const std::string::size_type Query = Url.find('?');
        return UTF8_TO_TCHAR((Query == std::string::npos ? Url : Url.substr(0, Query)).c_str());
    }
}

FLFFRetryingAssetAccessor::FLFFRetryingAssetAccessor(std::shared_ptr<IAssetAccessor> InInner)
    : Inner(MoveTemp(InInner))
{
}

void FLFFRetryingAssetAccessor::Install()
{
    // getAssetAccessor() hands out a const reference to a mutable function-local static that every tileset,
    // raster overlay and ion connection copies when it is created. Replacing it before the map loads routes all
    // Cesium traffic through the retry layer.
    std::shared_ptr<IAssetAccessor>& Global = const_cast<std::shared_ptr<IAssetAccessor>&>(getAssetAccessor());
    static bool bInstalled = false;
    if (!Global || bInstalled)
    {
        return;
    }
    bInstalled = true;
    Global =  std::make_shared<FLFFRetryingAssetAccessor>(Global);
    UE_LOG(LogVirtualStadium, Log, TEXT("Cesium: transient tile request failures will be retried up to %d times."),
        MaxAttempts - 1);
}

float FLFFRetryingAssetAccessor::GetRetryDelaySeconds(int32 Attempt)
{
    return 0.25f * static_cast<float>(1 << FMath::Clamp(Attempt - 1, 0, 5));
}

bool FLFFRetryingAssetAccessor::IsRetryableStatus(uint16 StatusCode)
{
    return StatusCode == 408 || StatusCode == 429 || StatusCode == 500 || StatusCode == 502 || StatusCode == 503 ||
           StatusCode == 504;
}

Future<std::shared_ptr<IAssetRequest>> FLFFRetryingAssetAccessor::get(
    const AsyncSystem& InAsyncSystem,
    const std::string& Url,
    const std::vector<THeader>& Headers)
{
    return Send(InAsyncSystem, "GET", Url, Headers, nullptr, 1);
}

Future<std::shared_ptr<IAssetRequest>> FLFFRetryingAssetAccessor::request(
    const AsyncSystem& InAsyncSystem,
    const std::string& Verb,
    const std::string& Url,
    const std::vector<THeader>& Headers,
    const std::span<const std::byte>& ContentPayload)
{
    std::shared_ptr<const std::vector<std::byte>> Payload;
    if (!ContentPayload.empty())
    {
        Payload = std::make_shared<const std::vector<std::byte>>(ContentPayload.begin(), ContentPayload.end());
    }
    return Send(InAsyncSystem, Verb, Url, Headers, MoveTemp(Payload), 1);
}

void FLFFRetryingAssetAccessor::tick() noexcept
{
    ResolveDueDelays();
    Inner->tick();
}

Future<std::shared_ptr<IAssetRequest>> FLFFRetryingAssetAccessor::Send(
    const AsyncSystem& InAsyncSystem,
    std::string Verb,
    std::string Url,
    std::vector<THeader> Headers,
    std::shared_ptr<const std::vector<std::byte>> Payload,
    int32 Attempt)
{
    Future<std::shared_ptr<IAssetRequest>> Pending =
        Verb == "GET" ? Inner->get(InAsyncSystem, Url, Headers)
                      : Inner->request(InAsyncSystem, Verb, Url, Headers,
                            Payload ? std::span<const std::byte>(*Payload) : std::span<const std::byte>());

    const bool bIdempotent = Verb == "GET" || Verb == "HEAD";
    std::shared_ptr<FLFFRetryingAssetAccessor> Self = shared_from_this();

    return MoveTemp(Pending)
        .thenImmediately([](std::shared_ptr<IAssetRequest>&& Request) {
            return FAttemptOutcome{MoveTemp(Request), false, {}};
        })
        .catchImmediately([](std::exception&& Error) {
            return FAttemptOutcome{nullptr, true, Error.what()};
        })
        .thenImmediately([Self, InAsyncSystem, Verb = MoveTemp(Verb), Url = MoveTemp(Url), Headers = MoveTemp(Headers),
                             Payload = MoveTemp(Payload), Attempt, bIdempotent](FAttemptOutcome&& Outcome) mutable
                         -> Future<std::shared_ptr<IAssetRequest>> {
            const CesiumAsync::IAssetResponse* Response = Outcome.Request ? Outcome.Request->response() : nullptr;
            const uint16 Status = Response ? Response->statusCode() : 0;
            const bool bTransient = Outcome.bFailed || IsRetryableStatus(Status);

            if (!bTransient)
            {
                if (Attempt > 1)
                {
                    const int32 Recovered = ++Self->RecoveredRequests;
                    UE_LOG(LogVirtualStadium, Log, TEXT("Cesium: %s succeeded on attempt %d (%d recovered so far)."),
                        *ShortUrl(Url), Attempt, Recovered);
                }
                return InAsyncSystem.createResolvedFuture(MoveTemp(Outcome.Request));
            }

            const FString Reason = Outcome.bFailed ? FString(UTF8_TO_TCHAR(Outcome.Error.c_str()))
                                                   : FString::Printf(TEXT("HTTP %u"), Status);
            const bool bCanRetry = bIdempotent && Attempt < MaxAttempts && !IsLocalFileUrl(Url) &&
                                   !IsEngineExitRequested();
            if (!bCanRetry)
            {
                if (Attempt > 1)
                {
                    const int32 Abandoned = ++Self->AbandonedRequests;
                    UE_LOG(LogVirtualStadium, Warning, TEXT("Cesium: %s still failing after %d attempts (%s); %d abandoned."),
                        *ShortUrl(Url), Attempt, *Reason, Abandoned);
                }
                if (Outcome.bFailed)
                {
                    CesiumAsync::Promise<std::shared_ptr<IAssetRequest>> Rejected =
                        InAsyncSystem.createPromise<std::shared_ptr<IAssetRequest>>();
                    Rejected.reject(std::runtime_error(Outcome.Error));
                    return Rejected.getFuture();
                }
                return InAsyncSystem.createResolvedFuture(MoveTemp(Outcome.Request));
            }

            const float Delay = GetRetryDelaySeconds(Attempt);
            UE_LOG(LogVirtualStadium, Verbose, TEXT("Cesium: %s failed (%s); retry %d in %.2f s."), *ShortUrl(Url), *Reason,
                Attempt, Delay);
            return DelayUntilTicked(InAsyncSystem, Delay)
                .thenImmediately([Self, InAsyncSystem, Verb = MoveTemp(Verb), Url = MoveTemp(Url),
                                     Headers = MoveTemp(Headers), Payload = MoveTemp(Payload), Attempt]() mutable {
                    return Self->Send(InAsyncSystem, MoveTemp(Verb), MoveTemp(Url), MoveTemp(Headers), MoveTemp(Payload),
                        Attempt + 1);
                });
        });
}
