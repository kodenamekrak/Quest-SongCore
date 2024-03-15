#include "SongCore.hpp"
#include "SongLoader/RuntimeSongLoader.hpp"
#include "logging.hpp"
#include "config.hpp"

#include "UnityEngine/HideFlags.hpp"
#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/Texture.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/TextureWrapMode.hpp"

static inline UnityEngine::HideFlags operator |(UnityEngine::HideFlags a, UnityEngine::HideFlags b) {
    return UnityEngine::HideFlags(a.value__ | b.value__);
}

namespace SongCore::API {
    namespace Capabilities {
        static UnorderedEventCallback<std::string_view, Capabilities::CapabilityEventKind> _capabilitiesUpdated;
        std::mutex _registeredCapabilitiesMutex;
        static std::vector<std::string> _registeredCapabilities;

        static std::string sanitize(std::string_view capability) {
            std::string sanitized;
            sanitized.reserve(capability.size());
            for (auto c : capability) {
                if (c == ' ') continue;
                sanitized.push_back(tolower(c));
            }
            return sanitized;
        }

        void RegisterCapability(std::string_view capability) {
            std::lock_guard<std::mutex> lock(_registeredCapabilitiesMutex);

            auto sanitized = sanitize(capability);
            auto itr = std::find(
                _registeredCapabilities.begin(),
                _registeredCapabilities.end(),
                sanitized
            );

            if (itr == _registeredCapabilities.end()) {
                _registeredCapabilities.emplace_back(sanitized);
                _capabilitiesUpdated.invoke(capability, CapabilityEventKind::Registered);
            } else {
                WARNING("Capability '{}' was registered more than once! not registering again", capability);
            }
        }

        void UnregisterCapability(std::string_view capability) {
            std::lock_guard<std::mutex> lock(_registeredCapabilitiesMutex);

            auto sanitized = sanitize(capability);
            auto itr = std::find(
                _registeredCapabilities.begin(),
                _registeredCapabilities.end(),
                sanitized
            );

            if (itr != _registeredCapabilities.end()) {
                _registeredCapabilities.erase(itr);
                _capabilitiesUpdated.invoke(capability, CapabilityEventKind::Unregistered);
            } else {
                WARNING("Capability '{}' was unregistered more than once! not unregistering again", capability);
            }
        }

        bool IsCapabilityRegistered(std::string_view capability) {
            std::lock_guard<std::mutex> lock(_registeredCapabilitiesMutex);

            auto sanitized = sanitize(capability);
            auto itr = std::find(
                _registeredCapabilities.begin(),
                _registeredCapabilities.end(),
                sanitized
            );

            // if itr != end, that means it was found in the vector
            return itr != _registeredCapabilities.end();
        }

        std::span<const std::string> GetRegisteredCapabilities() {
            return _registeredCapabilities;
        }

        UnorderedEventCallback<std::string_view, Capabilities::CapabilityEventKind>& GetCapabilitiesUpdatedEvent() {
            return _capabilitiesUpdated;
        }
    }

    namespace PlayButton {
        static std::vector<PlayButtonDisablingModInfo> _disablingModInfos;
        static UnorderedEventCallback<std::span<PlayButtonDisablingModInfo const>> _playButtonDisablingModsChangedEvent;

        void DisablePlayButton(std::string modID, std::string reason) {
            auto itr = std::find_if(_disablingModInfos.begin(), _disablingModInfos.end(), [&modID](auto& x){ return x.modID == modID; });
            if (itr == _disablingModInfos.end()) {
                _disablingModInfos.emplace_back(modID, reason);
                _playButtonDisablingModsChangedEvent.invoke(_disablingModInfos);
            } else {
                WARNING("Mod {} tried disabling the play button twice, which is not supported! current reason: {}, new reason: {}", modID, itr->reason, reason);
            }
        }

        void EnablePlayButton(std::string modID) {
            auto itr = std::find_if(_disablingModInfos.begin(), _disablingModInfos.end(), [&modID](auto& x){ return x.modID == modID; });
            if (itr != _disablingModInfos.end()) {
                _disablingModInfos.erase(itr);
                _playButtonDisablingModsChangedEvent.invoke(_disablingModInfos);
            } else {
                WARNING("Mod {} tried enabling the play button twice, which is not supported!", modID);
            }
        }

        UnorderedEventCallback<std::span<PlayButtonDisablingModInfo const>>& GetPlayButtonDisablingModsChangedEvent() {
            return _playButtonDisablingModsChangedEvent;
        }

        std::span<PlayButtonDisablingModInfo const> GetPlayButtonDisablingModInfos() {
            return _disablingModInfos;
        }
    }

    namespace Characteristics {
        static SafePtr<System::Collections::Generic::List_1<GlobalNamespace::BeatmapCharacteristicSO*>> _registeredCharacteristics;
        static UnorderedEventCallback<GlobalNamespace::BeatmapCharacteristicSO*, Characteristics::CharacteristicEventKind> _characteristicsUpdatedEvent;

