#pragma once

#include <SFML/Audio.hpp>

#include <string>
#include <vector>

// ══════════════════════════════════════════════════════════════════════════════
//  音效系统
//  SFML 3 音频 + 每个缓冲区独立池化播放（永不截断正在播放的音效）
// ══════════════════════════════════════════════════════════════════════════════

enum class Sound : uint8_t {
    // ── UI 音效 ──────────────────────────────────────────────────────────
    UiClick,         // 0
    CardSelect,      // 1
    Deny,            // 2
    MapHover,        // 3
    MapOpen,         // 4
    MapPing,         // 5
    MapSplitTick,    // 6

    // ── 战斗音效 ─────────────────────────────────────────────────────────
    PlayerTurn,      // 7
    EnemyTurn,       // 8
    SlashAttack,     // 9
    BluntAttack,     // 10
    HeavyAttack,     // 11
    DaggerThrow,     // 12
    DoomApply,       // 13
    Heal,            // 14
    BlockUp,         // 15
    StrengthUp,      // 16

    // ── 回合/战斗起止 ────────────────────────────────────────────────────
    BattleStart1,    // 17
    BattleStart2,    // 18
    BossAttack,      // 19
    Victory,         // 20
    Defeat,          // 21

    // ── 场景音效 ─────────────────────────────────────────────────────────
    RestJingle,      // 22
    RestAmb,         // 23

    // ── 卡牌音效（独立于战斗音效，用于打出卡牌时）────────────────────────
    CardDeal,        // 24
    CardExhaust,     // 25

    Count            // 26
};

class AudioSystem {
public:
    AudioSystem();
    bool isComplete() const noexcept { return m_complete; }

    // 播放音效（每种音效独立池化：同时播放同种音效时自动选择空闲槽位）
    void play(Sound sound);

    // 音量控制（0~100）
    void setSfxVolume(float volume);
    void setUiVolume(float volume);

    float sfxVolume() const noexcept { return m_sfxVolume; }
    float uiVolume()  const noexcept { return m_uiVolume; }

private:
    // ── 每个缓冲区对应一个独立 Sound 池（SFML 3：Sound 绑定 Buffer 不可更改）──
    struct SoundSlot {
        sf::SoundBuffer buffer;
        std::vector<sf::Sound> pool;
        int idx = 0;
        // play() 查找空闲槽位或覆盖最旧的，然后推进 idx
        void play(float volume);
    };

    static constexpr int kPoolSize = 3;  // 每种音效最多 3 个并发实例
    static constexpr int kSlotCount = static_cast<int>(Sound::Count);

    std::vector<SoundSlot> m_slots;  // 长度 = kSlotCount，索引与 Sound 枚举一一对应

    float m_sfxVolume = 80.f;
    float m_uiVolume  = 70.f;
    bool  m_complete  = true;

    void loadSlot(int slotIndex, const std::string& path);
    void buildPool(SoundSlot& slot);
    void applyUiVolume();
    void applySfxVolume();
};
