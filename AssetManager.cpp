#include "AssetManager.h"

#include <iostream>

AssetManager::AssetManager(const std::string& assetDir)
    : m_complete(true)
{
    // 卡牌纹理：asset_00.png ~ asset_08.png
    for (int i = 0; i < kTextureCount; ++i) {
        std::string filename = assetDir + "asset_"
                             + (i < 10 ? "0" : "")
                             + std::to_string(i)
                             + ".png";

        m_textures[i] = loadWithWhiteMask(filename);

        if (m_textures[i].getSize().x <= 1) {
            std::cerr << "[AssetManager] 缺失资源: " << filename << "\n";
            m_complete = false;
        }
    }

    // Boss 意图图标
    const char* intentNames[] = {"intent_attack.png", "intent_defend.png", "intent_buff.png"};
    for (size_t i = 0; i < static_cast<size_t>(IntentIcon::Count); ++i) {
        std::string filename = assetDir + intentNames[i];
        m_intentIcons[i] = loadWithWhiteMask(filename);
        // 意图图标缺失不标记为 incomplete（可选资源）
    }

    const char* extraNames[] = {
        "menu_background.png",
        "battle_background.png",
        "boss_final.png",
        "floor_enemies.png",
        "map_node_icons.png",
        "reward_icons.png"
    };
    for (size_t i = 0; i < static_cast<size_t>(ExtraTexture::Count); ++i) {
        std::string filename = assetDir + extraNames[i];
        m_extraTextures[i] = loadWithWhiteMask(filename);
    }
}

const sf::Texture& AssetManager::getTexture(int index) const
{
    if (index < 0 || index >= kTextureCount) {
        return m_textures[0];
    }
    return m_textures[index];
}

bool AssetManager::isComplete() const noexcept
{
    return m_complete;
}

const sf::Texture& AssetManager::getIntentIcon(IntentIcon icon) const
{
    auto idx = static_cast<size_t>(icon);
    if (idx >= static_cast<size_t>(IntentIcon::Count)) {
        return m_intentIcons[0];
    }
    return m_intentIcons[idx];
}

const sf::Texture& AssetManager::getExtraTexture(ExtraTexture texture) const
{
    auto idx = static_cast<size_t>(texture);
    if (idx >= static_cast<size_t>(ExtraTexture::Count)) {
        return m_extraTextures[0];
    }
    return m_extraTextures[idx];
}

sf::Texture AssetManager::loadWithWhiteMask(const std::string& path)
{
    sf::Texture tex;
    try {
        (void)tex.loadFromFile(path);
    } catch (...) {
        // 加载失败：1×1 品红占位
        sf::Image fallback(sf::Vector2u{1, 1}, sf::Color::Magenta);
        (void)tex.loadFromImage(fallback);
    }
    tex.setSmooth(true);
    return tex;
}
