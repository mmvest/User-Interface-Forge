#include <string>
#include <unordered_map>

#include <Windows.h>
#include <mmsystem.h>

#include <plog/Log.h>

#include "core\audio_manager.h"

namespace
{
    struct SoundRecord
    {
        std::wstring alias;
        std::wstring file_path;
    };

    // Loaded sounds by handle, plus a path index so repeat loads reuse the alias.
    std::unordered_map<int, SoundRecord> sounds;
    std::unordered_map<std::wstring, int> sounds_by_path;
    int next_sound_id = 1;

    // Sends one MCI command, logging the translated error on failure.
    bool SendMciCommand(const std::wstring& command, std::wstring* out_result = nullptr)
    {
        wchar_t result_buffer[128] = {};
        MCIERROR error = mciSendStringW(command.c_str(), result_buffer, ARRAYSIZE(result_buffer), nullptr);
        if (error != 0)
        {
            wchar_t error_text[256] = {};
            mciGetErrorStringW(error, error_text, ARRAYSIZE(error_text));
            PLOG_WARNING << "MCI command failed: \"" << command << "\" -> " << error_text;
            return false;
        }

        if (out_result)
        {
            *out_result = result_buffer;
        }
        return true;
    }

    const SoundRecord* FindSound(int sound_id)
    {
        auto record = sounds.find(sound_id);
        return (record == sounds.end()) ? nullptr : &record->second;
    }

    // MCI volume range is 0-1000.
    int ToMciVolume(double volume)
    {
        if (volume < 0.0) volume = 0.0;
        if (volume > 1.0) volume = 1.0;
        return static_cast<int>(volume * 1000.0);
    }
}

int AudioManager::Load(const std::wstring& file_path)
{
    auto existing = sounds_by_path.find(file_path);
    if (existing != sounds_by_path.end())
    {
        return existing->second;
    }

    // The alias carries a UiForge prefix so it can never collide with MCI aliases
    // the host application might be using.
    const int sound_id = next_sound_id;
    const std::wstring alias = L"uiforge_snd_" + std::to_wstring(sound_id);

    // mpegvideo decodes both mp3 and wav and, unlike waveaudio, supports setaudio volume.
    if (!SendMciCommand(L"open \"" + file_path + L"\" type mpegvideo alias " + alias))
    {
        return 0;
    }

    next_sound_id++;
    sounds[sound_id] = SoundRecord{ alias, file_path };
    sounds_by_path[file_path] = sound_id;
    PLOG_DEBUG << "Loaded sound " << sound_id << ": " << file_path;
    return sound_id;
}

bool AudioManager::Play(int sound_id, double volume, bool loop)
{
    const SoundRecord* sound = FindSound(sound_id);
    if (!sound)
    {
        return false;
    }

    SendMciCommand(L"setaudio " + sound->alias + L" volume to " + std::to_wstring(ToMciVolume(volume)));

    // Seek back to the start so replaying an already-played sound works.
    SendMciCommand(L"seek " + sound->alias + L" to start");
    return SendMciCommand(L"play " + sound->alias + (loop ? L" repeat" : L""));
}

bool AudioManager::Stop(int sound_id)
{
    const SoundRecord* sound = FindSound(sound_id);
    if (!sound)
    {
        return false;
    }
    return SendMciCommand(L"stop " + sound->alias);
}

bool AudioManager::IsPlaying(int sound_id)
{
    const SoundRecord* sound = FindSound(sound_id);
    if (!sound)
    {
        return false;
    }

    std::wstring mode;
    if (!SendMciCommand(L"status " + sound->alias + L" mode", &mode))
    {
        return false;
    }
    return mode == L"playing";
}

bool AudioManager::SetVolume(int sound_id, double volume)
{
    const SoundRecord* sound = FindSound(sound_id);
    if (!sound)
    {
        return false;
    }
    return SendMciCommand(L"setaudio " + sound->alias + L" volume to " + std::to_wstring(ToMciVolume(volume)));
}

void AudioManager::Release(int sound_id)
{
    auto record = sounds.find(sound_id);
    if (record == sounds.end())
    {
        return;
    }

    SendMciCommand(L"close " + record->second.alias);
    sounds_by_path.erase(record->second.file_path);
    sounds.erase(record);
}

void AudioManager::ReleaseAll()
{
    for (const auto& [sound_id, sound] : sounds)
    {
        SendMciCommand(L"close " + sound.alias);
    }
    sounds.clear();
    sounds_by_path.clear();
}
