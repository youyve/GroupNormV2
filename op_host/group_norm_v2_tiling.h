#include "register/tilingdata_base.h"

namespace optiling {
BEGIN_TILING_DATA_DEF(GroupNormV2TilingData)
  TILING_DATA_FIELD_DEF_ARR(uint64_t, 3, totalSize);
  TILING_DATA_FIELD_DEF_ARR(uint64_t, 3, batchSize);
  TILING_DATA_FIELD_DEF_ARR(uint64_t, 3, stepSize);
  TILING_DATA_FIELD_DEF(uint64_t, packNumber);
  TILING_DATA_FIELD_DEF(uint64_t, maxbatchSize);
  TILING_DATA_FIELD_DEF(uint64_t, maxstepSize);
  TILING_DATA_FIELD_DEF(uint64_t, maxtotalSize);
  TILING_DATA_FIELD_DEF(uint64_t, maxsquareSize);
  TILING_DATA_FIELD_DEF(uint64_t, numGroups);
  TILING_DATA_FIELD_DEF(uint64_t, groupSize);
  TILING_DATA_FIELD_DEF(float, eps);
END_TILING_DATA_DEF;

REGISTER_TILING_DATA_CLASS(GroupNormV2, GroupNormV2TilingData)
}
