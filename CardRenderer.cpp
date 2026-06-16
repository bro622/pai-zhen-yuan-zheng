#include "CardRenderer.h"

// ── 颜色常量（简约卡通风格）────────────────────────────────────────────────

const sf::Color CardRenderer::kSlotBG       (235, 232, 225);
const sf::Color CardRenderer::kSlotOutline   (210, 207, 200);
const sf::Color CardRenderer::kMatchOverlay  (220, 218, 212);

// ══════════════════════════════════════════════════════════════════════════════
//  构造（固定卡牌尺寸，居中布局）
// ══════════════════════════════════════════════════════════════════════════════

static constexpr float kFixedCardW = 110.f;
static constexpr float kFixedCardH = 110.f;  // 1:1 正方形

CardRenderer::CardRenderer(float winW, float winH,
                           float topBarH, float bottomBarH,
                           float /*gridPad*/, float cardGap)
{
    m_layout.cellW   = kFixedCardW;
    m_layout.cellH   = kFixedCardH;
    m_layout.cardGap = cardGap;

    // 网格总宽/总高
    float gridW = GameLogic::kGridCols * kFixedCardW
                + (GameLogic::kGridCols - 1) * cardGap;
    float gridH = GameLogic::kGridRows * kFixedCardH
                + (GameLogic::kGridRows - 1) * cardGap;

    // 居中于可用区域
    float areaTop = topBarH;
    float areaBot = winH - bottomBarH;
    m_layout.originX = (winW - gridW) * 0.5f;
    m_layout.originY = areaTop + (areaBot - areaTop - gridH) * 0.5f;

    // 预配置复用形状
    m_slotShape.setSize(sf::Vector2f{m_layout.cellW, m_layout.cellH});
    m_slotShape.setFillColor(kSlotBG);
    m_slotShape.setOutlineColor(kSlotOutline);
    m_slotShape.setOutlineThickness(1.f);

    m_matchOverlay.setSize(sf::Vector2f{m_layout.cellW, m_layout.cellH});
    m_matchOverlay.setFillColor(kMatchOverlay);
}

// ══════════════════════════════════════════════════════════════════════════════
//  核心渲染：绘制 4×4 网格
// ══════════════════════════════════════════════════════════════════════════════

void CardRenderer::drawGrid(const GameLogic& game,
                            const AssetManager& assets,
                            sf::RenderTarget& target)
{
    const auto& cards = game.getCards();

    for (int r = 0; r < GameLogic::kGridRows; ++r) {
        for (int c = 0; c < GameLogic::kGridCols; ++c) {
            const int idx = r * GameLogic::kGridCols + c;
            const Card& card = cards[idx];

            const float x = m_layout.originX + c * (m_layout.cellW + m_layout.cardGap);
            const float y = m_layout.originY + r * (m_layout.cellH + m_layout.cardGap);

            // 槽底色
            m_slotShape.setPosition(sf::Vector2f{x, y});
            target.draw(m_slotShape);

            // 已消耗：绘制低透明度占位，不绘制卡面
            if (card.destroyed) {
                m_matchOverlay.setPosition(sf::Vector2f{x, y});
                target.draw(m_matchOverlay);
                continue;
            }

            // 明牌：直接根据 ID 选择纹理 (asset_01~08 → textures[1~8])
            const int texIdx = card.id + 1;
            const sf::Texture& tex = assets.getTexture(texIdx);

            sf::Sprite sprite(tex);
            const auto texSize = tex.getSize();
            sprite.setScale(sf::Vector2f{
                m_layout.cellW / static_cast<float>(texSize.x),
                m_layout.cellH / static_cast<float>(texSize.y)
            });
            sprite.setPosition(sf::Vector2f{x, y});

            target.draw(sprite);
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  坐标查询：鼠标位置 → 卡牌索引
// ══════════════════════════════════════════════════════════════════════════════

int CardRenderer::hitTest(float mouseX, float mouseY) const
{
    for (int r = 0; r < GameLogic::kGridRows; ++r) {
        for (int c = 0; c < GameLogic::kGridCols; ++c) {
            const float x = m_layout.originX + c * (m_layout.cellW + m_layout.cardGap);
            const float y = m_layout.originY + r * (m_layout.cellH + m_layout.cardGap);
            if (mouseX >= x && mouseX <= x + m_layout.cellW &&
                mouseY >= y && mouseY <= y + m_layout.cellH) {
                return r * GameLogic::kGridCols + c;
            }
        }
    }
    return -1;
}

// ══════════════════════════════════════════════════════════════════════════════
//  布局查询
// ══════════════════════════════════════════════════════════════════════════════

const GridLayout& CardRenderer::getLayout() const noexcept
{
    return m_layout;
}
