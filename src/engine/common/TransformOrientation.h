#pragma once

#include <algorithm>
#include <glm/glm.hpp>

namespace Atlas::Common {

    inline bool HasReversedWinding(const glm::mat4& matrix) {
        return glm::determinant(glm::mat3(matrix)) < 0.0f;
    }

    template<class Iterator, class MatrixGetter>
    Iterator PartitionByWinding(Iterator begin, Iterator end, MatrixGetter matrixGetter) {
        return std::partition(begin, end, [&](const auto& value) {
            return !HasReversedWinding(matrixGetter(value));
        });
    }

}
