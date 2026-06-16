#include "GameLogic.h"

#include <algorithm>
#include <random>
#include <cassert>

// ────────────────────────────────────────────────────────────────────────────────
//  构造
// ────────────────────────────────────────────────────────────────────────────────

GameLogic::GameLogic()
    : m_state(GameState::PlayerTurn)
    , m_isRunBattle(false)
    , m_playerHP(kInitPlayerHP)
    , m_playerMaxHP(kInitPlayerHP)
    , m_block(0)
    , m_energy(kInitMaxEnergy)
    , m_maxEnergy(kInitMaxEnergy)
    , m_strength(0)
    , m_bossHP(kInitBossHP)
    , m_bossInitHP(kInitBossHP)
    , m_bossBlock(0)
    , m_vulnerableStacks(0)
    , m_weakStacks(0)
    , m_enemyTier(0)
    , m_nextIntent(IntentType::Attack)
    , m_intentValue(8)
    , m_intentPatternIdx(0)
{
    initGame();
}

// ────────────────────────────────────────────────────────────────────────────────
//  初始化
// ────────────────────────────────────────────────────────────────────────────────

void GameLogic::initGame()
{
    // 先填满格子，再重置数值
    for (auto& card : m_cards) {
        card.id = 0;
        card.destroyed = true;   // 触发 fillDestroyedSlots 填充
    }
    fillDestroyedSlots();

    m_state            = GameState::PlayerTurn;
    m_isRunBattle      = false;
    m_playerHP         = kInitPlayerHP;
    m_playerMaxHP      = kInitPlayerHP;
    m_block            = 0;
    m_energy           = kInitMaxEnergy;
    m_maxEnergy        = kInitMaxEnergy;
    m_strength         = 0;
    m_bossHP           = kInitBossHP;
    m_bossInitHP       = kInitBossHP;
    m_bossBlock        = 0;
    m_vulnerableStacks = 0;
    m_weakStacks       = 0;
    m_enemyTier        = 0;
    m_intentPatternIdx = 0;

    // 生成初始意图（玩家回合开始时即可看到）
    generateNextIntent();
}

void GameLogic::initGameWithParams(int playerHP, int bossHP, int maxEnergy, int enemyTier)
{
    initGame();

    m_playerHP    = playerHP;
    m_playerMaxHP = playerHP;
    m_bossHP      = bossHP;
    m_bossInitHP  = bossHP;
    m_maxEnergy   = maxEnergy;
    m_energy      = maxEnergy;
    m_enemyTier   = std::clamp(enemyTier, 0, 4);
    m_intentPatternIdx = 0;
    generateNextIntent();
}

// ────────────────────────────────────────────────────────────────────────────────
//  卡牌静态工具
// ────────────────────────────────────────────────────────────────────────────────

// 返回各 ID 卡牌的能量消耗
int GameLogic::getCardCost(int id) noexcept
{
    switch (id) {
        case 0: return 2;   // 火焰：2 费
        case 2: return 2;   // 钻石：2 费
        case 3: return 0;   // 能量球：0 费（回能牌）
        default: return 1;  // 其余：1 费
    }
}

// 伤害计算：易伤 > 0 时 ×1.5（向下取整）
int GameLogic::calcDamage(int baseDmg) const noexcept
{
    if (m_vulnerableStacks > 0) {
        return static_cast<int>(baseDmg * 1.5f);
    }
    return baseDmg;
}

// ────────────────────────────────────────────────────────────────────────────────
//  playCard — 核心结算
// ────────────────────────────────────────────────────────────────────────────────

