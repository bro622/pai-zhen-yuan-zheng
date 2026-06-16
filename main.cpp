#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <random>

#include "RunManager.h"
#include "AssetManager.h"
#include "AudioSystem.h"
#include "CardRenderer.h"

// ══════════════════════════════════════════════════════════════════════════════
//  常量配置
// ══════════════════════════════════════════════════════════════════════════════

static constexpr unsigned int kWinW = 960;
static constexpr unsigned int kWinH = 760;

static constexpr float kTopBarH    = 80.f;
static constexpr float kBottomBarH = 100.f;
static constexpr float kGridPad    = 24.f;
static constexpr float kCardGap    = 12.f;

static const sf::Color kBgColor    (245, 242, 235);      // 淡米色背景
static const sf::Color kGlassPanel (255, 255, 252, 230);  // 浅白面板
static const sf::Color kHPBarBg    (215, 212, 205);       // HP 条底色
static const sf::Color kBossHPFill (220,  80,  80);       // Boss HP
static const sf::Color kPlayerHPFill( 80, 185, 120);      // 玩家 HP
static const sf::Color kBlockColor ( 90, 150, 205);       // 护甲

static constexpr float kBtnW = 160.f;
static constexpr float kBtnH = 44.f;
static const sf::Color kBtnNormal(220, 218, 210);
static const sf::Color kBtnHover(205, 202, 195);
static const sf::Color kBtnText(50, 55, 70);              // 深蓝灰文字

static constexpr float kHelpBtnSize = 36.f;
static const sf::Color kHelpBtnColor(220, 218, 210);

// 主文字色（深蓝灰，不用纯黑）
static const sf::Color kTextPrimary  ( 45,  50,  65);
static const sf::Color kTextSecondary(110, 115, 130);
static const sf::Color kTextMuted    (150, 155, 165);

// 卡牌 ID → 资源索引映射（assets/ 中的文件顺序）
// ID 0=龙→asset_02, ID 1=凤→asset_08, ID 2=虎→asset_04, ID 3=鹤→asset_07,
// ID 4=莲→asset_06, ID 5=竹→asset_05, ID 6=剑→asset_01, ID 7=云→asset_03
static constexpr int kCardAssetMap[8] = {2, 8, 4, 7, 6, 5, 1, 3};
static const std::string kCardTooltips[8] = {
    /* 0 龙 */ "消耗: 2\n造成 14 伤害\n自身 -3 生命",
    /* 1 凤 */ "消耗: 1\n恢复 3 生命\n[消耗]",
    /* 2 虎 */ "消耗: 2\n力量 +2\n[消耗]",
    /* 3 鹤 */ "消耗: 0\n能量 +1\n[消耗]",
    /* 4 莲 */ "消耗: 1\n对 Boss 施加 1 层虚弱",
    /* 5 竹 */ "消耗: 1\n获得 5 护甲",
    /* 6 剑 */ "消耗: 1\n造成 6 伤害",
    /* 7 云 */ "消耗: 1\n造成 3 伤害\n施加 1 层易伤",
};

// 全局帮助面板规则文本（手动 \n 换行）
static const std::string kHelpRules =
    "=== 游戏规则 ===\n"
    "\n"
    "每回合获得能量。\n"
    "点击卡牌打出效果。\n"
    "行动结束后点击[结束回合]。\n"
    "\n"
    "--- 卡牌 ---\n"
    "红框 = 攻击牌\n"
    "蓝框 = 技能牌\n"
    "[消耗] = 本场战斗中移除\n"
    "\n"
    "--- 战斗 ---\n"
    "Boss 会按意图行动。\n"
    "护甲抵消伤害，回合末清空。\n"
    "易伤：受到 1.5 倍伤害。\n"
    "虚弱：Boss 伤害减半。\n"
    "\n"
    "--- 地图 ---\n"
    "战斗胜利后选择奖励和路线。\n"
    "休息点恢复 30% 生命上限。\n"
    "击败第 5 层 Boss 即可通关。\n"
    "\n"
    "按 [?] 或点击外部关闭。";

// ══════════════════════════════════════════════════════════════════════════════
//  动画系统常量
// ══════════════════════════════════════════════════════════════════════════════

static constexpr float kHoverScale     = 1.12f;   // 悬停放大
static constexpr float kClickScale     = 0.80f;   // 点击压缩
static constexpr float kNormalScale    = 1.00f;   // 正常
static constexpr float kScaleLerpSpeed = 12.f;    // 缩放插值速度（越大越快）
static constexpr float kClickBounceTime = 0.15f;  // 点击弹回时间（秒）

static constexpr float kFloatTextLife   = 1.0f;   // 跳字生命周期（秒）
static constexpr float kFloatTextSpeed  = -80.f;  // 跳字上移速度（像素/秒，负=上）

static constexpr float kShakeDuration   = 0.30f;  // 屏幕震动持续时间
static constexpr float kShakeIntensity  = 8.f;    // 震动初始幅度（像素）

// ══════════════════════════════════════════════════════════════════════════════
//  动画数据结构
// ══════════════════════════════════════════════════════════════════════════════

// 每张卡牌的缩放动画状态
struct CardAnim {
    float scale       = 1.f;      // 当前缩放
    float clickTimer  = 0.f;      // 点击弹回倒计时（>0 时锁定在 kClickScale）
};

// 浮动战斗跳字
struct FloatingText {
    sf::Text text;
    float    life;
    float    maxLife;
    sf::Vector2f velocity;

    FloatingText(const sf::Font& font, const std::string& str,
                 const sf::Color& color, float x, float y,
                 float lifeTime, sf::Vector2f vel)
        : text(font, sf::String::fromUtf8(str.begin(), str.end()), 22)
        , life(lifeTime)
        , maxLife(lifeTime)
        , velocity(vel)
    {
        text.setFillColor(color);
        text.setPosition(sf::Vector2f{x, y});
    }
};

// 屏幕震动状态
struct ScreenShake {
    float timer     = 0.f;
    float intensity = 0.f;
};

// ══════════════════════════════════════════════════════════════════════════════
//  UI 工具函数
// ══════════════════════════════════════════════════════════════════════════════

static void drawHPBar(sf::RenderWindow& win,
                      float x, float y, float w, float h,
                      int currentHP, int maxHP,
                      const sf::Color& fillColor,
                      const std::string& label,
                      const sf::Font& font)
{
    sf::RectangleShape panel(sf::Vector2f{w + 32.f, h + 24.f});
    panel.setPosition(sf::Vector2f{x - 16.f, y - 12.f});
    panel.setFillColor(kGlassPanel);
    panel.setOutlineColor(sf::Color(200, 198, 190));
    panel.setOutlineThickness(1.f);
    win.draw(panel);

    sf::RectangleShape bg(sf::Vector2f{w, h});
    bg.setPosition(sf::Vector2f{x, y});
    bg.setFillColor(kHPBarBg);
    bg.setOutlineColor(sf::Color(195, 192, 185));
    bg.setOutlineThickness(1.f);
    win.draw(bg);

    float ratio = (maxHP > 0) ? std::clamp(static_cast<float>(currentHP) / maxHP, 0.f, 1.f) : 0.f;
    sf::RectangleShape fill(sf::Vector2f{w * ratio, h});
    fill.setPosition(sf::Vector2f{x, y});
    fill.setFillColor(fillColor);
    win.draw(fill);

    std::string text = label + "  " + std::to_string(currentHP) + " / " + std::to_string(maxHP);
    sf::String display = sf::String::fromUtf8(text.begin(), text.end());
    sf::Text labelTxt(font, display, 16);
    labelTxt.setFillColor(kTextPrimary);
    auto bounds = labelTxt.getLocalBounds();
    labelTxt.setPosition(sf::Vector2f{
        x + (w - bounds.size.x) * 0.5f,
        y + (h - bounds.size.y) * 0.5f - bounds.position.y
    });
    win.draw(labelTxt);
}

