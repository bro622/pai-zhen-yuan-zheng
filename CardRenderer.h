#pragma once

#include <SFML/Graphics.hpp>

#include "AssetManager.h"
#include "GameLogic.h"

// ══════════════════════════════════════════════════════════════════════════════
//  卡牌网格渲染器
//  职责：持有 AssetManager 引用，根据 GameLogic 状态绘制 4×4 网格。
//       不处理输入事件，不修改逻辑状态——纯渲染。
// ══════════════════════════════════════════════════════════════════════════════

struct GridLayout {
    float cellW;      // 单元格宽度（像素）
    float cellH;      // 单元格高度（像素）
    float originX;    // 网格左上角 x
    float originY;    // 网格左上角 y
    float cardGap;    // 卡牌间距（像素）
};

class CardRenderer {
public:
    // 构造时计算网格布局并预分配精灵
    //   winW / winH   — 窗口尺寸
    //   topBarH       — 顶部 HP 区域高度
    //   bottomBarH    — 底部 HP 区域高度
    //   gridPad       — 网格外边距
    //   cardGap       — 卡牌间距
    CardRenderer(float winW, float winH,
                 float topBarH, float bottomBarH,
                 float gridPad, float cardGap);

    // ── 核心渲染 ─────────────────────────────────────────────────────────────

    // 绘制完整卡牌网格，根据每张牌的三态（盖上/翻开/已消除）选择纹理
    //   game  — GameLogic 只读引用，用于查询卡牌状态
    //   assets — 资源管理器，用于获取纹理
    //   target — 渲染目标窗口
    void drawGrid(const GameLogic& game,
                  const AssetManager& assets,
                  sf::RenderTarget& target);

    // ── 坐标查询 ─────────────────────────────────────────────────────────────

    // 鼠标像素坐标 → 卡牌索引 (0-15)，未命中返回 -1
    int hitTest(float mouseX, float mouseY) const;

    // 获取布局信息（只读）
    const GridLayout& getLayout() const noexcept;

private:
    GridLayout         m_layout;
    sf::RectangleShape m_slotShape;     // 槽底色矩形（复用）
    sf::RectangleShape m_matchOverlay;  // 已消除覆盖层（复用）
    // SFML 3: Sprite 无默认构造函数，改为 drawGrid 内局部创建

    // 颜色常量
    static const sf::Color kSlotBG;
    static const sf::Color kSlotOutline;
    static const sf::Color kMatchOverlay;
};