ActionResult GameLogic::playCard(int index)
{
    // ── 校验 1：必须是玩家回合 ─────────────────────────────────────────
    if (m_state != GameState::PlayerTurn)
        return { m_state, BattleEvent::None, 0 };

    // ── 校验 2：索引合法 ───────────────────────────────────────────────
    if (index < 0 || index >= kTotalCards)
        return { m_state, BattleEvent::None, 0 };

    // ── 校验 3：卡牌必须可用（未消耗）────────────────────────────────
    if (m_cards[index].destroyed)
        return { m_state, BattleEvent::None, 0 };

    // ── 校验 4：能量足够 ───────────────────────────────────────────────
    const int id   = m_cards[index].id;
    const int cost = getCardCost(id);

    if (m_energy < cost)
        return { m_state, BattleEvent::None, 0 };

    // ── 统一扣能量 ─────────────────────────────────────────────────────
    m_energy -= cost;

    // ── 分支结算（按 ID switch）───────────────────────────────────────
    switch (id) {

    // ──────────────────────────────────────────────────────────────────
    //  攻击类（打出后标记 destroyed = true，下回合刷新）
    // ──────────────────────────────────────────────────────────────────

    case 6: {   // 剑：6 + strength 伤害
        int dmg = calcDamage(6 + m_strength);
        int absorbed = std::min(m_bossBlock, dmg);
        m_bossBlock -= absorbed;
        m_bossHP    -= (dmg - absorbed);
        m_cards[index].destroyed = true;
        checkWinLose();
        return { m_state, BattleEvent::AttackPlayed, dmg - absorbed };
    }

    case 0: {   // 火焰：14 + strength 伤害，自伤 3
        int dmg = calcDamage(14 + m_strength);
        int absorbed = std::min(m_bossBlock, dmg);
        m_bossBlock -= absorbed;
        m_bossHP    -= (dmg - absorbed);
        m_playerHP  -= 3;
        m_cards[index].destroyed = true;
        checkWinLose();
        return { m_state, BattleEvent::SelfDamagePlayed, dmg - absorbed };
    }

    case 7: {   // 云：3 + strength 伤害，施加易伤
        int dmg = calcDamage(3 + m_strength);
        int absorbed = std::min(m_bossBlock, dmg);
        m_bossBlock -= absorbed;
        m_bossHP    -= (dmg - absorbed);
        m_vulnerableStacks += 1;
        m_cards[index].destroyed = true;
        checkWinLose();
        return { m_state, BattleEvent::VulnerableApplied, dmg };
    }

    // ──────────────────────────────────────────────────────────────────
    //  技能类（打出后全部消失，下回合刷新）
    // ──────────────────────────────────────────────────────────────────

    case 2: {   // 钻石：strength + 2
        m_strength += 2;
        m_cards[index].destroyed = true;
        return { m_state, BattleEvent::StrengthGained, 2 };
    }

    case 3: {   // 能量球：回 1 能量
        m_energy = std::min(m_maxEnergy, m_energy + 1);
        m_cards[index].destroyed = true;
        return { m_state, BattleEvent::EnergyRefunded, 1 };
    }

    case 1: {   // 药草：回 3 hp
        m_playerHP = std::min(m_playerMaxHP, m_playerHP + 3);
        m_cards[index].destroyed = true;
        return { m_state, BattleEvent::HealPlayed, 3 };
    }

    // ──────────────────────────────────────────────────────────────────
    //  防御/控制类（打出后消失，下回合刷新）
    // ──────────────────────────────────────────────────────────────────

    case 5: {   // 盾牌：block + 5
        m_block += 5;
        m_cards[index].destroyed = true;
        return { m_state, BattleEvent::DefensePlayed, 5 };
    }

    case 4: {   // 卷轴：Boss weak + 1
        m_weakStacks += 1;
        m_cards[index].destroyed = true;
        return { m_state, BattleEvent::WeakApplied, 1 };
    }

    default:
        return { m_state, BattleEvent::None, 0 };
    }
}

// ────────────────────────────────────────────────────────────────────────────────
//  endTurn — 主动结束回合
// ────────────────────────────────────────────────────────────────────────────────

