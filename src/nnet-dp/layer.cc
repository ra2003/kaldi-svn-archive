// nnet-dp/layer.cc

// Copyright 2012  Johns Hopkins University (author:  Daniel Povey)

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include "nnet-dp/layer.h"

namespace kaldi {


LinearLayer::LinearLayer(int size, BaseFloat diagonal_element,
                         BaseFloat learning_rate):
    learning_rate_(learning_rate) {
  KALDI_ASSERT(learning_rate >= 0.0 && learning_rate <= 1.0); // > 1.0 doesn't make sense.
  // 0 may be used just to disable learning.
  // Note: if diagonal_element == 1.0, learning will never move away from that
  // point (this is due to the preconditioning).
  KALDI_ASSERT(size > 0 && diagonal_element > 0.0 && diagonal_element <= 1.0);
  params_.Resize(size, size);
  double off_diag = (1.0 - diagonal_element) / (size - 1); // value of
  // off-diagonal elements that's needed if we want each column to sum to one.
  params_.Set(off_diag);
  for (int32 i = 0; i < size; i++)
    params_(i, i) = diagonal_element;
}


// Called from Backward().  Computes "input_deriv".
void TanhLayer::ComputeInputDeriv(const MatrixBase<BaseFloat> &sum_deriv,
                                  MatrixBase<BaseFloat> *input_deriv) const {
  
  // This would be simpler if we didn't allow frame splicing.  For
  // the case with no frame splicing, assume input_dim == full_input_dim.
  
  int32 chunk_size = sum_deriv.NumRows(); // Number of frames in this chunk.  This is fixed
  // during training; the stats take it as part of the initializer.
  int32 input_dim = input_deriv->NumCols(), full_input_dim = params_.NumCols(),
      output_dim = sum_deriv.NumCols();
  int32 num_spliced = full_input_dim / input_dim;
  KALDI_ASSERT(output_dim == params_.NumRows());
  KALDI_ASSERT(full_input_dim == num_spliced * input_dim);
  KALDI_ASSERT(chunk_size + num_spliced - 1 == input_deriv->NumRows()); // We'll shift the
  // input row by 1 each time... this equality has to hold for the splicing to work.

  input_deriv->SetZero();
  for (int32 s = 0; s < num_spliced; s++) { // without frame splicing, only do this once.
    SubMatrix<BaseFloat> input_deriv_part(*input_deriv, s, chunk_size, 0, input_dim);
    SubMatrix<BaseFloat> params_part(params_, 0, output_dim,
                                     input_dim * s, input_dim);
    input_deriv_part.AddMatMat(1.0, sum_deriv, kNoTrans,
                               params_part, kNoTrans, 1.0);
  }
}

void TanhLayer::AdjustLearningRate(const TanhLayer &previous_value,
                                   const TanhLayer &validation_gradient,
                                   BaseFloat learning_rate_ratio) {
  Matrix<BaseFloat> param_delta(params_);
  param_delta.AddMat(-1.0, previous_value.params_);
  BaseFloat dot_prod = TraceMatMat(param_delta, validation_gradient.params_,
                                   kTrans); // dot product if we view
  // the matrices as vectors.
  KALDI_ASSERT(learning_rate_ratio >= 1.0);
  BaseFloat new_learning_rate = learning_rate_;
  if (dot_prod > 0.0) new_learning_rate *= learning_rate_ratio;
  else new_learning_rate /= learning_rate_ratio;
  KALDI_VLOG(1) << "Tanh layer: dot product is " << dot_prod << ", changing learning rate "
                << "from " << learning_rate_ << " to " << new_learning_rate;
  learning_rate_ = new_learning_rate;
}

void LinearLayer::AdjustLearningRate(const LinearLayer &previous_value,
                                     const LinearLayer &validation_gradient,
                                     BaseFloat learning_rate_ratio) {
  Matrix<BaseFloat> param_delta(params_);
  param_delta.AddMat(-1.0, previous_value.params_);
  BaseFloat dot_prod = TraceMatMat(param_delta, validation_gradient.params_,
                                   kTrans); // dot product if we view
  // the matrices as vectors.
  KALDI_ASSERT(learning_rate_ratio >= 1.0);
  BaseFloat new_learning_rate = learning_rate_;
  if (dot_prod > 0.0) new_learning_rate *= learning_rate_ratio;
  else new_learning_rate /= learning_rate_ratio;
  KALDI_VLOG(1) << "Linear layer: dot product is " << dot_prod
                << ", changing learning rate " << "from "
                << learning_rate_ << " to " << new_learning_rate;
  learning_rate_ = new_learning_rate;
}

// TODO: these AdjustLearningRate functions are essentially the same; could
// move them to a base class.
void SoftmaxLayer::AdjustLearningRate(const SoftmaxLayer &previous_value,
                                      const SoftmaxLayer &validation_gradient,
                                      BaseFloat learning_rate_ratio) {
  Matrix<BaseFloat> param_delta(params_);
  param_delta.AddMat(-1.0, previous_value.params_);
  BaseFloat dot_prod = TraceMatMat(param_delta, validation_gradient.params_,
                                   kTrans); // dot product if we view
  // the matrices as vectors.
  KALDI_ASSERT(learning_rate_ratio >= 1.0);
  BaseFloat new_learning_rate = learning_rate_;
  if (dot_prod > 0.0) new_learning_rate *= learning_rate_ratio;
  else new_learning_rate /= learning_rate_ratio;
  KALDI_VLOG(1) << "Softmax layer: dot product is " << dot_prod
                << ", changing learning rate " << "from "
                << learning_rate_ << " to " << new_learning_rate;
  learning_rate_ = new_learning_rate;
}



// The backward pass.  Similar note about sizes and frame-splicing
// applies as in "Forward" [this affects "input" and "input_deriv"].
void LinearLayer::Backward(
    const MatrixBase<BaseFloat> &input,
    const MatrixBase<BaseFloat> &output_deriv,
    MatrixBase<BaseFloat> *input_deriv, // derivative w.r.t. input
    LinearLayer *model_to_update) const {
  
  input_deriv->AddMatMat(1.0, output_deriv, kNoTrans, params_, kNoTrans, 0.0);

  if (model_to_update != NULL)
    model_to_update->Update(input, output_deriv);
}


void LinearLayer::Write(std::ostream &out, bool binary) const {
  WriteToken(out, binary, "<LinearLayer>");
  WriteBasicType(out, binary, learning_rate_);
  params_.Write(out, binary);
  WriteToken(out, binary, "</LinearLayer>");  
}

void LinearLayer::Read(std::istream &in, bool binary) {
  ExpectToken(in, binary, "<LinearLayer>");
  ReadBasicType(in, binary, &learning_rate_);
  params_.Read(in, binary);
  ExpectToken(in, binary, "</LinearLayer>");  
}


// Update model parameters for linear layer.
void LinearLayer::Update(const MatrixBase<BaseFloat> &input,
                         const MatrixBase<BaseFloat> &output_deriv) {
  /*
    Note: for this particular type of layer [which transforms probabilities], we
    store the stats as they relate to the actual probabilities in the matrix, with
    a sum-to-one constraint on each row; but we do the gradient descent in the
    space of unnormalized log probabilities.  This is useful both in terms of
    preconditioning and in order to easily enforce the sum-to-one constraint on
    each column.

    For a column c of the matrix, we have a gradient g.
    Let l be the vector of unnormalized log-probs of that row; it has an arbitrary
    offset, but we just choose the point where it coincides with correctly normalized
    log-probs, so for each element:
    l_i = log(c_i).
    The functional relationship between l_i and c_i is:
    c_i = exp(l_i) / sum_j exp(l_j) . [softmax function from l to c.]
    Let h_i be the gradient w.r.t l_i.  We can compute this as follows.  The softmax function
    has a Jacobian equal to diag(c) - c c^T.  We have:
    h = (diag(c) - c c^T)  g
    We do the gradient-descent step on h, and when we convert back to c, we renormalize.
    [note: the renormalization would not even be necessary if the step size were infinitesimal;
    it's only needed due to second-order effects which slightly unnormalize each column.]
  */        
  
  if (is_gradient_) {
    // We just want the gradient: do a "vanilla" SGD type of update as
    // we would do on any layer.
    params_.AddMatMat(learning_rate_,
                      output_deriv, kTrans,
                      input, kNoTrans, 1.0); 
  } else {
    // This is the update; it is stochastic gradient descent, but
    // it's done in unnormalized-log parameter space.
    int32 num_rows = params_.NumRows(), num_cols = params_.NumCols();
    Matrix<BaseFloat> gradient(num_rows, num_cols); // objf gradient on this chunk.
    gradient.AddMatMat(1.0, output_deriv, kTrans,
                       input, kNoTrans, 1.0); 
    
    for (int32 col = 0; col < num_cols; col++) {
      Vector<BaseFloat> param_col(num_rows);
      param_col.CopyColFromMat(params_, col);
      Vector<BaseFloat> log_param_col(param_col);
      log_param_col.ApplyLog(); // note: works even for zero, but may have -inf
      for (int32 i = 0; i < num_rows; i++)
        if (log_param_col(i) < -1.0e+20)
          log_param_col(i) = -1.0e+20; // get rid of -inf's,as
      // as we're not sure exactly how BLAS will deal with them.
      Vector<BaseFloat> gradient_col(num_rows);
      gradient_col.CopyColFromMat(gradient, col);
      Vector<BaseFloat> log_gradient(num_rows);
      log_gradient.AddVecVec(1.0, param_col, gradient_col, 0.0); // h <-- diag(c) g.
      BaseFloat cT_g = VecVec(param_col, gradient_col);
      log_gradient.AddVec(-cT_g, param_col); // h += (c^T g) c .
      log_param_col.AddVec(learning_rate_, log_gradient); // Gradient step,
      // in unnormalized log-prob space.
      log_param_col.ApplySoftMax(); // Go back to probabilities.
      params_.CopyColFromVec(log_param_col, col); // Write back to parameters.
    }
  }
}


// We initialize the weights to be uniformly distributed on
// [-1/sqrt(n), +1/sqrt(n)], where n is the input dimension.
// Apparently this is widely used: see  glorot10a.pdf (search term), 
// Glorot and Bengio, "Understanding the difficulty of training deep feedforward networks".
TanhLayer::TanhLayer(int input_size, int output_size, BaseFloat learning_rate):
    learning_rate_(learning_rate),
    params_(output_size, input_size) {
  KALDI_ASSERT(input_size > 0 && output_size > 0);
  BaseFloat end_range = 1.0 / sqrt(input_size),
      begin_range = -end_range;
  for (int32 i = 0; i < output_size; i++) {
    for (int32 j = 0; j < input_size; j++) {
      BaseFloat r = begin_range + RandUniform() * (end_range - begin_range);
      params_(i, j) = r;
    }
  }
}

void TanhLayer::Write(std::ostream &out, bool binary) const {
  WriteToken(out, binary, "<TanhLayer>");
  WriteBasicType(out, binary, learning_rate_);
  params_.Write(out, binary);
  WriteToken(out, binary, "</TanhLayer>");  
}

void TanhLayer::Read(std::istream &in, bool binary) {
  ExpectToken(in, binary, "<TanhLayer>");
  ReadBasicType(in, binary, &learning_rate_);
  params_.Read(in, binary);
  ExpectToken(in, binary, "</TanhLayer>");  
}

void TanhLayer::Forward(const MatrixBase<BaseFloat> &input,
                        MatrixBase<BaseFloat> *output) const {
  // This would be simpler if we didn't allow frame splicing.  For
  // the case with no frame splicing, assume input_dim == full_input_dim.
  
  int32 chunk_size = output->NumRows(); // Number of frames in this chunk.  This is fixed
  // during training; the stats take it as part of the initializer.
  int32 input_dim = input.NumCols(), full_input_dim = params_.NumCols(),
      output_dim = output->NumCols();
  int32 num_spliced = full_input_dim / input_dim;
  KALDI_ASSERT(output_dim == params_.NumRows());
  KALDI_ASSERT(full_input_dim == num_spliced * input_dim);
  KALDI_ASSERT(chunk_size + num_spliced - 1 == input.NumRows()); // We'll shift the
  // input row by 1 each time... this equality has to hold for the splicing to work.

  for (int32 s = 0; s < num_spliced; s++) { // without frame splicing, only do this once.
    BaseFloat add_old_output = (s == 0 ? 0.0 : 1.0);
    SubMatrix<BaseFloat> input_part(input, s, chunk_size, 0, input_dim);
    SubMatrix<BaseFloat> params_part(params_, 0, output_dim,
                                     input_dim * s, input_dim);
    output->AddMatMat(1.0, input_part, kNoTrans, params_part, kTrans,
                      add_old_output);
  }
  ApplyTanh(output);
}

// Propagate the derivative back through the nonlinearity.
void TanhLayer::ComputeSumDeriv(const MatrixBase<BaseFloat> &output,
                                const MatrixBase<BaseFloat> &output_deriv,
                                MatrixBase<BaseFloat> *sum_deriv) const {
  /*
    Note on the derivative of the tanh function:
    tanh'(x) = sech^2(x) = -(tanh(x)+1) (tanh(x)-1) = 1 - tanh^2(x)
  */
  sum_deriv->CopyFromMat(output);
  sum_deriv->ApplyPow(2.0);
  sum_deriv->Scale(-1.0);
  sum_deriv->Add(1.0);
  // now sum_deriv is 1.0 - [output^2], which is the derivative of the tanh function.
  sum_deriv->MulElements(output_deriv);
  // now each element of sum_deriv is the derivative of the objective function
  // w.r.t. the input to the tanh function.
}


// The backward pass.  Similar note about sizes and frame-splicing
// applies as in "Forward" [this affects "input" and "input_deriv"].
void TanhLayer::Backward(
    const MatrixBase<BaseFloat> &input,
    const MatrixBase<BaseFloat> &output,
    const MatrixBase<BaseFloat> &output_deriv,
    MatrixBase<BaseFloat> *input_deriv, // derivative w.r.t. input.
    TanhLayer *layer_to_update) const {

  // sum_deriv will be the derivative of the objective function w.r.t. the sum
  // before the sigmoid is applied.
  Matrix<BaseFloat> sum_deriv(output.NumRows(), output.NumCols(),
                              kUndefined);
  ComputeSumDeriv(output, output_deriv, &sum_deriv);

  if (input_deriv)
    ComputeInputDeriv(sum_deriv, input_deriv);

  layer_to_update->Update(input, sum_deriv);
}


void TanhLayer::ApplyTanh(MatrixBase<BaseFloat> *output) const {
  // Apply tanh function to each element of the output...
  // function is -1 + 2 ( 1 / (1 + e^{-2 x}))
  
  int32 num_rows = output->NumRows(), num_cols = output->NumCols(),
      stride = output->Stride();
  
  BaseFloat *data = output->Data();
  for (int32 row = 0; row < num_rows; row++, data += stride) {
    for (int32 col = 0; col < num_cols; col++) {
      // This if-statement is intended to avoid overflow caused by exponentiating
      // large positive values.
      if (*data >= 0.0) {
        *data = -1.0 + 2.0 / (1 + exp(-2.0 * *data));
      } else {
        *data = 1.0 - 2.0 / (1 + exp(2.0 * *data));
      }
    }    
  }
}



void TanhLayer::Update(const MatrixBase<BaseFloat> &input,
                       const MatrixBase<BaseFloat> &sum_deriv) {
  // Note: "input" may have to be spliced.
  int32 input_dim = input.NumCols(), full_input_dim = params_.NumCols(),
      output_dim = sum_deriv.NumCols(),
      num_spliced = full_input_dim / input_dim;
  
  KALDI_ASSERT(output_dim == params_.NumRows());
  KALDI_ASSERT(full_input_dim == num_spliced * input_dim);
  KALDI_ASSERT(sum_deriv.NumRows() + num_spliced - 1 == input.NumRows()); // We'll shift the
  // input row by 1 each time... this equality has to hold for the splicing to work.
  
  for (int32 s = 0; s < num_spliced; s++) { // without frame splicing, we'd only do this once.
    SubMatrix<BaseFloat> input_part(input, s, sum_deriv.NumRows(), 0, input_dim);
    SubMatrix<BaseFloat> params_part(params_, 0, output_dim,
                                     input_dim * s, input_dim);
    params_part.AddMatMat(learning_rate_,
                          sum_deriv, kTrans,
                          input_part, kNoTrans, 1.0);
  }
}


SoftmaxLayer::SoftmaxLayer(int input_size, int output_size, BaseFloat learning_rate):
    learning_rate_(learning_rate),
    params_(output_size, input_size), occupancy_(output_size)
     {
  KALDI_ASSERT(learning_rate_ > 0.0 && learning_rate_ <= 1.0); // Note:
  // learning rate of zero may be used to disable learning for a particular
  // layer, but for this type of layer that doesn't really make sense, in
  // the usage situations we envisage.
}

void SoftmaxLayer::Write(std::ostream &out, bool binary) const {
  WriteToken(out, binary, "<SoftmaxLayer>");
  WriteBasicType(out, binary, learning_rate_);
  params_.Write(out, binary);
  occupancy_.Write(out, binary);
  WriteToken(out, binary, "</SoftmaxLayer>");  
}

void SoftmaxLayer::Read(std::istream &in, bool binary) {
  ExpectToken(in, binary, "<SoftmaxLayer>");
  ReadBasicType(in, binary, &learning_rate_);
  params_.Read(in, binary);
  occupancy_.Read(in, binary);
  ExpectToken(in, binary, "</SoftmaxLayer>");  
}

void SoftmaxLayer::Forward(const MatrixBase<BaseFloat> &input,
                           MatrixBase<BaseFloat> *output) const {
  // This would be simpler if we didn't allow frame splicing.  For
  // the case with no frame splicing, assume input_dim == full_input_dim.
  
  int32 chunk_size = output->NumRows(); // Number of frames in this chunk.  This is fixed
  // during training; the stats take it as part of the initializer.
  int32 input_dim = input.NumCols(), full_input_dim = params_.NumCols(),
      output_dim = output->NumCols();
  int32 num_spliced = full_input_dim / input_dim;
  KALDI_ASSERT(output_dim == params_.NumRows());
  KALDI_ASSERT(full_input_dim == num_spliced * input_dim);
  KALDI_ASSERT(chunk_size + num_spliced - 1 == input.NumRows()); // We'll shift the
  // input row by 1 each time... this equality has to hold for the splicing to work.

  for (int32 s = 0; s < num_spliced; s++) { // without frame splicing, only do this once.
    BaseFloat add_old_output = (s == 0 ? 0.0 : 1.0);
    SubMatrix<BaseFloat> input_part(input, s, chunk_size, 0, input_dim);
    SubMatrix<BaseFloat> params_part(params_, 0, output_dim,
                                     input_dim * s, input_dim);
    output->AddMatMat(1.0, input_part, kNoTrans, params_part, kTrans,
                      add_old_output);
  }
  ApplySoftmax(output);
}

void SoftmaxLayer::ApplySoftmax(MatrixBase<BaseFloat> *output) const {
  // Apply softmax to each row of the output.
  for (int32 r = 0; r < output->NumRows(); r++) {
    SubVector<BaseFloat> row(*output, r);
    row.ApplySoftMax();
  }
}

// Propagate the derivative back through the nonlinearity.
void SoftmaxLayer::ComputeSumDeriv(const MatrixBase<BaseFloat> &output,
                                   const MatrixBase<BaseFloat> &output_deriv,
                                   MatrixBase<BaseFloat> *sum_deriv) const {
  /*
    Note on the derivative of the softmax function: let it be
    p_i = exp(x_i) / sum_i exp_i
    The [matrix-valued] Jacobian of this function is
    diag(p) - p p^T
    Let the derivative vector at the output be e, and at the input be
    d.  We have
    d = diag(p) e - p (p^T e).
    d_i = p_i e_i - p_i (p^T e).    
  */
  const MatrixBase<BaseFloat> &P(output), &E(output_deriv);
  MatrixBase<BaseFloat> &D (*sum_deriv);
  for (int32 r = 0; r < P.NumRows(); r++) {
    SubVector<BaseFloat> p(P, r), e(E, r), d(D, r);
    d.AddVecVec(1.0, p, e, 0.0); // d_i = p_i e_i.
    BaseFloat pT_e = VecVec(p, e); // p^T e.
    d.AddVec(-pT_e, p); // d_i -= (p^T e) p_i
  }
}

// Called from Backward().  Computes "input_deriv".
void SoftmaxLayer::ComputeInputDeriv(const MatrixBase<BaseFloat> &sum_deriv,
                                     MatrixBase<BaseFloat> *input_deriv) const {
  // This would be simpler if we didn't allow frame splicing.  For
  // the case with no frame splicing, assume input_dim == full_input_dim.
  
  int32 chunk_size = sum_deriv.NumRows(); // Number of frames in this chunk.  This is fixed
  // during training; the stats take it as part of the initializer.
  int32 input_dim = input_deriv->NumCols(), full_input_dim = params_.NumCols(),
      output_dim = sum_deriv.NumCols();
  int32 num_spliced = full_input_dim / input_dim;
  KALDI_ASSERT(output_dim == params_.NumRows());
  KALDI_ASSERT(full_input_dim == num_spliced * input_dim);
  KALDI_ASSERT(chunk_size + num_spliced - 1 == input_deriv->NumRows()); // We'll shift the
  // input row by 1 each time... this equality has to hold for the splicing to work.

  input_deriv->SetZero();
  for (int32 s = 0; s < num_spliced; s++) { // without frame splicing, only do this once.
    SubMatrix<BaseFloat> input_deriv_part(*input_deriv, s, chunk_size, 0, input_dim);
    SubMatrix<BaseFloat> params_part(params_, 0, output_dim,
                                     input_dim * s, input_dim);
    input_deriv_part.AddMatMat(1.0, sum_deriv, kNoTrans, params_part, kNoTrans,
                               1.0);
  }
}

// The backward pass.  Similar note about sizes and frame-splicing
// applies as in "Forward" [this affects "input" and "input_deriv"].
void SoftmaxLayer::Backward(
    const MatrixBase<BaseFloat> &input,
    const MatrixBase<BaseFloat> &output,
    const MatrixBase<BaseFloat> &output_deriv,
    MatrixBase<BaseFloat> *input_deriv, // derivative w.r.t. input
    SoftmaxLayer *layer_to_update) const {
  
  // sum_deriv will be the derivative of the objective function w.r.t. the sum
  // before the sigmoid is applied.
  Matrix<BaseFloat> sum_deriv(output.NumRows(), output.NumCols(),
                              kUndefined);
  ComputeSumDeriv(output, output_deriv, &sum_deriv);
  
  ComputeInputDeriv(sum_deriv, input_deriv);
  
  layer_to_update->Update(input, sum_deriv, output);
}


void SoftmaxLayer::Update(const MatrixBase<BaseFloat> &input,
                          const MatrixBase<BaseFloat> &sum_deriv,
                          const MatrixBase<BaseFloat> &output) {
  // Note: "input" may have to be spliced.
  int32 input_dim = input.NumCols(), full_input_dim = params_.NumCols(),
      output_dim = output.NumCols(), num_spliced = full_input_dim / input_dim;
  
  KALDI_ASSERT(output_dim == params_.NumRows());
  KALDI_ASSERT(full_input_dim == num_spliced * input_dim);
  KALDI_ASSERT(output.NumRows() + num_spliced - 1 == input.NumRows()); // We'll shift the
  // input row by 1 each time... this equality has to hold for the splicing to work.
  
  for (int32 s = 0; s < num_spliced; s++) { // without frame splicing, we'd only do this once.
    SubMatrix<BaseFloat> input_part(input, s, output.NumRows(), 0, input_dim);
    SubMatrix<BaseFloat> params_part(params_, 0, output_dim,
                                     input_dim * s, input_dim);
    params_part.AddMatMat(learning_rate_,
                          sum_deriv, kTrans,
                          input_part, kNoTrans, 1.0);
  }
  occupancy_.AddRowSumMat(output);
}



} // namespace kaldi
