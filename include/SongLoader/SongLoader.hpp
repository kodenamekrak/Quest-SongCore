#pragma once

#include <compare>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include <mutex>
#include <set>

#include "CustomJSONData.hpp"
#include "Characteristics.hpp"
#include "SongCoreCustomLevelPack.hpp"

#include "System/Collections/Concurrent/ConcurrentDictionary_2.hpp"
#include "System/Collections/Generic/List_1.hpp"

#include "custom-types/shared/macros.hpp"

#include "GlobalNamespace/CustomPreviewBeatmapLevel.hpp"
#include "GlobalNamespace/CustomBeatmapLevelCollection.hpp"
#include "GlobalNamespace/CustomBeatmapLevelPack.hpp"
#include "GlobalNamespace/StandardLevelInfoSaveData.hpp"
#include "GlobalNamespace/EnvironmentInfoSO.hpp"
#include "GlobalNamespace/BeatmapLevelData.hpp"
#include "GlobalNamespace/BeatmapDataLoader.hpp"
#include "GlobalNamespace/ColorScheme.hpp"
#include "GlobalNamespace/CustomLevelLoader.hpp"
#include "GlobalNamespace/CachedMediaAsyncLoader.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollection.hpp"

#include "UnityEngine/MonoBehaviour.hpp"
#include "Zenject/IInitializable.hpp"
#include "lapiz/shared/macros.hpp"

#include "SongCoreCustomLevelPack.hpp"

namespace SongCore::SongLoader {
    using SongDict = ::System::Collections::Concurrent::ConcurrentDictionary_2<StringW, ::GlobalNamespace::CustomPreviewBeatmapLevel*>;
}

