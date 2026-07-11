/**
 * @file audio_manager.h
 * @brief Sound playback for forgescripts, backed by the Windows MCI subsystem (winmm).
 *
 * Supports the formats the MCI mpegvideo device decodes, in practice mp3 and wav.
 * Every loaded sound is an independent MCI alias, so different sounds can play
 * concurrently. Replaying a sound that is already playing restarts it.
 */
#pragma once

#include <string>

class AudioManager
{
    public:
        /**
         * @brief Opens a sound file and returns a handle for playback.
         *
         * Loads through the MCI mpegvideo device so mp3 and wav both support volume
         * control. Repeat loads of the same file return the same handle.
         *
         * @param file_path Full path to the sound file.
         * @return A positive sound handle, or 0 on failure (logged).
         */
        static int Load(const std::wstring& file_path);

        /**
         * @brief Plays a loaded sound from the beginning.
         *
         * @param sound_id Handle returned by Load().
         * @param volume Playback volume from 0.0 to 1.0.
         * @param loop True to repeat until stopped.
         * @return True when playback started.
         */
        static bool Play(int sound_id, double volume, bool loop);

        /**
         * @brief Stops a playing sound. No-op when the sound isn't playing.
         *
         * @param sound_id Handle returned by Load().
         * @return True when the stop command succeeded.
         */
        static bool Stop(int sound_id);

        /**
         * @brief Reports whether a sound is currently playing.
         *
         * @param sound_id Handle returned by Load().
         * @return True while the sound is playing.
         */
        static bool IsPlaying(int sound_id);

        /**
         * @brief Sets the volume of a loaded sound, affecting current and future playback.
         *
         * @param sound_id Handle returned by Load().
         * @param volume Volume from 0.0 to 1.0.
         * @return True when the volume was applied.
         */
        static bool SetVolume(int sound_id, double volume);

        /**
         * @brief Closes a sound and frees its MCI alias. The handle becomes invalid.
         *
         * @param sound_id Handle returned by Load().
         */
        static void Release(int sound_id);

        /**
         * @brief Closes every loaded sound. Called during core cleanup.
         */
        static void ReleaseAll();
};
