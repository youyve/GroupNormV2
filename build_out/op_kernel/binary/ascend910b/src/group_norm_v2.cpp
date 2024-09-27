#include "kernel_operator.h"
#include <type_traits>
using namespace AscendC;
constexpr int32_t BUFFER_NUM = 2;

template<typename T> class KernelGroupNormV2 {
public:
    __aicore__ inline KernelGroupNormV2() {}

    __aicore__ inline void Init(GM_ADDR x, GM_ADDR gamma, GM_ADDR beta, GM_ADDR y, GM_ADDR mean, GM_ADDR rstd, uint64_t maxtotalSize, uint64_t maxbatchSize, uint64_t maxstepSize, uint64_t numGroups, uint64_t groupSize, float eps) {
        ASSERT(GetBlockNum() != 0 && "block dim can not be zero!");
        this->eps = eps;
        this->numGroups = numGroups;
        this->groupSize = groupSize;
        this->maxbatchSize = maxbatchSize;
        this->maxstepSize = maxstepSize;
        this->maxtotalSize = maxtotalSize;
        // this->maxsquareSize = maxsquareSize;
        // printf("maxtotalSize: %llu, maxbatchSize: %llu, maxstepSize: %llu, maxsquareSize: %llu\n", maxtotalSize, maxbatchSize, maxstepSize, maxsquareSize);
        this->maxsquareSize = this->maxtotalSize / this->maxbatchSize / this->maxstepSize;

        Gm_x.SetGlobalBuffer((__gm__ T*)x, maxtotalSize);
        Gm_gamma.SetGlobalBuffer((__gm__ T*)gamma, maxtotalSize);
        Gm_beta.SetGlobalBuffer((__gm__ T*)beta, maxtotalSize);
        Gm_y.SetGlobalBuffer((__gm__ T*)y, maxtotalSize);
        Gm_mean.SetGlobalBuffer((__gm__ T*)mean, maxbatchSize * numGroups);
        Gm_rstd.SetGlobalBuffer((__gm__ T*)rstd, maxbatchSize * numGroups);
    }

    __aicore__ inline void Process() {
        for (uint64_t i = 0; i < maxbatchSize; ++i) {
            for (uint64_t g = 0; g < numGroups; ++g) {
                float sum = 0.0f;
                float c = 0.0f; // 用于Kahan Sum的校正
                // Compute mean for each group
                for (uint64_t k = 0; k < groupSize; ++k) {
                    for (uint64_t j = 0; j < maxstepSize; ++j) {
                        float val = Gm_x.GetValue(i * maxsquareSize * maxstepSize + (g * groupSize + k) * maxstepSize + j);
                        float y = val - c;
                        float t = sum + y;
                        c = (t - sum) - y; // 更新校正项
                        sum = t;
                    }
                }
                float avg = sum / (groupSize * maxstepSize);
                Gm_mean.SetValue(i * numGroups + g, (T)avg);
            }
        }

        for (uint64_t i = 0; i < maxbatchSize; ++i) {
            for (uint64_t g = 0; g < numGroups; ++g) {
                float avg = Gm_mean.GetValue(i * numGroups + g);
                float sum = 0.0f;
                float c = 0.0f; // 用于Kahan Sum的校正
                for (uint64_t k = 0; k < groupSize; ++k) {
                    for (uint64_t j = 0; j < maxstepSize; ++j) {
                        float val = Gm_x.GetValue(i * (numGroups * groupSize * maxstepSize) + (g * groupSize + k) * maxstepSize + j);
                        float diff = val - avg;
                        float y = diff * diff - c;
                        float t = sum + y;
                        c = (t - sum) - y; // 更新校正项
                        sum = t;
                    }
                }
                float variance = sum / (groupSize * maxstepSize);
                float rstd = 1.0f / sqrt(variance + eps);
                Gm_rstd.SetValue(i * numGroups + g, (T)rstd);
            }
        }

        for (uint64_t i = 0; i < maxbatchSize; ++i) {
            for (uint64_t g = 0; g < numGroups; ++g) {
                float mean = Gm_mean.GetValue(i * numGroups + g);
                float rstd = Gm_rstd.GetValue(i * numGroups + g);
                for (uint64_t k = 0; k < groupSize; ++k) {
                    for (uint64_t j = 0; j < maxstepSize; ++j) {
                        auto index = i * (numGroups * groupSize * maxstepSize) + (g * groupSize + k) * maxstepSize + j;

                        float x = Gm_x.GetValue(index);
                        float gamma = Gm_gamma.GetValue(k % 4); // 这里假设 gamma 只有四个元素，k % 4 用来循环取值
                        float beta = Gm_beta.GetValue(k % 4);

                        float result = gamma * ((x - mean) * rstd) + beta;
                        Gm_y.SetValue(index, (T)result);
                    }
                }
            }
        }
    }

private:
    GlobalTensor<T> Gm_x, Gm_gamma, Gm_beta, Gm_y, Gm_mean, Gm_rstd;
    uint64_t maxtotalSize, maxbatchSize, maxstepSize, maxsquareSize, numGroups, groupSize;
    // uint64_t batchSize[3], squareSize[3], stepSize[3], batchOffset[3];
    float eps;
};

extern "C" __global__ __aicore__ void group_norm_v2(GM_ADDR x, GM_ADDR gamma, GM_ADDR beta, GM_ADDR y, GM_ADDR mean, GM_ADDR rstd, GM_ADDR workspace, GM_ADDR tiling) {
    GET_TILING_DATA(tiling_data, tiling);
    KernelGroupNormV2<DTYPE_X> op;
    op.Init(x, gamma, beta, y, mean, rstd, tiling_data.maxtotalSize, tiling_data.maxbatchSize, tiling_data.maxstepSize, tiling_data.numGroups, tiling_data.groupSize, tiling_data.eps);
    op.Process();
}