        ListW<GlobalNamespace::BeatmapCharacteristicSO*> get_registeredCharacteristics() {
            if (_registeredCharacteristics) return _registeredCharacteristics.ptr();
            _registeredCharacteristics = System::Collections::Generic::List_1<GlobalNamespace::BeatmapCharacteristicSO*>::New_ctor();
            return _registeredCharacteristics.ptr();
        }

        void RegisterCustomCharacteristic(GlobalNamespace::BeatmapCharacteristicSO* characteristic) {
            characteristic->hideFlags = characteristic->hideFlags | UnityEngine::HideFlags::DontUnloadUnusedAsset;

            if (GetCharacteristicBySerializedName(static_cast<std::string>(characteristic->serializedName))) {
                get_registeredCharacteristics()->Add(characteristic);
                _characteristicsUpdatedEvent.invoke(characteristic, CharacteristicEventKind::Registered);
            } else {
                WARNING("Characteristic '{}' was registered more than once! not registering again", characteristic->serializedName);
            }
        }

        void UnregisterCustomCharacteristic(GlobalNamespace::BeatmapCharacteristicSO* characteristic) {
            auto idx = get_registeredCharacteristics()->IndexOf(characteristic);
            if (idx >= 0) {
                get_registeredCharacteristics()->RemoveAt(idx);
                _characteristicsUpdatedEvent.invoke(characteristic, CharacteristicEventKind::Unregistered);
            } else {
                WARNING("Characteristic '{}' was unregistered more than once! not unregistering again", characteristic->serializedName);
            }
        }

        std::span<GlobalNamespace::BeatmapCharacteristicSO*> GetRegisteredCharacteristics() {
            return get_registeredCharacteristics().ref_to();
        }

        GlobalNamespace::BeatmapCharacteristicSO* GetCharacteristicBySerializedName(std::string_view serializedName) {
            auto characteristic = get_registeredCharacteristics().find([&serializedName](auto x) { return x->serializedName == serializedName; });
            // value_or(nullptr) doesn't work because it's a reference wrapper in an optional
            return characteristic.has_value() ? characteristic.value() : nullptr;
        }

        UnorderedEventCallback<GlobalNamespace::BeatmapCharacteristicSO*, Characteristics::CharacteristicEventKind>& GetCharacteristicsUpdatedEvent() {
            return _characteristicsUpdatedEvent;
        }

        GlobalNamespace::BeatmapCharacteristicSO* CreateCharacteristic(UnityEngine::Sprite* icon, StringW characteristicName, StringW hintText, StringW serializedName, StringW compoundIdPartName, bool requires360Movement, bool containsRotationEvents, int sortingOrder) {
            icon->texture->wrapMode = UnityEngine::TextureWrapMode::Clamp;

            auto characteristic = UnityEngine::ScriptableObject::CreateInstance<GlobalNamespace::BeatmapCharacteristicSO*>();
            characteristic->hideFlags = characteristic->hideFlags | UnityEngine::HideFlags::DontUnloadUnusedAsset;
            characteristic->_icon = icon;
            characteristic->_descriptionLocalizationKey = hintText;
            characteristic->_serializedName = serializedName;
            characteristic->_characteristicNameLocalizationKey = characteristicName;
            characteristic->_compoundIdPartName = compoundIdPartName;
            characteristic->_requires360Movement = requires360Movement;
            characteristic->_containsRotationEvents = containsRotationEvents;
            characteristic->_sortingOrder = sortingOrder;

            return characteristic;
        }
    }

    namespace Loading {
        static UnorderedEventCallback<std::span<GlobalNamespace::CustomPreviewBeatmapLevel* const>> _songsLoadedEvent;
        static UnorderedEventCallback<> _songsWillRefreshEvent;
        static UnorderedEventCallback<SongCore::SongLoader::SongCoreCustomBeatmapLevelPackCollection*> _customLevelPacksWillRefreshEvent;
        static UnorderedEventCallback<SongCore::SongLoader::SongCoreCustomBeatmapLevelPackCollection*> _customLevelPacksRefreshedEvent;
        static UnorderedEventCallback<GlobalNamespace::CustomPreviewBeatmapLevel*> _songWillBeDeletedEvent;
        static UnorderedEventCallback<> _songDeletedEvent;

        std::shared_future<void> RefreshSongs(bool fullRefresh) {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return std::future<void>();
            return instance->RefreshSongs(fullRefresh);
        }

        void RefreshLevelPacks() {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return;
            return instance->RefreshLevelPacks();
        }

        std::future<void> DeleteSong(std::filesystem::path const& levelPath) {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return std::future<void>();
            return instance->DeleteSong(levelPath);
        }

        std::future<void> DeleteSong(GlobalNamespace::CustomPreviewBeatmapLevel* beatmapLevel) {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return std::future<void>();
            return instance->DeleteSong(beatmapLevel);
        }