static sf::Text makeText(const sf::Font& font, const std::string& str,
                          unsigned int size, const sf::Color& color,
                          float cx, float cy)
{
    sf::String display = sf::String::fromUtf8(str.begin(), str.end());
    sf::Text txt(font, display, size);
    txt.setFillColor(color);
    auto b = txt.getLocalBounds();
    txt.setPosition(sf::Vector2f{cx - b.size.x * 0.5f, cy - b.size.y * 0.5f - b.position.y});
    return txt;
}

static sf::FloatRect getEndTurnBtnRect()
{
    float bx = static_cast<float>(kWinW) - kGridPad - kBtnW;
    float by = static_cast<float>(kWinH) - kBottomBarH + 28.f;
    return sf::FloatRect(sf::Vector2f{bx, by}, sf::Vector2f{kBtnW, kBtnH});
}

static sf::FloatRect getMenuButtonRect(int index)
{
    float cx = static_cast<float>(kWinW) * 0.5f;
    float y = 380.f + index * 58.f;
    return sf::FloatRect(sf::Vector2f{cx - 110.f, y}, sf::Vector2f{220.f, 44.f});
}

static void drawButton(sf::RenderWindow& win, const sf::Font& font,
                       const sf::FloatRect& rect, const std::string& label,
                       bool hover, const sf::Color& accent)
{
    sf::RectangleShape btn(rect.size);
    btn.setPosition(rect.position);
    btn.setFillColor(hover ? sf::Color(210, 208, 200) : sf::Color(232, 229, 220));
    btn.setOutlineColor(hover ? accent : sf::Color(190, 188, 180));
    btn.setOutlineThickness(hover ? 2.f : 1.f);
    win.draw(btn);

    sf::String display = sf::String::fromUtf8(label.begin(), label.end());
    sf::Text txt(font, display, 18);
    txt.setFillColor(kBtnText);
    auto b = txt.getLocalBounds();
    txt.setPosition(sf::Vector2f{
        rect.position.x + (rect.size.x - b.size.x) * 0.5f,
        rect.position.y + (rect.size.y - b.size.y) * 0.5f - b.position.y
    });
    win.draw(txt);
}

static const char* getIntentName(IntentType intent)
{
    switch (intent) {
        case IntentType::Attack:       return "攻击";
        case IntentType::Defend:       return "防御";
        case IntentType::StrongAttack: return "强攻";
    }
    return "意图";
}

static const char* getCardTypeName(int id)
{
    switch (id) {
        case 0:
        case 6:
        case 7:
            return "攻";
        case 2:
        case 3:
            return "力";
        default:
            return "技";
    }
}

static sf::Color getCardTypeColor(int id)
{
    switch (id) {
        case 0:
        case 6:
        case 7:
            return sf::Color(220, 90, 75);
        case 2:
        case 3:
            return sf::Color(190, 140, 60);
        default:
            return sf::Color(70, 145, 205);
    }
}

static Sound getCardSfx(int cardId)
{
    switch (cardId) {
        case 0: return Sound::HeavyAttack;   // 龙：大伤害
        case 1: return Sound::Heal;          // 凤：回血
        case 2: return Sound::BluntAttack;   // 虎：重击/力量
        case 3: return Sound::StrengthUp;    // 鹤：能量回升
        case 4: return Sound::DoomApply;     // 莲：虚弱施加
        case 5: return Sound::BlockUp;       // 竹：护甲
        case 6: return Sound::SlashAttack;   // 剑：轻攻击
        case 7: return Sound::DaggerThrow;   // 云：弹药/飞镖
        default: return Sound::CardSelect;
    }
}

static void drawEndTurnBtn(sf::RenderWindow& win, const sf::Font& font, bool hover)
{
    auto r = getEndTurnBtnRect();
    sf::RectangleShape btn(r.size);
    btn.setPosition(r.position);
    btn.setFillColor(hover ? kBtnHover : kBtnNormal);
    btn.setOutlineColor(sf::Color(190, 188, 180));
    btn.setOutlineThickness(1.f);
    win.draw(btn);

    const std::string endTurnLabel = "结束回合";
    sf::String endTurnDisplay = sf::String::fromUtf8(endTurnLabel.begin(), endTurnLabel.end());
    sf::Text txt(font, endTurnDisplay, 18);
    txt.setFillColor(kBtnText);
    auto b = txt.getLocalBounds();
    txt.setPosition(sf::Vector2f{
        r.position.x + (r.size.x - b.size.x) * 0.5f,
        r.position.y + (r.size.y - b.size.y) * 0.5f - b.position.y
    });
    win.draw(txt);
}

// ══════════════════════════════════════════════════════════════════════════════
//  Boss 意图绘制
// ══════════════════════════════════════════════════════════════════════════════

static void drawBossIntent(sf::RenderWindow& win, const sf::Font& font,
                           const AssetManager& assets,
                           IntentType intent, int value,
                           float x, float y)
{
    // 意图图标映射
    AssetManager::IntentIcon icon = AssetManager::IntentIcon::Attack;
    sf::Color valueColor(255, 80, 80);  // 默认红色（攻击）

    switch (intent) {
        case IntentType::Attack:
            icon = AssetManager::IntentIcon::Attack;
            valueColor = sf::Color(255, 80, 80);
            break;
        case IntentType::Defend:
            icon = AssetManager::IntentIcon::Defend;
            valueColor = sf::Color(80, 160, 255);
            break;
        case IntentType::StrongAttack:
            icon = AssetManager::IntentIcon::Attack;
            valueColor = sf::Color(255, 40, 40);
            break;
    }

    const sf::Texture& tex = assets.getIntentIcon(icon);
    auto texSize = tex.getSize();

    // 图标尺寸（固定 40x40）
    constexpr float iconSize = 40.f;

    // 底座尺寸（图标 + 数值 + padding）
    constexpr float padX = 12.f, padY = 8.f;
    float boxW = iconSize + 50.f + padX * 2.f;
    float boxH = iconSize + padY * 2.f;

    // 底座背景（半透明圆角模拟）
    sf::RectangleShape bg(sf::Vector2f{boxW, boxH});
    bg.setPosition(sf::Vector2f{x, y});
    bg.setFillColor(sf::Color(255, 255, 250, 230));
    bg.setOutlineColor(sf::Color(200, 198, 190, 180));
    bg.setOutlineThickness(1.f);
    win.draw(bg);

    // 意图图标
    sf::Sprite iconSprite(tex);
    iconSprite.setScale(sf::Vector2f{
        iconSize / static_cast<float>(texSize.x),
        iconSize / static_cast<float>(texSize.y)});
    iconSprite.setPosition(sf::Vector2f{x + padX, y + padY});
    win.draw(iconSprite);

    // 意图数值
    std::string valueStr = std::to_string(value);
    sf::Text valueTxt(font, valueStr, 20);
    valueTxt.setFillColor(valueColor);
    auto vb = valueTxt.getLocalBounds();
    valueTxt.setPosition(sf::Vector2f{
        x + padX + iconSize + 8.f,
        y + (boxH - vb.size.y) * 0.5f - vb.position.y});
    win.draw(valueTxt);
}

static void drawOverlayPanel(sf::RenderWindow& win, float boxW, float boxH)
{
    sf::RectangleShape dim(sf::Vector2f{static_cast<float>(kWinW), static_cast<float>(kWinH)});
    dim.setFillColor(sf::Color(245, 242, 235, 180));
    win.draw(dim);

    float bx = (kWinW - boxW) * 0.5f;
    float by = (kWinH - boxH) * 0.5f;
    sf::RectangleShape box(sf::Vector2f{boxW, boxH});
    box.setPosition(sf::Vector2f{bx, by});
    box.setFillColor(kGlassPanel);
    box.setOutlineColor(sf::Color(200, 198, 190));
    box.setOutlineThickness(1.5f);
    win.draw(box);
}

static void drawCoverTexture(sf::RenderWindow& win, const sf::Texture& tex,
                             float alpha = 255.f,
                             sf::Vector2f offset = sf::Vector2f{0.f, 0.f})
{
    sf::Sprite sprite(tex);
    auto size = tex.getSize();
    float sx = static_cast<float>(kWinW) / static_cast<float>(size.x);
    float sy = static_cast<float>(kWinH) / static_cast<float>(size.y);
    float scale = std::max(sx, sy) * 1.03f;
    sprite.setScale(sf::Vector2f{scale, scale});
    sprite.setPosition(sf::Vector2f{
        (static_cast<float>(kWinW) - static_cast<float>(size.x) * scale) * 0.5f + offset.x,
        (static_cast<float>(kWinH) - static_cast<float>(size.y) * scale) * 0.5f + offset.y
    });
    sprite.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
    win.draw(sprite);
}

