#include "group_norm_v2_tiling.h"
#include "register/op_def_registry.h"
#include "tiling/platform/platform_ascendc.h"
#include <vector>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cmath> 

namespace optiling {
static ge::graphStatus TilingFunc(gert::TilingContext* context) {
    GroupNormV2TilingData tiling;

    uint64_t totalSize[3] = {};
    uint64_t batchSize[3] = {};
    uint64_t stepSize[3] = {};
    uint64_t length = 0;
    // uint64_t numGroups = 0;
    // float eps = 0;
    uint64_t maxbatchSize = 0;
    uint64_t maxstepSize = 0;
    uint64_t maxtotalSize = 0;
    uint64_t maxsquareSize = 0;


    for (int i = 0; i < 3; ++i)
        length = std::max<uint64_t>(length, context->GetInputShape(i)->GetStorageShape().GetDimNum());
    for (int i = 0; i < 3; ++i) {
        totalSize[i] = context->GetInputShape(i)->GetStorageShape().GetShapeSize();
        const gert::StorageShape* shape = context->GetInputShape(i);
        const char *str = context->GetAttrs()->GetAttrPointer<char>(1);
        // printf("dataformat: %s\n", str);
        auto numGroups = context->GetAttrs()->GetInt(0);
        std::vector<uint64_t> dim(length, 1);
        int n = length;
        for (int j = shape->GetStorageShape().GetDimNum() - 1; j >= 0; --j) {
            dim[--n] = shape->GetStorageShape().GetDim(j);
        }
        uint64_t channelDimSize = 0;

        if (strcmp(str, "NDHWC") == 0) {
            batchSize[i] = dim[0];
            stepSize[i] = dim[4];
            channelDimSize = dim[4]; // C通道数在最后一维
        }
        else if(strcmp(str, "NCDHW") == 0) {
            batchSize[i] = dim[0] * dim[1];
            stepSize[i] = 32;
            channelDimSize = dim[1]; // C通道数在第二维
        }
        else if(strcmp(str, "NHWC") == 0) {
            batchSize[i] = dim[0];
            stepSize[i] = dim[3];
            channelDimSize = dim[3]; // C通道数在最后一维
        }
        else if(strcmp(str, "NCHW") == 0) {
            batchSize[i] = dim[0] * dim[1];
            stepSize[i] = dim[2] * dim[3]
            channelDimSize = dim[1]; // C通道数在第二维
        }
        else {
            batchSize[i] = dim[0] * dim[1];
            stepSize[i] = 32;
            channelDimSize = dim[1]; // 默认情况下，假设通道数在第二维
        }

        if (i == 0) {
            uint64_t groupSize = channelDimSize / *numGroups;
            tiling.set_groupSize(groupSize);
        }

        tiling.set_numGroups(*numGroups);
    }
    tiling.set_totalSize(totalSize);
    tiling.set_batchSize(batchSize);
    tiling.set_stepSize(stepSize);

    auto numGroupsAttr = context->GetAttrs()->GetInt(0);
    auto epsAttr = context->GetAttrs()->GetFloat(2);
    tiling.set_eps(*epsAttr);

    uint64_t numGroups = *numGroupsAttr;  // 使用已获取的属性值
    float eps = *epsAttr;  // 使用已获取的属性值

    for (int i = 0; i < 1; ++i) {
        if (totalSize[i] > maxtotalSize)
            maxtotalSize = totalSize[i];  
    }

    maxbatchSize = batchSize[0];
    maxstepSize = stepSize[0];
    

    // 计算最终的 maxsquareSize
    maxsquareSize = maxtotalSize / maxbatchSize / maxstepSize;

    // 将计算出的参数设置到 tiling 中
    tiling.set_maxtotalSize(maxtotalSize);
    tiling.set_maxbatchSize(maxbatchSize);
    tiling.set_maxstepSize(maxstepSize);
    tiling.set_maxsquareSize(maxsquareSize);

    
    context->SetBlockDim(1);
    tiling.SaveToBuffer(context->GetRawTilingData()->GetData(), context->GetRawTilingData()->GetCapacity());
    context->GetRawTilingData()->SetDataSize(tiling.GetDataSize());
    // size_t *currentWorkspace = context->GetWorkspaceSizes(1);
    // currentWorkspace[0] = 0;
    return ge::GRAPH_SUCCESS;
}
}

namespace ge {
static ge::graphStatus InferShape(gert::InferShapeContext* context)
{
    const gert::Shape* x1_shape = context->GetInputShape(0);
    gert::Shape* y_shape = context->GetOutputShape(0);
    *y_shape = *x1_shape;
    return GRAPH_SUCCESS;
}
}

namespace ops {
class GroupNormV2 : public OpDef {
public:
    explicit GroupNormV2(const char* name) : OpDef(name)
    {
        this->Input("x")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND})
            .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND});
        this->Input("gamma")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND})
            .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND});
        this->Input("beta")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND})
            .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND});
        this->Output("y")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND})
            .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND});
        this->Output("mean")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND})
            .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND});
        this->Output("rstd")
            .ParamType(REQUIRED)
            .DataType({ge::DT_FLOAT16, ge::DT_FLOAT})
            .Format({ge::FORMAT_ND, ge::FORMAT_ND})
            .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND});
        this->Attr("num_groups").Int();
        this->Attr("data_format").AttrType(OPTIONAL).String("NCHW");
        this->Attr("eps").AttrType(OPTIONAL).Float(0.0001);
        this->Attr("is_training").AttrType(OPTIONAL).Bool(true);

        this->SetInferShape(ge::InferShape);

        this->AICore()
            .SetTiling(optiling::TilingFunc);
        this->AICore().AddConfig("ascend910b");

    }
};

OP_ADD(GroupNormV2);
}