        UnorderedEventCallback<std::span<GlobalNamespace::CustomPreviewBeatmapLevel* const>>& GetSongsLoadedEvent() {
            return _songsLoadedEvent;
        }

        UnorderedEventCallback<>& GetSongsWillRefreshEvent() {
            return _songsWillRefreshEvent;
        }

        UnorderedEventCallback<SongCore::SongLoader::SongCoreCustomBeatmapLevelPackCollection*>& GetCustomLevelPacksWillRefreshEvent() {
            return _customLevelPacksWillRefreshEvent;
        }

        UnorderedEventCallback<SongCore::SongLoader::SongCoreCustomBeatmapLevelPackCollection*>& GetCustomLevelPacksRefreshedEvent() {
            return _customLevelPacksRefreshedEvent;
        }

        UnorderedEventCallback<GlobalNamespace::CustomPreviewBeatmapLevel*>& GetSongWillBeDeletedEvent() {
            return _songWillBeDeletedEvent;
        }

        UnorderedEventCallback<>& GetSongDeletedEvent() {
            return _songDeletedEvent;
        }

        std::filesystem::path GetPreferredCustomLevelPath() {
            return config.PreferredCustomLevelPath;
        }

        std::span<std::filesystem::path const> GetRootCustomLevelPaths() {
            return config.RootCustomLevelPaths;
        }

        std::filesystem::path GetPreferredCustomWIPLevelPath() {
            return config.PreferredCustomWIPLevelPath;
        }

        std::span<std::filesystem::path const> GetRootCustomWIPLevelPaths() {
            return config.RootCustomWIPLevelPaths;
        }

        void AddLevelPath(std::filesystem::path const& path, bool wipPath) {
            auto& targetPaths = wipPath ? config.RootCustomWIPLevelPaths : config.RootCustomLevelPaths;
            auto itr = std::find(targetPaths.begin(), targetPaths.end(), path);
            if (itr == targetPaths.end()) {
                targetPaths.emplace_back(path);
                SaveConfig();
            } else {
                INFO("Path {} was already in the target collection, not adding again", path.string());
            }
        }

        void RemoveLevelPath(std::filesystem::path const& path, bool wipPath) {
            auto& targetPaths = wipPath ? config.RootCustomWIPLevelPaths : config.RootCustomLevelPaths;
            auto itr = std::find(targetPaths.begin(), targetPaths.end(), path);
            if (itr != targetPaths.end()) {
                targetPaths.erase(itr);
                SaveConfig();
            } else {
                INFO("Path {} wasn't in the target collection, nothing will happen", path.string());
            }
        }

        bool AreSongsRefreshing() {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return false;
            return instance->AreSongsRefreshing;
        }

        bool AreSongsLoaded() {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return false;
            return instance->AreSongsLoaded;
        }

        float LoadProgress() {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return 0.0f;
            return instance->Progress;
        }

        std::span<GlobalNamespace::CustomPreviewBeatmapLevel* const> GetAllLevels() {
            static std::array<GlobalNamespace::CustomPreviewBeatmapLevel*, 0x0> emptyArray;
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return emptyArray;
            return instance->AllLevels;
        }

        SongLoader::SongCoreCustomLevelPack* GetCustomLevelPack() {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return nullptr;
            return instance->CustomLevelPack;
        }

        SongLoader::SongCoreCustomLevelPack* GetCustomWIPLevelPack() {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return nullptr;
            return instance->CustomWIPLevelPack;
        }

        SongLoader::SongCoreCustomBeatmapLevelPackCollection* GetCustomLevelPackCollection() {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return nullptr;
            return instance->CustomBeatmapLevelPackCollection;
        }

        GlobalNamespace::CustomPreviewBeatmapLevel* GetLevelByPath(std::filesystem::path const& levelPath) {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return nullptr;
            return instance->GetLevelByPath(levelPath);
        }

        GlobalNamespace::CustomPreviewBeatmapLevel* GetLevelByLevelID(std::string_view levelID) {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return nullptr;
            return instance->GetLevelByLevelID(levelID);
        }

        GlobalNamespace::CustomPreviewBeatmapLevel* GetLevelByHash(std::string_view hash) {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return nullptr;
            return instance->GetLevelByHash(hash);
        }

        GlobalNamespace::CustomPreviewBeatmapLevel* GetLevelByFunction(std::function<bool(GlobalNamespace::CustomPreviewBeatmapLevel*)> searchFunction) {
            auto instance = SongLoader::RuntimeSongLoader::get_instance();
            if (!instance) return nullptr;
            return instance->GetLevelByFunction(searchFunction);
        }
    }

    namespace LevelSelect {
        UnorderedEventCallback<LevelWasSelectedEventArgs const&> _levelWasSelectedEvent;
        UnorderedEventCallback<LevelWasSelectedEventArgs const&>& GetLevelWasSelectedEvent() {
            return _levelWasSelectedEvent;
        }
    }
}
