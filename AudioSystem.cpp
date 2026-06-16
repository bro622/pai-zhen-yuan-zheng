#include "AudioSystem.h"
#include <iostream>

// ══════════════════════════════════════════════════════════════════════════════
//  音效文件映射（Sound 枚举索引 → 文件路径）
// ══════════════════════════════════════════════════════════════════════════════

static const char* kSlotPaths[] = {
    // UI (0~6)
    "assets/audio/ui/ui_click.wav",         // UiClick
    "assets/audio/ui/card_select.mp3",      // CardSelect
    "assets/audio/ui/deny.mp3",             // Deny
    "assets/audio/ui/map_hover.mp3",        // MapHover
    "assets/audio/ui/map_open.mp3",         // MapOpen
    "assets/audio/ui/map_ping.mp3",         // MapPing
    "assets/audio/ui/map_split_tick.mp3",   // MapSplitTick

    // 战斗 (7~16)
    "assets/audio/sfx/player_turn.mp3",     // PlayerTurn
    "assets/audio/sfx/enemy_turn.mp3",      // EnemyTurn
    "assets/audio/sfx/slash_attack.mp3",    // SlashAttack
    "assets/audio/sfx/blunt_attack.mp3",    // BluntAttack
    "assets/audio/sfx/heavy_attack.mp3",    // HeavyAttack
    "assets/audio/sfx/dagger_throw.mp3",    // DaggerThrow
    "assets/audio/sfx/doom_apply.mp3",      // DoomApply
    "assets/audio/sfx/burn_card.mp3",       // Heal (火焰/能量音色)
    "assets/audio/sfx/shovel.mp3",          // BlockUp (护甲/挖掘)
    "assets/audio/sfx/blunt_attack.mp3",    // StrengthUp (力量感，复用 blunt)

    // 战斗起止 (17~21)
    "assets/audio/sfx/battle_start_1.mp3",  // BattleStart1
    "assets/audio/sfx/battle_start_2.mp3",  // BattleStart2
    "assets/audio/sfx/hey.mp3",             // BossAttack (冲击感)
    "assets/audio/sfx/victory.mp3",         // Victory
    "assets/audio/sfx/death_stinger.mp3",   // Defeat

    // 场景 (22~23)
    "assets/audio/sfx/rest_jingle.mp3",     // RestJingle
    "assets/audio/sfx/rest_jingle.mp3",     // RestAmb (复用同一文件)

    // 卡牌 (24~25)
    "assets/audio/sfx/card_deal.mp3",       // CardDeal
    "assets/audio/sfx/card_exhaust.mp3"     // CardExhaust
};

static constexpr int kPathCount = sizeof(kSlotPaths) / sizeof(kSlotPaths[0]);

// ══════════════════════════════════════════════════════════════════════════════
//  SoundSlot::play — 查找空闲槽位或覆盖最旧
// ══════════════════════════════════════════════════════════════════════════════

void AudioSystem::SoundSlot::play(float volume)
{
    if (pool.empty()) return;

    // 策略：优先找停止的槽位；全部都在播放则覆盖最旧的
    int best = idx;
    bool foundStopped = false;
    for (int i = 0; i < static_cast<int>(pool.size()); ++i) {
        int ci = (idx + i) % static_cast<int>(pool.size());
        if (pool[ci].getStatus() == sf::Sound::Status::Stopped) {
            best = ci;
            foundStopped = true;
            break;
        }
    }

    sf::Sound& s = pool[best];
    s.setVolume(volume);
    s.play();
    idx = (best + 1) % static_cast<int>(pool.size());
}

// ══════════════════════════════════════════════════════════════════════════════
//  构造
// ══════════════════════════════════════════════════════════════════════════════

AudioSystem::AudioSystem()
{
    m_slots.resize(kSlotCount);

    // 加载所有缓冲区并构建池
    for (int i = 0; i < kSlotCount && i < kPathCount; ++i) {
        loadSlot(i, kSlotPaths[i]);
    }

    applyUiVolume();
    applySfxVolume();
}

void AudioSystem::loadSlot(int slotIndex, const std::string& path)
{
    SoundSlot& slot = m_slots[slotIndex];
    if (!slot.buffer.loadFromFile(path)) {
        std::cerr << "[AudioSystem] 加载失败: " << path << "\n";
        m_complete = false;
        return;
    }
    buildPool(slot);
}

void AudioSystem::buildPool(SoundSlot& slot)
{
    slot.pool.reserve(kPoolSize);
    for (int i = 0; i < kPoolSize; ++i) {
        slot.pool.emplace_back(slot.buffer);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  播放
// ══════════════════════════════════════════════════════════════════════════════

void AudioSystem::play(Sound sound)
{
    int idx = static_cast<int>(sound);
    if (idx < 0 || idx >= kSlotCount) return;

    // UI 音效用 uiVolume，其余用 sfxVolume
    float vol = (sound <= Sound::MapSplitTick) ? m_uiVolume : m_sfxVolume;
    m_slots[idx].play(vol);
}

// ══════════════════════════════════════════════════════════════════════════════
//  音量
// ══════════════════════════════════════════════════════════════════════════════

void AudioSystem::setUiVolume(float v)
{
    m_uiVolume = v;
    applyUiVolume();
}

void AudioSystem::setSfxVolume(float v)
{
    m_sfxVolume = v;
    applySfxVolume();
}

void AudioSystem::applyUiVolume()
{
    // UI 槽位：0~6
    for (int i = 0; i <= static_cast<int>(Sound::MapSplitTick) && i < kSlotCount; ++i) {
        for (auto& s : m_slots[i].pool) s.setVolume(m_uiVolume);
    }
}

void AudioSystem::applySfxVolume()
{
    // SFX 槽位：7+
    for (int i = static_cast<int>(Sound::PlayerTurn); i < kSlotCount; ++i) {
        for (auto& s : m_slots[i].pool) s.setVolume(m_sfxVolume);
    }
}