static void drawSheetIcon(sf::RenderWindow& win, const sf::Texture& tex,
                          int index, int cols, int rows,
                          float x, float y, float size,
                          float scaleMul = 1.f)
{
    auto texSize = tex.getSize();
    int cellW = static_cast<int>(texSize.x) / cols;
    int cellH = static_cast<int>(texSize.y) / rows;
    int col = index % cols;
    int row = index / cols;

    sf::Sprite sprite(tex);
    sprite.setTextureRect(sf::IntRect(
        sf::Vector2i{col * cellW, row * cellH},
        sf::Vector2i{cellW, cellH}));
    float finalSize = size * scaleMul;
    sprite.setScale(sf::Vector2f{
        finalSize / static_cast<float>(cellW),
        finalSize / static_cast<float>(cellH)});
    sprite.setPosition(sf::Vector2f{
        x - (finalSize - size) * 0.5f,
        y - (finalSize - size) * 0.5f});
    win.draw(sprite);
}

static int getMapIconIndex(NodeType type)
{
    switch (type) {
        case NodeType::Battle: return 0;
        case NodeType::Rest:   return 1;
        case NodeType::Boss:   return 2;
    }
    return 0;
}

static const char* getEnemyName(int tier)
{
    static const char* kNames[5] = {
        "训练面具",
        "藤盾守卫",
        "晶刃术士",
        "余烬冠军",
        "牌阵之主"
    };
    return kNames[std::clamp(tier, 0, 4)];
}

