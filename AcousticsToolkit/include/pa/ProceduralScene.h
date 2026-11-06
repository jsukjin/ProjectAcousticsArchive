// =============================================================================
// ProceduralScene.h  --  FBX 없이 테스트용 음향 씬을 코드로 만드는 헬퍼 (헤더 전용)
// -----------------------------------------------------------------------------
// Triton.Preprocessor 는 FBX/OBJ 파일뿐 아니라 TritonPreprocessor_AcousticMesh_Add 로
// "정점 + 삼각형(재질 코드 포함)" 을 직접 넣을 수도 있다. 이 헬퍼는 그 입력을 만든다.
//
// 단위는 미터, 좌표계는 Triton(Maya Z-up).  테스트용으로는
//   - 닫힌 박스(방): MeshTypeGeometry
//   - 바닥 사각형  : MeshTypeNavigation  (플레이어가 걸을 수 있는 영역 → 프로브 배치에 사용)
// 두 가지면 충분하다.
// =============================================================================
#pragma once

#include <array>
#include <map>
#include <utility>
#include <vector>

#include "TritonPreprocessorApiTypes.h"

namespace pa
{
    //! AcousticMesh_Add 에 그대로 넘길 수 있는 정점/삼각형 묶음
    struct MeshData
    {
        std::vector<ATKVectorD> vertices;
        std::vector<TritonAcousticMeshTriangleInformation> triangles;
    };

    //! 축 정렬 박스(8정점, 12삼각형).  Triton 은 법선 방향을 중요시하지 않지만
    //! 일관된 와인딩을 위해 바깥쪽이 앞면이 되도록 만든다.
    inline MeshData MakeBox(const ATKVectorD& mn, const ATKVectorD& mx, TritonMaterialCode material)
    {
        MeshData m;
        // 정점 인덱스 비트: bit0=x(min/max), bit1=y, bit2=z
        for (int i = 0; i < 8; ++i)
        {
            m.vertices.emplace_back((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z);
        }
        // 면 6개 × 삼각형 2개. 각 면은 (a,b,c,d) 사각형의 반시계 순서.
        static const int kQuads[6][4] = {
            {0, 2, 3, 1}, // -Z (바닥)
            {4, 5, 7, 6}, // +Z (천장)
            {0, 1, 5, 4}, // -Y
            {2, 6, 7, 3}, // +Y
            {0, 4, 6, 2}, // -X
            {1, 3, 7, 5}, // +X
        };
        for (const auto& q : kQuads)
        {
            m.triangles.push_back({ATKVectorI(q[0], q[1], q[2]), material});
            m.triangles.push_back({ATKVectorI(q[0], q[2], q[3]), material});
        }
        return m;
    }

    //! 수평 사각형(바닥/내비게이션용). z 높이에 놓이며 2삼각형.
    inline MeshData MakeHorizontalQuad(
        double minX, double minY, double maxX, double maxY, double z, TritonMaterialCode material)
    {
        MeshData m;
        m.vertices = {ATKVectorD(minX, minY, z), ATKVectorD(maxX, minY, z), ATKVectorD(maxX, maxY, z),
                      ATKVectorD(minX, maxY, z)};
        m.triangles.push_back({ATKVectorI(0, 1, 2), material});
        m.triangles.push_back({ATKVectorI(0, 2, 3), material});
        return m;
    }

    //! 메시가 "닫힌(watertight) 다면체" 인지 간단히 검사한다: 모든 모서리가 정확히 2개의 삼각형에 공유되어야 한다.
    inline bool IsWatertight(const MeshData& m)
    {
        std::map<std::pair<int, int>, int> edgeUse;
        auto addEdge = [&](int a, int b) { ++edgeUse[{a < b ? a : b, a < b ? b : a}]; };
        for (const auto& t : m.triangles)
        {
            addEdge(t.Indices.x, t.Indices.y);
            addEdge(t.Indices.y, t.Indices.z);
            addEdge(t.Indices.z, t.Indices.x);
        }
        for (const auto& kv : edgeUse)
        {
            if (kv.second != 2) return false;
        }
        return !edgeUse.empty();
    }

    //! 모든 삼각형 인덱스가 정점 범위 안에 있는지 검사한다.
    inline bool HasValidIndices(const MeshData& m)
    {
        const int n = static_cast<int>(m.vertices.size());
        for (const auto& t : m.triangles)
        {
            for (int idx : {t.Indices.x, t.Indices.y, t.Indices.z})
            {
                if (idx < 0 || idx >= n) return false;
            }
        }
        return true;
    }
} // namespace pa
