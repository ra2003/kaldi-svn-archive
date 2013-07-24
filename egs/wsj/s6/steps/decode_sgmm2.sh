#!/bin/bash

# Copyright 2012  Johns Hopkins University (Author: Daniel Povey).  Apache 2.0.

# This script does decoding with an SGMM system, with speaker vectors. 
# If the SGMM system was
# built on top of fMLLR transforms from a conventional system, you should
# provide the --transform-dir option.

# Begin configuration section.
stage=1
transform_dir=    # dir to find fMLLR transforms.
nj=4 # number of decoding jobs.
acwt=0.1  # Just a default value, used for adaptation and beam-pruning..
cmd=run.pl
beam=13.0
gselect=15  # Number of Gaussian-selection indices for SGMMs.  [Note:
            # the first_pass_gselect variable is used for the 1st pass of
            # decoding and can be tighter.
first_pass_gselect=3 # Use a smaller number of Gaussian-selection indices in 
            # the 1st pass of decoding (lattice generation).
max_active=7000
lat_beam=6.0 # Beam we use in lattice generation.
vecs_beam=4.0 # Beam we use to prune lattices while getting posteriors for 
    # speaker-vector computation.  Can be quite tight (actually we could
    # probably just do best-path.
use_fmllr=false
fmllr_iters=10
fmllr_min_count=1000
# End configuration section.

echo "$0 $@"  # Print the command line for logging

[ -f ./path.sh ] && . ./path.sh; # source the path.
. parse_options.sh || exit 1;

