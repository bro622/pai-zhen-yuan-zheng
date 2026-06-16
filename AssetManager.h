#pragma once

#include <SFML/Graphics.hpp>

#include <array>
#include <string>

// ══════════════════════════════════════════════════════════════════════════════
//  资源管理器
//  职责：一次性加载并缓存全部 9 张纹理（卡背 + 8 张卡面），
//       提供按索引查询纹理的只读接口。
// ══════════════════════════════════════════════════════════════════════════════

class AssetManager {
public:
    // asset_00.png ~ asset_08.png，共 9 张卡牌纹理
    static constexpr int kTextureCount = 9;

    // Boss 意图图标（独立于卡牌纹理）
    enum class IntentIcon : uint8_t { Attack, Defend, Buff, Count };

    enum class ExtraTexture : uint8_t {
        MenuBackground,
        BattleBackground,
        FinalBoss,
        FloorEnemies,
        MapNodeIcons,
        RewardIcons,
        Count
    };

    // 构造时自动从指定目录加载全部资源
    explicit AssetManager(const std::string& assetDir = "assets/");

    // 按索引获取纹理（const 引用，不可修改）
    //   index 0     → 卡背 (asset_00.png)
    //   index 1~8   → 对应 id 0~7 的卡面 (asset_01~08.png)
    const sf::Texture& getTexture(int index) const;

    // 获取 Boss 意图图标
    const sf::Texture& getIntentIcon(IntentIcon icon) const;

    // 获取背景、敌人、地图和奖励等扩展素材
    const sf::Texture& getExtraTexture(ExtraTexture texture) const;

    // 全部是否加载成功（无缺失资源）
    bool isComplete() const noexcept;

private:
    std::array<sf::Texture, kTextureCount> m_textures;
    std::array<sf::Texture, static_cast<size_t>(IntentIcon::Count)> m_intentIcons;
    std::array<sf::Texture, static_cast<size_t>(ExtraTexture::Count)> m_extraTextures;
    bool m_complete;   // 全部加载成功标记

    // 加载单张图片：读取 → 上传到 Texture
    static sf::Texture loadWithWhiteMask(const std::string& path);
};