DECLARE_CLASS_CODEGEN(SongCore::SongLoader, RuntimeSongLoader, UnityEngine::MonoBehaviour,
    DECLARE_CTOR(ctor);

    DECLARE_INSTANCE_METHOD(void, Awake);
    DECLARE_INSTANCE_METHOD(void, Update);

    DECLARE_INJECT_METHOD(void, Inject, GlobalNamespace::CustomLevelLoader* customLevelLoader, GlobalNamespace::BeatmapLevelsModel* _beatmapLevelsModel, GlobalNamespace::CachedMediaAsyncLoader* cachedMediaAsyncLoader, GlobalNamespace::BeatmapCharacteristicCollection* beatmapCharacteristicCollection);
    DECLARE_INSTANCE_FIELD_PRIVATE(GlobalNamespace::CustomLevelLoader*, _customLevelLoader);
    DECLARE_INSTANCE_FIELD_PRIVATE(GlobalNamespace::BeatmapLevelsModel*, _beatmapLevelsModel);
    DECLARE_INSTANCE_FIELD_PRIVATE(GlobalNamespace::CachedMediaAsyncLoader*, _cachedMediaAsyncLoader);
    DECLARE_INSTANCE_FIELD_PRIVATE(GlobalNamespace::BeatmapCharacteristicCollection*, _beatmapCharacteristicCollection);

    DECLARE_INSTANCE_FIELD_PRIVATE(SongCoreCustomLevelPack*, _customLevelPack);
    DECLARE_INSTANCE_FIELD_PRIVATE(SongCoreCustomLevelPack*, _customWIPLevelPack);

    DECLARE_INSTANCE_FIELD_PRIVATE(int, _songCount);

    DECLARE_INSTANCE_FIELD_PRIVATE(SongDict*, _customLevels);
    DECLARE_INSTANCE_FIELD_PRIVATE(SongDict*, _customWIPLevels);

    DECLARE_INSTANCE_FIELD_PRIVATE(GlobalNamespace::BeatmapDataLoader*, _beatmapDataLoader);

    std::filesystem::path _songPath, _wipSongPath;
    std::vector<GlobalNamespace::CustomPreviewBeatmapLevel*> _loadedLevels;
    std::vector<GlobalNamespace::BeatmapLevelData*> _song;

public:
    static inline const std::string CUSTOM_LEVEL_PREFIX_ID = "custom_level_";
    static inline const std::string CUSTOM_LEVEL_PACK_PREFIX_ID = "custom_levelPack_";

    /// @brief Returns an instance of the songloader
    RuntimeSongLoader* get_instance() { return _instance; };
    RuntimeSongLoader* _instance = nullptr;

    /// @brief Returns the preferred song path.
    std::filesystem::path get_SongPath() const;
    __declspec(property(get = get_SongPath)) std::filesystem::path SongPath;

    /// @brief Returns the current WIP song path.
    std::filesystem::path get_WIPSongPath() const;
    __declspec(property(get = get_WIPSongPath)) std::filesystem::path WIPSongPath;

    __declspec(property(get = get_songs)) ListW<GlobalNamespace::BeatmapData*> _songs[];

    /// @brief Returns the current song count
    int get_songCount() { return _songCount; };
    __declspec(property(get = get_songCount)) int songCount;

    /// @brief Returns a key value pair of songs
    SongDict* get_customLevels() { return _customLevels; };
    __declspec(property(get = get_customLevels)) SongLoader::SongDict* CustomLevels;

    /// @brief Returns a key value pair of the WIP songs
    SongDict* get_customWIPLevels() { return _customWIPLevels; };
    __declspec(property(get = get_customWIPLevels)) SongLoader::SongDict* CustomWIPLevels;

    /// @brief Returns a vector of currently loaded levels
    std::vector<GlobalNamespace::CustomPreviewBeatmapLevel*> get_loadedLevels() { return _loadedLevels; };

    /// @brief refreshes the loaded songs, loading any new ones
    /// @param fullRefresh whether to reload every song or only new ones
    std::shared_future<void> RefreshSongs(bool fullRefresh);

    private:
        /// @brief internal struct to keep track of levelpath and wip status of a song before it got loaded
        struct LevelPathAndWip {
            std::filesystem::path levelPath;
            bool isWip;

            auto operator<=>(LevelPathAndWip const& other) const {
                return levelPath <=> other.levelPath;
            }
        };

        /// @brief gets the environment info with environmentName, or default if not found
        /// @param environmentName the name to look for
        /// @param allDirections if the env was not found, use the default for alldirections or not
        /// @return environmentinfo
        GlobalNamespace::EnvironmentInfoSO* GetEnvironmentInfo(StringW environmentName, bool allDirections);

        /// @brief gets all the environment infos with environmentNames
        /// @param environmentsNames the names to look for
        /// @return environmentinfo array
        ArrayW<GlobalNamespace::EnvironmentInfoSO*> GetEnvironmentInfos(std::span<StringW const> environmentsNames);

        /// @brief constructs the color schemes from the savedata
        /// @param colorSchemeDatas the save data color schemes
        /// @return constructed color schemes
        ArrayW<GlobalNamespace::ColorScheme*> GetColorSchemes(std::span<GlobalNamespace::BeatmapLevelColorSchemeSaveData* const> colorSchemeDatas);

        /// @brief fixes up the difficulty beatmap sets from the game
        ListW<GlobalNamespace::PreviewDifficultyBeatmapSet*> GetDifficultyBeatmapSets(std::span<GlobalNamespace::StandardLevelInfoSaveData::DifficultyBeatmapSet* const> difficultyBeatmapSetDatas);

        /// @brief Loads songs at given path given
        /// @param path the path to the song
        /// @param isWip is this a wip song
        /// @param saveData the level save data, for custom levels this is always a custom level info savedata
        /// @param outHash output for the hash of this level, might be unneeded though
        /// @return loaded preview beatmap level, or nullptr if failed
        GlobalNamespace::CustomPreviewBeatmapLevel* LoadCustomPreviewBeatmapLevel(std::filesystem::path levelPath, bool isWip, SongCore::CustomJSONData::CustomLevelInfoSaveData* saveData, std::string& outHash);

        void CollectLevels(std::span<const std::filesystem::path> roots, bool isWip, std::set<LevelPathAndWip>& out);

        void LoadCustomLevelsFromPaths(std::span<const std::filesystem::path> paths, bool wip = false);
        void RefreshSongs_internal(bool refreshSongs);
        void RefreshSongWorkerThread(std::mutex* levelsItrMutex, std::set<LevelPathAndWip>::const_iterator* levelsItr, std::set<LevelPathAndWip>::const_iterator* levelsEnd);
        SongCore::CustomJSONData::CustomLevelInfoSaveData* GetStandardSaveData(std::filesystem::path path);

        std::shared_future<void> _currentlyLoadingFuture;
)