static void drawEnemyPortrait(sf::RenderWindow& win, const AssetManager& assets,
                              int tier, float cx, float y, float size)
{
    sf::RectangleShape plate(sf::Vector2f{size + 24.f, size + 42.f});
    plate.setPosition(sf::Vector2f{cx - (size + 24.f) * 0.5f, y - 12.f});
    plate.setFillColor(sf::Color(255, 255, 252, 220));
    plate.setOutlineColor(sf::Color(200, 198, 190));
    plate.setOutlineThickness(1.f);
    win.draw(plate);

    if (tier >= 4) {
        const sf::Texture& tex = assets.getExtraTexture(AssetManager::ExtraTexture::FinalBoss);
        drawSheetIcon(win, tex, 0, 1, 1, cx - size * 0.5f, y, size);
    } else {
        const sf::Texture& tex = assets.getExtraTexture(AssetManager::ExtraTexture::FloorEnemies);
        drawSheetIcon(win, tex, tier, 2, 2, cx - size * 0.5f, y, size);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  动画工具
// ══════════════════════════════════════════════════════════════════════════════

static float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float lerpDt(float current, float target, float speed, float dt)
{
    // 指数衰减插值，永远不到达但肉眼无差距
    return lerp(current, target, 1.f - std::exp(-speed * dt));
}

// 生成浮动跳字
static void spawnFloatText(std::vector<FloatingText>& texts,
                           const sf::Font& font,
                           const std::string& str,
                           const sf::Color& color,
                           float x, float y)
{
    texts.emplace_back(font, str, color, x, y,
                       kFloatTextLife,
                       sf::Vector2f{0.f, kFloatTextSpeed});
}

// 更新全部跳字
static void updateFloatTexts(std::vector<FloatingText>& texts, float dt)
{
    for (auto& ft : texts) {
        ft.life -= dt;
        ft.text.move(sf::Vector2f{ft.velocity.x * dt, ft.velocity.y * dt});

        // alpha 渐变
        float alpha = std::clamp(ft.life / ft.maxLife, 0.f, 1.f);
        sf::Color c = ft.text.getFillColor();
        c.a = static_cast<std::uint8_t>(alpha * 255.f);
        ft.text.setFillColor(c);
    }

    // 移除已死亡的跳字
    texts.erase(
        std::remove_if(texts.begin(), texts.end(),
                       [](const FloatingText& ft) { return ft.life <= 0.f; }),
        texts.end());
}

// 更新屏幕震动
static void updateShake(ScreenShake& shake, float dt)
{
    if (shake.timer > 0.f) {
        shake.timer -= dt;
        if (shake.timer <= 0.f) {
            shake.timer = 0.f;
        }
    }
}

// 触发屏幕震动
static void triggerShake(ScreenShake& shake)
{
    shake.timer     = kShakeDuration;
    shake.intensity = kShakeIntensity;
}

// 获取震动偏移
static sf::Vector2f getShakeOffset(ScreenShake& shake)
{
    if (shake.timer <= 0.f) return sf::Vector2f{0.f, 0.f};

    float decay = shake.timer / kShakeDuration;   // 1.0 → 0.0
    float amp   = shake.intensity * decay;

    // 简易伪随机：用 timer 的小数部分做 sin 偏移
    static std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    float ox = dist(rng) * amp;
    float oy = dist(rng) * amp;
    return sf::Vector2f{ox, oy};
}

// ══════════════════════════════════════════════════════════════════════════════
//  主函数
// ══════════════════════════════════════════════════════════════════════════════

int main()
{
    const std::string windowTitle = "牌阵远征";
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u{kWinW, kWinH}),
                            sf::String::fromUtf8(windowTitle.begin(), windowTitle.end()),
                            sf::Style::Close);
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/msyh.ttc"))
        (void)font.openFromFile("C:/Windows/Fonts/arial.ttf");

    AssetManager assets("assets/");
    CardRenderer renderer(kWinW, kWinH, kTopBarH, kBottomBarH, kGridPad, kCardGap);
    RunManager   run;

    // ── 动画状态 ──────────────────────────────────────────────────────────
    sf::Clock dtClock;
    std::array<CardAnim, GameLogic::kTotalCards> cardAnims{};
    std::vector<FloatingText> floatTexts;
    ScreenShake shake;
    sf::View    defaultView = window.getDefaultView();
    bool        isHelpOpen  = false;
    bool        showTutorial = false;
    bool        tutorialSeen = false;
    float       animTime = 0.f;

    // ── 音效系统 ──────────────────────────────────────────────────────────
    AudioSystem audio;
    int  prevHoveredCard = -1;
    bool firstMapOpen = true;

    // ══════════════════════════════════════════════════════════════════════════
    //  主循环
    // ══════════════════════════════════════════════════════════════════════════
    while (window.isOpen())
    {
        // ── Delta Time ───────────────────────────────────────────────────
        float dt = dtClock.restart().asSeconds();
        dt = std::clamp(dt, 0.f, 0.05f);  // 防止卡顿时 dt 爆炸
        animTime += dt;

        const RunState rs = run.getRunState();
        const sf::Vector2f mousePos = sf::Vector2f(
            static_cast<float>(sf::Mouse::getPosition(window).x),
            static_cast<float>(sf::Mouse::getPosition(window).y));

        const bool btnHover = (rs == RunState::Battle &&
                               run.getBattle().isPlayerTurn() &&
                               getEndTurnBtnRect().contains(mousePos));

        // ── 卡牌悬停检测（仅战斗中玩家回合）─────────────────────────────
        int hoveredCard = -1;
        if (rs == RunState::Battle && run.getBattle().isPlayerTurn()) {
            hoveredCard = renderer.hitTest(mousePos.x, mousePos.y);
            // 排除已销毁的卡牌
            if (hoveredCard >= 0 && run.getBattle().getCards()[hoveredCard].destroyed)
                hoveredCard = -1;
        }
        // 悬停音效（仅在进入新卡牌时触发）
        if (hoveredCard >= 0 && hoveredCard != prevHoveredCard) {
            audio.play(Sound::MapHover);
        }
        prevHoveredCard = hoveredCard;

        // ── 更新卡牌缩放动画 ────────────────────────────────────────────
        for (int i = 0; i < GameLogic::kTotalCards; ++i) {
            auto& anim = cardAnims[i];

            if (anim.clickTimer > 0.f) {
                // 点击弹回阶段：从 kClickScale 弹回 kNormalScale
                anim.clickTimer -= dt;
                float t = 1.f - (anim.clickTimer / kClickBounceTime);
                t = std::clamp(t, 0.f, 1.f);
                // ease-out 弹性
                anim.scale = lerp(kClickScale, kNormalScale, t * t);
            } else {
                // 正常悬停插值
                float target = (i == hoveredCard) ? kHoverScale : kNormalScale;
                anim.scale = lerpDt(anim.scale, target, kScaleLerpSpeed, dt);
            }
        }

        // ── 更新跳字 & 震动 ─────────────────────────────────────────────
        updateFloatTexts(floatTexts, dt);
        updateShake(shake, dt);

        // ── PHASE 1: 事件轮询 ─────────────────────────────────────────────
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                break;
            }

            // ── 帮助按钮 / 关闭帮助（全局拦截）──────────────────────────
            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb->button == sf::Mouse::Button::Left) {
                    float mx = static_cast<float>(mb->position.x);
                    float my = static_cast<float>(mb->position.y);

                    // 帮助按钮区域（右上角）
                    sf::FloatRect helpBtnRect(
                        sf::Vector2f{static_cast<float>(kWinW) - kGridPad - kHelpBtnSize,
                                     kGridPad},
                        sf::Vector2f{kHelpBtnSize, kHelpBtnSize});

                    if (helpBtnRect.contains(sf::Vector2f{mx, my})) {
                        isHelpOpen = !isHelpOpen;
                        continue;  // 拦截：不传递给后续逻辑
                    }

                    // 帮助面板打开时：点击任意处关闭帮助，但不触发游戏逻辑
                    if (isHelpOpen) {
                        isHelpOpen = false;
                        continue;  // 拦截
                    }
                }
            }

            // 帮助面板打开时：拦截所有键盘事件（除了 ? 键关闭）
            if (isHelpOpen) {
                if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                    if (kp->code == sf::Keyboard::Key::Slash ||
                        kp->code == sf::Keyboard::Key::H) {
                        isHelpOpen = false;
                    }
                }
                continue;  // 帮助打开时跳过所有其他事件处理
            }

            // ? 键呼出帮助
            if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                if (kp->code == sf::Keyboard::Key::Slash ||
                    kp->code == sf::Keyboard::Key::H) {
                    isHelpOpen = true;
                    continue;
                }
            }

            // ── 主菜单 ──────────────────────────────────────────────────
            if (rs == RunState::MainMenu) {
                if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                    if (kp->code == sf::Keyboard::Key::Enter ||
                        kp->code == sf::Keyboard::Key::Space) {
                        run.startNewRun();
                        audio.play(Sound::MapPing);
                        audio.play(Sound::BattleStart1);
                        if (!tutorialSeen) {
                            showTutorial = true;
                            tutorialSeen = true;
                        }
                        continue;
                    }
                    if (kp->code == sf::Keyboard::Key::Escape) {
                        window.close();
                    }
                }

                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        sf::Vector2f mp{
                            static_cast<float>(mb->position.x),
                            static_cast<float>(mb->position.y)
                        };
                        if (getMenuButtonRect(0).contains(mp)) {
                            run.startNewRun();
                            audio.play(Sound::MapPing);
                            audio.play(Sound::BattleStart1);
                            if (!tutorialSeen) {
                                showTutorial = true;
                                tutorialSeen = true;
                            }
                            continue;
                        } else if (getMenuButtonRect(1).contains(mp)) {
                            isHelpOpen = true;
                            audio.play(Sound::UiClick);
                            continue;
                        } else if (getMenuButtonRect(2).contains(mp)) {
                            audio.play(Sound::UiClick);
                            window.close();
                            continue;
                        }
                    }
                }
            }

            if (showTutorial) {
                if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                    if (kp->code == sf::Keyboard::Key::Enter ||
                        kp->code == sf::Keyboard::Key::Space ||
                        kp->code == sf::Keyboard::Key::Escape) {
                        showTutorial = false;
                        audio.play(Sound::UiClick);
                    }
                }
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        showTutorial = false;
                        audio.play(Sound::UiClick);
                    }
                }
                continue;
            }

            // ── 战斗中 ──────────────────────────────────────────────────
            if (rs == RunState::Battle) {
                const auto& battle = run.getBattle();

                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        float mx = static_cast<float>(mb->position.x);
                        float my = static_cast<float>(mb->position.y);

                        if (battle.isPlayerTurn()) {
                            if (getEndTurnBtnRect().contains(sf::Vector2f{mx, my})) {
                                // End Turn → 触发 Boss 攻击震动
                                ActionResult ar = run.endTurn();
                                if (ar.event == BattleEvent::BossAttacked) {
                                    audio.play(Sound::BossAttack);
                                    triggerShake(shake);
                                    // 在玩家状态栏上方生成跳字
                                    float fy = static_cast<float>(kWinH) - kBottomBarH - 10.f;
                                    spawnFloatText(floatTexts, font,
                                        "-" + std::to_string(ar.value),
                                        sf::Color(255, 80, 80),
                                        static_cast<float>(kWinW) * 0.5f, fy);
                                }
                            } else {
                                int idx = renderer.hitTest(mx, my);
                                if (idx >= 0) {
                                    // 拒绝音效：卡牌已消耗或能量不足
                                    const auto& cards = run.getBattle().getCards();
                                    if (cards[idx].destroyed ||
                                        run.getBattle().getEnergy() < GameLogic::getCardCost(cards[idx].id)) {
                                        audio.play(Sound::Deny);
                                    } else {
                                        audio.play(Sound::CardSelect);
                                    }
                                    ActionResult ar = run.playCard(idx);

                                    // 卡牌网格中心坐标
                                    auto layout = renderer.getLayout();
                                    int cr = idx / GameLogic::kGridCols;
                                    int cc = idx % GameLogic::kGridCols;
                                    float cardX = layout.originX
                                                + cc * (layout.cellW + layout.cardGap)
                                                + layout.cellW * 0.5f;
                                    float cardY = layout.originY
                                                + cr * (layout.cellH + layout.cardGap);

                                    // Boss HP 条中心 Y
                                    float bossY = 40.f;

                                    // 玩家 HP 条上方 Y
                                    float playerY = static_cast<float>(kWinH)
                                                  - kBottomBarH - 10.f;

                                    switch (ar.event) {
                                    case BattleEvent::AttackPlayed:
                                        spawnFloatText(floatTexts, font,
                                            "-" + std::to_string(ar.value),
                                            sf::Color(255, 80, 80), cardX, cardY);
                                        cardAnims[idx].clickTimer = kClickBounceTime;
                                        cardAnims[idx].scale = kClickScale;
                                        break;

                                    case BattleEvent::SelfDamagePlayed:
                                        // 龙：Boss 扣血 + 玩家自伤
                                        spawnFloatText(floatTexts, font,
                                            "-" + std::to_string(ar.value),
                                            sf::Color(255, 80, 80), cardX, cardY);
                                        spawnFloatText(floatTexts, font,
                                            "自身 -3",
                                            sf::Color(255, 160, 60),
                                            static_cast<float>(kWinW) * 0.3f, playerY);
                                        cardAnims[idx].clickTimer = kClickBounceTime;
                                        cardAnims[idx].scale = kClickScale;
                                        break;

                                    case BattleEvent::VulnerableApplied:
                                        spawnFloatText(floatTexts, font,
                                            "-" + std::to_string(ar.value),
                                            sf::Color(255, 80, 80), cardX, cardY);
                                        spawnFloatText(floatTexts, font,
                                            "易伤",
                                            sf::Color(255, 200, 60),
                                            static_cast<float>(kWinW) * 0.5f, bossY);
                                        cardAnims[idx].clickTimer = kClickBounceTime;
                                        cardAnims[idx].scale = kClickScale;
                                        break;

                                    case BattleEvent::DefensePlayed:
                                        spawnFloatText(floatTexts, font,
                                            "+" + std::to_string(ar.value) + " 护甲",
                                            sf::Color(100, 180, 255), cardX, cardY);
                                        cardAnims[idx].clickTimer = kClickBounceTime;
                                        cardAnims[idx].scale = kClickScale;
                                        break;

                                    case BattleEvent::StrengthGained:
                                        spawnFloatText(floatTexts, font,
                                            "+" + std::to_string(ar.value) + " 力量",
                                            sf::Color(255, 140, 60), cardX, cardY);
                                        cardAnims[idx].clickTimer = kClickBounceTime;
                                        cardAnims[idx].scale = kClickScale;
                                        break;

                                    case BattleEvent::WeakApplied:
                                        spawnFloatText(floatTexts, font,
                                            "虚弱",
                                            sf::Color(180, 100, 255),
                                            static_cast<float>(kWinW) * 0.5f, bossY);
                                        cardAnims[idx].clickTimer = kClickBounceTime;
                                        cardAnims[idx].scale = kClickScale;
                                        break;

                                    case BattleEvent::EnergyRefunded:
                                        spawnFloatText(floatTexts, font,
                                            "+1 能量",
                                            sf::Color(200, 180, 100), cardX, cardY);
                                        cardAnims[idx].clickTimer = kClickBounceTime;
                                        cardAnims[idx].scale = kClickScale;
                                        break;

                                    case BattleEvent::HealPlayed:
                                        spawnFloatText(floatTexts, font,
                                            "+" + std::to_string(ar.value) + " 生命",
                                            sf::Color(100, 220, 140), cardX, playerY);
                                        cardAnims[idx].clickTimer = kClickBounceTime;
                                        cardAnims[idx].scale = kClickScale;
                                        break;

                                    default:
                                        break;
                                    }
                                    // 打出卡牌音效
                                    audio.play(getCardSfx(cards[idx].id));
                                }
                            }
                        } else if (battle.isGameOver()) {
                            audio.play(battle.isBattleWon() ? Sound::Victory : Sound::Defeat);
                            run.onBattleResult();
                        }
                    }
                }

                if (battle.isGameOver()) {
                    if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                        (void)kp;
                        audio.play(battle.isBattleWon() ? Sound::Victory : Sound::Defeat);
                        run.onBattleResult();
                    }
                }
            }

            // ── 地图导航（点击节点选择路线）─────────────────────────────
            if (rs == RunState::MapNavigation) {
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        float mx = static_cast<float>(mb->position.x);
                        float my = static_cast<float>(mb->position.y);

                        // 检测点击了哪个可达节点（坐标与渲染保持一致）
                        auto nodes = run.getAvailableNodes();
                        float nodeW = 180.f, nodeH = 80.f;
                        float gap = 30.f;
                        float totalW = nodes.size() * nodeW + (nodes.size() - 1) * gap;
                        float startX = (static_cast<float>(kWinW) - totalW) * 0.5f;
                        float overlayBy = (static_cast<float>(kWinH) - 320.f) * 0.5f;
                        float nodeY = overlayBy + 110.f;

                        for (size_t i = 0; i < nodes.size(); ++i) {
                            float nx = startX + i * (nodeW + gap);
                            if (mx >= nx && mx <= nx + nodeW &&
                                my >= nodeY && my <= nodeY + nodeH) {
                                audio.play(Sound::UiClick);
                                run.chooseNode(nodes[i].index);
                                // 根据节点类型播放音效
                                if (nodes[i].type == NodeType::Rest)
                                    audio.play(Sound::RestJingle);
                                else
                                    audio.play(Sound::MapPing);
                                break;
                            }
                        }
                    }
                }
            }

            // ── 战斗奖励（三选一）───────────────────────────────────────
            if (rs == RunState::RewardSelection) {
                if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (mb->button == sf::Mouse::Button::Left) {
                        float mx = static_cast<float>(mb->position.x);
                        float my = static_cast<float>(mb->position.y);
                        float cardW = 250.f, cardH = 150.f, gap = 24.f;
                        float totalW = cardW * 3.f + gap * 2.f;
                        float startX = (static_cast<float>(kWinW) - totalW) * 0.5f;
                        float y = 315.f;

                        for (int i = 0; i < 3; ++i) {
                            sf::FloatRect rect(
                                sf::Vector2f{startX + i * (cardW + gap), y},
                                sf::Vector2f{cardW, cardH});
                            if (rect.contains(sf::Vector2f{mx, my})) {
                                audio.play(Sound::CardSelect);
                                run.chooseReward(i);
                                break;
                            }
                        }
                    }
                }

                if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                    if (kp->code == sf::Keyboard::Key::Num1) run.chooseReward(0);
                    if (kp->code == sf::Keyboard::Key::Num2) run.chooseReward(1);
                    if (kp->code == sf::Keyboard::Key::Num3) run.chooseReward(2);
                }
            }

            // ── 休息营地 ──────────────────────────────────────────────
            if (rs == RunState::RestSite) {
                if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                    if (kp->code == sf::Keyboard::Key::Enter ||
                        kp->code == sf::Keyboard::Key::Space) {
                        audio.play(Sound::CardDeal);
                        run.healAndAdvance();
                    }
                }
            }

            // ── 通关 / 死亡 ──────────────────────────────────────────
            if (rs == RunState::RunVictory || rs == RunState::RunDefeat) {
                if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                    if (kp->code == sf::Keyboard::Key::R) {
                        run.startNewRun();
                        audio.play(Sound::BattleStart1);
                        cardAnims = {};
                        floatTexts.clear();
                        showTutorial = false;
                    }
                }
            }
        }

        // ── PHASE 2: 渲染（应用屏幕震动）──────────────────────────────
        sf::View view = defaultView;
        sf::Vector2f shakeOff = getShakeOffset(shake);
        view.move(shakeOff);
        window.setView(view);

        window.clear(kBgColor);
        sf::Vector2f bgDrift{
            std::sin(animTime * 0.18f) * 10.f,
            std::cos(animTime * 0.14f) * 7.f
        };
        if (rs == RunState::MainMenu) {
            drawCoverTexture(window,
                             assets.getExtraTexture(AssetManager::ExtraTexture::MenuBackground),
                             180.f, bgDrift);
        } else {
            drawCoverTexture(window,
                             assets.getExtraTexture(AssetManager::ExtraTexture::BattleBackground),
                             115.f, bgDrift * 0.6f);
        }

        const float cx = kWinW * 0.5f;
        const float cy = kWinH * 0.5f;

        // ── 主菜单 ──────────────────────────────────────────────────────
        if (rs == RunState::MainMenu) {
            float titleBob = std::sin(animTime * 1.4f) * 4.f;
            window.draw(makeText(font, "牌阵远征", 56, kTextPrimary, cx, 185.f + titleBob));
            window.draw(makeText(font, "回合制明牌策略", 19, kTextSecondary, cx, 250.f + titleBob * 0.4f));
            window.draw(makeText(font, "规划能量，预判 Boss，闯过五层试炼。", 16,
                        kTextMuted, cx, 285.f));

            for (int i = 0; i < 3; ++i) {
                sf::FloatRect rect = getMenuButtonRect(i);
                bool hover = rect.contains(mousePos);
                const std::string label = (i == 0) ? "新的征程"
                                        : (i == 1) ? "玩法说明"
                                                   : "退出游戏";
                if (i == 0 && !hover) {
                    rect.position.y += std::sin(animTime * 2.2f) * 1.5f;
                }
                drawButton(window, font, rect, label, hover,
                           i == 0 ? sf::Color(80, 170, 110) : kTextSecondary);
            }

            window.draw(makeText(font, "Enter：开始   H 或 ?：规则   Esc：退出", 13,
                        kTextMuted, cx, 675.f));
        }

        // ── 战斗中 ──────────────────────────────────────────────────────
        if (rs == RunState::Battle) {
            const auto& battle = run.getBattle();
            int enemyTier = std::clamp(run.getMap().getCurrentFloor(), 0, 4);

            std::string floorText = "第 " + std::to_string(run.getMap().getCurrentFloor() + 1)
                                  + " 层 / "
                                  + std::to_string(MapManager::kTotalFloors);
            window.draw(makeText(font, floorText, 16, kTextSecondary, cx, 6.f));

            float enemyBob = std::sin(animTime * (enemyTier >= 4 ? 1.8f : 1.3f)) * 4.f;
            float enemyPulse = (enemyTier >= 4)
                             ? 1.f + std::sin(animTime * 2.6f) * 0.035f
                             : 1.f + std::sin(animTime * 1.9f) * 0.02f;
            drawEnemyPortrait(window, assets, enemyTier,
                              static_cast<float>(kWinW) - 92.f,
                              72.f + enemyBob,
                              72.f * enemyPulse);
            window.draw(makeText(font, getEnemyName(enemyTier), 14, kTextPrimary,
                        static_cast<float>(kWinW) - 92.f, 150.f));

            drawHPBar(window, 40.f, 24.f, static_cast<float>(kWinW) - 80.f, 32.f,
                      battle.getBossHP(), battle.getBossInitHP(),
                      kBossHPFill, "Boss", font);

            // Boss 意图图标（HP 条下方）
            if (!battle.isGameOver()) {
                drawBossIntent(window, font, assets,
                               battle.getBossIntent(), battle.getBossIntentValue(),
                               40.f, 64.f);
                std::string intentText = std::string("下一步：")
                                       + getIntentName(battle.getBossIntent())
                                       + " " + std::to_string(battle.getBossIntentValue());
                window.draw(makeText(font, intentText, 15, kTextSecondary,
                            165.f, 128.f));
            }

            float hpBarW = (static_cast<float>(kWinW) - 80.f - kBtnW - 20.f) * 0.5f;
            drawHPBar(window, 40.f, static_cast<float>(kWinH) - kBottomBarH + 24.f,
                      hpBarW, 32.f,
                      battle.getPlayerHP(), battle.getPlayerMaxHP(),
                      kPlayerHPFill, "生命", font);

            int maxBlock = std::max(battle.getBlock(), 16);
            drawHPBar(window, 40.f + hpBarW + 12.f,
                      static_cast<float>(kWinH) - kBottomBarH + 24.f,
                      hpBarW, 32.f,
                      battle.getBlock(), maxBlock,
                      kBlockColor, "护甲", font);

            std::string energyStr = "能量：" + std::to_string(battle.getEnergy())
                                  + " / " + std::to_string(battle.getMaxEnergy());
            window.draw(makeText(font, energyStr, 18, sf::Color(160, 140, 60),
                        100.f, static_cast<float>(kWinH) - 24.f));

            std::string statStr = "力量 " + std::to_string(battle.getStrength())
                                + "   Boss 虚弱 " + std::to_string(battle.getWeak())
                                + "   Boss 易伤 " + std::to_string(battle.getVulnerable());
            window.draw(makeText(font, statStr, 14, kTextMuted,
                        static_cast<float>(kWinW) * 0.5f,
                        static_cast<float>(kWinH) - 22.f));

            if (battle.isPlayerTurn()) {
                drawEndTurnBtn(window, font, btnHover);
                window.draw(makeText(font, "玩家回合：打出卡牌，或结束回合", 15,
                            kTextSecondary, cx, 132.f));
            }

            // ── 绘制卡牌网格（带动画缩放）────────────────────────────
            {
                const auto& cards = battle.getCards();
                auto layout = renderer.getLayout();

                for (int r = 0; r < GameLogic::kGridRows; ++r) {
                    for (int c = 0; c < GameLogic::kGridCols; ++c) {
                        int idx = r * GameLogic::kGridCols + c;
                        const Card& card = cards[idx];

                        float x = layout.originX + c * (layout.cellW + layout.cardGap);
                        float y = layout.originY + r * (layout.cellH + layout.cardGap);

                        // 槽底色（始终绘制）
                        sf::RectangleShape slot(sf::Vector2f{layout.cellW, layout.cellH});
                        slot.setPosition(sf::Vector2f{x, y});
                        slot.setFillColor(sf::Color(235, 232, 225));
                        slot.setOutlineColor(sf::Color(210, 207, 200));
                        slot.setOutlineThickness(1.f);
                        window.draw(slot);

                        if (card.destroyed) {
                            // 已消耗：深色空格，明确表示卡牌已移除
                            sf::RectangleShape overlay(sf::Vector2f{layout.cellW, layout.cellH});
                            overlay.setPosition(sf::Vector2f{x, y});
                            overlay.setFillColor(sf::Color(220, 218, 212));
                            overlay.setOutlineColor(sf::Color(200, 198, 192));
                            overlay.setOutlineThickness(1.f);
                            window.draw(overlay);
                            continue;
                        }

                        // 明牌 + 缩放动画
                        const sf::Texture& tex = assets.getTexture(kCardAssetMap[card.id]);
                        sf::Sprite sprite(tex);

                        auto texSize = tex.getSize();
                        float baseSx = layout.cellW / static_cast<float>(texSize.x);
                        float baseSy = layout.cellH / static_cast<float>(texSize.y);

                        float s = cardAnims[idx].scale;

                        // 以卡牌中心为缩放原点
                        float centerX = x + layout.cellW * 0.5f;
                        float centerY = y + layout.cellH * 0.5f;

                        sprite.setOrigin(sf::Vector2f{
                            static_cast<float>(texSize.x) * 0.5f,
                            static_cast<float>(texSize.y) * 0.5f});
                        sprite.setPosition(sf::Vector2f{centerX, centerY});
                        sprite.setScale(sf::Vector2f{baseSx * s, baseSy * s});

                        window.draw(sprite);

                        sf::Color typeColor = getCardTypeColor(card.id);
                        bool affordable = battle.getEnergy() >= GameLogic::getCardCost(card.id);

                        sf::RectangleShape border(sf::Vector2f{layout.cellW, layout.cellH});
                        border.setPosition(sf::Vector2f{x, y});
                        border.setFillColor(sf::Color::Transparent);
                        border.setOutlineColor(affordable ? typeColor : sf::Color(150, 150, 150));
                        border.setOutlineThickness(affordable ? 3.f : 2.f);
                        window.draw(border);

                        sf::CircleShape costBadge(15.f);
                        costBadge.setPosition(sf::Vector2f{x + 6.f, y + 6.f});
                        costBadge.setFillColor(sf::Color(255, 255, 250, 235));
                        costBadge.setOutlineColor(sf::Color(190, 170, 90));
                        costBadge.setOutlineThickness(1.f);
                        window.draw(costBadge);

                        sf::Text costTxt(font, std::to_string(GameLogic::getCardCost(card.id)), 16);
                        costTxt.setFillColor(sf::Color(145, 120, 45));
                        auto cb = costTxt.getLocalBounds();
                        costTxt.setPosition(sf::Vector2f{
                            x + 21.f - cb.size.x * 0.5f,
                            y + 21.f - cb.size.y * 0.5f - cb.position.y});
                        window.draw(costTxt);

                        sf::RectangleShape typeTag(sf::Vector2f{44.f, 20.f});
                        typeTag.setPosition(sf::Vector2f{x + layout.cellW - 50.f, y + layout.cellH - 26.f});
                        typeTag.setFillColor(sf::Color(typeColor.r, typeColor.g, typeColor.b, 220));
                        window.draw(typeTag);

                        std::string typeLabel = getCardTypeName(card.id);
                        sf::String typeDisplay = sf::String::fromUtf8(typeLabel.begin(), typeLabel.end());
                        sf::Text typeTxt(font, typeDisplay, 11);
                        typeTxt.setFillColor(sf::Color::White);
                        auto tb = typeTxt.getLocalBounds();
                        typeTxt.setPosition(sf::Vector2f{
                            x + layout.cellW - 28.f - tb.size.x * 0.5f,
                            y + layout.cellH - 16.f - tb.size.y * 0.5f - tb.position.y});
                        window.draw(typeTxt);

                        if (!affordable) {
                            sf::RectangleShape disabled(sf::Vector2f{layout.cellW, layout.cellH});
                            disabled.setPosition(sf::Vector2f{x, y});
                            disabled.setFillColor(sf::Color(80, 80, 80, 95));
                            window.draw(disabled);
                        }
                    }
                }
            }

            // ── 悬停提示框（Tooltip）— 帮助面板关闭时才显示 ─────────
            if (!isHelpOpen && battle.isPlayerTurn()) {
                int hoverIdx = renderer.hitTest(mousePos.x, mousePos.y);
                if (hoverIdx >= 0) {
                    const Card& hc = battle.getCards()[hoverIdx];
                    if (!hc.destroyed) {
                        auto layout = renderer.getLayout();
                        int hr = hoverIdx / GameLogic::kGridCols;
                        int hc_c = hoverIdx % GameLogic::kGridCols;
                        float tipX = layout.originX
                                   + hc_c * (layout.cellW + layout.cardGap)
                                   + layout.cellW + 8.f;
                        float tipY = layout.originY
                                   + hr * (layout.cellH + layout.cardGap);

                        // Tooltip 文本
                        sf::String tipDisplay = sf::String::fromUtf8(
                            kCardTooltips[hc.id].begin(), kCardTooltips[hc.id].end());
                        sf::Text tipTxt(font, tipDisplay, 13);
                        tipTxt.setFillColor(kTextPrimary);
                        auto tb = tipTxt.getLocalBounds();

                        // Tooltip 背景框
                        float padX = 10.f, padY = 8.f;
                        float tipW = tb.size.x + padX * 2.f;
                        float tipH = tb.size.y + padY * 2.f;

                        // 边界钳制（防止超出屏幕右/下边缘）
                        if (tipX + tipW > static_cast<float>(kWinW) - 8.f)
                            tipX = static_cast<float>(kWinW) - tipW - 8.f;
                        if (tipY + tipH > static_cast<float>(kWinH) - 8.f)
                            tipY = static_cast<float>(kWinH) - tipH - 8.f;

                        sf::RectangleShape tipBg(sf::Vector2f{tipW, tipH});
                        tipBg.setPosition(sf::Vector2f{tipX, tipY});
                        tipBg.setFillColor(sf::Color(255, 255, 252, 240));
                        tipBg.setOutlineColor(sf::Color(200, 198, 190));
                        tipBg.setOutlineThickness(1.f);
                        window.draw(tipBg);

                        tipTxt.setPosition(sf::Vector2f{tipX + padX, tipY + padY});
                        window.draw(tipTxt);
                    }
                }
            }

            // 战斗结束覆盖层
            if (battle.isGameOver()) {
                bool won = (battle.getGameState() == GameState::BattleWon ||
                            battle.getGameState() == GameState::GameOver_Win);
                drawOverlayPanel(window, 420.f, 160.f);
                float bx = (kWinW - 420.f) * 0.5f;
                float by = (kWinH - 160.f) * 0.5f;
                window.draw(makeText(font, won ? "战斗胜利" : "战斗失败", 36,
                            won ? sf::Color(80, 180, 120) : sf::Color(220, 90, 90),
                            cx, by + 40.f));
                window.draw(makeText(font, "点击或按任意键继续", 16,
                            kTextSecondary, cx, by + 110.f));
            }
        }

        // ── 战斗奖励（三选一）───────────────────────────────────────────
        if (rs == RunState::RewardSelection) {
            drawOverlayPanel(window, 860.f, 420.f);
            window.draw(makeText(font, "选择奖励", 32, kTextPrimary, cx, 205.f));
            window.draw(makeText(font, "胜利会延续到后续路线。选择一项强化。", 16,
                        kTextSecondary, cx, 243.f));

            float cardW = 250.f, cardH = 150.f, gap = 24.f;
            float totalW = cardW * 3.f + gap * 2.f;
            float startX = (static_cast<float>(kWinW) - totalW) * 0.5f;
            float y = 315.f;

            for (int i = 0; i < 3; ++i) {
                RewardOption reward = run.getRewardOption(i);
                sf::FloatRect rect(sf::Vector2f{startX + i * (cardW + gap), y},
                                   sf::Vector2f{cardW, cardH});
                bool hover = rect.contains(mousePos);

                sf::RectangleShape card(rect.size);
                card.setPosition(rect.position);
                card.setFillColor(hover ? sf::Color(238, 235, 224) : sf::Color(250, 248, 240));
                card.setOutlineColor(hover ? sf::Color(80, 170, 110) : sf::Color(198, 194, 184));
                card.setOutlineThickness(hover ? 3.f : 1.5f);
                window.draw(card);

                sf::Color rewardColor = (reward.type == RewardType::Heal)
                                      ? sf::Color(80, 175, 110)
                                      : (reward.type == RewardType::MaxHP)
                                      ? sf::Color(205, 85, 85)
                                      : sf::Color(170, 140, 55);
                drawSheetIcon(window,
                              assets.getExtraTexture(AssetManager::ExtraTexture::RewardIcons),
                              i, 3, 1,
                              rect.position.x + cardW * 0.5f - 24.f,
                              y + 16.f + std::sin(animTime * 2.0f + i) * 1.5f,
                              48.f,
                              hover ? 1.12f : 1.f);
                window.draw(makeText(font, reward.title, 21, rewardColor,
                            rect.position.x + cardW * 0.5f, y + 72.f));
                window.draw(makeText(font, reward.desc, 14, kTextSecondary,
                            rect.position.x + cardW * 0.5f, y + 106.f));
                window.draw(makeText(font, "按 " + std::to_string(i + 1) + " 选择", 13, kTextMuted,
                            rect.position.x + cardW * 0.5f, y + 132.f));
            }

            std::string hpStr = "当前生命：" + std::to_string(run.getPlayerHP())
                              + " / " + std::to_string(run.getPlayerMaxHP())
                              + "   最大能量：" + std::to_string(run.getMaxEnergy());
            window.draw(makeText(font, hpStr, 15, kTextMuted, cx, 590.f));
        }

        // ── 地图导航（二选一路线）───────────────────────────────────────
        if (rs == RunState::MapNavigation) {
            const auto& map  = run.getMap();
            int floor = map.getCurrentFloor();
            auto nodes = run.getAvailableNodes();

            // 标题
            drawOverlayPanel(window, 560.f, 320.f);
            float bx = (kWinW - 560.f) * 0.5f;
            float by = (kWinH - 320.f) * 0.5f;

            window.draw(makeText(font, "选择路线", 28,
                        kTextPrimary, cx, by + 30.f));

            std::string floorStr = "第 " + std::to_string(floor + 1)
                                 + " 层已完成";
            window.draw(makeText(font, floorStr, 18,
                        kTextSecondary, cx, by + 65.f));

            // 节点按钮（水平排列）
            float nodeW = 180.f, nodeH = 80.f;
            float gap = 30.f;
            float totalW = nodes.size() * nodeW + (nodes.size() - 1) * gap;
            float startX = (static_cast<float>(kWinW) - totalW) * 0.5f;
            float nodeY = by + 110.f;

            // 鼠标悬停检测
            sf::Vector2f mp = sf::Vector2f(
                static_cast<float>(sf::Mouse::getPosition(window).x),
                static_cast<float>(sf::Mouse::getPosition(window).y));

            for (size_t i = 0; i < nodes.size(); ++i) {
                float nx = startX + i * (nodeW + gap);
                sf::FloatRect rect(sf::Vector2f{nx, nodeY}, sf::Vector2f{nodeW, nodeH});
                bool hover = rect.contains(mp);

                // 按钮背景
                sf::RectangleShape btn(sf::Vector2f{nodeW, nodeH});
                btn.setPosition(sf::Vector2f{nx, nodeY});
                btn.setFillColor(hover ? sf::Color(210, 208, 200)
                                       : sf::Color(235, 232, 225));
                btn.setOutlineColor(sf::Color(195, 192, 185));
                btn.setOutlineThickness(hover ? 2.f : 1.f);
                window.draw(btn);

                drawSheetIcon(window,
                              assets.getExtraTexture(AssetManager::ExtraTexture::MapNodeIcons),
                              getMapIconIndex(nodes[i].type), 3, 1,
                              nx + nodeW * 0.5f - 20.f,
                              nodeY + 8.f + std::sin(animTime * 2.4f + static_cast<float>(i)) * 1.2f,
                              40.f,
                              hover ? 1.15f : 1.f);

                // 节点类型标签
                const char* label = "???";
                sf::Color color(180, 180, 180);
                switch (nodes[i].type) {
                    case NodeType::Battle: label = "战斗"; color = sf::Color(255, 120, 100); break;
                    case NodeType::Rest:   label = "休息"; color = sf::Color(100, 220, 140); break;
                    case NodeType::Boss:   label = "Boss"; color = sf::Color(255, 60, 60);   break;
                }
                window.draw(makeText(font, label, 20, color,
                            nx + nodeW * 0.5f, nodeY + 52.f));

                // 层数提示
                std::string nextStr = "第 " + std::to_string(floor + 2) + " 层";
                window.draw(makeText(font, nextStr, 14, kTextMuted,
                            nx + nodeW * 0.5f, nodeY + 72.f));
            }

            // 玩家状态
            std::string hpStr = "生命：" + std::to_string(run.getPlayerHP())
                              + " / " + std::to_string(run.getPlayerMaxHP());
            window.draw(makeText(font, hpStr, 16, kTextSecondary,
                        cx, by + 260.f));

            window.draw(makeText(font, "点击路线继续前进", 14,
                        kTextMuted, cx, by + 290.f));
        }

        // ── 休息营地 ──────────────────────────────────────────────────────
        if (rs == RunState::RestSite) {
            drawOverlayPanel(window, 420.f, 180.f);
            float bx = (kWinW - 420.f) * 0.5f;
            float by = (kWinH - 180.f) * 0.5f;
            window.draw(makeText(font, "休息点", 32, sf::Color(80, 170, 110), cx, by + 35.f));
            int heal = run.getPlayerMaxHP() * RunManager::kRestHealPct / 100;
            window.draw(makeText(font, "恢复 " + std::to_string(heal) + " 点生命", 20,
                        sf::Color(80, 170, 110), cx, by + 90.f));
            window.draw(makeText(font, "按 Enter 继续", 16,
                        kTextSecondary, cx, by + 140.f));
        }

        // ── 通关 ──────────────────────────────────────────────────────
        if (rs == RunState::RunVictory) {
            drawOverlayPanel(window, 480.f, 200.f);
            float bx = (kWinW - 480.f) * 0.5f;
            float by = (kWinH - 200.f) * 0.5f;
            window.draw(makeText(font, "通关成功！", 48, sf::Color(200, 170, 30), cx, by + 40.f));
            window.draw(makeText(font, "你征服了全部 5 层", 20, sf::Color(80, 170, 110), cx, by + 100.f));
            window.draw(makeText(font, "按 R 再来一局", 18, kTextSecondary, cx, by + 155.f));
        }

        // ── 死亡 ──────────────────────────────────────────────────────
        if (rs == RunState::RunDefeat) {
            drawOverlayPanel(window, 480.f, 200.f);
            float bx = (kWinW - 480.f) * 0.5f;
            float by = (kWinH - 200.f) * 0.5f;
            window.draw(makeText(font, "征程失败", 48, sf::Color(200, 80, 80), cx, by + 40.f));
            window.draw(makeText(font, "倒在第 " + std::to_string(run.getMap().getCurrentFloor() + 1) + " 层", 20,
                        sf::Color(180, 130, 130), cx, by + 100.f));
            window.draw(makeText(font, "按 R 重试", 18, kTextSecondary, cx, by + 155.f));
        }

        // ── 浮动跳字（在所有 UI 之上绘制）─────────────────────────────
        for (const auto& ft : floatTexts) {
            window.draw(ft.text);
        }

        // ── 帮助按钮（右上角，全局常驻）─────────────────────────────────
        {
            float hbX = static_cast<float>(kWinW) - kGridPad - kHelpBtnSize;
            float hbY = kGridPad;

            sf::Vector2f mpos = sf::Vector2f(
                static_cast<float>(sf::Mouse::getPosition(window).x),
                static_cast<float>(sf::Mouse::getPosition(window).y));
            sf::FloatRect hbRect(sf::Vector2f{hbX, hbY},
                                 sf::Vector2f{kHelpBtnSize, kHelpBtnSize});
            bool hbHover = hbRect.contains(mpos);

            sf::RectangleShape helpBtn(sf::Vector2f{kHelpBtnSize, kHelpBtnSize});
            helpBtn.setPosition(sf::Vector2f{hbX, hbY});
            helpBtn.setFillColor(hbHover ? sf::Color(205, 202, 195) : kHelpBtnColor);
            helpBtn.setOutlineColor(sf::Color(195, 192, 185));
            helpBtn.setOutlineThickness(1.f);
            window.draw(helpBtn);

            sf::Text qMark(font, "?", 20);
            qMark.setFillColor(kTextPrimary);
            auto qb = qMark.getLocalBounds();
            qMark.setPosition(sf::Vector2f{
                hbX + (kHelpBtnSize - qb.size.x) * 0.5f,
                hbY + (kHelpBtnSize - qb.size.y) * 0.5f - qb.position.y});
            window.draw(qMark);
        }

        // ── 首战教程覆盖层 ─────────────────────────────────────────────
        if (showTutorial) {
            drawOverlayPanel(window, 560.f, 280.f);
            float by = (kWinH - 280.f) * 0.5f;
            window.draw(makeText(font, "首次作战简报", 30, kTextPrimary, cx, by + 42.f));
            window.draw(makeText(font, "消耗能量打出卡牌。攻击牌会伤害 Boss。", 16,
                        kTextSecondary, cx, by + 95.f));
            window.draw(makeText(font, "观察 Boss 意图，强攻前尽量叠护甲。", 16,
                        kTextSecondary, cx, by + 128.f));
            window.draw(makeText(font, "赢下战斗、选择奖励，抵达第 5 层 Boss。", 16,
                        kTextSecondary, cx, by + 161.f));
            window.draw(makeText(font, "点击、Enter 或空格开始", 16,
                        sf::Color(80, 160, 110), cx, by + 225.f));
        }

        // ── 全局帮助面板 ────────────────────────────────────────────────
        if (isHelpOpen) {
            // 全屏深色遮罩
            sf::RectangleShape dim(sf::Vector2f{static_cast<float>(kWinW),
                                                static_cast<float>(kWinH)});
            dim.setFillColor(sf::Color(245, 242, 235, 210));
            window.draw(dim);

            // 帮助文本框
            float helpBoxW = 520.f, helpBoxH = 600.f;
            float helpBoxX = (static_cast<float>(kWinW) - helpBoxW) * 0.5f;
            float helpBoxY = (static_cast<float>(kWinH) - helpBoxH) * 0.5f;

            sf::RectangleShape helpBg(sf::Vector2f{helpBoxW, helpBoxH});
            helpBg.setPosition(sf::Vector2f{helpBoxX, helpBoxY});
            helpBg.setFillColor(sf::Color(255, 255, 252, 245));
            helpBg.setOutlineColor(sf::Color(200, 198, 190));
            helpBg.setOutlineThickness(2.f);
            window.draw(helpBg);

            // 规则文本（手动 \n 换行）
            sf::String rulesDisplay = sf::String::fromUtf8(kHelpRules.begin(), kHelpRules.end());
            sf::Text rulesTxt(font, rulesDisplay, 15);
            rulesTxt.setFillColor(kTextPrimary);
            rulesTxt.setPosition(sf::Vector2f{helpBoxX + 24.f, helpBoxY + 20.f});
            window.draw(rulesTxt);

            // 关闭提示
            const std::string closeLabel = "点击任意处或按 [?] 关闭";
            sf::String closeDisplay = sf::String::fromUtf8(closeLabel.begin(), closeLabel.end());
            sf::Text closeTxt(font, closeDisplay, 13);
            closeTxt.setFillColor(kTextMuted);
            auto cb = closeTxt.getLocalBounds();
            closeTxt.setPosition(sf::Vector2f{
                helpBoxX + (helpBoxW - cb.size.x) * 0.5f,
                helpBoxY + helpBoxH - 36.f});
            window.draw(closeTxt);
        }

        // ── 恢复默认 View ─────────────────────────────────────────────
        window.setView(defaultView);
        window.display();
    }

    return 0;
}
