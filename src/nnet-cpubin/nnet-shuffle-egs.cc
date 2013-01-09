// nnet-cpubin/nnet-shuffle-egs.cc

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

#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "hmm/transition-model.h"
#include "nnet-cpu/nnet-randomize.h"

int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    typedef kaldi::int32 int32;
    typedef kaldi::int64 int64;

    const char *usage =
        "Copy examples (typically single frames) for neural network training,\n"
        "from the input to output, but randomly shuffle the order.  This program will keep\n"
        "all of the examples in memory at once, so don't give it too many.\n"
        "\n"
        "Usage:  nnet-shuffle-egs [options] <egs-rspecifier> <egs-wspecifier>\n"
        "\n"
        "nnet-shuffle-egs --srand=1 ark:train.egs ark:shuffled.egs\n";
    
    int32 srand_seed = 0;
    ParseOptions po(usage);
    po.Register("srand", &srand_seed, "Seed for random number generator ");
    
    po.Read(argc, argv);

    srand(srand_seed);
    
    if (po.NumArgs() != 2) {
      po.PrintUsage();
      exit(1);
    }

    std::string examples_rspecifier = po.GetArg(1),
        examples_wspecifier = po.GetArg(2);

    SequentialNnetTrainingExampleReader example_reader(examples_rspecifier);

    // Putting in an extra level of indirection here to avoid excessive
    // computation and memory demands when we have to resize the vector.
    std::vector<NnetTrainingExample*> egs;

    for (; !example_reader.Done(); example_reader.Next())
      egs.push_back(new NnetTrainingExample(example_reader.Value()));

    std::random_shuffle(egs.begin(), egs.end());
    
    NnetTrainingExampleWriter example_writer(examples_wspecifier);
    for (size_t i = 0; i < egs.size(); i++) {
      std::ostringstream ostr;
      ostr << i;
      example_writer.Write(ostr.str(), *(egs[i]));
      delete egs[i];
    }
   
    KALDI_LOG << "Shuffled order of " << egs.size() << " neural-network training examples ";
    return (egs.size() == 0 ? 1 : 0);
  } catch(const std::exception &e) {
    std::cerr << e.what() << '\n';
    return -1;
  }
}


