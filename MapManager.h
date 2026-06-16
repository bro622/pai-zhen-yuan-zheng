#pragma once

#include <cstdint>
#include <vector>

// ══════════════════════════════════════════════════════════════════════════════
//  地图节点类型
// ══════════════════════════════════════════════════════════════════════════════

enum class NodeType : uint8_t {
    Battle,    // 普通战斗
    Rest,      // 休息营地
    Boss       // 最终 Boss
};

// ── 地图节点 ────────────────────────────────────────────────────────────────

struct MapNode {
    NodeType            type;
    int                 floor;      // 所属层数（0-indexed）
    int                 index;      // 同层横向索引（0-indexed）
    std::vector<int>    nextNodes;  // 下一层可访问节点的横向索引
};

// ══════════════════════════════════════════════════════════════════════════════
//  地图管理器（二选一分支路线）
//  职责：管理树状地图结构、追踪当前位置、提供可达节点查询。
//       不持有玩家数据，不执行战斗逻辑——纯数据层。
// ══════════════════════════════════════════════════════════════════════════════

class MapManager {
public:
    static constexpr int kTotalFloors = 5;

    MapManager();

    // 生成硬编码的 5 层树状地图
    void generateMap();

    // 获取当前节点的可选下一层节点列表
    std::vector<MapNode> getAvailableNextNodes() const;

    // 进入指定下一层节点
    //   targetIndex — 目标节点在下一层的横向索引
    //   返回 true 表示合法且已切换，false 表示不在可达列表中
    bool enterNode(int targetIndex);

    // 标记当前节点为已完成（由 RunManager 在战斗胜利/休息后调用）
    void completeCurrentNode();

    // ── 查询接口 ─────────────────────────────────────────────────────────

    int              getCurrentFloor() const noexcept;
    int              getCurrentNodeIndex() const noexcept;
    const MapNode&   getCurrentNode() const noexcept;
    const MapNode&   getNode(int floor, int index) const noexcept;
    NodeType         getCurrentNodeType() const noexcept;
    bool             isFinalFloor() const noexcept;
    bool             isMapComplete() const noexcept;

private:
    // m_map[floor][index] — 二维树状结构
    std::vector<std::vector<MapNode>> m_map;
    int m_currentFloor;
    int m_currentNodeIndex;
};
