#pragma once
// engine/capture/CaptureService.hpp
// Engine-owned visual artifact capture service.

#include "engine/capture/CaptureTypes.hpp"
#include "engine/threading/ThreadTypes.hpp"

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ndde {

class ThreadManagementService;

class CaptureService {
public:
    CaptureService() = default;
    ~CaptureService() = default;

    CaptureService(const CaptureService&) = delete;
    CaptureService& operator=(const CaptureService&) = delete;
    CaptureService(CaptureService&&) = delete;
    CaptureService& operator=(CaptureService&&) = delete;

    void set_thread_service(ThreadManagementService* threads,
                            ThreadRole owner_role = ThreadRole::Main) noexcept;

    void set_output_dir(std::filesystem::path directory);
    [[nodiscard]] const std::filesystem::path& output_dir() const noexcept { return m_output_dir; }

    void request_still(CaptureRequest request, CaptureRunMetadata metadata);

    [[nodiscard]] bool start_movie_frames(MovieFrameSequenceOptions options,
                                          CaptureRunMetadata metadata);
    void stop_movie_frames();
    [[nodiscard]] bool movie_frames_active() const noexcept { return m_movie_status.active; }
    [[nodiscard]] MovieFrameSequenceStatus movie_frame_status() const { return m_movie_status; }

    [[nodiscard]] std::vector<CaptureArtifact> consume_pending_stills();
    [[nodiscard]] std::span<const CaptureArtifact> completed_artifacts() const noexcept { return m_completed; }
    void clear_completed_artifacts();

    [[nodiscard]] static std::string normalize_stem(std::string_view text);
    [[nodiscard]] static std::string target_token(CaptureTarget target);

private:
    std::filesystem::path m_output_dir{"captures"};
    std::vector<CaptureArtifact> m_pending_stills;
    std::vector<CaptureArtifact> m_completed;
    MovieFrameSequenceOptions m_movie_options;
    MovieFrameSequenceStatus m_movie_status;
    ThreadManagementService* m_threads = nullptr;
    ThreadRole m_owner_role = ThreadRole::Main;

    [[nodiscard]] bool require_owner_thread(std::string_view api_name) const;
    [[nodiscard]] std::string make_run_id(const CaptureRunMetadata& metadata) const;
    [[nodiscard]] std::filesystem::path make_manifest_path(const std::filesystem::path& run_dir,
                                                           const std::string& run_id) const;
    [[nodiscard]] std::filesystem::path make_still_path(const std::filesystem::path& run_dir,
                                                        const std::string& run_id,
                                                        CaptureTarget target,
                                                        const CaptureRunMetadata& metadata) const;
    [[nodiscard]] static std::filesystem::path unique_path(std::filesystem::path path);
    [[nodiscard]] static std::string file_timestamp();

    void enqueue_manifest_write(std::filesystem::path manifest_path,
                                CaptureRequest request,
                                CaptureRunMetadata metadata,
                                std::vector<CaptureArtifact> artifacts) const;
    void enqueue_movie_frame_manifest_write(CaptureRunMetadata metadata,
                                            MovieFrameSequenceOptions options,
                                            MovieFrameSequenceStatus status) const;
    static void write_manifest(const std::filesystem::path& manifest_path,
                               const CaptureRequest& request,
                               const CaptureRunMetadata& metadata,
                               std::span<const CaptureArtifact> artifacts);
    static void write_movie_frame_manifest(const CaptureRunMetadata& metadata,
                                           const MovieFrameSequenceOptions& options,
                                           const MovieFrameSequenceStatus& status);
};

} // namespace ndde
