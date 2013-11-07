#!/bin/bash

# This runs on the 100 hour subset.  This version of the recipe runs on GPUs.
# We assume you have 8 GPU cards.  You have to use --num-threads 1 so it will
# use the version of the code that can use GPUs (the -parallel training code
# cannot use GPUs unless we make further modifications as the CUDA model assumes
# a single thread per GPU context, and we're not currently set up to create multiple
# GPU contexts.  We assume the queue is set up as in JHU (or
# as in the "Kluster" project on Sourceforge) where "gpu" is a consumable
# resource that you can set to number of GPU cards a machine has.

. cmd.sh

( 
  if [ ! -f exp/nnet5c/final.mdl ]; then
    steps/nnet2/train_tanh.sh --cmd "$decode_cmd" --parallel-opts "-l gpu=1" --io-opts "-tc 5" \
      --num-threads 1 --minibatch-size 512 --max-change 40.0 --mix-up 20000 --samples-per-iter 300000 \
      --num-epochs 10 --num-epochs-extra 3 --initial-learning-rate 0.0067 --final-learning-rate 0.00067 \
      --num-jobs-nnet 10 --num-hidden-layers 5 --hidden-layer-dim 1536 data/train_nodup data/lang \
        exp/tri4b exp/nnet5c || exit 1;
  fi

  for lm_suffix in tg fsh_tgpr; do
    steps/decode_nnet_cpu.sh --cmd "$decode_cmd" --nj 30 \
      --config conf/decode.config --transform-dir exp/tri4b/decode_eval2000_sw1_${lm_suffix} \
      exp/tri4b/graph_sw1_${lm_suffix} data/eval2000 exp/nnet5c/decode_eval2000_sw1_${lm_suffix} &
  done
)