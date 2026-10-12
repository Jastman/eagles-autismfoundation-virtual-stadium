#pragma once

#include "CoreMinimal.h"

#include <CesiumAsync/IAssetAccessor.h>

#include <atomic>
#include <memory>

/**
 * Wraps Cesium's global asset accessor and retries transient network failures (connection resets, TLS
 * handshake failures, timeouts, HTTP 408/429/5xx) with exponential backoff.
 *
 * cesium-native marks a tile whose request fails as permanently Failed and renders it as empty, so a single
 * dropped connection leaves a hole in the photorealistic tiles (the blue atmosphere planet shows through)
 * until the tileset is reloaded. Retrying below Cesium keeps those tiles loading.
 */
class FLFFRetryingAssetAccessor final : public CesiumAsync::IAssetAccessor,
                                        public std::enable_shared_from_this<FLFFRetryingAssetAccessor>
{
public:
    explicit FLFFRetryingAssetAccessor(std::shared_ptr<CesiumAsync::IAssetAccessor> InInner);

    /** Wraps Cesium for Unreal's global accessor. Call before any tileset loads. Safe to call more than once. */
    static void Install();

    virtual CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> get(
        const CesiumAsync::AsyncSystem& AsyncSystem,
        const std::string& Url,
        const std::vector<THeader>& Headers = {}) override;

    virtual CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> request(
        const CesiumAsync::AsyncSystem& AsyncSystem,
        const std::string& Verb,
        const std::string& Url,
        const std::vector<THeader>& Headers = std::vector<THeader>(),
        const std::span<const std::byte>& ContentPayload = {}) override;

    virtual void tick() noexcept override;

    static constexpr int32 MaxAttempts = 7;

    /** Delay before retry number Attempt (1-based): 0.25, 0.5, 1, 2, 4, 8 s. */
    static float GetRetryDelaySeconds(int32 Attempt);

    static bool IsRetryableStatus(uint16 StatusCode);

private:
    CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>> Send(
        const CesiumAsync::AsyncSystem& AsyncSystem,
        std::string Verb,
        std::string Url,
        std::vector<THeader> Headers,
        std::shared_ptr<const std::vector<std::byte>> Payload,
        int32 Attempt);

    std::shared_ptr<CesiumAsync::IAssetAccessor> Inner;
    std::atomic<int32> RecoveredRequests{0};
    std::atomic<int32> AbandonedRequests{0};
};