if [ $# -ne 3 ]; then
  echo "Usage: steps/decode_sgmm2.sh [options] <graph-dir> <data-dir> <decode-dir>"
  echo " e.g.: steps/decode_sgmm2.sh --transform-dir exp/tri3b/decode_dev93_tgpr \\"
  echo "      exp/sgmm3a/graph_tgpr data/test_dev93 exp/sgmm3a/decode_dev93_tgpr"
  echo "main options (for others, see top of script file)"
  echo "  --transform-dir <decoding-dir>           # directory of previous decoding"
  echo "                                           # where we can find transforms for SAT systems."
  echo "  --config <config-file>                   # config containing options"
  echo "  --nj <nj>                                # number of parallel jobs"
  echo "  --cmd <cmd>                              # Command to run in parallel with"
  echo "  --beam <beam>                            # Decoding beam; default 13.0"
  exit 1;
fi

graphdir=$1
data=$2
dir=$3
srcdir=`dirname $dir`; # Assume model directory one level up from decoding directory.

for f in $graphdir/HCLG.fst $data/feats.scp $srcdir/final.mdl $srcdir/final.mat; do
  [ ! -f $f ] && echo "$0: no such file $f" && exit 1;
done

sdata=$data/split$nj;
silphonelist=`cat $graphdir/phones/silence.csl` || exit 1
gselect_opt="--gselect=ark,s,cs:gunzip -c $dir/gselect.JOB.gz|"
gselect_opt_1stpass="$gselect_opt copy-gselect --n=$first_pass_gselect ark:- ark:- |"

mkdir -p $dir/log
split_data.sh $data $nj || exit 1;
echo $nj > $dir/num_jobs
splice_opts=`cat $srcdir/splice_opts || exit 1` # frame-splicing options.
cmvn_opts=`cat $srcdir/cmvn_opts || exit 1` 


feats="ark,s,cs:apply-cmvn $cmvn_opts --utt2spk=ark:$sdata/JOB/utt2spk scp:$sdata/JOB/cmvn.scp scp:$sdata/JOB/feats.scp ark:- | splice-feats $splice_opts ark:- ark:- | transform-feats $srcdir/final.mat ark:- ark:- |"

if [ ! -z "$transform_dir" ]; then
  echo "$0: using transforms from $transform_dir"
  [ ! -f $transform_dir/trans.1 ] && echo "$0: no such file $transform_dir/trans.1" && exit 1;
  [ "$nj" -ne "`cat $transform_dir/num_jobs`" ] \
    && echo "$0: #jobs mismatch with transform-dir." && exit 1;
  feats="$feats transform-feats --utt2spk=ark:$sdata/JOB/utt2spk ark,s,cs:$transform_dir/trans.JOB ark:- ark:- |"
elif grep 'transform-feats --utt2spk' $srcdir/log/acc.0.1.log 2>/dev/null; then
  echo "$0: **WARNING**: you seem to be using an SGMM system trained with transforms,"
  echo "  but you are not providing the --transform-dir option in test time."
fi
##

## Save Gaussian-selection info to disk.
# Note: we can use final.mdl regardless of whether there is an alignment model--
# they use the same UBM.

if [ $stage -le 1 ]; then
  $cmd JOB=1:$nj $dir/log/gselect.JOB.log \
    sgmm2-gselect --full-gmm-nbest=$gselect $srcdir/final.mdl \
    "$feats" "ark:|gzip -c >$dir/gselect.JOB.gz" || exit 1;
fi

# Generate state-level lattice which we can rescore.  This is done with the alignment
# model and no speaker-vectors.
if [ $stage -le 2 ]; then
  $cmd JOB=1:$nj $dir/log/decode_pass1.JOB.log \
    sgmm2-latgen-faster --max-active=$max_active --beam=$beam --lattice-beam=$lat_beam \
    --acoustic-scale=$acwt --determinize-lattice=false --allow-partial=true \
    --word-symbol-table=$graphdir/words.txt "$gselect_opt_1stpass" $srcdir/final.alimdl \
    $graphdir/HCLG.fst "$feats" "ark:|gzip -c > $dir/pre_lat.JOB.gz" || exit 1;
fi

# Estimate speaker vectors (1st pass).  Prune before determinizing
# because determinization can take a while on un-pruned lattices.
# Note: the sgmm2-post-to-gpost stage is necessary because we have
# a separate alignment-model and final model, otherwise we'd skip it 
# and use sgmm2-est-spkvecs.
if [ $stage -le 3 ]; then
  $cmd JOB=1:$nj $dir/log/vecs_pass1.JOB.log \
    gunzip -c $dir/pre_lat.JOB.gz \| \
    lattice-prune --acoustic-scale=$acwt --beam=$vecs_beam ark:- ark:- \| \
    lattice-determinize-pruned --acoustic-scale=$acwt --beam=$vecs_beam ark:- ark:- \| \
    lattice-to-post --acoustic-scale=$acwt ark:- ark:- \| \
    weight-silence-post 0.0 $silphonelist $srcdir/final.alimdl ark:- ark:- \| \
    sgmm2-post-to-gpost "$gselect_opt" $srcdir/final.alimdl "$feats" ark:- ark:- \| \
    sgmm2-est-spkvecs-gpost --spk2utt=ark:$sdata/JOB/spk2utt \
     $srcdir/final.mdl "$feats" ark,s,cs:- "ark:$dir/pre_vecs.JOB" || exit 1;
fi

# Estimate speaker vectors (2nd pass).  Since we already have spk vectors,
# at this point we need to rescore the lattice to get the correct posteriors.
if [ $stage -le 4 ]; then
  $cmd JOB=1:$nj $dir/log/vecs_pass2.JOB.log \
    gunzip -c $dir/pre_lat.JOB.gz \| \
    sgmm2-rescore-lattice --spk-vecs=ark:$dir/pre_vecs.JOB --utt2spk=ark:$sdata/JOB/utt2spk \
      "$gselect_opt" $srcdir/final.mdl ark:- "$feats" ark:- \| \
    lattice-prune --acoustic-scale=$acwt --beam=$vecs_beam ark:- ark:- \| \
    lattice-determinize-pruned --acoustic-scale=$acwt --beam=$vecs_beam ark:- ark:- \| \
    lattice-to-post --acoustic-scale=$acwt ark:- ark:- \| \
    weight-silence-post 0.0 $silphonelist $srcdir/final.mdl ark:- ark:- \| \
    sgmm2-est-spkvecs --spk2utt=ark:$sdata/JOB/spk2utt "$gselect_opt" --spk-vecs=ark:$dir/pre_vecs.JOB \
     $srcdir/final.mdl "$feats" ark,s,cs:- "ark:$dir/vecs.JOB" || exit 1;
fi
rm $dir/pre_vecs.*

if $use_fmllr; then
  # Estimate fMLLR transforms (note: these may be on top of any
  # fMLLR transforms estimated with the baseline GMM system.
  if [ $stage -le 5 ]; then # compute fMLLR transforms.
    echo "$0: computing fMLLR transforms."
    if [ ! -f $srcdir/final.fmllr_mdl ] || [ $srcdir/final.fmllr_mdl -ot $srcdir/final.mdl ]; then
      echo "$0: computing pre-transform for fMLLR computation."
      sgmm2-comp-prexform $srcdir/final.mdl $srcdir/final.occs $srcdir/final.fmllr_mdl || exit 1;
    fi
    $cmd JOB=1:$nj $dir/log/fmllr.JOB.log \
      gunzip -c $dir/pre_lat.JOB.gz \| \
      sgmm2-rescore-lattice --spk-vecs=ark:$dir/vecs.JOB --utt2spk=ark:$sdata/JOB/utt2spk \
      "$gselect_opt" $srcdir/final.mdl ark:- "$feats" ark:- \| \
      lattice-prune --acoustic-scale=$acwt --beam=$vecs_beam ark:- ark:- \| \
      lattice-determinize-pruned --acoustic-scale=$acwt --beam=$vecs_beam ark:- ark:- \| \
      lattice-to-post --acoustic-scale=$acwt ark:- ark:- \| \
      weight-silence-post 0.0 $silphonelist $srcdir/final.mdl ark:- ark:- \| \
      sgmm2-est-fmllr --spk2utt=ark:$sdata/JOB/spk2utt "$gselect_opt" --spk-vecs=ark:$dir/vecs.JOB \
       --fmllr-iters=$fmllr_iters --fmllr-min-count=$fmllr_min_count \
      $srcdir/final.fmllr_mdl "$feats" ark,s,cs:- "ark:$dir/trans.JOB" || exit 1;
  fi
  feats="$feats transform-feats --utt2spk=ark:$sdata/JOB/utt2spk ark,s,cs:$dir/trans.JOB ark:- ark:- |"  
fi

# Now rescore the state-level lattices with the adapted features and the
# corresponding model.  Prune and determinize the lattices to limit
# their size.
if [ $stage -le 6 ]; then
  $cmd JOB=1:$nj $dir/log/rescore.JOB.log \
    sgmm2-rescore-lattice "$gselect_opt" --utt2spk=ark:$sdata/JOB/utt2spk --spk-vecs=ark:$dir/vecs.JOB \
    $srcdir/final.mdl "ark:gunzip -c $dir/pre_lat.JOB.gz|" "$feats" ark:- \| \
    lattice-determinize-pruned --acoustic-scale=$acwt --beam=$lat_beam ark:- \
    "ark:|gzip -c > $dir/lat.JOB.gz" || exit 1;
fi
rm $dir/pre_lat.*.gz

# The output of this script is the files "lat.*.gz"-- we'll rescore this at different
# acoustic scales to get the final output.


if [ $stage -le 7 ]; then
  [ ! -x local/score.sh ] && \
    echo "Not scoring because local/score.sh does not exist or not executable." && exit 1;
  local/score.sh --cmd "$cmd" $data $graphdir $dir
fi
exit 0;