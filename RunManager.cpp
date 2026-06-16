#include "RunManager.h"

#include <algorithm>

// ────────────────────────────────────────────────────────────────────────────────
//  构造
// ────────────────────────────────────────────────────────────────────────────────

RunManager::RunManager()
    : m_runState(RunState::MainMenu)
    , m_playerHP(kPlayerInitHP)
    , m_playerMaxHP(kPlayerInitMaxHP)
    , m_maxEnergy(kInitMaxEnergy)
    , m_strength(0)
    , m_battlesWon(0)
{
}

// ────────────────────────────────────────────────────────────────────────────────
//  进程控制
// ────────────────────────────────────────────────────────────────────────────────

void RunManager::startNewRun()
{
    m_playerHP    = kPlayerInitHP;
    m_playerMaxHP = kPlayerInitMaxHP;
    m_maxEnergy   = kInitMaxEnergy;
    m_strength    = 0;
    m_battlesWon  = 0;

    m_map.generateMap();

    // 第 0 层第 0 个节点固定为 Battle，直接进入
    startBattle();
}

bool RunManager::chooseNode(int targetIndex)
{
    if (m_runState != RunState::MapNavigation)
        return false;

    if (!m_map.enterNode(targetIndex))
        return false;

    // 根据节点类型决定下一步
    switch (m_map.getCurrentNodeType()) {
        case NodeType::Battle:
        case NodeType::Boss:
            startBattle();
            break;

        case NodeType::Rest:
            m_runState = RunState::RestSite;
            break;
    }

    return true;
}

void RunManager::startBattle()
{
    int bossHP = calcBossHP();

    // 注入玩家跨节点数据
    m_battle.initGameWithParams(m_playerHP, bossHP, m_maxEnergy, m_map.getCurrentFloor());

    // 恢复上一战累积的力量
    // initGameWithParams 内部会重置 strength=0，
    // 需要在之后注入
    // TODO: 给 GameLogic 增加 setStrength() 接口后移除此限制
    // 当前设计：力量每场战斗独立，符合 Slay the Spire 风格

    // 设置为进程模式（胜利 → BattleWon 而非 GameOver_Win）
    m_battle.setRunBattleMode(true);

    m_runState = RunState::Battle;
}

void RunManager::onBattleResult()
{
    if (m_battle.getGameState() == GameState::GameOver_Lose) {
        m_runState = RunState::RunDefeat;
        return;
    }

    // BattleWon：同步玩家数据
    m_playerHP = m_battle.getPlayerHP();
    ++m_battlesWon;

    m_map.completeCurrentNode();

    // Boss 层胜利 → 全局通关
    if (m_map.isFinalFloor()) {
        m_runState = RunState::RunVictory;
        return;
    }

    // 进入奖励选择界面，再回到路线选择
    m_runState = RunState::RewardSelection;
}

void RunManager::healAndAdvance()
{
    if (m_runState != RunState::RestSite)
        return;

    // 恢复 30% maxHP
    int heal = m_playerMaxHP * kRestHealPct / 100;
    m_playerHP = std::min(m_playerHP + heal, m_playerMaxHP);

    m_map.completeCurrentNode();

    // 回到路线选择（等待玩家选择下一层节点）
    m_runState = RunState::MapNavigation;
}

void RunManager::chooseReward(int index)
{
    if (m_runState != RunState::RewardSelection)
        return;

    RewardOption reward = getRewardOption(index);

    switch (reward.type) {
        case RewardType::Heal: {
            int heal = 10 + m_battlesWon * 3;
            m_playerHP = std::min(m_playerHP + heal, m_playerMaxHP);
            break;
        }

        case RewardType::MaxHP:
            m_playerMaxHP += 5;
            m_playerHP = std::min(m_playerHP + 8, m_playerMaxHP);
            break;

        case RewardType::Energy:
            if (m_maxEnergy < 5) {
                ++m_maxEnergy;
            } else {
                m_playerHP = std::min(m_playerHP + 10, m_playerMaxHP);
            }
            break;
    }

    m_runState = RunState::MapNavigation;
}

// ────────────────────────────────────────────────────────────────────────────────
//  查询接口
// ────────────────────────────────────────────────────────────────────────────────

RunState         RunManager::getRunState()     const noexcept { return m_runState; }
int              RunManager::getPlayerHP()    const noexcept { return m_playerHP; }
int              RunManager::getPlayerMaxHP() const noexcept { return m_playerMaxHP; }
int              RunManager::getMaxEnergy()   const noexcept { return m_maxEnergy; }
const GameLogic& RunManager::getBattle()      const noexcept { return m_battle; }
const MapManager& RunManager::getMap()        const noexcept { return m_map; }

std::vector<MapNode> RunManager::getAvailableNodes() const
{
    return m_map.getAvailableNextNodes();
}

RewardOption RunManager::getRewardOption(int index) const noexcept
{
    static constexpr RewardOption kRewards[3] = {
        { RewardType::Heal,   "随军医师",  "战斗后恢复生命。" },
        { RewardType::MaxHP,  "铁心护符",  "生命上限 +5，并恢复 5 点生命。" },
        { RewardType::Energy, "灵能核心",  "最大能量 +1，最多提升到 5。" }
    };

    if (index < 0 || index >= 3)
        return kRewards[0];
    return kRewards[index];
}

ActionResult RunManager::playCard(int index) { return m_battle.playCard(index); }
ActionResult RunManager::endTurn()           { return m_battle.endTurn(); }

// ────────────────────────────────────────────────────────────────────────────────
//  内部工具
// ────────────────────────────────────────────────────────────────────────────────

int RunManager::calcBossHP() const noexcept
{
    NodeType type = m_map.getCurrentNodeType();
    int floor = m_map.getCurrentFloor();

    if (type == NodeType::Boss) {
        return 105;
    }

    // 普通战斗：随层数递增
    return 38 + floor * 10;
}
