// Copyright (c) 2020, Qihoo, Inc.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef TENSORNET_OPTIMIZER_ADA_GRAD_KERNEL_H_
#define TENSORNET_OPTIMIZER_ADA_GRAD_KERNEL_H_

#include "core/ps/optimizer/optimizer_kernel.h"

namespace tensornet {

class DenseAdaGradValue {
public:
    DenseAdaGradValue(const AdaGrad* opt, int len);

    void SetWeight(butil::IOBuf& w_buf);

    const Eigen::ArrayXf& GetWeight() const {
        return w_;
    }

    void Apply(const AdaGrad* opt, const Eigen::ArrayXf& g);

    size_t DataSize() const {
        return w_.size() * sizeof(float) * 4;
    }

    void Serialized(butil::IOBuf& buf) const;

    void DeSerialized(butil::IOBuf& buf);

private:
    Eigen::ArrayXf w_;
    Eigen::ArrayXf d2sum_;
    Eigen::ArrayXf g2sum_;
    Eigen::ArrayXf m_;
};

typedef DenseKernelBlock<AdaGrad, DenseAdaGradValue> DenseAdaGradKernelBlock;

class SparseAdaGradValue {
public:
    SparseAdaGradValue(int dim, const AdaGrad* opt);

    ~SparseAdaGradValue() {
        if (!IsMiniDim()) {
            delete w_.p;
        }
    }

    int Dim() { return dim_; }

    bool IsMiniDim() {
        // UnionWeight could store two float
        if (2 > dim_) {
            return true;
        } else {
            return false;
        }
    }

    float* Weight() {
        if (IsMiniDim()) {
            return w_.v;
        } else {
            return w_.p;
        }
    }

    float& G2sum() {
        return g2sum_;
    }

    uint32_t Version() {
        return version_;
    }

    void IncreaseVersion() {
        version_++;
    }

    void AddShow(float show) {
        show_ += show;
    }

    void Apply(const AdaGrad* opt, SparseGradInfo& grad_info);

    void Serialized(butil::IOBuf& buf);

    void DeSerialized(butil::IOBuf& buf);

    void ShowDecay(const AdaGrad* opt);

private:
    UnionWeight w_;
    float g2sum_;
    int dim_ = 0;
    uint32_t version_ = 0;
    float show_ = 0.0;
};

typedef SparseKernelBlock<AdaGrad, SparseAdaGradValue> SparseAdaGradKernelBlock;

}  // namespace tensornet

#endif  // !TENSORNET_OPTIMIZER_ADA_GRAD_KERNEL_H_
