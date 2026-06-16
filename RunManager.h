#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "GameLogic.h"
#include "MapManager.h"

// ══════════════════════════════════════════════════════════════════════════════
//  全局进程状态机
// ══════════════════════════════════════════════════════════════════════════════

enum class RunState : uint8_t {
    MainMenu,          // 初始菜单
    MapNavigation,     // 路线选择（查看可达节点，点击进入）
    Battle,            // 战斗中（委托给 GameLogic）
    RewardSelection,   // 战斗胜利后的奖励选择
    RestSite,          // 休息营地
    RunVictory,        // 通关
    RunDefeat          // 死亡
};

enum class RewardType : uint8_t {
    Heal,
    MaxHP,
    Energy
};

struct RewardOption {
    RewardType  type;
    const char* title;
    const char* desc;
};

// ══════════════════════════════════════════════════════════════════════════════
//  进程管理器
// ══════════════════════════════════════════════════════════════════════════════

class RunManager {
public:
    static constexpr int kPlayerInitHP    = 30;
    static constexpr int kPlayerInitMaxHP = 30;
    static constexpr int kInitMaxEnergy   = 3;
    static constexpr int kRestHealPct     = 30;

    RunManager();

    // ── 进程控制 ─────────────────────────────────────────────────────────────

    void startNewRun();

    // 选择路线节点（从 MapNavigation 状态调用）
    //   targetIndex — 下一层节点的横向索引
    //   返回 true 表示成功进入
    bool chooseNode(int targetIndex);

    // 战斗结束后调用
    void onBattleResult();

    // 休息营地：回血 + 返回地图
    void healAndAdvance();

    // 战斗奖励：选择后进入地图
    void chooseReward(int index);

    // ── 查询接口 ─────────────────────────────────────────────────────────────

    RunState           getRunState()     const noexcept;
    int                getPlayerHP()    const noexcept;
    int                getPlayerMaxHP() const noexcept;
    int                getMaxEnergy()   const noexcept;

    const GameLogic&   getBattle()       const noexcept;
    const MapManager&  getMap()          const noexcept;

    // 获取当前可选的下一层节点
    std::vector<MapNode> getAvailableNodes() const;
    RewardOption getRewardOption(int index) const noexcept;

    // 操作代理（仅 Battle 状态有效）
    ActionResult       playCard(int index);
    ActionResult       endTurn();

private:
    RunState    m_runState;
    MapManager  m_map;

    // 玩家跨节点持久数据
    int         m_playerHP;
    int         m_playerMaxHP;
    int         m_maxEnergy;
    int         m_strength;    // 永久力量（跨战斗保留）
    int         m_battlesWon;

    // 当前战斗实例
    GameLogic   m_battle;

    // 内部工具
    int  calcBossHP() const noexcept;
    void startBattle();
};
