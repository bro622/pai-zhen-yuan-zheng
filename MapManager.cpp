#include "MapManager.h"

// ────────────────────────────────────────────────────────────────────────────────
//  构造
// ────────────────────────────────────────────────────────────────────────────────

MapManager::MapManager()
    : m_currentFloor(0)
    , m_currentNodeIndex(0)
{
    generateMap();
}

// ────────────────────────────────────────────────────────────────────────────────
//  地图生成（硬编码 5 层树状结构）
//
//  Floor 0:  [Battle]
//              ↙     ↘
//  Floor 1:  [Battle]  [Rest]
//              ↙  ↘     ↙  ↘
//  Floor 2:  [Rest] [Battle] [Rest]
//              ↘      ↓      ↙
//  Floor 3:       [Battle] [Rest]
//                 ↘       ↙
//  Floor 4:        [Boss]
// ────────────────────────────────────────────────────────────────────────────────

void MapManager::generateMap()
{
    m_map.clear();
    m_currentFloor     = 0;
    m_currentNodeIndex = 0;

    // ── Floor 0: 1 个节点 (Battle) ─────────────────────────────────────
    {
        std::vector<MapNode> floor0;
        floor0.push_back({ NodeType::Battle, 0, 0, {0, 1} });
        m_map.push_back(std::move(floor0));
    }

    // ── Floor 1: 2 个节点 (Battle, Rest) ───────────────────────────────
    {
        std::vector<MapNode> floor1;
        floor1.push_back({ NodeType::Battle, 1, 0, {0, 1} });  // → floor2[0], floor2[1]
        floor1.push_back({ NodeType::Rest,   1, 1, {1, 2} });  // → floor2[1], floor2[2]
        m_map.push_back(std::move(floor1));
    }

    // ── Floor 2: 3 个节点 (Rest, Battle, Rest) ─────────────────────────
    {
        std::vector<MapNode> floor2;
        floor2.push_back({ NodeType::Rest,   2, 0, {0} });
        floor2.push_back({ NodeType::Battle, 2, 1, {0, 1} });
        floor2.push_back({ NodeType::Rest,   2, 2, {1} });
        m_map.push_back(std::move(floor2));
    }

    // ── Floor 3: 2 个节点 (Battle, Rest) ───────────────────────────────
    {
        std::vector<MapNode> floor3;
        floor3.push_back({ NodeType::Battle, 3, 0, {0} });
        floor3.push_back({ NodeType::Rest,   3, 1, {0} });
        m_map.push_back(std::move(floor3));
    }

    // ── Floor 4: 1 个节点 (Boss) ───────────────────────────────────────
    {
        std::vector<MapNode> floor4;
        floor4.push_back({ NodeType::Boss, 4, 0, {} });  // 终点，无后继
        m_map.push_back(std::move(floor4));
    }
}

// ────────────────────────────────────────────────────────────────────────────────
//  可达节点查询
// ────────────────────────────────────────────────────────────────────────────────

std::vector<MapNode> MapManager::getAvailableNextNodes() const
{
    std::vector<MapNode> result;

    if (m_currentFloor < 0 || m_currentFloor >= kTotalFloors)
        return result;

    if (m_currentNodeIndex < 0 ||
        m_currentNodeIndex >= static_cast<int>(m_map[m_currentFloor].size()))
        return result;

    const MapNode& current = m_map[m_currentFloor][m_currentNodeIndex];

    // 当前节点无后继（Boss 终点）
    if (current.nextNodes.empty())
        return result;

    int nextFloor = m_currentFloor + 1;
    if (nextFloor >= kTotalFloors)
        return result;

    for (int idx : current.nextNodes) {
        if (idx >= 0 && idx < static_cast<int>(m_map[nextFloor].size())) {
            result.push_back(m_map[nextFloor][idx]);
        }
    }

    return result;
}

// ────────────────────────────────────────────────────────────────────────────────
//  节点进入
// ────────────────────────────────────────────────────────────────────────────────

bool MapManager::enterNode(int targetIndex)
{
    // 校验：当前层合法
    if (m_currentFloor < 0 || m_currentFloor >= kTotalFloors)
        return false;

    if (m_currentNodeIndex < 0 ||
        m_currentNodeIndex >= static_cast<int>(m_map[m_currentFloor].size()))
        return false;

    const MapNode& current = m_map[m_currentFloor][m_currentNodeIndex];

    // 校验：targetIndex 在 nextNodes 列表中
    bool found = false;
    for (int next : current.nextNodes) {
        if (next == targetIndex) {
            found = true;
            break;
        }
    }
    if (!found)
        return false;

    // 校验：目标节点在下一层中存在
    int nextFloor = m_currentFloor + 1;
    if (nextFloor >= kTotalFloors)
        return false;
    if (targetIndex < 0 || targetIndex >= static_cast<int>(m_map[nextFloor].size()))
        return false;

    // 切换位置
    m_currentFloor     = nextFloor;
    m_currentNodeIndex = targetIndex;

    return true;
}

void MapManager::completeCurrentNode()
{
    if (m_currentFloor >= 0 && m_currentFloor < kTotalFloors &&
        m_currentNodeIndex >= 0 &&
        m_currentNodeIndex < static_cast<int>(m_map[m_currentFloor].size()))
    {
        // 节点标记已完成（MapNode 无 isCompleted 字段，
        //  由 RunManager 通过状态机隐式追踪，此处预留扩展）
    }
}

// ────────────────────────────────────────────────────────────────────────────────
//  查询接口
// ────────────────────────────────────────────────────────────────────────────────

int MapManager::getCurrentFloor() const noexcept
{
    return m_currentFloor;
}

int MapManager::getCurrentNodeIndex() const noexcept
{
    return m_currentNodeIndex;
}

const MapNode& MapManager::getCurrentNode() const noexcept
{
    return getNode(m_currentFloor, m_currentNodeIndex);
}

const MapNode& MapManager::getNode(int floor, int index) const noexcept
{
    if (floor >= 0 && floor < kTotalFloors &&
        index >= 0 && index < static_cast<int>(m_map[floor].size()))
    {
        return m_map[floor][index];
    }
    // 越界安全回退：返回第 0 层第 0 个节点
    return m_map[0][0];
}

NodeType MapManager::getCurrentNodeType() const noexcept
{
    return getCurrentNode().type;
}

bool MapManager::isFinalFloor() const noexcept
{
    return m_currentFloor == kTotalFloors - 1;
}

bool MapManager::isMapComplete() const noexcept
{
    // Boss 节点在最后一层且无后继 → 进入 Boss 并胜利即通关
    return isFinalFloor() && getCurrentNode().nextNodes.empty();
}