ActionResult GameLogic::endTurn()
{
    if (m_state != GameState::PlayerTurn)
        return { m_state, BattleEvent::None, 0 };

    // Boss 攻击，获取实际扣血量
    int actualDmg = executeBossTurn();

    if (!isGameOver()) {
        // Boss 攻击后未死 → 刷新棋盘 → 回到玩家回合
        executeBoardRefresh();
        return { m_state, BattleEvent::BoardRefreshed, 0 };
    }

    // 玩家死亡或 Boss 未造成伤害时返回对应事件
    if (actualDmg > 0)
        return { m_state, BattleEvent::BossAttacked, actualDmg };

    return { m_state, BattleEvent::None, 0 };
}

// ────────────────────────────────────────────────────────────────────────────────
//  查询接口
// ────────────────────────────────────────────────────────────────────────────────

const std::array<Card, GameLogic::kTotalCards>& GameLogic::getCards() const noexcept
{
    return m_cards;
}

GameState GameLogic::getGameState()      const noexcept { return m_state; }
int       GameLogic::getPlayerHP()      const noexcept { return m_playerHP; }
int       GameLogic::getPlayerMaxHP()   const noexcept { return m_playerMaxHP; }
int       GameLogic::getBlock()         const noexcept { return m_block; }
int       GameLogic::getEnergy()        const noexcept { return m_energy; }
int       GameLogic::getMaxEnergy()     const noexcept { return m_maxEnergy; }
int       GameLogic::getBossHP()        const noexcept { return m_bossHP; }
int       GameLogic::getBossInitHP()    const noexcept { return m_bossInitHP; }
int       GameLogic::getStrength()      const noexcept { return m_strength; }
int       GameLogic::getVulnerable()    const noexcept { return m_vulnerableStacks; }
int       GameLogic::getWeak()          const noexcept { return m_weakStacks; }
int       GameLogic::getEnemyTier()     const noexcept { return m_enemyTier; }

IntentType GameLogic::getBossIntent()      const noexcept { return m_nextIntent; }
int        GameLogic::getBossIntentValue() const noexcept { return m_intentValue; }

bool GameLogic::isGameOver() const noexcept
{
    return m_state == GameState::GameOver_Win
        || m_state == GameState::GameOver_Lose
        || m_state == GameState::BattleWon;
}

bool GameLogic::isBattleWon() const noexcept
{
    return m_state == GameState::BattleWon;
}

void GameLogic::setRunBattleMode(bool enabled) noexcept
{
    m_isRunBattle = enabled;
}

bool GameLogic::isPlayerTurn() const noexcept
{
    return m_state == GameState::PlayerTurn;
}

// ────────────────────────────────────────────────────────────────────────────────
//  内部回合推进
// ────────────────────────────────────────────────────────────────────────────────

int GameLogic::executeBossTurn() noexcept
{
    m_state = GameState::BossTurn;

    int actualDmg = 0;

    switch (m_nextIntent) {

    case IntentType::Attack: {
        // 普通攻击：造成伤害（受虚弱影响）
        int rawDmg = m_intentValue;
        if (m_weakStacks > 0) rawDmg = rawDmg / 2;

        int absorbed = std::min(m_block, rawDmg);
        m_block    -= absorbed;
        m_playerHP -= (rawDmg - absorbed);
        actualDmg   = rawDmg - absorbed;
        break;
    }

    case IntentType::Defend: {
        // 防御：Boss 叠甲
        m_bossBlock += m_intentValue;
        break;
    }

    case IntentType::StrongAttack: {
        // 强攻：高伤害（受虚弱影响）
        int rawDmg = m_intentValue;
        if (m_weakStacks > 0) rawDmg = rawDmg / 2;

        int absorbed = std::min(m_block, rawDmg);
        m_block    -= absorbed;
        m_playerHP -= (rawDmg - absorbed);
        actualDmg   = rawDmg - absorbed;
        break;
    }
    }

    // 玩家护甲不保留到下一回合
    m_block = 0;

    // 回合末：易伤 / 虚弱层数各 -1
    if (m_vulnerableStacks > 0) --m_vulnerableStacks;
    if (m_weakStacks > 0)       --m_weakStacks;

    checkWinLose();
    return actualDmg;
}

