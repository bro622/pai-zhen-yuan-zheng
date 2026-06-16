#pragma once

#include <array>
#include <cstdint>

// ── 卡牌数据 ────────────────────────────────────────────────────────────────

struct Card {
    int  id;           // 图案编号 0-7
    bool destroyed;    // 是否已消耗（空白格，下回合由 fillDestroyedSlots 刷新）
};

// ── Boss 意图类型 ──────────────────────────────────────────────────────────

enum class IntentType : uint8_t {
    Attack,        // 普通攻击（6-9 伤害）
    Defend,        // 防御（Boss 叠甲 5-7）
    StrongAttack   // 强攻（15-20 伤害）
};

// ── 游戏状态机 ──────────────────────────────────────────────────────────────

enum class GameState : uint8_t {
    PlayerTurn,        // 玩家可点击卡牌或结束回合
    BossTurn,          // Boss 回合结算（短暂中间态）
    BoardRefresh,      // 刷新棋盘、恢复能量（短暂中间态）
    GameOver_Win,      // Boss HP ≤ 0（独立模式：游戏胜利）
    GameOver_Lose,     // 玩家 HP ≤ 0
    BattleWon          // Boss HP ≤ 0（进程模式：战斗胜利，等待 RunManager 推进）
};

// ── 战斗事件（供渲染层差异化表现）────────────────────────────────────────────

enum class BattleEvent : uint8_t {
    None,
    AttackPlayed,      // 攻击牌（扣 Boss 血）
    DefensePlayed,     // 防御牌（加护甲）
    StrengthGained,    // 力量增加
    WeakApplied,       // 给 Boss 施加虚弱
    VulnerableApplied, // 给 Boss 施加易伤
    EnergyRefunded,    // 能量回复
    HealPlayed,        // 回血
    SelfDamagePlayed,  // 自伤（龙）
    BossAttacked,      // Boss 攻击玩家
    BoardRefreshed     // 棋盘已刷新
};

// ── 单次操作结算快照 ────────────────────────────────────────────────────────

struct ActionResult {
    GameState    newState;  // 操作后的状态
    BattleEvent  event;    // 触发的事件类型
    int          value;    // 伤害值 / 护甲值 / 治疗量等
};

// ══════════════════════════════════════════════════════════════════════════════
//  核心控制器
// ══════════════════════════════════════════════════════════════════════════════

class GameLogic {
public:
    // 网格常量
    static constexpr int kGridRows    = 4;
    static constexpr int kGridCols    = 4;
    static constexpr int kTotalCards  = kGridRows * kGridCols;  // 16
    static constexpr int kCardTypes   = 8;                       // ID 0-7

    // 玩家初始值
    static constexpr int kInitPlayerHP    = 30;
    static constexpr int kInitMaxEnergy   = 3;

    // Boss 初始值
    static constexpr int kInitBossHP      = 50;

    GameLogic();

    // ── 初始化 ──────────────────────────────────────────────────────────────

    // 重置全部状态，随机生成 16 张卡牌
    void initGame();

    // 带参数初始化（由 RunManager 调用，注入层数数值）
    void initGameWithParams(int playerHP, int bossHP, int maxEnergy, int enemyTier = 0);

    // ── 玩家操作 ────────────────────────────────────────────────────────────

    // 打出一张卡牌，index ∈ [0, 15]
    ActionResult playCard(int index);

    // 主动结束回合（即使 energy > 0 也可调用）
    ActionResult endTurn();

    // ── 查询接口（只读，供渲染层）───────────────────────────────────────────

    const std::array<Card, kTotalCards>& getCards() const noexcept;

    GameState getGameState()      const noexcept;
    int       getPlayerHP()      const noexcept;
    int       getPlayerMaxHP()   const noexcept;
    int       getBlock()         const noexcept;
    int       getEnergy()        const noexcept;
    int       getMaxEnergy()     const noexcept;
    int       getBossHP()        const noexcept;
    int       getBossInitHP()    const noexcept;
    int       getStrength()      const noexcept;
    int       getVulnerable()    const noexcept;
    int       getWeak()          const noexcept;
    int       getEnemyTier()     const noexcept;

    // Boss 意图查询（供渲染层预判显示）
    IntentType getBossIntent()      const noexcept;
    int        getBossIntentValue() const noexcept;

    bool      isGameOver()       const noexcept;  // GameOver_Win | GameOver_Lose | BattleWon
    bool      isBattleWon()      const noexcept;  // 仅 BattleWon
    bool      isPlayerTurn()     const noexcept;

    // 设置运行模式：true → 进程模式（BattleWon），false → 独立模式（GameOver_Win）
    void      setRunBattleMode(bool enabled) noexcept;

    // 卡牌静态工具
    static int  getCardCost(int id) noexcept;

private:
    std::array<Card, kTotalCards> m_cards;
    GameState m_state;
    bool      m_isRunBattle;   // true → 进程模式，胜利后 BattleWon；false → 独立模式，GameOver_Win

    // 玩家属性
    int m_playerHP;
    int m_playerMaxHP;
    int m_block;           // 护甲（回合末归零）
    int m_energy;
    int m_maxEnergy;
    int m_strength;        // 额外力量增伤（永久累积）

    // Boss 属性
    int m_bossHP;
    int m_bossInitHP;
    int m_bossBlock;         // Boss 自身护甲（Defend 意图时获得）
    int m_vulnerableStacks;  // 易伤层数（受伤 ×1.5，回合末 -1）
    int m_weakStacks;        // 虚弱层数（Boss 伤害 ÷2，回合末 -1）
    int m_enemyTier;         // 0-4：按楼层区分敌人行为

    // Boss 意图系统
    IntentType m_nextIntent;
    int        m_intentValue;
    int        m_intentPatternIdx;  // 循环模式索引（0→1→2→3→0...）

    // 内部回合推进
    int  executeBossTurn() noexcept;      // Boss 按意图行动，返回实际扣血量
    void executeBoardRefresh() noexcept;  // 刷新棋盘、恢复能量、生成新意图
    void checkWinLose() noexcept;         // 判定胜负
    void fillDestroyedSlots();            // 补充空白格
    void generateNextIntent();            // 生成下一个 Boss 意图

    // 伤害计算（含易伤倍率、虚弱减半）
    int calcDamage(int baseDmg) const noexcept;
};
