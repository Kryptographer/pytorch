#include "caffe2/operators/elementwise_ops.h"

#include <algorithm>

namespace caffe2 {

REGISTER_CPU_OPERATOR(
    Not,
    UnaryElementwiseOp<BoolTypes, CPUContext, NotFunctor<CPUContext>>);
REGISTER_CPU_OPERATOR(
    Sign,
    UnaryElementwiseOp<NumericTypes, CPUContext, SignFunctor<CPUContext>>);

#define REGISTER_CPU_COMPARE_OPERATOR(Op)                     \
  REGISTER_CPU_OPERATOR(                                      \
      Op,                                                     \
      BinaryElementwiseOp<                                    \
          TensorTypes<bool, int32_t, int64_t, float, double>, \
          CPUContext,                                         \
          Op##Functor<CPUContext>,                            \
          FixedType<bool>>)

REGISTER_CPU_COMPARE_OPERATOR(EQ);
REGISTER_CPU_COMPARE_OPERATOR(NE);
REGISTER_CPU_COMPARE_OPERATOR(LT);
REGISTER_CPU_COMPARE_OPERATOR(LE);
REGISTER_CPU_COMPARE_OPERATOR(GT);
REGISTER_CPU_COMPARE_OPERATOR(GE);

#undef REGISTER_CPU_COMPARE_OPERATOR

#define REGISTER_CPU_LOGICAL_BINARY_OPERATOR(Op) \
  REGISTER_CPU_OPERATOR(                         \
      Op, BinaryElementwiseOp<BoolTypes, CPUContext, Op##Functor<CPUContext>>)

REGISTER_CPU_LOGICAL_BINARY_OPERATOR(And);
REGISTER_CPU_LOGICAL_BINARY_OPERATOR(Or);
REGISTER_CPU_LOGICAL_BINARY_OPERATOR(Xor);

#undef REGISTER_CPU_LOGICAL_BINARY_OPERATOR

#define REGISTER_CPU_BITWISE_BINARY_OPERATOR(Op) \
  REGISTER_CPU_OPERATOR(                         \
      Op,                                        \
      BinaryElementwiseOp<IntBoolTypes, CPUContext, Op##Functor<CPUContext>>)

REGISTER_CPU_BITWISE_BINARY_OPERATOR(BitwiseAnd);
REGISTER_CPU_BITWISE_BINARY_OPERATOR(BitwiseOr);
REGISTER_CPU_BITWISE_BINARY_OPERATOR(BitwiseXor);

#undef REGISTER_CPU_BITWISE_BINARY_OPERATOR

template <typename T>
void SRLHelper::sum2one(const T* x, T* y, size_t n) {
  *y = ConstEigenArrayMap<T>(x, n, 1).sum();
}

template <typename T>
void SRLHelper::RunWithBroadcastFront(
    const T* x,
    T* y,
    size_t pre,
    size_t n,
    CPUContext*) {
  EigenArrayMap<T>(y, n, 1) = ConstEigenArrayMap<T>(x, n, pre).rowwise().sum();
}

template <typename T>
void SRLHelper::RunWithBroadcastBack(
    const T* x,
    T* y,
    size_t post,
    size_t n,
    CPUContext*) {
  EigenArrayMap<T>(y, 1, n) = ConstEigenArrayMap<T>(x, post, n).colwise().sum();
}

template <typename T>
void SRLHelper::RunWithBroadcast2(
    const T* a,
    T* y,
    size_t pre,
    size_t n,
    size_t post,
    CPUContext*) {
  for (int i = 0; i < n; ++i) {
    y[i] = 0;
    for (int j = 0; j < pre; ++j) {
      for (int k = 0; k < post; ++k) {
        y[i] += a[(j * n + i) * post + k];
      }
    }
  }
}

template <>
template <typename T>
bool SumReduceLikeOp<CPUContext>::DoRunWithType() {
  const auto& A = Input(0);
  const auto& B = Input(1);
  auto* C = Output(0);
  CAFFE_ENFORCE(&B != C, "In-place is not allowed.");
  C->ResizeLike(B);
  const T* Adata = A.template data<T>();
  auto* Cdata = C->template mutable_data<T>();
  if (B.size() == 1) {
    auto count = A.size();
    SRLHelper::sum2one<T>(Adata, Cdata, count);
  } else {
    size_t pre, n, post;
    std::tie(pre, n, post) =
        elementwise_ops_utils::ComputeLegacyBroadcastSizes(A, B, axis_);
    if (post == 1) {
      SRLHelper::RunWithBroadcastFront<T>(Adata, Cdata, pre, n, &context_);
    } else if (pre == 1) {
      SRLHelper::RunWithBroadcastBack<T>(Adata, Cdata, post, n, &context_);
    } else {
      SRLHelper::RunWithBroadcast2<T>(Adata, Cdata, pre, n, post, &context_);
    }
  }
  return true;
}

REGISTER_CPU_OPERATOR(SumReduceLike, SumReduceLikeOp<CPUContext>);

} // namespace caffe2