void GameLogic::executeBoardRefresh() noexcept
{
    m_state = GameState::BoardRefresh;

    fillDestroyedSlots();

    // 恢复能量
    m_energy = m_maxEnergy;

    // Boss 护甲每回合重置
    m_bossBlock = 0;

    // 生成下一回合意图（玩家可立即看到）
    generateNextIntent();

    m_state = GameState::PlayerTurn;
}

void GameLogic::checkWinLose() noexcept
{
    // Boss 先死判定（攻击牌打出后立即检查）
    if (m_bossHP <= 0) {
        m_bossHP = 0;
        // 进程模式 → BattleWon（等待 RunManager 推进）
        // 独立模式 → GameOver_Win（游戏胜利）
        m_state = m_isRunBattle ? GameState::BattleWon : GameState::GameOver_Win;
        return;
    }

    // 玩家死亡判定（Boss 攻击后 或 火焰自伤后检查）
    if (m_playerHP <= 0) {
        m_playerHP = 0;
        m_state    = GameState::GameOver_Lose;
    }
}

void GameLogic::fillDestroyedSlots()
{
    std::random_device rd;
    std::mt19937       rng(rd());
    std::uniform_int_distribution<int> dist(0, kCardTypes - 1);

    for (auto& card : m_cards) {
        if (card.destroyed) {
            card.id        = dist(rng);
            card.destroyed = false;
        }
    }
}

// ────────────────────────────────────────────────────────────────────────────────
//  Boss 意图生成
//  循环模式：Attack → Attack → Defend → StrongAttack → ...
//  Boss HP < 50% 时，Attack 升级为 StrongAttack
// ────────────────────────────────────────────────────────────────────────────────

void GameLogic::generateNextIntent()
{
    // 循环模式：0=Attack, 1=Attack, 2=Defend, 3=StrongAttack
    static const IntentType kPatterns[5][4] = {
        // Floor 1: 教学型，攻击节奏温和
        { IntentType::Attack, IntentType::Defend, IntentType::Attack, IntentType::Attack },
        // Floor 2: 守卫型，护甲更多
        { IntentType::Defend, IntentType::Attack, IntentType::Defend, IntentType::StrongAttack },
        // Floor 3: 剑士型，连续压血
        { IntentType::Attack, IntentType::Attack, IntentType::StrongAttack, IntentType::Defend },
        // Floor 4: 冠军型，强攻频率提升
        { IntentType::Attack, IntentType::StrongAttack, IntentType::Defend, IntentType::StrongAttack },
        // Floor 5: 最终 Boss，混合循环
        { IntentType::StrongAttack, IntentType::Defend, IntentType::Attack, IntentType::StrongAttack }
    };

    std::random_device rd;
    std::mt19937       rng(rd());

    int tier = std::clamp(m_enemyTier, 0, 4);
    IntentType base = kPatterns[tier][m_intentPatternIdx % 4];

    // Boss HP < 50% 时，普通攻击升级为强攻
    if (base == IntentType::Attack && m_bossHP <= m_bossInitHP / 2) {
        base = IntentType::StrongAttack;
    }

    // 随机数值
    int value = 0;
    switch (base) {
        case IntentType::Attack:
            value = std::uniform_int_distribution<int>(4 + tier, 6 + tier)(rng);
            break;
        case IntentType::Defend:
            value = std::uniform_int_distribution<int>(4 + tier, 6 + tier)(rng);
            break;
        case IntentType::StrongAttack:
            value = std::uniform_int_distribution<int>(8 + tier, 11 + tier * 2)(rng);
            break;
    }

    m_nextIntent = base;
    m_intentValue = value;

    // 推进模式索引
    ++m_intentPatternIdx;
}
