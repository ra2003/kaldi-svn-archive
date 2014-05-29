#!/bin/bash

# Copyright 2010-2012 Microsoft Corporation  Johns Hopkins University (Author: Daniel Povey)
# Copyright 2014 Mirsk Digital ApS  (Author: Andreas Kirkedal)

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
# WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
# MERCHANTABLITY OR NON-INFRINGEMENT.
# See the Apache 2 License for the specific language governing permissions and
# limitations under the License.

KALDI_ROOT=$(pwd)/../../..

exproot=$(pwd)
dir=data/local/dict
espeakdir='espeak-1.48.04-source'
mkdir -p $dir


# Dictionary preparation:


# Normalise transcripts and create a transcript file
# Removes '.,:;?' and removes '\' before '\Komma' (dictated ',') 
# outputs a normalised transcript without utterance ids and a list of utterance ids 
echo "Normalising "
python3 local/normalize_transcript_prefixed.py local/norm_dk/numbersUp.tbl data/train/text1 data/train/onlyids $dir/transcripts.tmp

# Additional normalisation, uppercasing, writing numbers etc.
# and recombine with 
local/norm_dk/format_text.sh am $dir/transcripts.tmp > $dir/transcripts.am
cp $dir/transcripts.am data/train/onlytext
paste data/train/onlyids data/train/onlytext > data/train/text 


# lmsents is output by sprak_data_prep.sh and contains
# sentences that are disjoint from the test and dev set 
python3 local/normalize_transcript.py local/norm_dk/numbersUp.tbl data/local/data/lmsents $dir/lmsents.norm
wait

# Create wordlist from the AM transcripts
cat $dir/transcripts.am | tr [:blank:] '\n' | sort -u > $dir/wlist.txt &




# Because training data is read aloud, there are many occurences of the same
# sentence and bias towards the domain. Make a version where  
# the sentences are unique to reduce bias.
local/norm_dk/format_text.sh lm $dir/lmsents.norm > $dir/transcripts.txt
sort -u $dir/transcripts.txt > $dir/transcripts.uniq


# Install eSpeak if it is not installed already

if hash espeak 2>/dev/null;
  then
    echo 'eSpeak installed'
  else
    cd $KALDI_ROOT/tools || exit 1; 
    wget http://sourceforge.net/projects/espeak/files/espeak/espeak-1.48/${espeakdir}.zip
    wait
    unzip -q $espeakdir.zip
    cd $espeakdir/src
    # Remove dependency to portaudio - we only need the text-to-phoneme system
    perl -pi.back -e 's/^(AUDIO = portaudio)$/\#\1/' -e 's/^\#(AUDIO = portaudio2)$/\#\1/' Makefile
    make || exit 1;
    echo 'Installed eSpeak'
    cd $exproot || exit 1;
fi



# Wait for the wordlist to be fully created
wait 


# Run wordlist through espeak to get phonetics
# improvised parallelisation - simple call because 'split' often has different versions
split -l 10000 $dir/wlist.txt $dir/Wtemp_
for w in $dir/Wtemp_*; do
  (cat $w | espeak -q -vda -x > $w.pho) &
done

wait

cat $dir/Wtemp_*.pho > $dir/plist.txt
rm -f $dir/Wtemp_*


# Filter transcription
# Remove diacritics, language annotation ((da), (en), (fr) etc.), insert space between symbols, remove 
# initial and trailing spaces and collapse 2 or more spaces to one space

cat $dir/plist.txt | perl -pe 's/\([[a-z]{2}\)//g' | perl -pe 's// /g' | perl -pe 's/ a I / aI /g' | perl -pe 's/ d Z / dZ /g' | perl -pe 's/ \? / /g' | perl -pe 's/ ([\#]) /\+ /g' | perl -pe 's/([\@n3]) \- /\1\- /g' | perl -pe "s/[\_\:\!\'\,\|2]//g" | perl -pe 's/ \- / /g' | tr -s ' ' | perl -pe 's/^ +| +$//g' > $dir/plist2.txt

#Some question marks are not caught above
perl -pe 's/ \? / /g' $dir/plist2.txt > $dir/plist3.txt

# Create lexicon.txt and put it in data/local/dict
paste $dir/wlist.txt $dir/plist3.txt > $dir/lexicon1.txt

# Remove entries without transcription
grep -P  "^.+\t.+$" $dir/lexicon1.txt > $dir/lexicon2.txt

# Copy pre-made phone table with
cp local/dictsrc/complexphones.txt $dir/nonsilence_phones.txt


# Add "!SIL SIL" to lexicon.txt
echo -e '!SIL\tSIL' > $dir/lex_first
echo -e '<UNK>\tSPN' >> $dir/lex_first
cat $dir/lexicon2.txt >> $dir/lex_first
mv $dir/lex_first $dir/lexicon.txt

# silence phones, one per line.
echo SIL > $dir/silence_phones.txt
echo SIL > $dir/optional_silence.txt

touch $dir/extra_questions.txt


## TODO: add cleanup commands

echo "Dictionary preparation succeeded"
